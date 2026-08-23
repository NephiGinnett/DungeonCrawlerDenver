#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCClimbPoint.generated.h"

class ADCCCharacterBase;
class UBoxComponent;
class UStaticMeshComponent;
class UWorld;

// -----------------------------------------------------------------------------
// A CLIMBABLE SURFACE
//
// Not a skill check. Anyone may attempt any climbable face; what decides how far
// they get is STAMINA, and how efficiently they spend it is their Climbing Rank.
//
// That is a deliberate design choice over a pass/fail roll. A roll answers "are
// you allowed to try", which is the least interesting question at the table -
// and a failed roll is just a lost turn. Stamina answers "how far can you get
// before your arms give out", which the whole party can watch happening and
// shout advice about. Rank still matters: it makes every second cheaper and the
// climb faster, so a practised climber goes further on the same lungs.
//
// The flow is deliberately player-driven: open the menu, pick the skill, click
// the face, confirm. Nothing happens by walking near it, because the design doc
// wants players choosing to do things rather than being prompted into them.
// -----------------------------------------------------------------------------

UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCClimbPoint : public AActor
{
	GENERATED_BODY()

public:
	ADCCClimbPoint();

	/**
	 * The climbable volume. Clicking anywhere on it starts the flow, and it also
	 * bounds where a climber may move.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climb")
	TObjectPtr<UBoxComponent> Surface;

	/** Optional visual marker so the opportunity is visible, not just discoverable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climb")
	TObjectPtr<UStaticMeshComponent> Marker;

	/** Shown in the confirmation box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (MultiLine = "true"))
	FText PromptText;

	/** Skill whose Rank makes climbing cheaper and faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	FName ClimbSkillId = TEXT("Climbing");

	/** How far from the surface a character may be and still start a climb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ReachDistance = 220.0f;

	/** Highest point a climber may reach, in world Z. Derived from the box if zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	float ClimbTopZ = 0.0f;

	/** Lowest point, in world Z. Derived from the box if zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	float ClimbBottomZ = 0.0f;

	/** True when the character is close enough to start. */
	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsWithinReach(const ADCCCharacterBase* Character) const;

	/** World Z the climber may not pass, accounting for the unset-means-derive rule. */
	UFUNCTION(BlueprintPure, Category = "Climb")
	float GetTopZ() const;

	UFUNCTION(BlueprintPure, Category = "Climb")
	float GetBottomZ() const;

	/**
	 * Where a climber should sit while attached: pulled against the face, at the
	 * given height.
	 *
	 * Keeps the climber visually on the surface rather than floating in front of
	 * it, and gives a consistent anchor when the face is approached from an angle.
	 */
	UFUNCTION(BlueprintPure, Category = "Climb")
	FVector GetAttachLocationFor(const ADCCCharacterBase* Character, float WorldZ) const;

	/** Somewhere safe to step off at the top, just inland of the lip. */
	UFUNCTION(BlueprintPure, Category = "Climb")
	FVector GetDismountLocation(const ADCCCharacterBase* Character) const;

	/**
	 * Server only. What makes an un-authored wall climbable.
	 *
	 * Reuses an existing climb point (authored or previously synthesized) if one
	 * already covers this spot, so repeatedly clicking the same generic wall does
	 * not litter the level with duplicate actors. Otherwise probes up and down
	 * from HitLocation to estimate how tall the surface actually is, and spawns
	 * a new point sized to that span.
	 *
	 * The box is deliberately unrotated: GetAttachLocationFor and IsWithinReach
	 * both work off the component's world AABB and the character's own position
	 * relative to it, not the box's facing, so a precisely-oriented panel was
	 * never required - only that the AABB covers the reachable patch of wall.
	 *
	 * IgnoreActor is excluded from the up/down probe traces - without it, the
	 * requesting character's own capsule (standing right where it clicked) can
	 * be hit by a probe instead of the wall, corrupting the estimated span.
	 * Optional rather than required so the signature stays usable from
	 * contexts with no requesting actor to ignore.
	 */
	static ADCCClimbPoint* FindOrCreateForSurfaceHit(UWorld* World, const FVector& HitLocation,
		const FVector& HitNormal, const AActor* IgnoreActor = nullptr);
};
