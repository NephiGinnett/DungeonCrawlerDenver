#include "Rooms/DCCClimbPoint.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"

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

	return Target + Outward.GetSafeNormal() * CapsuleRadius;
}

FVector ADCCClimbPoint::GetDismountLocation(const ADCCCharacterBase* Character) const
{
	const float HalfHeight = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 90.0f;

	// Over the lip rather than on it: stepping off onto the very edge tends to
	// drop the character straight back down the face they just climbed.
	const FVector Inward = GetActorForwardVector() * 120.0f;

	FVector Dismount = GetActorLocation() + Inward;
	Dismount.Z = GetTopZ() + HalfHeight + 10.0f;

	return Dismount;
}
