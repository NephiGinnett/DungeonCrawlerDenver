#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCIntroDirector.generated.h"

class UBoxComponent;
class UCameraComponent;
class ADCCCharacterBase;
class APlayerController;

// ---------------------------------------------------------------------------
// THE MORNING OF, AND THEN THE COLLAPSE
//
// The game's first ninety seconds, in one actor. A crawler wakes up in their
// own bedroom on an ordinary morning, gets up, walks around a room full of
// their own things, opens the front door, and loses the planet.
//
// WHY THIS IS CODE AND NOT A LEVEL SEQUENCE
//
// A Level Sequence is the right tool for this in a normal project and is not
// available in this one: there is no sequencer toolset in the MCP setup, so a
// .uasset sequence would have to be hand-authored in the editor and could not
// be edited, diffed or fixed from here. A director actor running timed beats
// is uglier on paper and is the version that can actually be maintained - the
// same argument DCCHUD.h already makes for drawing the menu on Canvas instead
// of authoring a UMG widget.
//
// THE SILENCE IS THE POINT
//
// Nothing in the bedroom speaks. No System AI, no announcements, no HUD
// prompts, no objective tracker. The tone bible's whole voice depends on the
// audience having heard quiet first - the AI's corporate cheer is only funny
// against something, and this is that something. Its first line in the entire
// game lands after the collapse, which makes the collapse its introduction as
// well as the dungeon's.
//
// So: resist adding a tutorial prompt here. "Press W to walk" would cost the
// intro the only thing it is for.
// ---------------------------------------------------------------------------

/** Which beat the intro is currently playing. Drives Tick and nothing else. */
UENUM(BlueprintType)
enum class EDCCIntroPhase : uint8
{
	/** Not started, or already finished and torn down. */
	Idle,

	/** Lying down, looking at the ceiling, fading up from black. */
	Waking,

	/** Camera rising from pillow height to standing, pitching level. */
	Rising,

	/** Control handed back. Free to wander the bedroom. Nothing is watching yet. */
	Exploring,

	/** Through the front door. The last few seconds of the old world. */
	Street,

	/** It goes. */
	Collapsing,

	MAX UMETA(Hidden)
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	ADCCIntroDirector();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * The doorway out of the apartment. Crossing it starts the collapse.
	 *
	 * Deliberately a trigger on the director rather than a separate actor: the
	 * intro is one thing that happens once, and splitting it across two placed
	 * actors is two things to wire up and one of them to forget.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Intro")
	TObjectPtr<UBoxComponent> DoorTrigger;

	/**
	 * Where the camera lies while asleep. Positioned at pillow height, pitched
	 * up at the ceiling; Rising animates it to standing before handing back.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Intro")
	TObjectPtr<UCameraComponent> IntroCamera;

	// --- Timings -----------------------------------------------------------

	/** Black before the eyes open. Long enough to read as sleep, not as a load screen. */
	UPROPERTY(EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 2.5f;

	/** Time spent looking at the ceiling before getting up. */
	UPROPERTY(EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.0"))
	float LyingSeconds = 3.0f;

	/**
	 * How long standing up takes.
	 *
	 * Time-driven rather than waiting for an input on purpose: an intro that
	 * needs a keypress to continue can be sat in forever by a player who has
	 * walked away, and in a five-player session that stalls everyone.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.1"))
	float RiseSeconds = 2.2f;

	/** Beat outside the door before anything happens. The last quiet moment. */
	UPROPERTY(EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.0"))
	float StreetSeconds = 3.5f;

	/** The collapse itself, from first tremor to full black. */
	UPROPERTY(EditAnywhere, Category = "Intro|Timing", meta = (ClampMin = "0.1"))
	float CollapseSeconds = 6.0f;

	// --- Geometry ----------------------------------------------------------

	/** Camera height while asleep, relative to this actor. Pillow, not floor. */
	UPROPERTY(EditAnywhere, Category = "Intro|Camera")
	FVector SleepingCameraOffset = FVector(0.0f, 0.0f, 45.0f);

	/** Camera height once standing. Eye level. */
	UPROPERTY(EditAnywhere, Category = "Intro|Camera")
	FVector StandingCameraOffset = FVector(0.0f, 0.0f, 165.0f);

	/** Pitched up at the ceiling. Straightens to level over RiseSeconds. */
	UPROPERTY(EditAnywhere, Category = "Intro|Camera")
	float SleepingCameraPitch = 62.0f;

	/** Peak camera displacement during the collapse, in centimetres. */
	UPROPERTY(EditAnywhere, Category = "Intro|Camera", meta = (ClampMin = "0.0"))
	float CollapseShakeAmplitude = 24.0f;

	// --- Destination -------------------------------------------------------

	/**
	 * Where the crawler arrives once the world is gone - the top of the Queue,
	 * in world space.
	 *
	 * A raw location rather than a PlayerStart reference because the Queue's
	 * entrance is a specific spot on a specific floor, and a PlayerStart placed
	 * there would also become a candidate for ordinary respawns.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Destination")
	FVector DungeonArrivalLocation = FVector(0.0f, 0.0f, 210.0f);

	UPROPERTY(EditAnywhere, Category = "Intro|Destination")
	FRotator DungeonArrivalRotation = FRotator::ZeroRotator;

	/**
	 * How far each crawler lands from DungeonArrivalLocation, spread around a
	 * ring rather than stacked on the exact same point.
	 *
	 * Five capsules landing on one coordinate is the same "materialising inside
	 * one another" problem ADCCGameMode's own spawn selection exists to avoid -
	 * the collapse should not reintroduce it.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Destination", meta = (ClampMin = "0.0"))
	float ArrivalSpreadRadius = 150.0f;

	/** Off for a session that should skip straight to the dungeon. */
	UPROPERTY(EditAnywhere, Category = "Intro")
	bool bRunOnBeginPlay = true;

	/** Server only. Puts the local crawler to sleep and starts the sequence. */
	UFUNCTION(BlueprintCallable, Category = "Intro")
	void BeginIntro();

	/**
	 * Server only. Skips everything and drops the crawler at the arrival point.
	 *
	 * Exists for testing and for the DM: sitting through a two-minute cold open
	 * on the fifteenth playtest of the afternoon is not a good use of anyone's
	 * evening.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Intro")
	void SkipIntro();

	UFUNCTION(BlueprintPure, Category = "Intro")
	EDCCIntroPhase GetPhase() const { return Phase; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleDoorOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& Sweep);

	/**
	 * Every beat change runs here so all five clients see the same thing at the
	 * same time. The camera work is local to each machine - each player is
	 * looking through their own eyes - but WHEN it happens is the server's call.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EnterPhase(EDCCIntroPhase NewPhase);

	/** Applies whatever a phase needs on entry: view target, fade, input lock. */
	void ApplyPhase(EDCCIntroPhase NewPhase);

	/** The local player's controller, or null on a machine that has none yet. */
	APlayerController* GetLocalController() const;

	/** The local player's pawn as a crawler, or null. */
	ADCCCharacterBase* GetLocalCrawler() const;

	/** Hands the camera back to the pawn and returns control. */
	void ReleaseToPlayer(float BlendSeconds);

	/**
	 * Server only. Teleports EVERY connected crawler to DungeonArrivalLocation,
	 * spread around ArrivalSpreadRadius so they do not stack.
	 *
	 * Deliberately not GetLocalCrawler() - that resolves GetFirstPlayerController,
	 * which on the server is one arbitrary connection (the host, on a listen
	 * server), not "everyone". Both call sites that end the intro need every
	 * one of the five players moved, not just whichever one the server happens
	 * to be.
	 */
	void TeleportAllCrawlersToDungeon() const;

private:
	EDCCIntroPhase Phase = EDCCIntroPhase::Idle;

	/** Seconds spent in the current phase. Reset on every transition. */
	float PhaseElapsed = 0.0f;

	/**
	 * Latched so a second crawler walking through the doorway cannot restart a
	 * collapse that is already underway - the same guard ADCCFloorExit uses to
	 * stop five people printing five floor-complete announcements.
	 */
	bool bCollapseTriggered = false;
};
