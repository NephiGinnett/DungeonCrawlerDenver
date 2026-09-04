#include "Rooms/DCCArrivalTrigger.h"

#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ADCCArrivalTrigger::ADCCArrivalTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(300.0f, 300.0f, 250.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ADCCArrivalTrigger::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArrivalTrigger, bFired);
}

void ADCCArrivalTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCArrivalTrigger::HandleOverlapBegin);
	}

	// Runs on clients too: the arriving actors are ordinary placed actors, so
	// without this they would be visible on every machine from the start.
	ApplyArrivalState();
}

void ADCCArrivalTrigger::HandleOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority() || bFired)
	{
		return;
	}

	const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(OtherActor);
	if (!Character || !Character->IsCrawler() || !Character->IsAlive())
	{
		return;
	}

	// Nothing arrives until the party has actually been where they were meant
	// to go - otherwise walking past the shaft head is enough to summon a
	// confrontation about material nobody has collected yet.
	if (Prerequisite && !Prerequisite->HasFired())
	{
		return;
	}

	Fire();
}

void ADCCArrivalTrigger::Fire()
{
	if (!HasAuthority() || bFired)
	{
		return;
	}

	bFired = true;
	ApplyArrivalState();

	if (!ArrivalAnnouncement.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			if (ADCCGameState* GameState = World->GetGameState<ADCCGameState>())
			{
				GameState->AddLogEntry(EDCCLogCategory::Encounter, ArrivalAnnouncement);
			}
		}
	}
}

void ADCCArrivalTrigger::OnRep_Fired()
{
	ApplyArrivalState();
}

void ADCCArrivalTrigger::ApplyArrivalState()
{
	for (AActor* Actor : ArrivingActors)
	{
		if (!Actor)
		{
			continue;
		}

		Actor->SetActorHiddenInGame(!bFired);
		Actor->SetActorEnableCollision(bFired);
	}
}
