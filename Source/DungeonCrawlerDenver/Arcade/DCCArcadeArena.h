#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCArcadeArena.generated.h"

class APlayerController;
class UStaticMesh;
class UMaterialInterface;

/**
 * Base for an arcade game's world. The GameMode spawns one in InitGame on an
 * empty map; it builds the place at runtime (like the hub and the dungeon),
 * lights it, provides PlayerStarts, runs the game on the server, and tells
 * every player what is going on through two replicated fields the HUD draws:
 * a Title and a few StatusLines.
 *
 * Geometry is built locally on every machine in BeginPlay and is deterministic,
 * so nothing but the status, the cars and the players needs replicating.
 */
UCLASS(Abstract)
class DUNGEONCRAWLERDENVER_API ADCCArcadeArena : public AActor
{
	GENERATED_BODY()

public:
	ADCCArcadeArena();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, from the GameMode's InitGame: PlayerStarts must exist before anyone logs in. */
	void SpawnStarts();

	UPROPERTY(Replicated)
	FString Title;

	UPROPERTY(Replicated)
	TArray<FString> StatusLines;

	UPROPERTY(Replicated)
	FLinearColor Accent = FLinearColor(1.0f, 0.8f, 0.4f);

	/**
	 * Server. The round is over. Games with a GetGameId() put up the end-of-round
	 * choice (PLAY AGAIN / BACK TO THE DEN, 2026-11-13): everyone votes with a
	 * button, majority wins, a tie or no votes goes to the Den, and it is decided
	 * the moment everyone has voted or after Seconds (at least 20). Without a game
	 * id it is the old countdown back to the lobby.
	 */
	void ReturnToLobbyIn(float Seconds);

	/** Server, from UDCCWalletComponent::Server_ArcadeVote. Changing your mind is allowed. */
	void CastVote(APlayerController* PC, bool bPlayAgain);

	/** The end-of-round choice is up. The HUD shows the vote buttons while this is true. */
	UPROPERTY(Replicated)
	bool bRoundOver = false;

	UPROPERTY(Replicated)
	int32 SecondsToDecide = 0;

	UPROPERTY(Replicated)
	int32 VotesAgain = 0;

	UPROPERTY(Replicated)
	int32 VotesDen = 0;

	UPROPERTY(Replicated)
	int32 VotersExpected = 0;

	/** A boss bar across the screen (2026-11-15, Gate Run's Gatekeeper). Empty name or health below 0 hides it. */
	UPROPERTY(Replicated)
	FString BossName;
	UPROPERTY(Replicated)
	float BossHealth = -1.0f;

	/** What the HUD shows in this game (2026-11-13: only what players need). */
	virtual bool ShowsHealth() const { return true; }
	/** Weapons and other hand-held gear are drawn. On everywhere since 2026-11-15 (Zoe: "we don't need to hide weapons
	 *  anymore. I just didn't want them distracting in the beginning"). A game can still return false to hide them. */
	virtual bool ShowsHeldGear() const { return true; }
	/** The skill hotbar, stamina and the climbing-kit panel. */
	virtual bool ShowsClimbHud() const { return false; }

	/** Keep the full status panel up for the whole round. Other games fold to alert lines after a minute. */
	virtual bool KeepsStatusPanel() const { return false; }

	/** Server: a player who died has just been given a new body. Fourteener moves them to their checkpoint. */
	virtual void OnRespawned(APlayerController* PC) {}

	/** This game's id in DCCArcade::Games() ("Climb"...), for PLAY AGAIN. None for the lobby and the front door. */
	virtual FName GetGameId() const { return NAME_None; }

protected:
	/** Server, before the PlayerStarts are placed (a seed, say, that decides where they go). */
	virtual void PrepareServer() {}
	virtual void BuildLocal() {}
	virtual void ServerBegin() {}
	virtual void ServerTick(float DeltaSeconds) {}
	virtual void ClientTick(float DeltaSeconds) {}
	virtual void GetStartTransforms(TArray<FTransform>& Out) const;

	struct FSky
	{
		float SunPitch = -40.0f;
		float SunIntensity = 8.0f;
		FLinearColor SunColour = FLinearColor::White;
		float SkyIntensity = 1.0f;
		FLinearColor FogColour = FLinearColor(0.45f, 0.55f, 0.7f);
		float FogDensity = 0.01f;
		/**
		 * Character shade correction (2026-11-14, Zoe: characters "look like washed out grayscale characters because
		 * they are so much brighter and fluorescent than their surroundings"). Every character in the arena gets
		 * M_CharacterShade as an overlay: it multiplies them darker and toward the scene's own colour, sparing the edges.
		 * Negative means automatic: the darker the scene (SunIntensity), the stronger the blend; tint and rim come
		 * from FogColour. 0 turns it off.
		 */
		float ShadeStrength = -1.0f;
		float ShadeRimStrength = -1.0f;
	};
	virtual FSky GetSky() const { return FSky(); }

	// Local, non-replicated building blocks.
	/** bCollide false for scenery nothing should ever bump into (cheaper, too). */
	AActor* Block(const FVector& Center, const FVector& Size, const FLinearColor& Colour, float Yaw = 0.0f, bool bCollide = true);

	/**
	 * A real model from the installed packs (2026-11-13), e.g. "/Game/Viking_village/Meshes/Architecture/SM_Cabin_01".
	 * Base is where the bottom middle of the model goes. Fit is the box to fit it in: components left at 0 are
	 * ignored, and the model keeps its proportions (the smallest ratio wins) unless bStretch, which fits each axis.
	 * Fit all zero keeps the model's own size, times Scale. Returns null (and logs) if the mesh is missing, so a
	 * pack that is not installed costs decoration, never a crash.
	 */
	AActor* Prop(const TCHAR* MeshPath, const FVector& Base, float Yaw, const FVector& Fit, bool bCollide = false,
		bool bStretch = false, float Scale = 1.0f);

	/** Swaps the material on something Block/Shape/Prop made, e.g. a rock material on the mountain. */
	static void Paint(AActor* Actor, const TCHAR* MaterialPath);

	/** Like Block, for any engine basic shape ("Cylinder", "Cone", "Sphere", "Cube"), with full rotation. */
	AActor* Shape(const TCHAR* ShapeName, const FVector& Center, const FVector& Size, const FLinearColor& Colour,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = true);
	void Sign(const FVector& Location, float Yaw, const FString& Text, const FColor& Colour, float Size);
	void Light(const FVector& Location, const FLinearColor& Colour, float Intensity, float Radius);
	void DestroyLocal(TArray<TObjectPtr<AActor>>& Actors);

	/** Every connected player controller (server). */
	void GetPlayers(TArray<APlayerController*>& Out) const;

	double ServerTime() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalActors;

	/** This machine's character shade (see FSky::ShadeStrength), shared by every character. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> CharacterShade;
	float NextShadeCheck = 0.0f;
	void ApplyCharacterShade();

	/** Blocks spawned by Block() land here unless redirected (Neon streams its own). */
	TArray<TObjectPtr<AActor>>* BlockSink = nullptr;

private:
	void SetupSky();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	double ReturnAt = -1.0;
	bool bTravelling = false;

	TMap<TWeakObjectPtr<APlayerController>, bool> RoundVotes;
	void TickRoundOver();

	/** Players without a pawn (died) since when; they respawn at a start after a few seconds. */
	TMap<TWeakObjectPtr<APlayerController>, double> NoPawnSince;
};
