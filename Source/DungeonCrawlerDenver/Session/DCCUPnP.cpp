#include "Session/DCCUPnP.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
	FCriticalSection Lock;
	DCCUPnP::EState State = DCCUPnP::EState::Idle;
	uint32 PublicIp = 0;
	FString Message;
	int32 Version = 0;

	// What Stop needs to undo the mapping.
	FString RouterHost;
	int32 RouterPort = 0;
	FString ControlPath;
	FString ServiceType;
	int32 MappedPort = 0;
	bool bExitHooked = false;

	void Set(DCCUPnP::EState NewState, const FString& Text, uint32 Ip = 0)
	{
		FScopeLock Guard(&Lock);
		State = NewState;
		Message = Text;
		if (Ip != 0) { PublicIp = Ip; }
		++Version;
		UE_LOG(LogTemp, Log, TEXT("[DCC UPnP] %s"), *Text);
	}

	ISocketSubsystem* Net() { return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM); }

	bool ParseIPv4(const FString& Text, uint32& Out)
	{
		TArray<FString> Parts;
		Text.TrimStartAndEnd().ParseIntoArray(Parts, TEXT("."));
		if (Parts.Num() != 4) { return false; }
		Out = 0;
		for (const FString& Part : Parts)
		{
			if (!Part.IsNumeric()) { return false; }
			const int32 V = FCString::Atoi(*Part);
			if (V < 0 || V > 255) { return false; }
			Out = (Out << 8) | uint32(V);
		}
		return true;
	}

	/** A reply header's value, case-insensitively ("LOCATION:" or "Location:"). */
	FString Header(const FString& Reply, const TCHAR* Name)
	{
		TArray<FString> Lines;
		Reply.ParseIntoArrayLines(Lines);
		const FString Want = FString(Name) + TEXT(":");
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(Want, ESearchCase::IgnoreCase))
			{
				return Line.RightChop(Want.Len()).TrimStartAndEnd();
			}
		}
		return FString();
	}

	/** The text between <Tag> and </Tag>, at or after From. */
	FString Between(const FString& Text, const FString& Tag, int32 From = 0)
	{
		const FString Open = TEXT("<") + Tag + TEXT(">");
		const int32 A = Text.Find(Open, ESearchCase::IgnoreCase, ESearchDir::FromStart, From);
		if (A == INDEX_NONE) { return FString(); }
		const int32 B = Text.Find(TEXT("</") + Tag + TEXT(">"), ESearchCase::IgnoreCase, ESearchDir::FromStart, A);
		return B == INDEX_NONE ? FString() : Text.Mid(A + Open.Len(), B - A - Open.Len()).TrimStartAndEnd();
	}

	/** "http://192.168.1.1:5000/rootDesc.xml" -> host, port, path. */
	bool SplitUrl(const FString& Url, FString& Host, int32& Port, FString& Path)
	{
		FString Rest = Url;
		if (!Rest.RemoveFromStart(TEXT("http://"), ESearchCase::IgnoreCase)) { return false; }
		int32 Slash = INDEX_NONE;
		FString HostPort = Rest;
		Path = TEXT("/");
		if (Rest.FindChar(TEXT('/'), Slash))
		{
			HostPort = Rest.Left(Slash);
			Path = Rest.Mid(Slash);
		}
		FString PortText;
		Port = 80;
		if (HostPort.Split(TEXT(":"), &Host, &PortText)) { Port = FCString::Atoi(*PortText); }
		else { Host = HostPort; }
		return !Host.IsEmpty();
	}

	/** Find the router: an SSDP search on the local network, three seconds at most. Returns its description URL. */
	FString Discover()
	{
		ISocketSubsystem* Sockets = Net();
		FSocket* Udp = Sockets ? Sockets->CreateSocket(NAME_DGram, TEXT("DCC UPnP search"), false) : nullptr;
		if (!Udp) { return FString(); }
		Udp->SetNonBlocking(true);
		Udp->SetBroadcast(true);
		TSharedRef<FInternetAddr> To = Sockets->CreateInternetAddr();
		bool bValid = false;
		To->SetIp(TEXT("239.255.255.250"), bValid);
		To->SetPort(1900);
		for (const TCHAR* Target : { TEXT("urn:schemas-upnp-org:device:InternetGatewayDevice:1"), TEXT("urn:schemas-upnp-org:service:WANIPConnection:1"),
			TEXT("urn:schemas-upnp-org:service:WANPPPConnection:1") })
		{
			const FString Search = FString::Printf(TEXT("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: %s\r\n\r\n"), Target);
			const FTCHARToUTF8 Bytes(*Search);
			int32 Sent = 0;
			Udp->SendTo(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length(), Sent, *To);
		}
		FString Found;
		const double Until = FPlatformTime::Seconds() + 3.0;
		uint8 Buffer[2048];
		while (Found.IsEmpty() && FPlatformTime::Seconds() < Until)
		{
			int32 Read = 0;
			TSharedRef<FInternetAddr> From = Sockets->CreateInternetAddr();
			if (Udp->RecvFrom(Buffer, sizeof(Buffer) - 1, Read, *From) && Read > 0)
			{
				Buffer[Read] = 0;
				const FString Reply = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer));
				Found = Header(Reply, TEXT("LOCATION"));
			}
			else
			{
				FPlatformProcess::Sleep(0.05f);
			}
		}
		Udp->Close();
		Sockets->DestroySocket(Udp);
		return Found;
	}

	/** One HTTP/1.1 exchange with the router over a plain TCP socket. Also reports which local address reached it. */
	bool Http(const FString& Host, int32 Port, const FString& Request, FString& OutReply, FString* OutLocalIp = nullptr)
	{
		ISocketSubsystem* Sockets = Net();
		FSocket* Tcp = Sockets ? Sockets->CreateSocket(NAME_Stream, TEXT("DCC UPnP http"), false) : nullptr;
		if (!Tcp) { return false; }
		TSharedRef<FInternetAddr> To = Sockets->CreateInternetAddr();
		bool bValid = false;
		To->SetIp(*Host, bValid);
		To->SetPort(Port);
		bool bOk = bValid && Tcp->Connect(*To);
		if (bOk && OutLocalIp)
		{
			TSharedRef<FInternetAddr> Local = Sockets->CreateInternetAddr();
			Tcp->GetAddress(*Local);
			*OutLocalIp = Local->ToString(false);
		}
		if (bOk)
		{
			const FTCHARToUTF8 Bytes(*Request);
			int32 Sent = 0;
			bOk = Tcp->Send(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length(), Sent) && Sent == Bytes.Length();
		}
		TArray<uint8> All;
		if (bOk)
		{
			const double Until = FPlatformTime::Seconds() + 4.0;
			uint8 Buffer[4096];
			while (FPlatformTime::Seconds() < Until)
			{
				if (!Tcp->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(250))) { continue; }
				int32 Read = 0;
				if (!Tcp->Recv(Buffer, sizeof(Buffer), Read) || Read <= 0) { break; } // closed (we asked for Connection: close)
				All.Append(Buffer, Read);
			}
		}
		Tcp->Close();
		Sockets->DestroySocket(Tcp);
		All.Add(0);
		OutReply = UTF8_TO_TCHAR(reinterpret_cast<const char*>(All.GetData()));
		return bOk && !OutReply.IsEmpty();
	}

	bool Soap(const FString& Action, const FString& Args, FString& OutReply, FString* OutLocalIp = nullptr)
	{
		const FString Body = FString::Printf(TEXT("<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
			"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>\r\n"),
			*Action, *ServiceType, *Args, *Action);
		const FTCHARToUTF8 BodyBytes(*Body);
		const FString Request = FString::Printf(TEXT("POST %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n"
			"SOAPAction: \"%s#%s\"\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s"),
			*ControlPath, *RouterHost, RouterPort, *ServiceType, *Action, BodyBytes.Length(), *Body);
		return Http(RouterHost, RouterPort, Request, OutReply, OutLocalIp) && OutReply.Contains(TEXT(" 200 "));
	}

	bool IsPrivateOrShared(uint32 Ip)
	{
		const uint32 A = Ip >> 24, B = (Ip >> 16) & 255;
		return A == 10 || A == 127 || A == 0 || (A == 172 && B >= 16 && B <= 31) || (A == 192 && B == 168) || (A == 100 && B >= 64 && B <= 127)
			|| (A == 169 && B == 254);
	}

	void Run(int32 Port)
	{
		const FString Location = Discover();
		if (Location.IsEmpty())
		{
			Set(DCCUPnP::EState::Failed, TEXT("No internet invite: the router did not answer (its UPnP may be off). Use the Tailscale invite."));
			return;
		}
		FString Host, Path;
		int32 HostPort = 80;
		FString Description;
		if (!SplitUrl(Location, Host, HostPort, Path)
			|| !Http(Host, HostPort, FString::Printf(TEXT("GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n"), *Path, *Host, HostPort), Description))
		{
			Set(DCCUPnP::EState::Failed, TEXT("No internet invite: could not read the router's description. Use the Tailscale invite."));
			return;
		}
		// The connection service: WANIPConnection (cable, fibre) or WANPPPConnection (DSL).
		FString Control;
		for (const TCHAR* Type : { TEXT("urn:schemas-upnp-org:service:WANIPConnection:2"), TEXT("urn:schemas-upnp-org:service:WANIPConnection:1"),
			TEXT("urn:schemas-upnp-org:service:WANPPPConnection:1") })
		{
			const int32 At = Description.Find(Type, ESearchCase::IgnoreCase);
			if (At != INDEX_NONE)
			{
				Control = Between(Description, TEXT("controlURL"), At);
				ServiceType = Type;
				break;
			}
		}
		if (Control.IsEmpty())
		{
			Set(DCCUPnP::EState::Failed, TEXT("No internet invite: the router does not offer port forwarding. Use the Tailscale invite."));
			return;
		}
		RouterHost = Host;
		RouterPort = HostPort;
		if (Control.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase))
		{
			FString CHost;
			int32 CPort = 80;
			SplitUrl(Control, CHost, CPort, ControlPath);
			RouterHost = CHost;
			RouterPort = CPort;
		}
		else
		{
			ControlPath = Control.StartsWith(TEXT("/")) ? Control : TEXT("/") + Control;
		}

		// Which of this PC's addresses faces the router: the forward points there.
		FString Reply, LocalIp;
		if (!Soap(TEXT("GetExternalIPAddress"), FString(), Reply, &LocalIp))
		{
			Set(DCCUPnP::EState::Failed, TEXT("No internet invite: the router would not say its public address. Use the Tailscale invite."));
			return;
		}
		uint32 External = 0;
		if (!ParseIPv4(Between(Reply, TEXT("NewExternalIPAddress")), External) || External == 0)
		{
			Set(DCCUPnP::EState::Failed, TEXT("No internet invite: the router has no public address yet. Use the Tailscale invite."));
			return;
		}
		if (IsPrivateOrShared(External))
		{
			Set(DCCUPnP::EState::Failed, TEXT("No internet invite: your internet provider shares one address between many homes, so friends cannot reach this PC directly. Use the Tailscale invite."));
			return;
		}
		auto Map = [&](int32 Lease)
		{
			const FString Args = FString::Printf(TEXT("<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>UDP</NewProtocol>"
				"<NewInternalPort>%d</NewInternalPort><NewInternalClient>%s</NewInternalClient><NewEnabled>1</NewEnabled>"
				"<NewPortMappingDescription>Dungeon Crawler Denver</NewPortMappingDescription><NewLeaseDuration>%d</NewLeaseDuration>"), Port, Port, *LocalIp, Lease);
			FString MapReply;
			return Soap(TEXT("AddPortMapping"), Args, MapReply);
		};
		// Some routers only take permanent forwards, some only timed ones: try both.
		if (!Map(0) && !Map(6 * 3600))
		{
			Set(DCCUPnP::EState::Failed, TEXT("No internet invite: the router refused to open the port. Use the Tailscale invite, or forward UDP 7777 by hand."));
			return;
		}
		MappedPort = Port;
		Set(DCCUPnP::EState::Open, TEXT("Internet invite ready: the router opened the game's port. Friends anywhere can use it."), External);
	}
}

namespace DCCUPnP
{
	void Start(int32 Port)
	{
		{
			FScopeLock Guard(&Lock);
			// Once per run: a router that said no will say no again, and asking again every few seconds made the
			// party menu redraw (and ask again) in a loop (found 2026-11-16, first real session).
			if (State != EState::Idle)
			{
				return;
			}
		}
		Set(EState::Working, TEXT("Asking the router for an internet invite..."));
		if (!bExitHooked)
		{
			bExitHooked = true;
			FCoreDelegates::OnPreExit.AddStatic(&DCCUPnP::Stop);
		}
		Async(EAsyncExecution::Thread, [Port]() { Run(Port); });
	}

	EState GetState() { FScopeLock Guard(&Lock); return State; }
	uint32 GetPublicIp() { FScopeLock Guard(&Lock); return PublicIp; }
	FString Describe() { FScopeLock Guard(&Lock); return Message; }
	int32 GetVersion() { FScopeLock Guard(&Lock); return Version; }

	void Stop()
	{
		if (MappedPort == 0 || ControlPath.IsEmpty())
		{
			return;
		}
		const FString Args = FString::Printf(TEXT("<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>UDP</NewProtocol>"), MappedPort);
		FString Reply;
		Soap(TEXT("DeletePortMapping"), Args, Reply);
		MappedPort = 0;
		UE_LOG(LogTemp, Log, TEXT("[DCC UPnP] port closed again."));
	}
}
