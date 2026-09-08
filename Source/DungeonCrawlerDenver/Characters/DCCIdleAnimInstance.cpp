#include "Characters/DCCIdleAnimInstance.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UDCCIdleAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Spread starting phase across instances so a room full of crawlers
	// reads as individually alive rather than a single puppet with several
	// bodies nodding in perfect unison.
	HeadNodPhaseOffset = FMath::FRandRange(0.0f, 2.0f * PI);
}

void UDCCIdleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(TryGetPawnOwner());

	// Off for anything that is not a living, present crawler, or that is
	// already holding a deliberate pose - a selfie or a skill cast both
	// want a still head, not a nod competing for attention under them.
	if (!Character || !Character->IsCrawler() || !Character->IsAlive() || Character->IsDowned()
		|| Character->IsInSelfieMode() || Character->IsCasting())
	{
		HeadNodPitch = 0.0f;
		return;
	}

	if (!bHeadBoneResolved)
	{
		bHeadBoneResolved = true;
		if (const USkeletalMeshComponent* SkelMesh = GetSkelMeshComponent())
		{
			for (const FName& Candidate : HeadBoneCandidates)
			{
				if (SkelMesh->GetBoneIndex(Candidate) != INDEX_NONE)
				{
					HeadBoneName = Candidate;
					break;
				}
			}
		}
	}

	if (HeadBoneName.IsNone())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float TimeSeconds = World ? World->GetTimeSeconds() : 0.0f;
	HeadNodPitch = FMath::Sin(TimeSeconds * HeadNodSpeed + HeadNodPhaseOffset) * HeadNodAmplitudeDegrees;
}
