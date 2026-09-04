#include "Rooms/DCCClimbPoint.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"

ADCCClimbPoint::ADCCClimbPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replicated so clients can resolve the surface a click landed on and so the
	// server and client agree on its bounds.
	bReplicates = true;

	Surface = CreateDefaultSubobject<UBoxComponent>(TEXT("Surface"));
	SetRootComponent(Surface);
	Surface->SetBoxExtent(FVector(60.0f, 300.0f, 400.0f));

	// Blocks the visibility channel only: the click trace has to hit it, but it
	// must not push characters around or obstruct movement, because the real
	// rock behind it already does the physical blocking.
	Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Surface->SetCollisionResponseToAllChannels(ECR_Ignore);
	Surface->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Surface);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PromptText = NSLOCTEXT("DCC", "ClimbPromptDefault", "Begin climbing?");
}

bool ADCCClimbPoint::IsWithinReach(const ADCCCharacterBase* Character) const
{
	if (!Character || !Surface)
	{
		return false;
	}

	// Distance to the box rather than to the actor origin: a wide face should be
	// reachable along its whole length, not only near its centre.
	const FVector ClosestPoint = Surface->Bounds.GetBox().GetClosestPointTo(Character->GetActorLocation());

	return FVector::Dist(ClosestPoint, Character->GetActorLocation()) <= ReachDistance;
}

float ADCCClimbPoint::GetTopZ() const
{
	if (!FMath::IsNearlyZero(ClimbTopZ))
	{
		return ClimbTopZ;
	}

	return Surface ? Surface->Bounds.GetBox().Max.Z : GetActorLocation().Z;
}

float ADCCClimbPoint::GetBottomZ() const
{
	if (!FMath::IsNearlyZero(ClimbBottomZ))
	{
		return ClimbBottomZ;
	}

	return Surface ? Surface->Bounds.GetBox().Min.Z : GetActorLocation().Z;
}

FVector ADCCClimbPoint::GetAttachLocationFor(const ADCCCharacterBase* Character, float WorldZ) const
{
	if (!Character || !Surface)
	{
		return GetActorLocation();
	}

	const float CapsuleRadius = Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleRadius()
		: 35.0f;

	const FBox Box = Surface->Bounds.GetBox();
	const FVector CharacterLocation = Character->GetActorLocation();

	// Nearest point on the face at the requested height, then held off by the
	// capsule radius so the climber rests against the surface instead of inside
	// it. Sliding along the face is allowed; leaving it is not.
	FVector Target = Box.GetClosestPointTo(FVector(CharacterLocation.X, CharacterLocation.Y, WorldZ));
	Target.Z = WorldZ;

	FVector Outward = CharacterLocation - Target;
	Outward.Z = 0.0f;

	if (Outward.IsNearlyZero())
	{
		Outward = -GetActorForwardVector();
	}

	return Target + Outward.GetSafeNormal() * FMath::Max(0.0f, CapsuleRadius + SurfaceOffset);
}

FVector ADCCClimbPoint::GetDismountLocation(const ADCCCharacterBase* Character) const
{
	const float HalfHeight = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 90.0f;

	// The same "away from the wall, into the room" direction GetAttachLocationFor
	// derives from the character's actual position against the box - NOT
	// GetActorForwardVector(), which is the bug this replaces. A synthetic climb
	// point is always spawned at FRotator::ZeroRotator (see
	// FindOrCreateForSurfaceHit, which leaves it unrotated on purpose), so its
	// forward vector is unconditionally world +X regardless of which way that
	// particular wall actually faces. On any wall that doesn't happen to face
	// +X - most of them - this stepped the dismount sideways along the face or
	// straight back into the solid wall instead of into the room, which the
	// movement component then shoved back out of. That push is exactly what
	// reads as "shaky at the top" and "clips through the wall and falls" -
	// this was never a jitter bug, it was a wrong-direction bug.
	FVector AwayFromWall = -GetActorForwardVector();

	if (Character && Surface)
	{
		const FVector ClosestPoint = Surface->Bounds.GetBox().GetClosestPointTo(Character->GetActorLocation());
		FVector Candidate = Character->GetActorLocation() - ClosestPoint;
		Candidate.Z = 0.0f;

		if (!Candidate.IsNearlyZero())
		{
			AwayFromWall = Candidate.GetSafeNormal();
		}
	}

	const float TopZ = GetTopZ();
	const FVector LipXY = GetActorLocation();

	// GetTopZ() is where the WALL stops (authored box, or the last solid probe
	// hit for a synthesized point) - it was never a promise that the actual
	// floor above sits at exactly that height. Trusting it blindly is the
	// "top shelf doesn't match up with the top floor" bug: landing short drops
	// the climber back down the face, landing long strands them mid-air over
	// the room. Verifying a few candidate landing spots against a real floor
	// trace, and only falling back to the old guess if none hit, is what
	// makes the eventual dismount land on ground that is actually there.
	if (UWorld* World = GetWorld())
	{
		constexpr float ProbeDistances[] = { 120.0f, 200.0f, 280.0f };

		FCollisionQueryParams Params;
		Params.bTraceComplex = true;
		if (Character)
		{
			Params.AddIgnoredActor(Character);
		}

		for (float Distance : ProbeDistances)
		{
			const FVector CandidateXY = LipXY + AwayFromWall * Distance;
			const FVector TraceStart(CandidateXY.X, CandidateXY.Y, TopZ + 150.0f);
			const FVector TraceEnd(CandidateXY.X, CandidateXY.Y, TopZ - 150.0f);

			FHitResult FloorHit;
			if (World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Visibility, Params))
			{
				// Within a story of the climb's own top, not several floors
				// down through a gap - a trace eventually hitting SOMETHING
				// is not the same as that something being the landing.
				if (FMath::Abs(FloorHit.Location.Z - TopZ) <= 200.0f)
				{
					FVector Verified = FloorHit.Location;
					Verified.Z += HalfHeight + 10.0f;
					return Verified;
				}
			}
		}
	}

	// Nothing verified - the old estimate, unchanged, so a climb point with no
	// nearby floor (or no UWorld, in some odd calling context) still resolves
	// to somewhere rather than nowhere.
	FVector Dismount = LipXY + AwayFromWall * 120.0f;
	Dismount.Z = TopZ + HalfHeight + 10.0f;

	return Dismount;
}

ADCCClimbPoint* ADCCClimbPoint::FindOrCreateForSurfaceHit(UWorld* World, const FVector& HitLocation,
	const FVector& HitNormal, const AActor* IgnoreActor)
{
	if (!World)
	{
		return nullptr;
	}

	// Reuse range is generous relative to the reach distance a character clicks
	// from (see ADCCCharacterBase::GenericClimbReachDistance) - two clicks a
	// couple of metres apart on the same rock face should find the same point,
	// not spawn a neighbour.
	constexpr float ReuseRadius = 300.0f;

	for (TActorIterator<ADCCClimbPoint> It(World); It; ++It)
	{
		ADCCClimbPoint* Existing = *It;
		if (!Existing || !Existing->Surface)
		{
			continue;
		}

		const FVector ClosestPoint = Existing->Surface->Bounds.GetBox().GetClosestPointTo(HitLocation);
		if (FVector::Dist(ClosestPoint, HitLocation) <= ReuseRadius)
		{
			return Existing;
		}
	}

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	if (IgnoreActor)
	{
		// Without this, the requesting character's own capsule - standing right
		// where it clicked - could be what a probe hits instead of the wall,
		// corrupting the estimated span.
		Params.AddIgnoredActor(IgnoreActor);
	}

	// Bottom: search down for the floor this wall actually stands on. Falls
	// back to a fixed drop below the hit if nothing is found, which just means
	// a generous rather than an exact climb range - not a failure.
	float BottomZ = HitLocation.Z - 400.0f;
	{
		FHitResult FloorHit;
		const FVector Start = HitLocation + FVector(0.0f, 0.0f, 50.0f);
		const FVector End = HitLocation - FVector(0.0f, 0.0f, 3000.0f);
		if (World->LineTraceSingleByChannel(FloorHit, Start, End, ECC_Visibility, Params))
		{
			BottomZ = FloorHit.Location.Z;
		}
	}

	// Top: walk upward in steps, probing back into the wall at each height from
	// just outside its surface. The first step that finds nothing solid is
	// where the wall stops, so the previous step's height is the usable top -
	// capped so one click can never make something absurdly tall climbable.
	const FVector ProbeOutward = HitNormal.GetSafeNormal() * 80.0f;
	float TopZ = HitLocation.Z;
	constexpr float StepHeight = 300.0f;
	constexpr float MaxClimbSpan = 1800.0f;

	for (int32 Step = 1; Step <= 6; ++Step)
	{
		const float ProbeZ = HitLocation.Z + Step * StepHeight;
		if (ProbeZ - HitLocation.Z > MaxClimbSpan)
		{
			break;
		}

		const FVector ProbeStart = FVector(HitLocation.X, HitLocation.Y, ProbeZ) + ProbeOutward;
		const FVector ProbeEnd = FVector(HitLocation.X, HitLocation.Y, ProbeZ) - ProbeOutward;

		FHitResult WallHit;
		if (!World->LineTraceSingleByChannel(WallHit, ProbeStart, ProbeEnd, ECC_Visibility, Params))
		{
			break;
		}

		TopZ = ProbeZ;
	}

	// No FMath::Max floor here on purpose - TopZ already starts at HitLocation.Z,
	// which the sweep that got us here proves is solid wall, so a short wall
	// (Step 1's probe finding nothing) correctly yields a short climb span
	// rather than being forced 300 units taller than any probe actually
	// verified. An earlier version of this function did force that floor,
	// which meant clicking near the top of a short wall could synthesize a
	// climb box extending into open air above the real geometry - the MaxClimbSpan
	// break above is the only cap this needs, and it is already a ceiling, not
	// a floor.
	const FVector SpawnLocation(HitLocation.X, HitLocation.Y, (BottomZ + TopZ) * 0.5f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADCCClimbPoint* NewPoint = World->SpawnActor<ADCCClimbPoint>(SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!NewPoint)
	{
		return nullptr;
	}

	// A squarish footprint rather than a thin oriented panel: unrotated, so it
	// covers the reachable patch of wall regardless of which way the surface
	// actually faces (see the class comment on FindOrCreateForSurfaceHit).
	if (NewPoint->Surface)
	{
		NewPoint->Surface->SetBoxExtent(FVector(250.0f, 250.0f, (TopZ - BottomZ) * 0.5f));
	}

	NewPoint->ClimbBottomZ = BottomZ;
	NewPoint->ClimbTopZ = TopZ;
	NewPoint->PromptText = NSLOCTEXT("DCC", "ClimbPromptGeneric", "Climb this surface?");

	return NewPoint;
}
