#include "Combat/DCCGameMode.h"
#include "Session/DCCSaveSystem.h"
#include "Session/DCCSession.h"
#include "Session/DCCProfile.h"
#include "Session/DCCBuildStamp.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerState.h"

#include "Combat/DCCGameState.h"
#include "Characters/DCCCharacterBase.h"
#include "Rooms/DCCRoom.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "UI/DCCHUD.h"
#include "UI/DCCEventLogTypes.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SocketSubsystem.h"

ADCCGameMode::ADCCGameMode()
{
	// Defaults rather than per-Blueprint wiring. GameStateClass in particular has
	// a silent failure mode - without ADCCGameState the pause/queue loop does
	// nothing at all, and nothing says so.
	GameStateClass = ADCCGameState::StaticClass();
	HUDClass = ADCCHUD::StaticClass();

	// Everyone arrives at once at the start of a session, so seamless travel and
	// mid-match joining both matter from the first playtest.
	bUseSeamlessTravel = true;
}

ADCCRoom* ADCCGameMode::FindAsyncLoginRoom() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ADCCRoom> It(World); It; ++It)
	{
		ADCCRoom* Room = *It;

		// Derived from room type rather than authored per level, so a floor
		// cannot accidentally declare a boss arena a safe place to appear.
		if (Room && Room->AllowsAsyncLogin())
		{
			return Room;
		}
	}

	return nullptr;
}

float ADCCGameMode::DistanceToNearestCrawler(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return TNumericLimits<float>::Max();
	}

	float Nearest = TNumericLimits<float>::Max();

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		const ADCCCharacterBase* Character = *It;

		// Corpses do not reserve a spawn point. Downed crawlers do - landing on
		// somebody who is bleeding out would shove them around the floor.
		if (!Character || !Character->IsCrawler() || Character->IsPendingKillPending())
		{
			continue;
		}

		if (!Character->IsAlive() && !Character->IsDowned())
		{
			continue;
		}

		Nearest = FMath::Min(Nearest, FVector::Dist(Location, Character->GetActorLocation()));
	}

	return Nearest;
}

AActor* ADCCGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// A crawler with a bedroom goes to their bedroom, before any other rule:
	// the onboarding level is built around everyone waking up in their own room.
	{
		FSavedOnboarding& Saved = OnboardingByController.FindOrAdd(Player);
		if (Saved.CrawlerId.IsNone())
		{
			Saved.CrawlerId = PickFreeCrawlerId();
			UE_LOG(LogTemp, Log, TEXT("[DCC Crawlers] %s assigned crawler %s."), *GetNameSafe(Player), *Saved.CrawlerId.ToString());
		}
		if (!Saved.bHasProgress)
		{
			if (APlayerStart* Bedroom = FindTaggedStart(Saved.CrawlerId))
			{
				return Bedroom;
			}
		}
	}

	// A match already underway means this is a late joiner, and the Safe Room is
	// where they belong - see ADCCRoom::AllowsAsyncLogin, which was built for
	// exactly this and has been waiting for a caller.
	const bool bMatchUnderway = GetWorld()->GetTimeSeconds() > 5.0f;

	if (bRouteLateJoinersToSafeRoom && bMatchUnderway)
	{
		if (ADCCRoom* SafeRoom = FindAsyncLoginRoom())
		{
			// Returned as the start actor itself; the engine spawns at its
			// transform, which for a room is its centre.
			return SafeRoom;
		}
	}

	// Otherwise: the PlayerStart furthest from anyone already standing around.
	AActor* Best = nullptr;
	float BestClearance = -1.0f;

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}

		const float Clearance = DistanceToNearestCrawler(Start->GetActorLocation());

		// An empty level returns FLT_MAX for every start, so the first one wins
		// and the behaviour matches the old single-start default exactly.
		if (Clearance > BestClearance)
		{
			BestClearance = Clearance;
			Best = Start;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("[DCC Spawn] %s: chose %s at %s"), *GetNameSafe(Player), *GetNameSafe(Best),
		Best ? *Best->GetActorLocation().ToCompactString() : TEXT("-"));
	if (Best)
	{
		// Only worth saying when it was actually a problem - five people joining
		// a level with five starts should not print five warnings.
		if (BestClearance < SpawnClearanceRadius)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[DCC Spawn] Best PlayerStart is only %.0f units from another crawler. "
					"Add more PlayerStarts if players are landing on each other."),
				BestClearance);
		}

		return Best;
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void ADCCGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Announced through the same feed as everything else, so the table sees
	// arrivals in the running account of the session rather than only in a log.
	const int32 Count = GetNumPlayers();

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogPlayerJoined",
			"A crawler has entered the floor. {0} now competing. Please welcome them warmly and briefly."),
		FText::AsNumber(Count)));

	UE_LOG(LogTemp, Log, TEXT("[DCC Spawn] Player joined. %d total."), Count);
}

void ADCCGameMode::DCCHost()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ?listen is the whole of it. Everything else in the multiplayer plan -
	// direct IP, Tailscale, no matchmaking - follows from the host simply being
	// a running copy of the game with the flag set.
	const FString LevelName = World->GetMapName();
	const FString Command = FString::Printf(TEXT("open %s?listen"), *LevelName);

	UE_LOG(LogTemp, Warning, TEXT("[DCC Session] Hosting: %s"), *Command);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->ConsoleCommand(Command);
	}
}

void ADCCGameMode::DCCWhoAmI()
{
	// Printed rather than shown in the HUD because the person who needs it is
	// reading it aloud to four other people, and the console can be copied from.
	bool bCanBind = false;
	TSharedPtr<FInternetAddr> Addr;

	if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		Addr = Sockets->GetLocalHostAddr(*GLog, bCanBind);
	}

	const FString Local = Addr.IsValid() ? Addr->ToString(false) : TEXT("unknown");

	UE_LOG(LogTemp, Warning,
		TEXT("[DCC Session] This machine: %s. Others join with:  open %s"), *Local, *Local);

	UE_LOG(LogTemp, Warning,
		TEXT("[DCC Session] If you are not on the same LAN, use the Tailscale address instead - "
			"the LAN one will not route."));
}

// ---------------------------------------------------------------------------
// Crawler identity
// ---------------------------------------------------------------------------

FString ADCCGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	const FString Requested = UGameplayStatics::ParseOption(Options, TEXT("Crawler"));

	// A player from another machine has to prove the crawler is theirs (2026-11-14). The saves are on this host,
	// so this is where the password is checked: the join URL carries a hash, never the password. The host's own
	// player has no connection and was checked at the main menu. A level change reconnects everyone with the same
	// URL, so the hash arrives again each time; a connection that already proved itself this session is also let
	// through, in case a travel ever drops the option.
	if (UNetConnection* Connection = NewPlayerController ? Cast<UNetConnection>(NewPlayerController->Player) : nullptr)
	{
		static TMap<FString, FString> Proven; // crawler -> address that proved it
		const FString Address = Connection->LowLevelGetRemoteAddress(false);
		const FString Hash = UGameplayStatics::ParseOption(Options, TEXT("Pw"));

		// Everyone has to run the same build. A "dev" host (the editor) lets anyone in for testing.
		const FString TheirBuild = UGameplayStatics::ParseOption(Options, TEXT("Build"));
		if (!DCCBuildStamp::IsDev() && TheirBuild != DCCBuildStamp::Get())
		{
			const FString* Seen = Proven.Find(Requested.ToLower());
			if (!(TheirBuild.IsEmpty() && Seen && *Seen == Address))
			{
				UE_LOG(LogTemp, Warning, TEXT("[DCC Crawlers] Refused %s: build %s, host is %s."), *Address,
					TheirBuild.IsEmpty() ? TEXT("(older than stamps)") : *TheirBuild, *DCCBuildStamp::Get());
				return FString::Printf(TEXT("Your game is a different version from the host's (yours: %s, host: %s). Get the latest build, then join again."),
					TheirBuild.IsEmpty() ? TEXT("older") : *TheirBuild, *DCCBuildStamp::Get());
			}
		}
		if (Requested.IsEmpty())
		{
			return TEXT("Pick your crawler on the main menu, then use JOIN A PARTY.");
		}
		const FString* Before = Proven.Find(Requested.ToLower());
		const bool bProvenHere = Before && *Before == Address;
		if (DCCProfile::CheckPassword(Requested, Hash))
		{
			Proven.Add(Requested.ToLower(), Address);
		}
		else if (!(Hash.IsEmpty() && bProvenHere))
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Crawlers] Refused %s as %s: wrong password."), *Address, *Requested);
			return FString::Printf(TEXT("Wrong password for %s. Type it on the main menu, under the crawlers."), *Requested);
		}
	}

	if (!Requested.IsEmpty() && NewPlayerController)
	{
		// Matched case-insensitively against the roster so "?Crawler=cj" works,
		// but stored in the roster's own spelling so it matches PlayerStart tags.
		FName Id(*Requested);
		for (const FName& Known : CrawlerRoster)
		{
			if (Known.ToString().Equals(Requested, ESearchCase::IgnoreCase))
			{
				Id = Known;
				break;
			}
		}
		OnboardingByController.FindOrAdd(NewPlayerController).CrawlerId = Id;
		UE_LOG(LogTemp, Log, TEXT("[DCC Crawlers] %s joined as %s."), *GetNameSafe(NewPlayerController), *Id.ToString());
	}

	return Result;
}

void ADCCGameMode::Logout(AController* Exiting)
{
	// Frees the name so the same person can rejoin as themselves. Progress is
	// lost with it; a rejoin is a fresh wake-up, which is acceptable for now.
	OnboardingByController.Remove(Exiting);

	// Their progress is on disk now, so a rejoin picks up where they left off.
	if (Exiting)
	{
		DCCSave::Save(Cast<ADCCCharacterBase>(Exiting->GetPawn()));
	}
	Super::Logout(Exiting);
}

APlayerStart* ADCCGameMode::FindTaggedStart(FName Tag) const
{
	if (Tag.IsNone() || !GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == Tag)
		{
			return *It;
		}
	}
	return nullptr;
}

FName ADCCGameMode::PickFreeCrawlerId() const
{
	for (const FName& Candidate : CrawlerRoster)
	{
		bool bTaken = false;
		for (const TPair<TWeakObjectPtr<AController>, FSavedOnboarding>& Pair : OnboardingByController)
		{
			if (Pair.Key.IsValid() && Pair.Value.CrawlerId == Candidate)
			{
				bTaken = true;
				break;
			}
		}

		// Only names with a bedroom are handed out automatically, so a level
		// without the onboarding wing leaves everyone unidentified - which is
		// what lets an unidentified crawler pick up any personal item in a test.
		if (!bTaken && FindTaggedStart(Candidate))
		{
			return Candidate;
		}
	}
	return NAME_None;
}

FName ADCCGameMode::GetCrawlerIdFor(AController* Controller) const
{
	const FSavedOnboarding* Saved = OnboardingByController.Find(Controller);
	return Saved ? Saved->CrawlerId : NAME_None;
}

void ADCCGameMode::RememberOnboarding(AController* Controller, const UDCCOnboardingComponent* Onboarding)
{
	if (!Controller || !Onboarding)
	{
		return;
	}

	FSavedOnboarding& Saved = OnboardingByController.FindOrAdd(Controller);
	if (!Onboarding->CrawlerId.IsNone())
	{
		Saved.CrawlerId = Onboarding->CrawlerId;
	}
	Saved.bHasProgress = true;
	Saved.Stage = static_cast<uint8>(Onboarding->GetStage());
	Saved.Features = Onboarding->GetUnlockedFeatures();
}

void ADCCGameMode::RestoreOnboarding(AController* Controller, UDCCOnboardingComponent* Onboarding)
{
	if (!Controller || !Onboarding)
	{
		return;
	}

	const FSavedOnboarding* Saved = OnboardingByController.Find(Controller);
	UE_LOG(LogTemp, Verbose, TEXT("[DCC Crawlers] Restore for %s: %s (progress=%d)"), *GetNameSafe(Controller),
		Saved ? *Saved->CrawlerId.ToString() : TEXT("no record"), Saved ? Saved->bHasProgress : 0);
	if (Saved)
	{
		Onboarding->ApplyRestoredState(Saved->CrawlerId, Saved->bHasProgress,
			static_cast<EDCCOnboardingStage>(Saved->Stage), Saved->Features);
	}

	// Identity across games (2026-11-13). A player's name follows them through
	// seamless travel, so after arriving in another game (where nobody has a
	// tagged PlayerStart) their crawler is recovered from it; the host falls
	// back to the crawler picked in the party menu. Then the name is set to the
	// crawler, so the per-game profile and the DCD save share one key.
	APlayerState* PlayerState = Controller->PlayerState;
	FName Id = Onboarding->CrawlerId;
	if (Id.IsNone() && PlayerState)
	{
		for (const FName& Known : CrawlerRoster)
		{
			if (Known.ToString().Equals(PlayerState->GetPlayerName(), ESearchCase::IgnoreCase))
			{
				Id = Known;
				break;
			}
		}
		if (Id.IsNone() && Controller->IsLocalController())
		{
			Id = DCCSession::GetPreferredCrawler();
		}
		if (!Id.IsNone())
		{
			Onboarding->CrawlerId = Id;
			Onboarding->ApplyCrawlerLook();
		}
	}
	if (!Id.IsNone() && PlayerState && PlayerState->GetPlayerName() != Id.ToString())
	{
		ChangeName(Controller, Id.ToString(), false);
	}
}