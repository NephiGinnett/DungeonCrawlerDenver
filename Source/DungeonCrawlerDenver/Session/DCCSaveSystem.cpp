#include "Session/DCCSaveSystem.h"
#include "Session/DCCSaveGame.h"

#include "Characters/DCCCharacterBase.h"
#include "Classes/DCCClassComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Favor/DCCFavorComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Progression/DCCAchievementComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/DCCItemVisuals.h"

namespace
{
	/** Which component, and which of its properties, make up a crawler's progress. */
	struct FSavedPart
	{
		const TCHAR* Key;
		UActorComponent* (*Get)(ADCCCharacterBase*);
		TArray<const TCHAR*> Properties;
	};

	const TArray<FSavedPart>& Parts()
	{
		static const TArray<FSavedPart> List = {
			{ TEXT("Stats"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Stats; },
				{ TEXT("AbilityScores"), TEXT("Level"), TEXT("ExperiencePoints"), TEXT("Gold"), TEXT("StatPointsSpent") } },
			{ TEXT("Skills"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Skills; },
				{ TEXT("KnownSkills") } },
			{ TEXT("Favor"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Favor; },
				{ TEXT("Favor") } },
			{ TEXT("Inventory"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Inventory; },
				{ TEXT("Items"), TEXT("StowedConsumables"), TEXT("StowedRemains"), TEXT("LootBoxCounts") } },
			{ TEXT("Equipment"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Equipment; },
				{ TEXT("Equipped") } },
			{ TEXT("Achievements"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Achievements; },
				{ TEXT("Progress"), TEXT("StatTotals") } },
			{ TEXT("Class"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Class; },
				{ TEXT("SelectedClassId"), TEXT("Perks") } },
			{ TEXT("Onboarding"), [](ADCCCharacterBase* C) -> UActorComponent* { return C->Onboarding; },
				{ TEXT("Stage"), TEXT("UnlockedFeatures") } },
		};
		return List;
	}

	/** Save key -> the pawn allowed to write it (the one that loaded it). */
	TMap<FString, TWeakObjectPtr<ADCCCharacterBase>> GOwners;

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
		return TEXT("DCC_") + Safe;
	}
}

namespace DCCSave
{
	FString SaveKeyFor(const ADCCCharacterBase* Crawler)
	{
		if (!Crawler || !Crawler->IsPlayerControlled())
		{
			return FString();
		}
		if (Crawler->Onboarding && !Crawler->Onboarding->CrawlerId.IsNone())
		{
			return Crawler->Onboarding->CrawlerId.ToString();
		}
		const APlayerState* PlayerState = Crawler->GetPlayerState();
		return PlayerState ? PlayerState->GetPlayerName() : FString();
	}

	void Load(ADCCCharacterBase* Crawler)
	{
		if (!Crawler || !Crawler->HasAuthority())
		{
			return;
		}
		const FString Key = SaveKeyFor(Crawler);
		if (Key.IsEmpty())
		{
			return;
		}

		// This pawn is now the only one allowed to write this crawler's save.
		GOwners.Add(Key, Crawler);

		UDCCCrawlerSave* SaveGame = Cast<UDCCCrawlerSave>(UGameplayStatics::LoadGameFromSlot(SlotFor(Key), 0));
		if (!SaveGame)
		{
			UE_LOG(LogTemp, Log, TEXT("[DCC Save] No save for %s yet; starting fresh."), *Key);
			Save(Crawler);
			DCCItemToast::NoteCarryIn(Crawler);
			return;
		}

		int32 Applied = 0;
		for (const FSavedPart& Part : Parts())
		{
			UActorComponent* Component = Part.Get(Crawler);
			if (!Component)
			{
				continue;
			}
			for (const TCHAR* Name : Part.Properties)
			{
				const FString* Text = SaveGame->Values.Find(FString::Printf(TEXT("%s.%s"), Part.Key, Name));
				FProperty* Property = Text ? Component->GetClass()->FindPropertyByName(Name) : nullptr;
				if (Property && Property->ImportText_InContainer(**Text, Component, Component, PPF_None))
				{
					++Applied;
				}
			}
		}

		// Derived values follow from what was restored: enhanced scores from
		// base plus gear, then HP, mana and the XP bar. Gear meshes rehang.
		if (Crawler->Stats)
		{
			Crawler->Stats->RecalculateEnhancedScores(/*bRefillToFull=*/true);
			Crawler->Stats->RefreshDerivedStats(/*bRefillToFull=*/true);
		}
		if (Crawler->Equipment)
		{
			Crawler->Equipment->RefreshAttachments();
		}

		UE_LOG(LogTemp, Log, TEXT("[DCC Save] Loaded %s (%d values, saved %s)."), *Key, Applied, *SaveGame->SavedAt.ToString());

		if (Crawler->Onboarding && Crawler->Stats)
		{
			int32 Boxes = 0;
			if (Crawler->Inventory)
			{
				for (const int32 Count : Crawler->Inventory->LootBoxCounts)
				{
					Boxes += Count;
				}
			}
			Crawler->Onboarding->NotifyOwner(FText::Format(
				NSLOCTEXT("DCC", "SaveWelcomeBack", "Welcome back, {0}. Level {1}, {2} gold, {3} sealed {3}|plural(one=box,other=boxes). The System kept your seat warm. It charged you for that."),
				FText::FromString(Key), FText::AsNumber(Crawler->Stats->Level), FText::AsNumber(Crawler->Stats->Gold), FText::AsNumber(Boxes)));
		}
		DCCItemToast::NoteCarryIn(Crawler);
	}

	bool Save(ADCCCharacterBase* Crawler)
	{
		if (!Crawler || !Crawler->HasAuthority())
		{
			return false;
		}
		FString Key = SaveKeyFor(Crawler);
		if (Key.IsEmpty())
		{
			// Unpossessed already (the game closing, a player who just left):
			// still the crawler that loaded this save, so still allowed to keep it.
			for (const TPair<FString, TWeakObjectPtr<ADCCCharacterBase>>& Pair : GOwners)
			{
				if (Pair.Value.Get() == Crawler)
				{
					Key = Pair.Key;
					break;
				}
			}
		}
		const TWeakObjectPtr<ADCCCharacterBase>* Owner = Key.IsEmpty() ? nullptr : GOwners.Find(Key);
		if (!Owner || Owner->Get() != Crawler)
		{
			return false;
		}

		UDCCCrawlerSave* SaveGame = Cast<UDCCCrawlerSave>(UGameplayStatics::CreateSaveGameObject(UDCCCrawlerSave::StaticClass()));
		if (!SaveGame)
		{
			return false;
		}
		SaveGame->CrawlerId = FName(*Key);
		SaveGame->SavedAt = FDateTime::Now();

		for (const FSavedPart& Part : Parts())
		{
			UActorComponent* Component = Part.Get(Crawler);
			if (!Component)
			{
				continue;
			}
			for (const TCHAR* Name : Part.Properties)
			{
				if (FProperty* Property = Component->GetClass()->FindPropertyByName(Name))
				{
					FString Text;
					Property->ExportText_InContainer(0, Text, Component, nullptr, Component, PPF_None);
					SaveGame->Values.Add(FString::Printf(TEXT("%s.%s"), Part.Key, Name), Text);
				}
			}
		}

		const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SlotFor(Key), 0);
		UE_LOG(LogTemp, Verbose, TEXT("[DCC Save] %s %s."), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *Key);
		return bSaved;
	}

	void SaveAll(UWorld* World, const TCHAR* Reason)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			Count += Save(*It) ? 1 : 0;
		}
		if (Count > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[DCC Save] %s: saved %d crawler(s)."), Reason, Count);
		}
	}
}

// ---------------------------------------------------------------------------
// Autosave
// ---------------------------------------------------------------------------

bool UDCCSaveSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDCCSaveSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	TWeakObjectPtr<UWorld> WeakWorld(&InWorld);
	InWorld.GetTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateLambda([WeakWorld]()
	{
		DCCSave::SaveAll(WeakWorld.Get(), TEXT("Autosave"));
	}), 60.0f, /*bLoop=*/true);
}

void UDCCSaveSubsystem::Deinitialize()
{
	DCCSave::SaveAll(GetWorld(), TEXT("World closing"));
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Console
// ---------------------------------------------------------------------------

static void DCCSaveNowCommand(UWorld* World)
{
	DCCSave::SaveAll(World, TEXT("DCC.Save"));
}

static void DCCWipeSaveCommand(const TArray<FString>& Args)
{
	if (Args.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Save] Usage: DCC.WipeSave <Crawler>, e.g. DCC.WipeSave CJ"));
		return;
	}
	const FString Slot = TEXT("DCC_") + Args[0];
	// The live pawn (if any) must not write it straight back on the next autosave.
	GOwners.Remove(Args[0]);
	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UE_LOG(LogTemp, Warning, TEXT("[DCC Save] %s %s. Rejoin (or respawn) to start that crawler fresh."),
		bDeleted ? TEXT("Deleted") : TEXT("No save named"), *Slot);
}

static FAutoConsoleCommandWithWorld GSaveNowCmd(
	TEXT("DCC.Save"),
	TEXT("Save every crawler now (the game also saves every minute)."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DCCSaveNowCommand));

static FAutoConsoleCommand GWipeSaveCmd(
	TEXT("DCC.WipeSave"),
	TEXT("DCC.WipeSave <Crawler>: delete that crawler's save on this machine."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DCCWipeSaveCommand));
