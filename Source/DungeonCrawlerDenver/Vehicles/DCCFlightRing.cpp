#include "Vehicles/DCCFlightRing.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Quests/DCCQuestComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicles/DCCRover.h"

ADCCFlightRing::ADCCFlightRing()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // bHasEverBeenPassed needs to reach every viewer, not just the driver.

	RingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RingMesh"));
	SetRootComponent(RingMesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMeshFinder(
		TEXT("/Game/Evil_planet/Meshes/Portal/SM_ring_01.SM_ring_01"));
	if (RingMeshFinder.Succeeded())
	{
		RingMesh->SetStaticMesh(RingMeshFinder.Object);
	}
	// Deliberately no collision on the visual mesh - see the class comment on
	// why a near-miss should never bounce or stop the player. The mesh's
	// default orientation lies flat with its hole facing local Z (like a
	// manhole cover); pitching it 90 degrees turns the hole to face local X,
	// which is the axis every rotation below treats as "through."
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	PassThroughVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PassThroughVolume"));
	PassThroughVolume->SetupAttachment(RingMesh);
	// A first estimate of the ring's inner opening, not a measured one - this
	// project's MCP toolset has no way to inspect a mesh's hole diameter
	// directly, only its outer bounds (SM_ring_01 is ~740 units across
	// overall). Sized comfortably inside that so the volume cannot be
	// mistaken for the ring's own material by a camera screenshot check.
	// Thin along local X (the through-axis, matching RingMesh's pitch above)
	// so passing near-tangent to the ring doesn't count from meters away.
	PassThroughVolume->SetBoxExtent(FVector(60.0f, 200.0f, 200.0f));
	PassThroughVolume->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // Undo the mesh's pitch - the volume is defined in the ring's un-rotated local space.
	PassThroughVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PassThroughVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	PassThroughVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PassThroughVolume->SetGenerateOverlapEvents(true);

	// The real bug, found the hard way (Zoe, 2026-09-03: "the obstacle target
	// rings also do nothing when I pass through them"): a UPrimitiveComponent
	// defaults its own Object Type to WorldDynamic, and ADCCRover::Collision
	// responds to WorldDynamic with Block (SetCollisionResponseToAllChannels
	// (ECR_Block) in the rover's own constructor - only Pawn is carved out as
	// Overlap there). QueryOnly means that mismatch produces no physical
	// block either, so a Block-vs-Overlap disagreement between the two sides
	// resolved as neither - the rover sailed straight through with nothing
	// firing at all. Declaring this volume's own Object Type as ECC_Pawn
	// makes it match the one channel the rover's collision already grants
	// Overlap - the minimal, surgical fix, rather than touching the rover's
	// own response matrix and risking its unrelated Block behaviour against
	// every other WorldDynamic actor in the world.
	PassThroughVolume->SetCollisionObjectType(ECC_Pawn);

	ObjectiveGlow = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ObjectiveGlow"));
	ObjectiveGlow->SetupAttachment(RingMesh);
	ObjectiveGlow->bAutoActivate = false; // RefreshObjectiveHighlight decides, not construction.
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> GlowFinder(
		TEXT("/Game/Alien_Portal/FX/Particles/NS_blue_FX.NS_blue_FX"));
	if (GlowFinder.Succeeded())
	{
		ObjectiveGlow->SetAsset(GlowFinder.Object);
	}

	// Same system as the persistent glow - a one-shot spawn of it reads as
	// a brief flash rather than a loop, since SpawnSystemAtLocation runs it
	// once and self-destroys rather than looping like the attached component.
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FlareFinder(
		TEXT("/Game/Alien_Portal/FX/Particles/NS_blue_FX.NS_blue_FX"));
	if (FlareFinder.Succeeded())
	{
		PassFlareEffect = FlareFinder.Object;
	}
}

void ADCCFlightRing::BeginPlay()
{
	Super::BeginPlay();

	// Every machine computes its own highlight from replicated state rather
	// than the server pushing a "you're it" RPC - simpler, and correct even
	// for a client that joins mid-course.
	RefreshObjectiveHighlight();
	GetWorldTimerManager().SetTimer(ObjectiveHighlightTimer, this,
		&ADCCFlightRing::RefreshObjectiveHighlight, 0.5f, /*bLoop=*/true);
}

void ADCCFlightRing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCFlightRing, bHasEverBeenPassed);
}

void ADCCFlightRing::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	// Already passed - a ring should fire exactly once, ever (Zoe,
	// 2026-09-03: "make sure that the target rings can only be triggered
	// once"). OnRep_HasEverBeenPassed below drops PassThroughVolume's own
	// collision the moment this becomes true, which is what actually
	// prevents a second overlap from firing at all; this check is the
	// defense-in-depth for the frame or two before that replicates out to
	// remote clients.
	if (bHasEverBeenPassed)
	{
		return;
	}

	ADCCRover* Rover = Cast<ADCCRover>(OtherActor);
	if (!Rover || !Rover->Driver)
	{
		return;
	}

	// Local-only feedback, same as every other flavour notice in this
	// project (ShowLocalNotice no-ops for AI, and only actually draws on the
	// locally controlled client) - there is nothing here that needs to be
	// authoritative or seen by anyone but the pilot.
	Rover->Driver->ShowLocalNotice(FText::Format(
		NSLOCTEXT("DCCFlightRing", "RingPassed", "Ring {0} of {1}!"),
		FText::AsNumber(RingIndex), FText::AsNumber(RingCount)));

	// Sound and flare are pure cosmetics, same reasoning as the notice above
	// - every machine just spawns its own local copy rather than this
	// needing a multicast RPC. Zoe, 2026-09-04: "going through the rings
	// play a small sound and flare to indicate you made it."
	if (PassFlareEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PassFlareEffect,
			GetActorLocation(), GetActorRotation());
	}
	if (PassSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PassSound, GetActorLocation());
	}

	// Overlap detection runs independently on every machine (the rover's
	// transform replicates; each client's own collision still fires its own
	// overlap events against it), so only the server may act on this or
	// every client would race to "pass" the ring the moment they see it.
	if (HasAuthority())
	{
		Rover->AwardPilotingBonusSeconds(PilotingBonusSeconds);
		UDCCQuestComponent::NotifyEvent(Rover->Driver, EDCCObjectiveType::Custom, TEXT("FlightRingPassed"), 1);

		// "And also give you a speed boost" (Zoe, 2026-09-04) - on top of
		// the piloting-time bonus above, a genuine mechanical push.
		if (bBoostOnPass)
		{
			Rover->GrantBoost();
		}

		bHasEverBeenPassed = true;
		OnRep_HasEverBeenPassed(); // The server doesn't get its own OnRep call.
	}
}

void ADCCFlightRing::OnRep_HasEverBeenPassed()
{
	if (bHasEverBeenPassed)
	{
		// "Disappear after being triggered" (Zoe, 2026-09-03) - hidden for
		// good, and both components drop collision so there is nothing left
		// to overlap even under replication lag. RefreshObjectiveHighlight
		// below already turns this ring's own glow off on its own
		// (bShouldGlow is false the moment bHasEverBeenPassed is true), so
		// nothing extra is needed for that here.
		RingMesh->SetVisibility(false, true);
		RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PassThroughVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	RefreshObjectiveHighlight();

	// A ring further along its course may have been sitting lit as a
	// fallback (RefreshObjectiveHighlight has no earlier ring to defer to
	// yet); once an earlier one reports passed, every ring re-checks so the
	// glow hands off promptly instead of waiting for its own next tick.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADCCFlightRing> It(World); It; ++It)
		{
			if (ADCCFlightRing* Other = *It; Other && Other != this && Other->RingCount == RingCount)
			{
				Other->RefreshObjectiveHighlight();
			}
		}
	}
}

void ADCCFlightRing::RefreshObjectiveHighlight()
{
	UWorld* World = GetWorld();
	if (!World || !ObjectiveGlow)
	{
		return;
	}

	int32 LowestUnpassedIndex = TNumericLimits<int32>::Max();
	for (TActorIterator<ADCCFlightRing> It(World); It; ++It)
	{
		const ADCCFlightRing* Candidate = *It;
		if (Candidate && Candidate->RingCount == RingCount && !Candidate->bHasEverBeenPassed)
		{
			LowestUnpassedIndex = FMath::Min(LowestUnpassedIndex, Candidate->RingIndex);
		}
	}

	// Every ring passed: nothing to point at, leave every glow off rather
	// than picking an arbitrary one to relight.
	const bool bShouldGlow = !bHasEverBeenPassed && RingIndex == LowestUnpassedIndex;
	if (bShouldGlow && !ObjectiveGlow->IsActive())
	{
		ObjectiveGlow->Activate(true);
	}
	else if (!bShouldGlow && ObjectiveGlow->IsActive())
	{
		ObjectiveGlow->Deactivate();
	}
}
