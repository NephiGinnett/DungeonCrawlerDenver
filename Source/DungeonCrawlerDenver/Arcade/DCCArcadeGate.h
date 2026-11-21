#pragma once

#include "CoreMinimal.h"
#include "Arcade/DCCArcadeArena.h"
#include "Puzzles/DCCPuzzleDoor.h"
#include "DCCArcadeGate.generated.h"

class ADCCCharacterBase;
class ADCCEnemyCharacter;
class UDCCDungeonGrid;

/**
 * Gate Run (2026-11-15, Zoe: "a large shining portal opens up, and the player enters it ... a series of 3-10
 * connected rooms with challengers, enemies, puzzles"). A free-standing minigame on the RPG systems: real stats,
 * skills, gear, the dungeon's enemies and chests, and the Puzzles/ doors, plates and breakers.
 *
 *  - The plaza: everyone gathers at a shining gate. It opens after a short countdown; walk into it.
 *  - The gate: 3 to 10 connected rooms (UDCCDungeonGrid, the same layout as the World Dungeon) built from a
 *    replicated seed on every machine. Its rank (E to S) comes from the room count and sets how hard it hits.
 *  - Rooms: FIGHT (clear it), TRIAL (step in the ring, survive three waves), VAULT (a pressure plate or a breaker
 *    panel opens it; the crowbar always works and costs Favor), TREASURE (a guarded chest), and the GATEKEEPER in
 *    the farthest room. The way home is already standing there, dark, and it lights when the Gatekeeper falls.
 *    Step through to clear the gate.
 *  - More rooms (2026-11-15): DUEL (one named Champion), QUIET ROOM (empty, until someone reaches the middle and the
 *    shades come out of the walls), REST SHRINE (the Warden's light heals the whole party, once), and THE GAUNTLET
 *    (floor tiles light up in moving waves and burn; cross on the dark ones to the green pad).
 *  - More rooms (2026-11-20): THE TOLL (hold the booth while the line arrives), THE OUTAGE (the lights go, then two
 *    waves from the walls), THE CLOCK (three waves that do not wait), and THE HOLDING PEN (clear the ring; the fence burns).
 *  - The Gatekeeper's hall: pillars and fire, a boss bar, slams telegraphed by a red ring under whoever it picks,
 *    and shades called at two-thirds and one-third health.
 *  - The gate collapses when the clock runs out. Whatever was cleared still pays.
 * Held gear shows here (other arcade games hide it), because here it is the point.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeGate : public ADCCArcadeArena
{
	GENERATED_BODY()

public:
	virtual FName GetGameId() const override { return TEXT("Gate"); }
	virtual UDCCDungeonGrid* GetChaseGrid() const override { return Grid; }
	virtual bool KeepsStatusPanel() const override { return true; }
	virtual bool ShowsHeldGear() const override { return true; }
	virtual void OnRespawned(APlayerController* PC) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr float CountdownSeconds = 8.0f;

	/** Dev (DCC.GateIn): open the gate now and send every player in. With a room index, to that room instead. */
	void DebugEnterAll(int32 RoomIndex = 0);
	/** Dev (DCC.GateClear): knock out every foe in a room (or all rooms with -1), and solve its vault. */
	void DebugClearRoom(int32 RoomIndex);

protected:
	virtual void PrepareServer() override;
	virtual void BuildLocal() override;
	virtual void ServerBegin() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void ClientTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;

private:
	/** The layout: replicated so every machine builds the same rooms. */
	UPROPERTY(Replicated)
	int32 Seed = 0;

	UPROPERTY(Replicated)
	int32 RoomCount = 0;

	UPROPERTY(Replicated)
	bool bExitOpen = false;

	/** The Gatekeeper's slam: where, how wide, and when it lands (server clock; 0 = none). Every machine draws the ring. */
	UPROPERTY(Replicated)
	FVector SlamCenter = FVector::ZeroVector;
	UPROPERTY(Replicated)
	double SlamAt = 0.0;
	UPROPERTY(Replicated)
	float SlamRadius = 360.0f;

	UPROPERTY(Transient)
	TObjectPtr<UDCCDungeonGrid> Grid;

	struct FRoomState
	{
		FName Role;
		TArray<TWeakObjectPtr<ADCCEnemyCharacter>> Foes;
		TWeakObjectPtr<ADCCPuzzleDoor> Vault;
		FVector Center = FVector::ZeroVector;
		FVector Prize = FVector::ZeroVector;
		bool bStarted = false;
		bool bCleared = false;
		int32 WavesLeft = 0;
		double NextWaveAt = 0.0;
		/** The toll booth: seconds someone has stood in the window. */
		double Held = 0.0;
		/** The gauntlet: where its tiles start, how many, and the pad that ends it. */
		FVector TileMin = FVector::ZeroVector;
		int32 TilesX = 0;
		int32 TilesY = 0;
		FVector Goal = FVector::ZeroVector;
		/** The boss room: the Gatekeeper, its phase (0, 1 at two-thirds, 2 at one-third), the next slam. */
		TWeakObjectPtr<ADCCEnemyCharacter> Boss;
		int32 Phase = 0;
		double NextSlamAt = 0.0;
	};
	TArray<FRoomState> Rooms;

	/** The gauntlet's floor tiles on this machine, coloured every frame from the server clock. */
	struct FTile { int32 Room = 0; int32 X = 0; int32 Y = 0; TWeakObjectPtr<AActor> Actor; uint8 Shown = 255; };
	TArray<FTile> Tiles;
	static constexpr float TileSize = 200.0f;
	/** 0 dark, 1 about to burn, 2 burning. The same on every machine for the same moment. */
	static uint8 TileState(int32 Room, int32 X, int32 Y, double Time);
	TMap<TWeakObjectPtr<APlayerController>, double> NextBurnAt;
	bool bSlamResolved = true;
	TWeakObjectPtr<AActor> SlamRing;
	FVector SlamRingScale = FVector::OneVector;
	void DressRoom(int32 RoomIndex);
	void TickRoomsServer(double Now, const TArray<APlayerController*>& Players);
	void SpawnShades(int32 RoomIndex, int32 Count);

	/** Built on this machine yet (it waits for the seed on a client). */
	bool bDungeonBuilt = false;
	bool bExitDrawn = false;

	// Server state.
	bool bGateOpen = false;
	bool bWrapped = false;
	double OpensAt = 0.0;
	double EndsAt = 0.0;
	int32 Score = 0;
	int32 RankIndex = 0;
	TSet<TWeakObjectPtr<APlayerController>> Entered;
	/** Kills since the last payout. Paid every few seconds, so a fight is one notice, not ten. */
	int32 PendingKills = 0;
	double NextKillPayout = 0.0;
	/** Server: tickets to everyone who has entered the gate (and is still connected). */
	void PayParty(int32 Tickets, const FString& Why);

	static FVector DungeonOrigin() { return FVector(-5200.0f, 30000.0f, 0.0f); }
	static FVector EntryPortal() { return FVector(0.0f, 1800.0f, 0.0f); }
	static FName RoleFor(int32 InSeed, int32 Index, int32 Count);
	static const TCHAR* RankName(int32 Index);
	static int32 RankFor(int32 Rooms);

	void BuildDungeon();
	/** The ring is the gate. Power is the shine, the lights and the sparks. The way home is placed dark and lit later. */
	void DrawPortal(const FVector& At, float Height, const FLinearColor& Colour, float Yaw, bool bFrame = true, bool bPowered = true);
	void SpawnRoomContent();
	ADCCEnemyCharacter* SpawnFoe(TSubclassOf<ADCCEnemyCharacter> Class, const FVector& At, int32 RoomIndex, bool bBoss);
	void SpawnWave(int32 RoomIndex, int32 Count);
	void SpawnChest(const FVector& At, int32 Tier);
	void SpawnVault(int32 RoomIndex, bool bPlates);
	void ClearRoom(int32 RoomIndex, const TCHAR* Why);
	void SendIn(APawn* Pawn, int32 Slot);
	void RefreshStatus();
	void Wrap(bool bCleared);
	bool FindRoomCell(int32 RoomIndex, FRandomStream& R, FVector& Out) const;

	UFUNCTION()
	void OnFoeDied(ADCCCharacterBase* Character);

	UFUNCTION()
	void OnVaultOpened(ADCCPuzzleDoor* Door, EDCCPuzzleSolveMethod Method);
};
