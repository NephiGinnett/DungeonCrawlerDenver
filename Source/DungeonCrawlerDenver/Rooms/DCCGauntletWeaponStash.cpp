#include "Rooms/DCCGauntletWeaponStash.h"

#include "Rooms/DCCGauntletEncounter.h"
#include "Items/DCCPickup_Equipment.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Engine/World.h"

ADCCGauntletWeaponStash::ADCCGauntletWeaponStash()
{
	PrimaryActorTick.bCanEverTick = false;

	// A pure placement marker - the pickup it spawns is what's actually
	// visible/interactable and replicates on its own.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ADCCGauntletWeaponStash::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() || !Gauntlet)
	{
		return;
	}

	Gauntlet->OnKillCountChanged.AddDynamic(this, &ADCCGauntletWeaponStash::HandleKillCountChanged);

	// Tier 0 is on the ground from the moment the gauntlet exists, not only
	// after the first kill - "you take the weapon" implies there is always
	// something there to take.
	SpawnTier(0);
}

void ADCCGauntletWeaponStash::HandleKillCountChanged(int32 NewKillCount)
{
	if (!HasAuthority() || WeaponTierItemIds.IsEmpty())
	{
		return;
	}

	// Clamped rather than wrapping or growing past the list - the last tier
	// is meant to stay up for the rest of the gauntlet once reached, not run
	// out and leave nothing on the ground.
	const int32 TierIndex = FMath::Min(NewKillCount / KillsPerTier, WeaponTierItemIds.Num() - 1);
	if (TierIndex != CurrentTierIndex)
	{
		SpawnTier(TierIndex);
	}
}

void ADCCGauntletWeaponStash::SpawnTier(int32 TierIndex)
{
	UWorld* World = GetWorld();
	if (!World || !WeaponTierItemIds.IsValidIndex(TierIndex))
	{
		return;
	}

	CurrentTierIndex = TierIndex;

	if (CurrentPickup)
	{
		CurrentPickup->Destroy();
		CurrentPickup = nullptr;
	}

	// EquipmentItemId has to be set BEFORE BeginPlay, which is where
	// ADCCPickup_Equipment resolves it into a mesh - same
	// SpawnActorDeferred + FinishSpawning shape as
	// ADCCDungeonGenerator::BuildRoomShell uses for exactly this reason.
	ADCCPickup_Equipment* Pickup = World->SpawnActorDeferred<ADCCPickup_Equipment>(
		ADCCPickup_Equipment::StaticClass(), GetActorTransform());
	if (!Pickup)
	{
		return;
	}

	Pickup->EquipmentItemId = WeaponTierItemIds[TierIndex];
	Pickup->FinishSpawning(GetActorTransform());
	CurrentPickup = Pickup;

	// Only announced from the second tier on - the starting weapon simply
	// being on the ground needs no fanfare, an upgrade does.
	if (TierIndex > 0)
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "GauntletWeaponUpgrade", "The gauntlet's weapon stash upgraded - tier {0}."),
			FText::AsNumber(TierIndex + 1)));
	}
}
