#include "Rooms/DCCExitPortal.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCCExitPortal::ADCCExitPortal()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	SetRootComponent(Frame);
	Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FrameMesh(
		TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
	if (FrameMesh.Succeeded())
	{
		Frame->SetStaticMesh(FrameMesh.Object);
	}

	// A doorway-shaped slab, same convention as ADCCFloorExit's own placeholder frame.
	Frame->SetRelativeScale3D(FVector(0.4f, 3.0f, 4.5f));

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Frame);
	Trigger->SetBoxExtent(FVector(120.0f, 160.0f, 220.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Undo the frame's non-uniform scale, same reason ADCCFloorExit's does.
	Trigger->SetRelativeScale3D(FVector(2.5f, 1.0f / 3.0f, 1.0f / 4.5f));

	// Alien_Portal's own portal Blueprint - "one of our fancy portals from the
	// library", per the ask. Swappable per instance without a recompile if a
	// different pack reads better in context.
	static ConstructorHelpers::FClassFinder<AActor> DefaultPortalVisual(
		TEXT("/Game/Alien_Portal/BP/BP_portal.BP_portal_C"));
	if (DefaultPortalVisual.Succeeded())
	{
		PortalVisualClass = DefaultPortalVisual.Class;
	}
}

void ADCCExitPortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCExitPortal, bActive);
}

void ADCCExitPortal::BeginPlay()
{
	Super::BeginPlay();

	ApplyActiveVisuals();

	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCExitPortal::HandleTriggerOverlap);
	}
}

void ADCCExitPortal::Activate()
{
	if (!HasAuthority() || bActive)
	{
		return;
	}

	bActive = true;
	ApplyActiveVisuals();
}

void ADCCExitPortal::OnRep_Active()
{
	ApplyActiveVisuals();
}

void ADCCExitPortal::ApplyActiveVisuals()
{
	// Off: no visual, no way to trigger it. On: spawn the portal and open the
	// door. Idempotent either way, so calling this from both BeginPlay and
	// every state change never double-spawns or leaks the old actor.
	if (bActive)
	{
		if (!SpawnedVisual && PortalVisualClass)
		{
			// In front of the frame, along its own forward vector, so the
			// portal reads as coming out of the console rather than floating
			// inside it.
			const FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 60.0f;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (UWorld* World = GetWorld())
			{
				SpawnedVisual = World->SpawnActor<AActor>(PortalVisualClass, SpawnLocation, GetActorRotation(), Params);
			}
		}

		Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
	else
	{
		if (SpawnedVisual)
		{
			SpawnedVisual->Destroy();
			SpawnedVisual = nullptr;
		}

		Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
}

void ADCCExitPortal::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bActive)
	{
		return;
	}

	ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(OtherActor);
	if (!Character || !Character->IsAlive() || !Character->IsCrawler())
	{
		return;
	}

	Character->EnterAwaitingOtherCrawlers();
}
