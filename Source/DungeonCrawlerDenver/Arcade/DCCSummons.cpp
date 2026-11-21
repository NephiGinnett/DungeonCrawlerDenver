#include "Arcade/DCCSummons.h"

#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCArcadeGames.h"
#include "Characters/DCCAllyCharacters.h"
#include "Engine/DataTable.h"
#include "Rooms/DCCDungeonGrid.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Session/DCCProfile.h"
#include "Session/DCCSession.h"
#include "TimerManager.h"

namespace DCCSummons
{
	namespace
	{
		// How often each rarity comes up in a pull, before it is shared among the friends of that rarity.
		constexpr float TierWeights[TierCount] = { 55.0f, 28.0f, 13.0f, 4.0f };

		// The allies standing in this world, in the same order as Party()'s lists. Reset when the world changes
		// (a new game), so each game brings everyone's allies in again.
		TWeakObjectPtr<UWorld> TrackedWorld;
		TMap<FString, TArray<TWeakObjectPtr<ADCCSummonedAlly>>>& Spawned()
		{
			static TMap<FString, TArray<TWeakObjectPtr<ADCCSummonedAlly>>> Map;
			return Map;
		}

		void ResetIfNewWorld(UWorld* World)
		{
			if (TrackedWorld.Get() != World)
			{
				TrackedWorld = World;
				Spawned().Reset();
			}
		}

	}

	const TCHAR* TierName(uint8 Tier)
	{
		switch (Tier)
		{
		case 1: return TEXT("Rare");
		case 2: return TEXT("Epic");
		case 3: return TEXT("Legendary");
		default: return TEXT("Common");
		}
	}

	FLinearColor TierColour(uint8 Tier)
	{
		switch (Tier)
		{
		case 1: return FLinearColor(0.30f, 0.62f, 1.0f);
		case 2: return FLinearColor(0.78f, 0.38f, 1.0f);
		case 3: return FLinearColor(1.0f, 0.74f, 0.22f);
		default: return FLinearColor(0.80f, 0.82f, 0.86f);
		}
	}

	const TCHAR* VariantName(uint8 Variant)
	{
		return Variant == Shiny ? TEXT("Shiny") : Variant == Shadow ? TEXT("Shadow") : TEXT("");
	}

	float ShinyChance(uint8 Tier)
	{
		return 0.08f + 0.06f * FMath::Min<uint8>(Tier, TierCount - 1);
	}

	void ComputeTiers(TArray<uint8>& OutTiers)
	{
		// Popularity, the way the show measures it: Dead Air views, plus a big bump for every win anywhere.
		const TArray<FName>& Friends = DCCSession::Roster();
		TArray<TPair<float, int32>> Ranked;
		for (int32 i = 0; i < Friends.Num(); ++i)
		{
			const FString Key = Friends[i].ToString();
			const float Wins = DCCProfile::GetForKey(Key, TEXT("Climb"), TEXT("Summits")) + DCCProfile::GetForKey(Key, TEXT("Gate"), TEXT("Clears"))
				+ DCCProfile::GetForKey(Key, TEXT("Kart"), TEXT("Wins"));
			Ranked.Add({ DCCProfile::GetForKey(Key, TEXT("Haunt"), TEXT("TotalViews")) + 1500.0f * Wins, i });
		}
		// Most popular first; a tie keeps the roster's order, so it is the same on every pull.
		Ranked.StableSort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key > B.Key; });
		OutTiers.Init(0, Friends.Num());
		for (int32 Rank = 0; Rank < Ranked.Num(); ++Rank)
		{
			OutTiers[Ranked[Rank].Value] = Rank == 0 ? 3 : Rank == 1 ? 2 : Rank <= 3 ? 1 : 0;
		}
	}

	TArray<float> FriendOdds(const TArray<uint8>& Tiers, const FString& Summoner)
	{
		const TArray<FName>& Friends = DCCSession::Roster();
		TArray<float> Odds;
		Odds.Init(0.0f, Friends.Num());
		int32 InTier[TierCount] = { 0, 0, 0, 0 };
		for (int32 i = 0; i < Friends.Num() && i < Tiers.Num(); ++i)
		{
			if (Friends[i].ToString() != Summoner)
			{
				++InTier[FMath::Min<uint8>(Tiers[i], TierCount - 1)];
			}
		}
		float Total = 0.0f;
		for (int32 t = 0; t < TierCount; ++t)
		{
			Total += InTier[t] > 0 ? TierWeights[t] : 0.0f;
		}
		for (int32 i = 0; i < Friends.Num() && i < Tiers.Num() && Total > 0.0f; ++i)
		{
			const uint8 T = FMath::Min<uint8>(Tiers[i], TierCount - 1);
			if (Friends[i].ToString() != Summoner && InTier[T] > 0)
			{
				Odds[i] = TierWeights[T] / Total / InTier[T];
			}
		}
		return Odds;
	}

	FSummon Roll(const TArray<uint8>& Tiers, const FString& Summoner, const UDataTable* EquipmentTable)
	{
		FSummon Out;
		const TArray<FName>& Friends = DCCSession::Roster();
		const TArray<float> Odds = FriendOdds(Tiers, Summoner);
		float Pick = FMath::FRand();
		for (int32 i = 0; i < Odds.Num(); ++i)
		{
			if (Odds[i] <= 0.0f)
			{
				continue;
			}
			Out.Friend = Friends[i];
			Out.Tier = Tiers.IsValidIndex(i) ? Tiers[i] : 0;
			Pick -= Odds[i];
			if (Pick <= 0.0f)
			{
				break;
			}
		}
		const float Look = FMath::FRand();
		Out.Variant = Look < ShinyChance(Out.Tier) ? Shiny : Look < ShinyChance(Out.Tier) + ShadowChance ? Shadow : Plain;
		if (EquipmentTable && FMath::FRand() < ArmedChance)
		{
			// Any one-handed or two-handed weapon in the gear table.
			TArray<FName> Weapons;
			for (const TPair<FName, uint8*>& Row : EquipmentTable->GetRowMap())
			{
				const FDCCEquipmentDef* Def = reinterpret_cast<const FDCCEquipmentDef*>(Row.Value);
				if (Def && Def->Slot == EDCCEquipmentSlot::MainHand)
				{
					Weapons.Add(Row.Key);
				}
			}
			if (!Weapons.IsEmpty())
			{
				Out.Weapon = Weapons[FMath::RandRange(0, Weapons.Num() - 1)];
			}
		}
		return Out;
	}

	TMap<FString, TArray<FSummon>>& Party()
	{
		static TMap<FString, TArray<FSummon>> Map;
		return Map;
	}

	void Track(UWorld* World, const FString& Player, ADCCSummonedAlly* Ally)
	{
		ResetIfNewWorld(World);
		Spawned().FindOrAdd(Player).Add(Ally);
	}

	ADCCSummonedAlly* SpawnAlly(UWorld* World, const FSummon& Summon, ADCCCharacterBase* Owner, const FVector& Where, float DropHeight, const FVector& Exit)
	{
		if (!World || Summon.Friend.IsNone())
		{
			return nullptr;
		}
		const FRotator Facing = Owner ? FRotator(0.0f, (Owner->GetActorLocation() - Where).Rotation().Yaw, 0.0f) : FRotator::ZeroRotator;
		const FTransform Transform(Facing, Where + FVector(0.0f, 0.0f, 100.0f + DropHeight));
		ADCCSummonedAlly* Ally = World->SpawnActorDeferred<ADCCSummonedAlly>(ADCCSummonedAlly::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Ally)
		{
			return nullptr;
		}
		Ally->Friend = Summon.Friend;
		Ally->Tier = Summon.Tier;
		Ally->Variant = Summon.Variant;
		Ally->ExitDirection = Exit;
		Ally->SummonerName = Owner && Owner->GetPlayerState() ? Owner->GetPlayerState()->GetPlayerName() : FString();
		Ally->FollowOwner = Owner;
		// Named before it is first seen, so its nameplate never shows the engine's name while it comes down.
		Ally->DisplayName = FText::FromString(FString::Printf(TEXT("%s%s%s"), VariantName(Summon.Variant), Summon.Variant ? TEXT(" ") : TEXT(""), *Summon.Friend.ToString()));
		if (DropHeight > 0.0f)
		{
			// No brain until it has landed: it floats down the tube, then steps out.
			Ally->AutoPossessAI = EAutoPossessAI::Disabled;
		}
		Ally->FinishSpawning(Transform);
		// How it gets about: the rooms' grid where the game has one, otherwise straight at its crawler.
		ADCCArcadeArena* Arena = nullptr;
		for (TActorIterator<ADCCArcadeArena> It(World); It; ++It) { Arena = *It; break; }
		if (UDCCDungeonGrid* Grid = Arena ? Arena->GetChaseGrid() : nullptr)
		{
			Ally->SetChaseGrid(Grid);
		}
		else
		{
			Ally->bFollowStraight = true;
		}
		if (Ally->Onboarding)
		{
			Ally->Onboarding->CrawlerId = Summon.Friend;
			Ally->Onboarding->ApplyCrawlerLook();
		}
		Ally->DisplayName = FText::FromString(FString::Printf(TEXT("%s%s%s"), VariantName(Summon.Variant), Summon.Variant ? TEXT(" ") : TEXT(""), *Summon.Friend.ToString()));
		if (!Summon.Weapon.IsNone() && Ally->Equipment)
		{
			Ally->Equipment->Equip(Summon.Weapon);
		}

		if (DropHeight > 0.0f)
		{
			if (UCharacterMovementComponent* Move = Ally->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Flying);
				Move->GravityScale = 0.0f;
			}
			// Down the tube over a couple of seconds, easing in at the bottom.
			const FVector Top = Transform.GetLocation();
			const FVector Bottom = Where + FVector(0.0f, 0.0f, 100.0f);
			const double Start = World->GetTimeSeconds() + 0.9; // after the beam has lit
			const double Land = Start + 0.55;                    // shot down the tube, not floated
			const double Out = Land + 0.9;                       // then out through the cut-out in the glass
			const FVector Exit = Ally->ExitDirection.IsNearlyZero() ? FVector(0.0f, 1.0f, 0.0f) : Ally->ExitDirection;
			TWeakObjectPtr<ADCCSummonedAlly> Weak(Ally);
			TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
			TSharedRef<bool> bLanded = MakeShared<bool>(false);
			World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([Weak, Top, Bottom, Start, Land, Out, Exit, Handle, bLanded]()
			{
				ADCCSummonedAlly* Self = Weak.Get();
				if (!Self || !Self->GetWorld())
				{
					return;
				}
				const double Now = Self->GetWorld()->GetTimeSeconds();
				if (!*bLanded)
				{
					const float U = FMath::Clamp(float((Now - Start) / (Land - Start)), 0.0f, 1.0f);
					Self->SetActorLocation(FMath::Lerp(Top, Bottom, U * U)); // speeding up all the way down
					if (U >= 1.0f)
					{
						*bLanded = true;
						if (UCharacterMovementComponent* Move = Self->GetCharacterMovement())
						{
							Move->GravityScale = 1.0f;
							Move->SetMovementMode(MOVE_Walking);
						}
						Self->SpawnDefaultController();
						Self->LaunchCharacter(FVector(0.0f, 0.0f, 220.0f), false, true); // a little bounce on landing
					}
					return;
				}
				// Walk out through the gap in the glass, then the usual following takes over.
				if (Now < Out)
				{
					Self->AddMovementInput(Exit, 1.0f);
					return;
				}
				Self->GetWorld()->GetTimerManager().ClearTimer(*Handle);
			}), 0.02f, true);
		}
		UE_LOG(LogTemp, Log, TEXT("[DCC Summon] %s%s (%s)%s for %s"), *FString::Printf(TEXT("%s%s"), VariantName(Summon.Variant), Summon.Variant ? TEXT(" ") : TEXT("")), *Summon.Friend.ToString(),
			TierName(Summon.Tier), Summon.Weapon.IsNone() ? TEXT("") : *FString::Printf(TEXT(" armed with %s"), *Summon.Weapon.ToString()), *Ally->SummonerName);
		return Ally;
	}

	void BringAlong(UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return;
		}
		ResetIfNewWorld(World);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			ADCCCharacterBase* Owner = Cast<ADCCCharacterBase>(PC ? PC->GetPawn() : nullptr);
			const FString Name = PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
			TArray<FSummon>* List = Name.IsEmpty() ? nullptr : Party().Find(Name);
			if (!List || List->IsEmpty())
			{
				continue;
			}
			TArray<TWeakObjectPtr<ADCCSummonedAlly>>& Here = Spawned().FindOrAdd(Name);
			// A death forgets that ally. A missing body (the last game unloaded it, or the spawn failed) does not:
			// that slot is filled again below, in place, so the lists stay paired.
			for (int32 i = FMath::Min(Here.Num(), List->Num()) - 1; i >= 0; --i)
			{
				ADCCSummonedAlly* Ally = Here[i].Get();
				if (Ally && !Ally->IsAlive())
				{
					UE_LOG(LogTemp, Log, TEXT("[DCC Summon] %s's %s went down."), *Name, *(*List)[i].Friend.ToString());
					List->RemoveAt(i);
					Here.RemoveAt(i);
				}
				else if (Ally && Owner && Ally->FollowOwner.Get() != Owner)
				{
					Ally->FollowOwner = Owner; // a respawned body
				}
			}
			if (!Owner || !Owner->IsAlive())
			{
				continue;
			}
			auto Beside = [Owner](int32 i)
			{
				return Owner->GetActorRightVector() * (i % 2 ? -160.0f : 160.0f) - Owner->GetActorForwardVector() * 120.0f;
			};
			for (int32 i = 0; i < FMath::Min(Here.Num(), List->Num()); ++i)
			{
				if (Here[i].IsValid())
				{
					continue;
				}
				if (ADCCSummonedAlly* Born = SpawnAlly(World, (*List)[i], Owner, Owner->GetActorLocation() - FVector(0, 0, 90.0f) + Beside(i), 0.0f))
				{
					Here[i] = Born;
					UE_LOG(LogTemp, Log, TEXT("[DCC Summon] %s's %s came along"), *Name, *(*List)[i].Friend.ToString());
				}
			}
			while (Here.Num() < List->Num())
			{
				const int32 i = Here.Num();
				ADCCSummonedAlly* Born = SpawnAlly(World, (*List)[i], Owner, Owner->GetActorLocation() - FVector(0, 0, 90.0f) + Beside(i), 0.0f);
				Here.Add(Born);
				if (Born)
				{
					UE_LOG(LogTemp, Log, TEXT("[DCC Summon] %s's %s came along"), *Name, *(*List)[i].Friend.ToString());
				}
			}
		}
	}

	void TickArena(ADCCArcadeArena* Arena)
	{
		if (Arena && Arena->HasAuthority())
		{
			BringAlong(Arena->GetWorld());
		}
	}
}
