#include "Session/DCCProfile.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"

namespace
{
	TMap<FString, TObjectPtr<UDCCProfileSave>>& Cache()
	{
		static TMap<FString, TObjectPtr<UDCCProfileSave>> Map;
		return Map;
	}

	FString SlotFor(const FString& Key)
	{
		FString Safe = Key;
		for (TCHAR& C : Safe)
		{
			if (!FChar::IsAlnum(C) && C != TEXT('_') && C != TEXT('-'))
			{
				C = TEXT('_');
			}
		}
		return TEXT("Profile_") + Safe;
	}

	UDCCProfileSave* Open(const FString& Key)
	{
		if (Key.IsEmpty())
		{
			return nullptr;
		}
		if (TObjectPtr<UDCCProfileSave>* Found = Cache().Find(Key))
		{
			if (*Found)
			{
				return *Found;
			}
		}
		UDCCProfileSave* Profile = Cast<UDCCProfileSave>(UGameplayStatics::LoadGameFromSlot(SlotFor(Key), 0));
		if (!Profile)
		{
			Profile = Cast<UDCCProfileSave>(UGameplayStatics::CreateSaveGameObject(UDCCProfileSave::StaticClass()));
			Profile->PlayerKey = Key;
		}
		Profile->AddToRoot(); // cached for the life of the process, across level travel
		Cache().Add(Key, Profile);
		return Profile;
	}

	TSet<FString>& Dirty()
	{
		static TSet<FString> Keys;
		return Keys;
	}

	void Write(UDCCProfileSave* Profile)
	{
		if (Profile)
		{
			UGameplayStatics::SaveGameToSlot(Profile, SlotFor(Profile->PlayerKey), 0);
			Dirty().Add(Profile->PlayerKey);
		}
	}

	FString BaseSlotFor(const FString& Key)
	{
		return TEXT("ProfileBase_") + SlotFor(Key).RightChop(8); // "Profile_" off the front
	}

	UDCCProfileSave* FromBytes(const TArray<uint8>& Bytes)
	{
		return Bytes.IsEmpty() ? nullptr : Cast<UDCCProfileSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
	}

	/** How a stat merges: 0 add the difference, 1 keep the higher, 2 keep the lower. */
	int32 MergeRule(const FString& StatName)
	{
		FString Game, Stat;
		StatName.Split(TEXT("."), &Game, &Stat);
		if (Stat.StartsWith(TEXT("Fastest")) || Stat.StartsWith(TEXT("Shortest")))
		{
			return 2;
		}
		if (Stat.StartsWith(TEXT("Best")))
		{
			return 1;
		}
		// Truck and mine upgrade levels: a level, not an amount (Scrap is the Chop Shop's money and adds).
		if (Game == TEXT("Chop") && Stat != TEXT("Scrap"))
		{
			return 1;
		}
		return 0;
	}

	FString StatKey(FName Game, FName Stat)
	{
		return Game.ToString() + TEXT(".") + Stat.ToString();
	}
}

namespace DCCProfile
{
	FString KeyFor(const APlayerState* PlayerState)
	{
		return PlayerState ? PlayerState->GetPlayerName() : FString();
	}

	float Get(const APlayerState* PlayerState, FName Game, FName Stat, float Default)
	{
		return GetForKey(KeyFor(PlayerState), Game, Stat, Default);
	}

	float GetForKey(const FString& Key, FName Game, FName Stat, float Default)
	{
		const UDCCProfileSave* Profile = Open(Key);
		const float* Value = Profile ? Profile->Stats.Find(StatKey(Game, Stat)) : nullptr;
		return Value ? *Value : Default;
	}

	bool RecordBest(const APlayerState* PlayerState, FName Game, FName Stat, float Value, bool bHigherIsBetter)
	{
		UDCCProfileSave* Profile = Open(KeyFor(PlayerState));
		if (!Profile)
		{
			return false;
		}
		const FString Key = StatKey(Game, Stat);
		const float* Old = Profile->Stats.Find(Key);
		const bool bBetter = !Old || (bHigherIsBetter ? Value > *Old : Value < *Old);
		if (bBetter)
		{
			Profile->Stats.Add(Key, Value);
			Write(Profile);
		}
		return bBetter;
	}

	void Add(const APlayerState* PlayerState, FName Game, FName Stat, float Delta)
	{
		UDCCProfileSave* Profile = Open(KeyFor(PlayerState));
		if (!Profile)
		{
			return;
		}
		Profile->Stats.FindOrAdd(StatKey(Game, Stat)) += Delta;
		Write(Profile);
	}

	void SetForKey(const FString& Key, FName Game, FName Stat, float Value)
	{
		if (UDCCProfileSave* Profile = Open(Key))
		{
			Profile->Stats.Add(StatKey(Game, Stat), Value);
			Write(Profile);
		}
	}

	const TCHAR* const BasePassword = TEXT("bidet");

	FString HashPassword(const FString& Key, const FString& Password)
	{
		return FMD5::HashAnsiString(*FString::Printf(TEXT("dcc-crawler|%s|%s"), *Key.ToLower(), *Password));
	}

	bool CheckPassword(const FString& Key, const FString& Hash)
	{
		const UDCCProfileSave* Profile = Open(Key);
		if (!Profile || Hash.IsEmpty())
		{
			return false;
		}
		const FString Stored = Profile->PasswordHash.IsEmpty() ? HashPassword(Key, BasePassword) : Profile->PasswordHash;
		return Stored.Equals(Hash, ESearchCase::IgnoreCase);
	}

	bool ChangePassword(const FString& Key, const FString& OldHash, const FString& NewHash)
	{
		UDCCProfileSave* Profile = Open(Key);
		if (!Profile || NewHash.IsEmpty() || !CheckPassword(Key, OldHash))
		{
			return false;
		}
		Profile->PasswordHash = NewHash;
		Write(Profile);
		return true;
	}

	int32 GetTickets(const FString& Key)
	{
		return FMath::RoundToInt(GetForKey(Key, TEXT("Arcade"), TEXT("Tickets"), 0.0f));
	}

	int32 AddTickets(const FString& Key, int32 Delta)
	{
		UDCCProfileSave* Profile = Open(Key);
		if (!Profile)
		{
			return 0;
		}
		float& Balance = Profile->Stats.FindOrAdd(StatKey(TEXT("Arcade"), TEXT("Tickets")));
		Balance = FMath::Max(0.0f, Balance + Delta);
		if (Delta > 0)
		{
			Profile->Stats.FindOrAdd(StatKey(TEXT("Arcade"), TEXT("TicketsEarned"))) += Delta;
		}
		Write(Profile);
		return FMath::RoundToInt(Balance);
	}

	bool SpendTickets(const FString& Key, int32 Price)
	{
		if (Price < 0 || GetTickets(Key) < Price)
		{
			return false;
		}
		AddTickets(Key, -Price);
		return true;
	}

	TArray<FName> GetCosmetics(const FString& Key)
	{
		const UDCCProfileSave* Profile = Open(Key);
		return Profile ? Profile->Cosmetics : TArray<FName>();
	}

	void GrantCosmetic(const FString& Key, FName CosmeticId)
	{
		UDCCProfileSave* Profile = Open(Key);
		if (Profile && !Profile->Cosmetics.Contains(CosmeticId))
		{
			Profile->Cosmetics.Add(CosmeticId);
			Write(Profile);
		}
	}

	TMap<FName, FName> GetWearing(const FString& Key)
	{
		const UDCCProfileSave* Profile = Open(Key);
		return Profile ? Profile->Wearing : TMap<FName, FName>();
	}

	void SetWearing(const FString& Key, FName Slot, FName CosmeticId)
	{
		UDCCProfileSave* Profile = Open(Key);
		if (!Profile)
		{
			return;
		}
		if (CosmeticId.IsNone())
		{
			Profile->Wearing.Remove(Slot);
		}
		else
		{
			Profile->Wearing.Add(Slot, CosmeticId);
		}
		Write(Profile);
	}

	TMap<FName, int32> GetTrophies(const FString& Key)
	{
		const UDCCProfileSave* Profile = Open(Key);
		return Profile ? Profile->Trophies : TMap<FName, int32>();
	}

	void AddTrophy(const FString& Key, FName Id, int32 Count)
	{
		if (UDCCProfileSave* Profile = Open(Key))
		{
			Profile->Trophies.FindOrAdd(Id) += Count;
			Write(Profile);
		}
	}

	bool TakeTrophy(const FString& Key, FName Id)
	{
		UDCCProfileSave* Profile = Open(Key);
		int32* Have = Profile ? Profile->Trophies.Find(Id) : nullptr;
		if (!Have || *Have <= 0)
		{
			return false;
		}
		if (--*Have == 0)
		{
			Profile->Trophies.Remove(Id);
		}
		Write(Profile);
		return true;
	}

	FString Leaderboard(UWorld* World, FName Game, FName Stat, bool bHigherIsBetter, TFunction<FString(float)> Format)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			return FString();
		}
		TArray<TPair<FString, float>> Rows;
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const UDCCProfileSave* Profile = Open(KeyFor(PlayerState));
			if (const float* Value = Profile ? Profile->Stats.Find(StatKey(Game, Stat)) : nullptr)
			{
				Rows.Add({ KeyFor(PlayerState), *Value });
			}
		}
		Rows.Sort([bHigherIsBetter](const TPair<FString, float>& A, const TPair<FString, float>& B)
		{
			return bHigherIsBetter ? A.Value > B.Value : A.Value < B.Value;
		});
		FString Out;
		for (const TPair<FString, float>& Row : Rows)
		{
			Out += FString::Printf(TEXT("%s%s "), Out.IsEmpty() ? TEXT("") : TEXT("   "), *Row.Key) + Format(Row.Value);
		}
		return Out.IsEmpty() ? FString(TEXT("no records yet")) : Out;
	}
}

namespace DCCProfile
{
	TArray<uint8> Export(const FString& Key)
	{
		TArray<uint8> Bytes;
		if (UDCCProfileSave* Profile = Open(Key))
		{
			UGameplayStatics::SaveGameToMemory(Profile, Bytes);
		}
		return Bytes;
	}

	TArray<uint8> ExportBase(const FString& Key)
	{
		TArray<uint8> Bytes;
		if (!Key.IsEmpty())
		{
			if (USaveGame* Base = UGameplayStatics::LoadGameFromSlot(BaseSlotFor(Key), 0))
			{
				UGameplayStatics::SaveGameToMemory(Base, Bytes);
			}
		}
		return Bytes;
	}

	bool MergeFromClient(const FString& Key, const TArray<uint8>& CurrentBytes, const TArray<uint8>& BaseBytes)
	{
		UDCCProfileSave* Host = Open(Key);
		const UDCCProfileSave* Visitor = FromBytes(CurrentBytes);
		const UDCCProfileSave* Base = FromBytes(BaseBytes);
		if (!Host || !Visitor || !Visitor->PlayerKey.Equals(Key, ESearchCase::IgnoreCase))
		{
			return false;
		}
		const bool bHostEmpty = Host->Stats.IsEmpty() && Host->Cosmetics.IsEmpty() && Host->Trophies.IsEmpty();
		const int32 BaseRevision = Base ? Base->Revision : 0;
		if (bHostEmpty || Host->Revision < BaseRevision)
		{
			// This PC's copy is new or stale (the crawler has been merged somewhere newer since): theirs wins outright.
			Host->Stats = Visitor->Stats;
			Host->Cosmetics = Visitor->Cosmetics;
			Host->Wearing = Visitor->Wearing;
			Host->Trophies = Visitor->Trophies;
			if (Host->PasswordHash.IsEmpty())
			{
				Host->PasswordHash = Visitor->PasswordHash;
			}
			UE_LOG(LogTemp, Log, TEXT("[DCC Profile] %s: took their own copy (this PC's was %s)."), *Key, bHostEmpty ? TEXT("empty") : TEXT("older"));
		}
		else
		{
			// Add what they did since the copy they last saw. With no base (the first sync), that is all of it.
			TSet<FString> Names;
			for (const TPair<FString, float>& Pair : Host->Stats) { Names.Add(Pair.Key); }
			for (const TPair<FString, float>& Pair : Visitor->Stats) { Names.Add(Pair.Key); }
			for (const FString& Name : Names)
			{
				const float* Mine = Host->Stats.Find(Name);
				const float* Theirs = Visitor->Stats.Find(Name);
				const float* Was = Base ? Base->Stats.Find(Name) : nullptr;
				const int32 Rule = MergeRule(Name);
				float Result = Mine ? *Mine : 0.0f;
				if (Rule == 1)
				{
					Result = FMath::Max(Mine ? *Mine : -FLT_MAX, Theirs ? *Theirs : -FLT_MAX);
				}
				else if (Rule == 2)
				{
					Result = FMath::Min(Mine ? *Mine : FLT_MAX, Theirs ? *Theirs : FLT_MAX);
				}
				else
				{
					Result += (Theirs ? *Theirs : 0.0f) - (Was ? *Was : 0.0f);
					Result = FMath::Max(0.0f, Result);
				}
				Host->Stats.Add(Name, Result);
			}
			for (const FName& Id : Visitor->Cosmetics) { Host->Cosmetics.AddUnique(Id); }
			if (!Visitor->Wearing.IsEmpty()) { Host->Wearing = Visitor->Wearing; } // what they last chose to wear
			TSet<FName> Ids;
			for (const TPair<FName, int32>& Pair : Host->Trophies) { Ids.Add(Pair.Key); }
			for (const TPair<FName, int32>& Pair : Visitor->Trophies) { Ids.Add(Pair.Key); }
			for (const FName& Id : Ids)
			{
				const int32 Count = Host->Trophies.FindRef(Id) + Visitor->Trophies.FindRef(Id) - (Base ? Base->Trophies.FindRef(Id) : 0);
				if (Count > 0) { Host->Trophies.Add(Id, Count); } else { Host->Trophies.Remove(Id); }
			}
			UE_LOG(LogTemp, Log, TEXT("[DCC Profile] %s: merged their changes since revision %d."), *Key, BaseRevision);
		}
		Host->Revision = FMath::Max(Host->Revision, Visitor->Revision) + 1;
		Write(Host);
		return true;
	}

	void AdoptFromHost(const TArray<uint8>& Merged)
	{
		UDCCProfileSave* Incoming = FromBytes(Merged);
		if (!Incoming || Incoming->PlayerKey.IsEmpty())
		{
			return;
		}
		UDCCProfileSave* Local = Open(Incoming->PlayerKey);
		if (!Local)
		{
			return;
		}
		Local->Stats = Incoming->Stats;
		Local->Cosmetics = Incoming->Cosmetics;
		Local->Wearing = Incoming->Wearing;
		Local->Trophies = Incoming->Trophies;
		Local->Revision = Incoming->Revision;
		if (!Incoming->PasswordHash.IsEmpty())
		{
			Local->PasswordHash = Incoming->PasswordHash;
		}
		UGameplayStatics::SaveGameToSlot(Local, SlotFor(Local->PlayerKey), 0);
		UGameplayStatics::SaveGameToSlot(Incoming, BaseSlotFor(Local->PlayerKey), 0);
	}

	bool TakeDirty(const FString& Key)
	{
		return !Key.IsEmpty() && Dirty().Remove(Key) > 0;
	}
}
