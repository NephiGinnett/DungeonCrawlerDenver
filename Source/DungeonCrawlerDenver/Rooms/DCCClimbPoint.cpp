#include "Rooms/DCCClimbPoint.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"

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

void ADCCClimbPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCClimbPoint, ClimbTopZ);
	DOREPLIFETIME(ADCCClimbPoint, ClimbBottomZ);
	DOREPLIFETIME(ADCCClimbPoint, WallNormal);
	DOREPLIFETIME(ADCCClimbPoint, WallPoint);
}

bool ADCCClimbPoint::IsWithinReach(const ADCCCharacterBase* Character) const
{
	if (Character && IsWallPlane())
	{
		const float PlaneDistance = FVector::DotProduct(Character->GetActorLocation() - WallPoint, WallNormal);
		return PlaneDistance > -50.0f && PlaneDistance <= ReachDistance + 60.0f;
	}
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
	const float CapsuleRadius = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleRadius()
		: 35.0f;

	// A plain wall: slide onto the plane, then hold off it by the capsule radius.
	// Same distance every frame, wherever on the face, so nothing jitters.
	if (Character && IsWallPlane())
	{
		const FVector Location = Character->GetActorLocation();
		const float PlaneDistance = FVector::DotProduct(Location - WallPoint, WallNormal);
		FVector OnPlane = Location - WallNormal * PlaneDistance;
		OnPlane.Z = WorldZ;
		// +22: the cliff-rock tiles on Fourteener bulge out past the wall plane,
		// and climbers' faces were sinking into them (2026-11-13).
		return OnPlane + WallNormal * (CapsuleRadius + SurfaceOffset + 22.0f);
	}

	if (!Character || !Surface)
	{
		return GetActorLocation();
	}

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

namespace
{
	float CapsuleRadiusOr(const ADCCCharacterBase* Character)
	{
		return Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 35.0f;
	}
}

FVector ADCCClimbPoint::GetDismountLocation(const ADCCCharacterBase* Character) const
{
	const float HalfHeight = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 90.0f;

	// A plain wall: the floor at the top is over the lip, INTO the wall's
	// footprint (-WallNormal), not out in the air in front of it.
	if (Character && IsWallPlane())
	{
		const float TopZ = GetTopZ();
		const FVector Location = Character->GetActorLocation();
		const float PlaneDistance = FVector::DotProduct(Location - WallPoint, WallNormal);
		const FVector Lip = Location - WallNormal * PlaneDistance;
		if (UWorld* World = GetWorld())
		{
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Character);
			for (const float Distance : { 70.0f, 120.0f, 180.0f, 260.0f })
			{
				const FVector XY = Lip - WallNormal * Distance;
				FHitResult Floor;
				if (World->LineTraceSingleByChannel(Floor, FVector(XY.X, XY.Y, TopZ + 200.0f), FVector(XY.X, XY.Y, TopZ - 60.0f), ECC_WorldStatic, Params)
					&& Floor.ImpactNormal.Z > 0.6f)
				{
					// Room to stand? A capsule sweep from just above.
					const FVector Stand(XY.X, XY.Y, Floor.ImpactPoint.Z + HalfHeight + 4.0f);
					if (!World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_Pawn,
						FCollisionShape::MakeCapsule(CapsuleRadiusOr(Character), HalfHeight - 2.0f), Params))
					{
						return Stand;
					}
				}
			}
		}
		return FVector(Lip.X, Lip.Y, TopZ) - WallNormal * 90.0f + FVector(0, 0, HalfHeight + 10.0f);
	}

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

	// The wall's facing, flattened: walls are climbed straight up.
	FVector Normal = HitNormal;
	Normal.Z = 0.0f;
	Normal = Normal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return nullptr;
	}

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	if (IgnoreActor)
	{
		// Without this, the requesting character's own capsule - standing right
		// where it clicked - could be what a probe hits instead of the wall.
		Params.AddIgnoredActor(IgnoreActor);
	}
	for (TActorIterator<ADCCClimbPoint> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It); // older climb boxes are never the wall
	}

	// Is there solid wall at this height, right here on the plane?
	auto WallAt = [World, &Params, &HitLocation, &Normal](float Z)
	{
		const FVector At(HitLocation.X, HitLocation.Y, Z);
		FHitResult Hit;
		return World->LineTraceSingleByChannel(Hit, At + Normal * 80.0f, At - Normal * 40.0f, ECC_WorldStatic, Params)
			&& FVector::DotProduct(Hit.ImpactNormal, Normal) > 0.7f;
	};

	// Reuse a plane point on the same face if its column covers this spot.
	for (TActorIterator<ADCCClimbPoint> It(World); It; ++It)
	{
		ADCCClimbPoint* Existing = *It;
		if (!Existing || !Existing->IsWallPlane())
		{
			continue;
		}
		const FVector D = HitLocation - Existing->WallPoint;
		if (FVector::DotProduct(Existing->WallNormal, Normal) > 0.98f
			&& FMath::Abs(FVector::DotProduct(D, Normal)) < 30.0f
			&& D.Size2D() < 400.0f
			&& HitLocation.Z >= Existing->ClimbBottomZ - 50.0f && HitLocation.Z <= Existing->ClimbTopZ)
		{
			return Existing;
		}
	}

	// Bottom: the floor this wall stands on.
	float BottomZ = HitLocation.Z - 400.0f;
	{
		FHitResult FloorHit;
		const FVector Start = HitLocation + Normal * 60.0f + FVector(0.0f, 0.0f, 50.0f);
		if (World->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0.0f, 0.0f, 6000.0f), ECC_WorldStatic, Params))
		{
			BottomZ = FloorHit.Location.Z;
		}
	}

	// Top: climb the plane in 50 cm steps until the wall stops, then pin the lip
	// to a few centimetres. No cap: a 24 m face is climbable to its real top.
	float Solid = HitLocation.Z;
	float Air = -1.0f;
	for (float Z = HitLocation.Z + 50.0f; Z < HitLocation.Z + 12000.0f; Z += 50.0f)
	{
		if (!WallAt(Z))
		{
			Air = Z;
			break;
		}
		Solid = Z;
	}
	if (Air < 0.0f)
	{
		return nullptr; // a wall with no top: not something to climb
	}
	for (int32 i = 0; i < 6; ++i)
	{
		const float Mid = (Solid + Air) * 0.5f;
		(WallAt(Mid) ? Solid : Air) = Mid;
	}
	const float TopZ = Solid;

	const FVector SpawnLocation(HitLocation.X, HitLocation.Y, (BottomZ + TopZ) * 0.5f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCClimbPoint* NewPoint = World->SpawnActor<ADCCClimbPoint>(SpawnLocation, Normal.Rotation(), SpawnParams);
	if (!NewPoint)
	{
		return nullptr;
	}

	// A thin panel on the wall that collides with nothing: the rock does the
	// blocking, and an invisible box must never be what a trace or camera finds.
	if (NewPoint->Surface)
	{
		NewPoint->Surface->SetBoxExtent(FVector(5.0f, 250.0f, FMath::Max(50.0f, (TopZ - BottomZ) * 0.5f)));
		NewPoint->Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	NewPoint->WallNormal = Normal;
	NewPoint->WallPoint = HitLocation;
	NewPoint->ClimbBottomZ = BottomZ;
	NewPoint->ClimbTopZ = TopZ;
	NewPoint->PromptText = NSLOCTEXT("DCC", "ClimbPromptGeneric", "Climb this surface?");
	return NewPoint;
}
