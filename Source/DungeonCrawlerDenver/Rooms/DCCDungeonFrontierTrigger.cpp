#include "Rooms/DCCDungeonFrontierTrigger.h"

#include "Rooms/DCCDungeonGenerator.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/BoxComponent.h"

ADCCDungeonFrontierTrigger::ADCCDungeonFrontierTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(150.0f, 150.0f, 200.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
}

void ADCCDungeonFrontierTrigger::BeginPlay()
{
	Super::BeginPlay();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCDungeonFrontierTrigger::HandleBeginOverlap);
}

void ADCCDungeonFrontierTrigger::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// Generation is server state - a client walking through this on its own
	// screen must not spawn anything locally, only the authoritative copy of
	// this trigger (which every client has, since it replicates) gets to ask.
	if (!HasAuthority())
	{
		return;
	}

	// Only a crawler opens new dungeon - an enemy or a thrown crate wandering
	// through should never generate content on someone's behalf.
	const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(OtherActor);
	if (!Character || !Character->IsCrawler() || !Character->IsAlive())
	{
		return;
	}

	if (ADCCDungeonGenerator* GeneratorPtr = Generator.Get())
	{
		GeneratorPtr->GenerateRoomAt(TargetCell, EntryDirection);
	}

	// Resolved - whatever is beyond this doorway exists now, so the connector
	// itself has nothing left to do.
	Destroy();
}
