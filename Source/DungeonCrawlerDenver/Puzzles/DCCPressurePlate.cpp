#include "Puzzles/DCCPressurePlate.h"

#include "Puzzles/DCCPuzzleDoor.h"
#include "Characters/DCCCharacterBase.h"
#include "Items/DCCPickup.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCCPressurePlate::ADCCPressurePlate()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	// Wide and flat by default - it should read as something you step onto.
	Mesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.1f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Mesh);
	Trigger->SetBoxExtent(FVector(60.0f, 60.0f, 80.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void ADCCPressurePlate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCPressurePlate, bSatisfied);
	DOREPLIFETIME(ADCCPressurePlate, CurrentMassKilograms);
}

void ADCCPressurePlate::BeginPlay()
{
	Super::BeginPlay();

	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCPressurePlate::HandleOverlapBegin);
		Trigger->OnComponentEndOverlap.AddDynamic(this, &ADCCPressurePlate::HandleOverlapEnd);
	}

	if (HasAuthority())
	{
		RecalculateMass();
	}
	ApplySatisfiedVisuals();
}

void ADCCPressurePlate::HandleOverlapBegin(UPrimitiveComponent*, AActor*, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (HasAuthority())
	{
		RecalculateMass();
	}
}

void ADCCPressurePlate::HandleOverlapEnd(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32)
{
	if (HasAuthority())
	{
		RecalculateMass();
	}
}

void ADCCPressurePlate::RecalculateMass()
{
	if (!HasAuthority() || !Trigger)
	{
		return;
	}

	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping);

	float Total = 0.0f;
	for (AActor* Actor : Overlapping)
	{
		if (!Actor || Actor == this)
		{
			continue;
		}

		if (const ADCCPickup* Pickup = Cast<ADCCPickup>(Actor))
		{
			// A prop still in someone's hands is not resting on the plate -
			// the carry system reparents it to the carrier, so an attached
			// pickup is being held, not placed.
			if (Pickup->GetAttachParentActor() == nullptr)
			{
				Total += Pickup->MassKilograms;
			}
			continue;
		}

		if (const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(Actor))
		{
			if (Character->IsAlive())
			{
				Total += CharacterMassKilograms;
			}
		}
	}

	CurrentMassKilograms = Total;

	const bool bNowSatisfied = Total >= RequiredMassKilograms;
	if (bNowSatisfied != bSatisfied)
	{
		bSatisfied = bNowSatisfied;
		ApplySatisfiedVisuals();
		EvaluateLinkedDoor();
	}
}

void ADCCPressurePlate::OnRep_Satisfied()
{
	ApplySatisfiedVisuals();
}

void ADCCPressurePlate::ApplySatisfiedVisuals()
{
	if (Mesh)
	{
		// Sinks slightly when weighted. Cheap, readable, and needs no new art.
		Mesh->SetRelativeScale3D(FVector(2.0f, 2.0f, bSatisfied ? 0.05f : 0.1f));
	}
}

void ADCCPressurePlate::EvaluateLinkedDoor()
{
	if (!HasAuthority() || !LinkedDoor || LinkedDoor->IsOpen())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Every plate pointing at the same door has to be happy at once. Gathered
	// by iteration rather than by the door holding a list, so placement is
	// one-directional: drop a plate, point it at a door, done.
	for (TActorIterator<ADCCPressurePlate> It(World); It; ++It)
	{
		const ADCCPressurePlate* Plate = *It;
		if (Plate && Plate->LinkedDoor == LinkedDoor && !Plate->IsSatisfied())
		{
			return;
		}
	}

	LinkedDoor->Unlock();
}
