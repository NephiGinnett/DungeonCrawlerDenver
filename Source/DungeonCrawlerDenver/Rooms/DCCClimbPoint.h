#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stats/DCCAbilityTypes.h"
#include "DCCClimbPoint.generated.h"

class ADCCCharacterBase;
class UBoxComponent;
class UStaticMeshComponent;
class UAnimationAsset;

// -----------------------------------------------------------------------------
// A CLIMBABLE SURFACE
//
// The first piece of the design doc's "anything is possible" promise that is not
// combat: an obstacle the fiction says you might get over, offered to the whole
// table rather than to whoever happens to be standing closest.
//
// Deliberately an authorable actor with two configurable points rather than a
// hardcoded spot, because every rock face and wall in the level is a different
// height. Drop one, set where the climb ends, and it works.
//
// Server-authoritative: the roll happens on the server and the outcome reaches
// everyone by replicated movement. A client may ASK to climb and cannot decide
// whether it worked.
// -----------------------------------------------------------------------------

UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCClimbPoint : public AActor
{
	GENERATED_BODY()

public:
	ADCCClimbPoint();

	virtual void Tick(float DeltaSeconds) override;

	/** Where the prompt appears and the climb can be started from. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climb")
	TObjectPtr<UBoxComponent> Trigger;

	/** Optional visual marker, so the opportunity is visible and not just a hint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climb")
	TObjectPtr<UStaticMeshComponent> Marker;

	/** Shown to any crawler standing in the trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (MultiLine = "true"))
	FText PromptText;

	/** Where a successful climb puts the character, relative to this actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	FVector ClimbEndOffset = FVector(0.0f, 0.0f, 500.0f);

	/** Which stat the attempt rolls. Strength by default; Dexterity suits a scramble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	EDCCAbility ClimbAbility = EDCCAbility::Strength;

	/** Difficulty to beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "1"))
	int32 DifficultyClass = 12;

	/**
	 * Skill whose Rank is added, if the climber knows it.
	 *
	 * Left as a plain name rather than a hard reference so a table that has no
	 * Climbing row yet still works - the check simply runs on the stat alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	FName ClimbSkillId = TEXT("Climbing");

	/** Seconds the ascent takes. The climb animation is scaled to fit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.1"))
	float ClimbDurationSeconds = 2.5f;

	/**
	 * Whether a character who already failed may try again.
	 *
	 * On by default: the published rules mark a skill on any attempt, pass or
	 * fail, so repeated tries are how a bad climber eventually gets better at
	 * climbing. Turn off for a one-shot story obstacle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	bool bAllowRetry = true;

	/** True while at least one crawler is inside the trigger. */
	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsCharacterInRange(const ADCCCharacterBase* Character) const;

	/**
	 * Server only. Rolls the check and starts the ascent on success.
	 *
	 * Returns false when refused - out of range, already climbing, or the roll
	 * failed. The reason is narrated to the event feed either way, because a
	 * failed climb the table can see is a better beat than a silent no.
	 */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	bool TryClimb(ADCCCharacterBase* Character);

protected:
	virtual void BeginPlay() override;

	/** One in-progress ascent. Several characters may climb at once. */
	struct FActiveClimb
	{
		TWeakObjectPtr<ADCCCharacterBase> Character;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Elapsed = 0.0f;
	};

	TArray<FActiveClimb> ActiveClimbs;

	/** Characters that have already succeeded, so the prompt stops nagging them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCCharacterBase>> Succeeded;
};
