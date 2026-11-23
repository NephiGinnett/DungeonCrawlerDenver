#include "Session/DCCIntegrity.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCGameState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Progression/DCCAchievementComponent.h"
#include "Session/DCCProfile.h"
#include "Session/DCCSaveSystem.h"
#include "Stats/DCCStatsComponent.h"

namespace
{
	struct FBucket
	{
		float Tokens = -1.0f; // -1: not started (starts full)
		double Last = 0.0;
		double LastStrike = -1000.0;
	};
	/** "Vaughn|casino" -> bucket. */
	TMap<FString, FBucket>& Buckets()
	{
		static TMap<FString, FBucket> Map;
		return Map;
	}

	/** Key -> real time the garnish ends. */
	TMap<FString, double>& Garnish()
	{
		static TMap<FString, double> Map;
		return Map;
	}

	/** Strikes waiting for a pawn (from the profile merge, which has none). */
	TMap<FString, FString>& PendingStrikes()
	{
		static TMap<FString, FString> Map;
		return Map;
	}

	struct FReportState
	{
		TArray<int32> LastTruth;
		int32 Streak = 0;
	};
	TMap<TWeakObjectPtr<ADCCCharacterBase>, FReportState>& Reports()
	{
		static TMap<TWeakObjectPtr<ADCCCharacterBase>, FReportState> Map;
		return Map;
	}

	FString KeyOf(const ADCCCharacterBase* Who)
	{
		return Who ? DCCProfile::KeyFor(Who->GetPlayerState()) : FString();
	}

	void Say(const UObject* Context, const FString& Line)
	{
		ADCCGameState::Log(Context, EDCCLogCategory::Announcement, FText::FromString(Line));
		UE_LOG(LogTemp, Warning, TEXT("[DCC Integrity] %s"), *Line);
	}

	void SyncTickets(ADCCCharacterBase* Who, const FString& Key)
	{
		if (Who && Who->Wallet)
		{
			Who->Wallet->Tickets = DCCProfile::GetTickets(Key);
		}
	}
}

namespace DCCIntegrity
{
	bool Allow(AActor* WhoActor, const TCHAR* Bucket, float PerSecond, float Burst)
	{
		ADCCCharacterBase* Who = Cast<ADCCCharacterBase>(WhoActor);
		// The host's own crawler and anything that is not a player are trusted: there is nobody to cheat against.
		if (!Who || !Who->HasAuthority() || Who->IsLocallyControlled() || !Who->GetPlayerState())
		{
			return true;
		}
		const FString Key = KeyOf(Who);
		FBucket& B = Buckets().FindOrAdd(Key + TEXT("|") + Bucket);
		const double Now = FPlatformTime::Seconds();
		if (B.Tokens < 0.0f)
		{
			B.Tokens = Burst;
			B.Last = Now;
		}
		B.Tokens = FMath::Min(Burst, B.Tokens + float(Now - B.Last) * PerSecond);
		B.Last = Now;
		if (B.Tokens >= 1.0f)
		{
			B.Tokens -= 1.0f;
			return true;
		}
		// Refused. One strike per burst of spam, not one per refused press.
		if (Now - B.LastStrike > 30.0)
		{
			B.LastStrike = Now;
			Flag(Who, FString::Printf(TEXT("pressing the %s buttons faster than a human hand can"), Bucket));
		}
		return false;
	}

	void Flag(ADCCCharacterBase* Who, const FString& Reason)
	{
		if (!Who || !Who->HasAuthority())
		{
			return;
		}
		const FString Key = KeyOf(Who);
		if (Key.IsEmpty())
		{
			return;
		}
		const int32 Strike = Strikes(Key) + 1;
		DCCProfile::SetForKey(Key, TEXT("Integrity"), TEXT("Strikes"), float(Strike));
		Who->NoteCheatStrikes(Strike);

		// The audience loves a cheater. The house does not.
		const int32 Tickets = DCCProfile::GetTickets(Key);
		FString Line;
		if (Strike == 1)
		{
			const int32 Fine = FMath::Max(1, Tickets / 10);
			DCCProfile::AddTickets(Key, -Fine);
			Line = FString::Printf(TEXT("INTEGRITY NOTICE. %s was caught %s. The audience is thrilled. The house fined %d tickets. That was the warning."),
				*Key, *Reason, Fine);
		}
		else if (Strike == 2)
		{
			const int32 Fine = FMath::Max(1, Tickets / 4);
			DCCProfile::AddTickets(Key, -Fine);
			Garnish().Add(Key, FPlatformTime::Seconds() + 300.0);
			Line = FString::Printf(TEXT("SECOND STRIKE. %s was caught %s, again. Fined %d tickets, and every ticket they win for five minutes goes to the house."),
				*Key, *Reason, Fine);
		}
		else
		{
			const int32 Fine = Tickets / 2;
			DCCProfile::AddTickets(Key, -Fine);
			Garnish().Add(Key, FPlatformTime::Seconds() + 600.0);
			Line = FString::Printf(TEXT("FULL AUDIT. %s was caught %s. Strike %d. Half their tickets (%d) are gone, ten minutes of winnings are garnished, and the nameplate says CHEATER now. It is also open season: every other crawler can hit %s until they leave, and %s cannot hit back."),
				*Key, *Reason, Strike, Fine, *Key, *Key);
		}
		SyncTickets(Who, Key);
		Say(Who, Line);
		UDCCAchievementComponent::RecordFor(Who, EDCCAchievementStat::CheatsCaught, 1);
	}

	void FlagKey(UWorld* World, const FString& Key, const FString& Reason)
	{
		if (!World || Key.IsEmpty())
		{
			return;
		}
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			if (It->IsPlayerControlled() && KeyOf(*It).Equals(Key, ESearchCase::IgnoreCase))
			{
				Flag(*It, Reason);
				return;
			}
		}
		PendingStrikes().Add(Key, Reason);
	}

	void BuildReport(const ADCCCharacterBase* Who, TArray<int32>& Out)
	{
		Out.Reset();
		if (!Who || !Who->Stats)
		{
			return;
		}
		for (int32 i = 0; i < int32(EDCCAbility::MAX); ++i)
		{
			Out.Add(Who->Stats->EnhancedScores.GetScore(EDCCAbility(i)));
		}
		Out.Add(Who->Stats->Level);
		Out.Add(Who->Stats->Gold);
		Out.Add(Who->Wallet ? Who->Wallet->Tickets : 0);
	}

	void CheckReport(ADCCCharacterBase* Who, const TArray<int32>& Seen)
	{
		if (!Who || !Who->HasAuthority() || Who->IsLocallyControlled())
		{
			return;
		}
		// A strike waiting from the profile merge lands now that there is someone to say it to.
		if (FString* Pending = PendingStrikes().Find(KeyOf(Who)))
		{
			const FString Reason = *Pending;
			PendingStrikes().Remove(KeyOf(Who));
			Flag(Who, Reason);
		}
		TArray<int32> Truth;
		BuildReport(Who, Truth);
		FReportState& State = Reports().FindOrAdd(Who);
		if (Seen.Num() != Truth.Num())
		{
			return;
		}
		// Higher than the host's number, and the host's number has not moved: replication has had every chance to catch
		// up. Lower is never suspicious (a payout on its way).
		int32 Worst = -1;
		for (int32 i = 0; i < Truth.Num(); ++i)
		{
			if (Seen[i] > Truth[i])
			{
				Worst = i;
				break;
			}
		}
		const bool bSteady = State.LastTruth == Truth;
		State.LastTruth = Truth;
		if (Worst < 0 || !bSteady)
		{
			State.Streak = FMath::Min(State.Streak, 0);
			if (Worst < 0 && State.Streak < 0) { ++State.Streak; } // the cooldown after a strike runs out
			return;
		}
		if (++State.Streak >= 3)
		{
			static const TCHAR* What[] = { TEXT("Strength"), TEXT("Intelligence"), TEXT("Constitution"), TEXT("Dexterity"), TEXT("Charisma"),
				TEXT("level"), TEXT("gold"), TEXT("tickets") };
			const TCHAR* Name = Worst < UE_ARRAY_COUNT(What) ? What[Worst] : TEXT("numbers");
			UE_LOG(LogTemp, Warning, TEXT("[DCC Integrity] %s shows %s %d, the books say %d"), *KeyOf(Who), Name, Seen[Worst], Truth[Worst]);
			Flag(Who, FString::Printf(TEXT("editing their own %s (%d on their screen, %d on the books)"), Name, Seen[Worst], Truth[Worst]));
			State.Streak = -12; // a minute before the same edit can strike again
		}
	}

	bool IsGarnished(const FString& Key)
	{
		const double* Until = Garnish().Find(Key);
		return Until && FPlatformTime::Seconds() < *Until;
	}

	int32 Strikes(const FString& Key)
	{
		return FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Integrity"), TEXT("Strikes"), 0.0f));
	}

	bool Audit(UWorld* World, const FString& Key, FString& OutReport)
	{
		if (!World || Key.IsEmpty())
		{
			return false;
		}
		// The profile: tickets to 10. The revision jumps, so the host's copy wins the next merge outright
		// (DCCProfile::MergeFromClient) and the audited numbers travel back to the guest's own PC.
		const int32 Was = DCCProfile::GetTickets(Key);
		DCCProfile::AddTickets(Key, 10 - Was);
		DCCProfile::BumpRevision(Key, 1000);

		ADCCCharacterBase* Found = nullptr;
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			if (It->IsPlayerControlled() && KeyOf(*It).Equals(Key, ESearchCase::IgnoreCase))
			{
				Found = *It;
				break;
			}
		}
		if (Found && Found->Stats)
		{
			UDCCStatsComponent* S = Found->Stats;
			for (int32 i = 0; i < int32(EDCCAbility::MAX); ++i)
			{
				S->AbilityScores.SetScore(EDCCAbility(i), 2);
			}
			S->Level = 1;
			S->ExperiencePoints = 0;
			S->StatPointsSpent = 0;
			S->Gold = 0;
			S->RecalculateEnhancedScores(true);
			SyncTickets(Found, Key);
			DCCSave::Save(Found);
		}
		OutReport = FString::Printf(TEXT("%s audited: %d tickets to 10%s."), *Key, Was,
			Found ? TEXT(", level 1, every score 2, no gold") : TEXT(" (not here, so only the tickets)"));
		Say(World, FString::Printf(TEXT("THE AUDIT. Management has gone through %s's books line by line. Level one. Every score a two. Pockets empty. Ten tickets, for the bus."),
			*Key));
		return true;
	}
}
