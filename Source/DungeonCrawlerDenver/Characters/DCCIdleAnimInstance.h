#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DCCIdleAnimInstance.generated.h"

class ADCCCharacterBase;

/**
 * Drives a small set of continuous idle cosmetics that plain AnimSequence
 * playback can't express - starting with a slow head nod. Deliberately a
 * native base class rather than doing this math in the AnimGraph itself:
 * the graph side stays to a single "Transform (Modify) Bone" node reading
 * HeadNodPitch, while everything about WHAT that value is - the sine wave,
 * the gating (alive, a crawler, not mid-selfie-pose or mid-cast) - lives
 * here where it is easy to read, tune, and extend later. Zoe, 2026-09-04:
 * "have the players heads tilt to the beat of the music?... even just a
 * slow nod not to the beat would be nice." This is the slow-nod half -
 * true beat sync would need Engine's own (free, already-installed but
 * disabled) AudioSynesthesia plugin baking an onset curve for a specific
 * song, which is a separate, larger piece of work.
 *
 * One instance of this class serves EITHER skeleton family in this project
 * (the shared Meshy rig, the mannequin) - see HeadBoneCandidates for why a
 * second subclass is not needed just because the bone name differs.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCIdleAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Additive pitch, in degrees, for the AnimGraph's head Modify Bone node to apply. */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Idle")
	float HeadNodPitch = 0.0f;

	/**
	 * Bone the AnimGraph should target. Resolved once, lazily, from
	 * HeadBoneCandidates against this mesh's actual skeleton - stays
	 * NAME_None (and HeadNodPitch stays 0) if nothing on the list exists,
	 * rather than feeding the Modify Bone node a bone it does not have.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Idle")
	FName HeadBoneName = NAME_None;

	/**
	 * Tried in order; whichever exists on this skeleton wins. Covers both
	 * bone-naming conventions in this project - "Head" on the shared Meshy
	 * rig, "head" on the mannequin - without needing a second AnimBlueprint.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|Idle")
	TArray<FName> HeadBoneCandidates = { TEXT("Head"), TEXT("head") };

	UPROPERTY(EditDefaultsOnly, Category = "DCC|Idle")
	float HeadNodAmplitudeDegrees = 4.0f;

	/** Radians/sec inside the sine - keep this well under 1 for "a slow nod", not a bob. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|Idle")
	float HeadNodSpeed = 0.6f;

private:
	/** Randomised per-instance so a group of crawlers reads as individually alive rather than nodding in lockstep. */
	float HeadNodPhaseOffset = 0.0f;

	bool bHeadBoneResolved = false;
};
