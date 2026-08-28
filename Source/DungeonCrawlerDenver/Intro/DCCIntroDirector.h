#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCIntroDirector.generated.h"

class UBoxComponent;
class UCameraComponent;
class UNiagaraSystem;
class USoundBase;
class ADCCCharacterBase;
class APlayerController;

/** One background building or one hero-building chunk, animated during Collapsing. */
USTRUCT()
struct FDCCCollapsingBuilding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Intro|Collapse")
	TObjectPtr<AActor> BuildingActor;

	/**
	 * Extra delay, in seconds, before this building starts falling - stacked on
	 * top of the automatic distance-based stagger below. Leave at 0 for the
	 * background skyline; a hero-building chunk can be nudged so pieces don't
	 * all let go on the exact same frame.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse", meta = (ClampMin = "0.0"))
	float ExtraDelaySeconds = 0.0f;

	/**
	 * True for the handful of hero-building chunks: tumbles with a topple
	 * rotation and a larger fall distance instead of a plain squash. Reserve
	 * this for pieces the camera is actually close to - it costs nothing extra
	 * per-actor, but a distant skyline building tumbling reads as jitter rather
	 * than a fall.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse")
	bool bIsHeroChunk = false;

	/** Cached at collapse start so Tick can Lerp away from a known-good value. */
	FVector CachedScale = FVector::OneVector;
	FVector CachedLocation = FVector::ZeroVector;
	FRotator CachedRotation = FRotator::ZeroRotator;

	/** Deterministic per-building fall direction, derived from its own position
	 *  rather than replicated - every client computes the same tumble without
	 *  needing a synced random seed. */
	FVector TumbleAxis = FVector::UpVector;

	bool bDustSpawned = false;
};

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	/**
	 * The establishing shot for the Collapse: pulled back far enough to show
	 * the hero building and the skyline around it, rather than IntroCamera's
	 * first-person eye height. Position and aim this in the editor once the
	 * skyline is placed - it is a plain child component, so it moves with the
	 * director like everything else here.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Intro|Collapse")
	TObjectPtr<UCameraComponent> CollapseCamera;

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

	// --- Party-ready gate ----------------------------------------------------
	//
	// The Collapse is one shared cutscene and has to start for everyone at
	// once, so the door does not fire on the first crawler through it the way
	// it used to. Instead each arrival is parked here; whoever is not last
	// sees "Waiting for other players" and is otherwise free to keep exploring
	// the demo space - there is no cinematic lock while waiting, because the
	// Collapsing phase already forces every client's camera onto
	// CollapseCamera regardless of where their pawn physically stands, so
	// nothing is gained by freezing them at the door and Street's own existing
	// comment about not punishing a player for walking outside still applies.

	/**
	 * Crawlers who have reached the exit and are waiting on the rest of the
	 * party. Membership rather than a count: the HUD needs to ask "is it ME
	 * who is waiting", not just "how many so far".
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Intro")
	TArray<TObjectPtr<ADCCCharacterBase>> ReadyForCollapse;

	UFUNCTION(BlueprintPure, Category = "Intro")
	bool IsReadyForCollapse(const ADCCCharacterBase* Crawler) const { return ReadyForCollapse.Contains(Crawler); }

	UFUNCTION(BlueprintPure, Category = "Intro")
	int32 GetReadyCount() const { return ReadyForCollapse.Num(); }

	/**
	 * How many crawlers the gate is waiting for.
	 *
	 * Reads GameState's connected PlayerArray rather than a hardcoded five, so
	 * a two-player PIE test gates on two instead of hanging forever waiting
	 * for three players who were never going to connect - the same reasoning
	 * ADCCGameMode::PostLogin already uses for its own player-count line.
	 */
	UFUNCTION(BlueprintPure, Category = "Intro")
	int32 GetExpectedPartySize() const;

	// --- The Collapse: buildings, shake, sound, dust --------------------------
	//
	// Cheap procedural collapse, not Chaos destruction - see the design doc
	// (Docs/the-collapse-cutscene.md) for why. Background buildings are a
	// plain squash toward the ground; hero-building chunks additionally
	// tumble. Sound, camera shake and dust are what actually sell it - see
	// CollapseCrashSounds and DustEffect below.

	/**
	 * The distant skyline. Squash-scales toward the ground, staggered by
	 * distance from this actor so the collapse reads as spreading outward
	 * from the hero building rather than every building dropping in unison.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse")
	TArray<FDCCCollapsingBuilding> SkylineBuildings;

	/**
	 * The building the party was just inside. Expected to be 4-8 separate
	 * static mesh actors - modular kit pieces stacked to form one building,
	 * not one mesh - so each can tumble independently without needing any
	 * mesh-fracturing tool this project's MCP setup does not have. Mark each
	 * entry's bIsHeroChunk true.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse")
	TArray<FDCCCollapsingBuilding> HeroBuildingChunks;

	/** How long one building takes to squash from full height to flat. */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse", meta = (ClampMin = "0.1"))
	float BuildingCollapseDuration = 1.1f;

	/** How much farther a hero chunk falls and rotates, on top of the squash. */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse", meta = (ClampMin = "0.0"))
	float HeroChunkFallDistance = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Intro|Collapse", meta = (ClampMin = "0.0"))
	float HeroChunkTumbleDegrees = 70.0f;

	/**
	 * Seconds of stagger per meter of distance from this actor - the
	 * "spreading outward" timing. Purely a function of each building's own
	 * static position, so every client computes an identical schedule with no
	 * replicated random seed required.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse", meta = (ClampMin = "0.0"))
	float CollapseStaggerSecondsPerMeter = 0.006f;

	/** Gap between each successive entry in CollapseCrashSounds - the "layered/
	 *  staggered slightly per building" texture the design doc asks for. */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse|FX", meta = (ClampMin = "0.0"))
	float CrashSoundStaggerSeconds = 0.2f;

	/**
	 * One or more crash sounds, played 2D (not attenuated by distance) so the
	 * whole party hears the same wall of noise regardless of where their own
	 * pawn happens to be standing - this is a shared cutscene, not five
	 * separate positional events. Layer a few staggered plays for scale per
	 * the design doc; left unassigned by default because no crash/impact SFX
	 * exists anywhere in this project yet (see Docs/the-collapse-cutscene.md).
	 * Silent-but-functional if never assigned - camera shake and dust still
	 * carry the moment.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse|FX")
	TArray<TSoftObjectPtr<USoundBase>> CollapseCrashSounds;

	/**
	 * Dust/debris puff spawned at each building's base the instant it starts
	 * falling. Defaults to Dam_city's own smoke system via a constructor
	 * default (same asset pack as the default skyline meshes, so the look is
	 * already coherent) but is overridable per the project's established
	 * "C++ default, Blueprint/instance override" split.
	 */
	UPROPERTY(EditAnywhere, Category = "Intro|Collapse|FX")
	TSoftObjectPtr<UNiagaraSystem> DustEffect;

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

	/**
	 * Runs once, the instant Collapsing begins: caches every configured
	 * building's starting transform, derives its deterministic tumble axis,
	 * and plays the first crash sound. Called from every machine's ApplyPhase
	 * rather than gated to the server, so a dedicated server's own copy of
	 * the cached data (never rendered, but still evaluated by Tick below)
	 * stays consistent with what players see.
	 */
	void BeginCollapseSequence();

	/**
	 * Per-frame building animation for Collapsing: squash for the skyline,
	 * squash-plus-tumble for hero chunks. Every input (PhaseElapsed, each
	 * building's own cached starting transform) is already synced or
	 * deterministic, so this runs identically on every machine with no
	 * replication of the result required - the same trick Tick's hand-rolled
	 * camera shake already relies on.
	 */
	void TickCollapsingBuildings();

	/** Seconds a building waits before it starts moving: distance-based
	 *  stagger plus its own authored ExtraDelaySeconds. */
	float GetBuildingStaggerDelay(const FDCCCollapsingBuilding& Building) const;

private:
	EDCCIntroPhase Phase = EDCCIntroPhase::Idle;

	/** Seconds spent in the current phase. Reset on every transition. */
	float PhaseElapsed = 0.0f;

	/**
	 * Latched once every crawler has reached the exit, so a sixth overlap (a
	 * waiting player wandering back through the trigger) cannot restart a
	 * collapse that is already underway - the same guard ADCCFloorExit uses to
	 * stop five people printing five floor-complete announcements.
	 */
	bool bCollapseTriggered = false;

	/** CollapseCamera's authored relative transform, cached once at collapse
	 *  start so Tick's shake has a fixed baseline to offset from. */
	FVector CollapseCameraBaseLocation = FVector::ZeroVector;
	FRotator CollapseCameraBaseRotation = FRotator::ZeroRotator;

	/** How many of CollapseCrashSounds have played so far this collapse. */
	int32 NextCrashSoundIndex = 0;
};
