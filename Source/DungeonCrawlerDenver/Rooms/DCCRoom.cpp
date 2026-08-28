#include "Rooms/DCCRoom.h"

#include "Rooms/DCCEncounter.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ADCCRoom::ADCCRoom()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bAlwaysRelevant = true;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);

	// Roughly one 10m room at default scale; designers resize per node.
	Bounds->SetBoxExtent(FVector(500.0f, 500.0f, 300.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionObjectType(ECC_WorldStatic);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Bounds->SetGenerateOverlapEvents(true);

	// Visible in editor for layout, invisible in play.
	Bounds->SetHiddenInGame(true);
}

void ADCCRoom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCRoom, bCleared);
}

void ADCCRoom::BeginPlay()
{
	Super::BeginPlay();

	// Overlap bookkeeping is server-only; clients would otherwise build a
	// parallel occupancy list from whatever subset of pawns they can see.
	if (HasAuthority())
	{
		if (Bounds)
		{
			Bounds->OnComponentBeginOverlap.AddDynamic(this, &ADCCRoom::HandleBeginOverlap);
			Bounds->OnComponentEndOverlap.AddDynamic(this, &ADCCRoom::HandleEndOverlap);
		}

		if (Encounter)
		{
			Encounter->OnEncounterStateChanged.AddDynamic(this, &ADCCRoom::HandleEncounterStateChanged);
		}
		else if (RoomType == EDCCRoomType::Combat)
		{
			UE_LOG(LogTemp, Warning, TEXT("DCCRoom '%s' is a Combat room with no Encounter assigned."), *GetName());
		}

		// Occupancy is reconciled on an interval instead of trusting overlap
		// events to arrive at a convenient moment - see ReconcileOccupancy for
		// why a one-shot sweep here is not enough. First fire is immediate.
		GetWorldTimerManager().SetTimer(
			OccupancyTimerHandle,
			this,
			&ADCCRoom::ReconcileOccupancy,
			FMath::Max(0.05f, OccupancyReconcileInterval),
			/*bLoop=*/true,
			/*InFirstDelay=*/0.0f);
	}
}

TArray<ADCCCharacterBase*> ADCCRoom::GetOccupants() const
{
	TArray<ADCCCharacterBase*> Result;
	Result.Reserve(Occupants.Num());

	for (const TObjectPtr<ADCCCharacterBase>& Occupant : Occupants)
	{
		if (Occupant)
		{
			Result.Add(Occupant);
		}
	}

	return Result;
}

void ADCCRoom::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AddOccupant(Cast<ADCCCharacterBase>(OtherActor));
}

void ADCCRoom::AddOccupant(ADCCCharacterBase* Character)
{
	if (!Character || Occupants.Contains(Character))
	{
		return;
	}

	Occupants.Add(Character);
	OnOccupancyChanged.Broadcast(this, Occupants.Num());

	UE_LOG(LogTemp, Log, TEXT("DCCRoom '%s': %s entered (crawler=%d, occupants=%d)."),
		*GetName(), *Character->GetName(), Character->IsCrawler() ? 1 : 0, Occupants.Num());

	// Only a crawler arriving counts as entering the node. Enemies spawned
	// inside the room overlap it too, and must not arm their own encounter.
	//
	// Asks the character's class, not its controller: overlaps can be observed
	// before possession, so a controller-based check misclassifies a player.
	if (!Character->IsCrawler())
	{
		return;
	}

	// Mirrors onto the character so the HUD's room tag can read it without a
	// second room-tracking system - see CurrentRoomDisplayName's own comment.
	Character->CurrentRoomId = RoomId;
	Character->CurrentRoomDisplayName = RoomDisplayName.IsEmpty() ? FText::FromName(RoomId) : RoomDisplayName;

	BP_OnPlayerEntered(Character);

	if (bStartEncounterOnEntry)
	{
		TriggerEncounter();
	}
}

void ADCCRoom::RemoveOccupant(ADCCCharacterBase* Character)
{
	if (Occupants.Remove(Character) > 0)
	{
		OnOccupancyChanged.Broadcast(this, Occupants.Num());

		// Only clear if this was still the room on record for them - two room
		// volumes can briefly overlap at a doorway, and the other room's own
		// AddOccupant may already have overwritten this before we get here.
		if (Character && Character->CurrentRoomId == RoomId)
		{
			Character->CurrentRoomId = NAME_None;
			Character->CurrentRoomDisplayName = FText::GetEmpty();
		}
	}
}

void ADCCRoom::ReconcileOccupancy()
{
	if (!HasAuthority() || !Bounds)
	{
		return;
	}

	TArray<AActor*> Overlapping;
	Bounds->GetOverlappingActors(Overlapping, ADCCCharacterBase::StaticClass());

	for (AActor* Actor : Overlapping)
	{
		AddOccupant(Cast<ADCCCharacterBase>(Actor));
	}

	// Reverse iteration: entries are removed in place, and stale (destroyed)
	// occupants have to be dropped too or a cleared room keeps counting corpses.
	for (int32 Index = Occupants.Num() - 1; Index >= 0; --Index)
	{
		ADCCCharacterBase* Occupant = Occupants[Index].Get();

		if (!Occupant)
		{
			Occupants.RemoveAt(Index);
			OnOccupancyChanged.Broadcast(this, Occupants.Num());
			continue;
		}

		if (!Overlapping.Contains(Occupant))
		{
			RemoveOccupant(Occupant);
		}
	}
}

void ADCCRoom::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	RemoveOccupant(Cast<ADCCCharacterBase>(OtherActor));
}

void ADCCRoom::TriggerEncounter()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCRoom::TriggerEncounter called on a client; ignoring. Rooms are server-authoritative."));
		return;
	}

	if (bEncounterTriggered || !Encounter)
	{
		return;
	}

	bEncounterTriggered = true;
	Encounter->StartEncounter();
}

void ADCCRoom::HandleEncounterStateChanged(EDCCEncounterState NewState)
{
	if (!HasAuthority() || NewState != EDCCEncounterState::Cleared)
	{
		return;
	}

	bCleared = true;

	// The server does not receive its own OnRep.
	OnRep_Cleared();
}

void ADCCRoom::OnRep_Cleared()
{
	if (bCleared)
	{
		BP_OnRoomCleared();
	}
}
