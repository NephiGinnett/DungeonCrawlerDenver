#include "Quests/DCCQuestMarker.h"

#include "Quests/DCCQuestComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

ADCCQuestMarker::ADCCQuestMarker()
{
	// Ticks for the bob and the billboarding, both cosmetic and both cheap.
	PrimaryActorTick.bCanEverTick = true;

	// Replicated so a client's marker agrees with the server about whether its
	// objective is still outstanding, rather than each machine guessing.
	bReplicates = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetSphereRadius(TriggerRadius);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Trigger);
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	Beacon->SetMobility(EComponentMobility::Movable);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, 330.0f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(46.0f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Movable for the same reason every other runtime text component in this
	// project is: a Static text component ignores transform changes entirely.
	Label->SetMobility(EComponentMobility::Movable);
}

void ADCCQuestMarker::BeginPlay()
{
	Super::BeginPlay();

	Trigger->SetSphereRadius(TriggerRadius);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCQuestMarker::HandleTriggerOverlap);

	BeaconBaseZ = Beacon ? Beacon->GetRelativeLocation().Z : 150.0f;

	if (Label)
	{
		Label->SetText(MarkerLabel);
		Label->SetTextRenderColor(MarkerColour.ToFColor(/*bSRGB=*/true));
	}

	// Hidden until proven otherwise, so a level full of future markers does not
	// light up like a christmas tree on the first frame.
	RefreshActiveState();
}

FVector ADCCQuestMarker::GetGoalLocation() const
{
	return Label ? Label->GetComponentLocation() : GetActorLocation();
}

void ADCCQuestMarker::RefreshActiveState()
{
	const UDCCQuestComponent* Quests = UDCCQuestComponent::Get(this);

	// No quest component at all means nothing can possibly be tracking this tag.
	// Staying visible in that case would be worse than useless - it would point
	// the party at a goal the game has no way to complete.
	const bool bShouldBeActive = Quests && Quests->IsObjectiveTagActive(ObjectiveTag);

	if (bShouldBeActive == bActive)
	{
		return;
	}

	bActive = bShouldBeActive;

	const bool bVisible = bActive || !bHideWhenComplete;

	if (Beacon)
	{
		Beacon->SetVisibility(bVisible);
	}

	if (Label)
	{
		Label->SetVisibility(bVisible);
	}
}

void ADCCQuestMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TimeUntilRefresh -= DeltaSeconds;
	if (TimeUntilRefresh <= 0.0f)
	{
		TimeUntilRefresh = 0.25f;
		RefreshActiveState();
	}

	if (!bActive)
	{
		return;
	}

	// Bob and spin. Purely to separate the marker from level geometry - a static
	// glowing box reads as scenery, and players walk straight past scenery.
	if (Beacon)
	{
		BobPhase += DeltaSeconds * 2.0f;

		FVector Location = Beacon->GetRelativeLocation();
		Location.Z = BeaconBaseZ + FMath::Sin(BobPhase) * 18.0f;

		Beacon->SetRelativeLocation(Location);
		Beacon->AddLocalRotation(FRotator(0.0f, DeltaSeconds * 45.0f, 0.0f));
	}

	// Billboarded on each machine towards its own viewer, for the same reason the
	// nameplates are: the server's idea of "facing the camera" means nothing.
	//
	// Local, not first: GetFirstPlayerController returns the host on a listen
	// server and would point every client's marker label at the host's camera.
	if (Label)
	{
		if (const APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr)
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

			// Forward must point AT the viewer - see the identical fix and comment
			// on ADCCCharacterBase's nameplate billboarding, same bug.
			Label->SetWorldRotation((ViewLocation - Label->GetComponentLocation()).Rotation());
		}
	}
}

void ADCCQuestMarker::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	// Server-authoritative like everything else that moves the game forward: a
	// client walking into a box does not get to decide the quest advanced.
	if (!HasAuthority() || !bActive)
	{
		return;
	}

	const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(OtherActor);
	if (!Character || !Character->IsCrawler() || !Character->IsAlive())
	{
		return;
	}

	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::ReachLocation, ObjectiveTag);

	RefreshActiveState();
}
