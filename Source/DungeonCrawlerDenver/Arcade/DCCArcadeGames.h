#pragma once

#include "CoreMinimal.h"
#include "Arcade/DCCArcadeArena.h"
#include "DCCArcadeGames.generated.h"

class ADCCRover;
class APlayerState;

/** The Den: the arcade lobby. Stand on a game's pad to vote for it. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeLobby : public ADCCArcadeArena
{
	friend class FDCCDenDecor; // Arcade/DCCDenDecor.cpp: each vote pad dressed as its game
	GENERATED_BODY()
protected:
	virtual void BuildLocal() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void ClientTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;
public:
	virtual bool ShowsHealth() const override { return false; } // nothing can hurt you in the Den
private:
	FVector PadCenter(int32 Index) const;
	/** The prize counter: stand on it and the Den shop opens (on your machine only). */
	FVector ShopPadCenter() const { return GetActorLocation() + FVector(2000.0f, -1050.0f, 0.0f); }
	/** The Chop Shop (2026-11-14): the same counter screen, opened on its upgrades tab. */
	FVector ChopPadCenter() const { return GetActorLocation() + FVector(-2000.0f, -1050.0f, 0.0f); }
	bool bWasOnShopPad = false;
	bool bWasOnChopPad = false;
	double CountdownEndsAt = -1.0;
	bool bAllVotedCountdown = false;
	bool bLeaving = false;
	float RefreshTimer = 0.0f;
};

/** A falling rock in Fourteener. Replicated as where and when, so every machine draws the same fall. */
USTRUCT()
struct FDCCRockfall
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FVector Start = FVector::ZeroVector;

	/** Server clock. Before it, the rock sits at the top glowing (the warning); after it, it falls. */
	UPROPERTY()
	double StartTime = 0.0;

	/** Where it comes to rest: a ledge, High Camp, or the ground. */
	UPROPERTY()
	float StopZ = 0.0f;
};

/**
 * Fourteener: co-op climbing up a peak, starting from Fort Tiner, the base camp
 * at its foot (2026-11-13).
 *
 * The mountain: a wide lower block (to High Camp, halfway) and a narrow upper
 * block (to the summit). Four faces, two kinds of route:
 *   - the EXPRESS face (toward the fort): ledges only every 12 m, no ice, and it
 *     takes twice the rockfall. Fast and exposed.
 *   - the three SWITCHBACK faces: a ledge every 6 m, but icy patches.
 * High Camp (the shelf where the mountain narrows) is a checkpoint: die above
 * it and you come back there.
 *
 * Hazards: rockfall (a glowing rock at the top for 1.5 s, then it falls, and
 * ledges stop it, so they are shelter), gusts (warned, then three times the
 * climbing stamina cost on one face for a few seconds), and ice (twice the cost).
 *
 * Co-op: stand on a ledge above a climbing teammate and they are ROPED IN
 * (cheaper climbing, no fall damage, points for you). Two or more resting on
 * the same ledge for a few seconds PITCH A CAMP: faster rest there, and it
 * becomes everyone's respawn point.
 *
 * Points become arcade tickets when the round ends.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeClimb : public ADCCArcadeArena
{
	GENERATED_BODY()
public:
	virtual FName GetGameId() const override { return TEXT("Climb"); }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRespawned(APlayerController* PC) override;
	virtual bool ShowsClimbHud() const override { return true; }

	/** Server: hand a Fourteener item (DCCClimbItems) to a crawler. */
	void GiveItem(class ADCCCharacterBase* Crawler, FName ItemId, bool bQuiet);
	/** Server: [Q] and [T] from UDCCWalletComponent. */
	void UseTool(class ADCCCharacterBase* Crawler);
	void NextTool(class ADCCCharacterBase* Crawler);
	/** A hotbar quick slot: use this particular tool, whichever one [T] has selected. */
	void UseToolId(class ADCCCharacterBase* Crawler, FName ToolId);
	/** Fort Tiner's Trading Post (2026-11-15): stand on its pad to buy climbing kit with tickets. */
	FVector TradingPostPad() const { return FortCenter() + FVector(250.0f, 1200.0f - 330.0f - 420.0f, 0.0f); }
	bool IsAtTradingPost(const FVector& Location) const { return FVector::Dist2D(Location, TradingPostPad()) < 700.0f && FMath::Abs(Location.Z - TradingPostPad().Z) < 500.0f; }
protected:
	virtual void BuildLocal() override;
	virtual void ServerBegin() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void ClientTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;
private:
	friend class FDCCClimbAmbience; // Arcade/DCCClimbAmbience.cpp: sound and feel
	/** Arcade/DCCClimbStrands.cpp: winding spires and rock layers round the trunk. */
	void BuildStrands();
	UPROPERTY(Replicated)
	TArray<FDCCRockfall> Rocks;

	/** The face a gust is warned for or hitting (0 east/express, 1 north, 2 west, 3 south), or -1. */
	UPROPERTY(Replicated)
	int32 GustFace = -1;

	UPROPERTY(Replicated)
	bool bGustActive = false;

	/** Camps pitched by the crew this round. */
	UPROPERTY(Replicated)
	TArray<FVector> Camps;

	void BuildMountain();
	void BuildFort();
	void SpawnTownsfolk();
	void AddPoints(APlayerState* PlayerState, int32 Points, const TCHAR* Why);
	void PayOut();
	bool bWasOnTradingPad = false;
	FVector FortCenter() const { return GetActorLocation() + FVector(WideHalf + 3400.0f, 0.0f, 0.0f); }

	// Mountain geometry, shared by building, hazards and checks.
	static FVector FaceNormal(int32 Face);
	static FVector FaceAlong(int32 Face);
	static FString FaceName(int32 Face);
	int32 FaceOf(const FVector& Location) const;
	static float HalfWidthAt(float Z) { return Z < MidZ ? WideHalf : NarrowHalf; }
	/** Ledge heights on a face: every 12 m on the express face, every 6 m on the switchbacks, none at High Camp. */
	static TArray<float> LedgeHeights(int32 Face);
	static bool IsExpress(int32 Face) { return Face == 0; }

	struct FIce { int32 Face; float ZMin; float ZMax; float Lateral; float Width; };
	static const TArray<FIce>& IcePatches();

	// Server hazards and co-op.
	void TickRocks();
	void TickHazardsAndTeam(float DeltaSeconds);
	void QueueRock();
	void Warn(int32 Face, const FString& Message);
	bool IsStanding(const class ADCCCharacterBase* Crawler) const;

	/** A player's Fourteener kit for this round: tools in hand, boosts, artifacts carried. */
	struct FClimbKit
	{
		TArray<FName> Tools;
		int32 Selected = 0;
		double ChalkUntil = 0.0;
		bool bCrampons = false;
		bool bWindCloak = false;
		int32 HardHats = 0;
		int32 SafetyLines = 0;
		TArray<FName> Artifacts;
		/** 100 fed .. 0 starving. Drains over the round, faster on the wall. Caps max stamina (TickKits). */
		float Hunger = 100.0f;
		int32 HungerTold = 0;
	};
	TMap<TWeakObjectPtr<APlayerState>, FClimbKit> Kits;
	FClimbKit* KitOf(const class ADCCCharacterBase* Crawler);
	void SyncKit(class ADCCCharacterBase* Crawler);
	void TickKits(float StepSeconds);
	void SpawnChests();

	TMap<TWeakObjectPtr<APlayerState>, float> SessionBest;
	TMap<TWeakObjectPtr<APlayerState>, int32> Points;
	TMap<TWeakObjectPtr<APlayerState>, float> BelayCredit;
	TMap<TWeakObjectPtr<APlayerState>, FVector> Checkpoints;
	TSet<TWeakObjectPtr<APlayerState>> Summited;
	TSet<TWeakObjectPtr<APlayerState>> ReachedHighCamp;
	TSet<uint64> RockHits;
	TMap<FIntVector, float> CampProgress;
	FString HazardLine;
	double HazardLineUntil = 0.0;
	double NextRockAt = 0.0;
	double NextGustAt = 0.0;
	double GustStartsAt = 0.0;
	double GustEndsAt = 0.0;
	int32 NextRockId = 1;
	bool bFirstSummitTaken = false;
	bool bTeamBonusPaid = false;
	bool bPaidOut = false;
	double StartedAt = 0.0;
	float RefreshTimer = 0.0f;
	float TeamTimer = 0.0f;

	// Local visuals.
	TWeakObjectPtr<class APointLight> Campfire;
	TMap<int32, TWeakObjectPtr<AActor>> RockVisuals;

	/**
	 * Sky raptors (2026-11-14, Zoe: "giant birds that fly around and dive bomb or throw rocks at the players").
	 * Arcade/DCCClimbBirds.cpp. Two circle the peak; now and then one screams, paints a red line at a climber and
	 * dives along it (step off the line and it misses), or flies over a climber and drops a rock (a hard hat still
	 * stops it). Off while dcc.ClimbHazards is 0.
	 */
	struct FBird
	{
		TWeakObjectPtr<class ADCCMountainBird> Actor;
		float Angle = 0.0f;       // around the peak
		float Height = 0.0f;      // circling height, above the arena origin
		float Phase = 0.0f;
		double NextAttackAt = 0.0;
		double StateUntil = 0.0;
		FVector From = FVector::ZeroVector, To = FVector::ZeroVector;
		TWeakObjectPtr<class ADCCCharacterBase> Prey;
		int32 PreyFace = 0;
		bool bHit = false;
	};
	TArray<FBird> Birds;
public:
	/** Dev check (DCC.BirdAttack): the first bird dives (or drops, if bDrop) at the local player now, wherever they are. */
	void DebugBirdAttack(bool bDrop);
private:
	void SpawnBirds();
	void TickBirds(float DeltaSeconds);
	TArray<TWeakObjectPtr<AActor>> GustStreaks;
	TArray<TWeakObjectPtr<AActor>> RopeVisuals;
	int32 CampsBuilt = 0;

	// The mountain's size (2026-11-13: "much taller and wider"). Everything on it is laid out from these.
	static constexpr float TopZ = 16000.0f;    // 160 m (was 96; Zoe wanted it much taller and harder)
	static constexpr float MidZ = 8000.0f;     // High Camp, halfway
	static constexpr float WideHalf = 1200.0f; // lower block: 24 m across
	static constexpr float NarrowHalf = 700.0f; // upper block: 14 m across
	static constexpr float SummitFeet = 14115.0f;
	static constexpr float TrailheadFeet = 9000.0f;
	static constexpr float RoundSeconds = 900.0f; // fifteen minutes: a slower climb up a taller mountain
	static constexpr float Gravity = 980.0f;
};

/** A net on the asphalt. Replicated as where and until when, so every machine draws the same patch. */
USTRUCT()
struct FDCCRallyBoot
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FVector At = FVector::ZeroVector;

	UPROPERTY()
	double Until = 0.0;

	/** Who dropped it. They can drive back over it. */
	UPROPERTY()
	FString Owner;
};

/** One truck's line on the Repo Rally board and shift report (2026-11-14). Server fills, everyone reads. */
USTRUCT()
struct FDCCRallyCrewRow
{
	GENERATED_BODY()

	UPROPERTY() FString Name;
	UPROPERTY() float Score = 0.0f;
	UPROPERTY() int32 Nabs = 0;
	UPROPERTY() float Ledger = 0.0f;
	UPROPERTY() float Fuel = 100.0f;
	UPROPERTY() int32 Fines = 0;
	/** Paid at the end of the shift; 0 until then. */
	UPROPERTY() int32 Tickets = 0;
	/** 0 stock, 1 runner, 2 net, 3 winch, 4 tank. */
	UPROPERTY() int32 Role = 0;
	/** Seconds of the quickest drop, 0 if none. */
	UPROPERTY() float BestNab = 0.0f;
	/** 100 is a sound truck, 0 is wrecked (2026-11-14). Sitting at a gas station repairs it. */
	UPROPERTY() float Hull = 100.0f;
	UPROPERTY() bool bRepairing = false;
};

/** One mark on the board (2026-11-14). */
USTRUCT()
struct FDCCRallyMarkRow
{
	GENERATED_BODY()

	UPROPERTY() FString Plate;
	/** 0 parked, 1 boxed in, 2 running, 3 skittish, 4 spooked (in the air), 5 on a line, 6 on a bumper, 7 respawning. */
	UPROPERTY() uint8 State = 0;
	/** 0..1 how far the reel has it. */
	UPROPERTY() float Strain = 0.0f;
	/** 0 clean .. 100 wrecked. */
	UPROPERTY() float Damage = 0.0f;
	/** The truck with the line on it, if any. */
	UPROPERTY() FString Tower;
};

class ATextRenderActor;

/**
 * Repo Rally: a shift on the streets, racing the other trucks to the marks
 * (2026-11-13). Every player drives a wrecker. Right mouse aims the hook, Q
 * fires it. The line reels the marked car onto the back bumper, and the truck
 * tows it to the yard. The drop pays for how fast the trip was and how clean
 * the car still is. A hook in the wrong car is a fine.
 *
 * The shift gets harder as it goes: parked, then boxed in by other cars, then
 * driving, then cars that leave the ground when a truck gets close or a hook
 * misses them. One line per mark. A second truck can net a runner or take a
 * different car. Splitting up covers more of the city.
 *
 * Gas is on the tab. The tab can go red. In debt, the other repo crews hunt
 * you, and the yard pump closes. They do not take marks.
 *
 * Roles, bought at the yard bays and bolted on for this shift only:
 *   Runner  faster, a shorter hook
 *   Net     throws a net that slows a mark so the reel bites
 *   Winch   a longer shot and a harder reel
 *   Tank    fuel lasts, boosts last
 *
 * The city is dressed from the synthwave and cyberpunk packs (DressCity,
 * DressStreetLife, BuildStreetFun). Nothing here is written into the DCD
 * character save.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeKart : public ADCCArcadeArena
{
	GENERATED_BODY()
public:
	virtual FName GetGameId() const override { return TEXT("Kart"); }
	virtual bool KeepsStatusPanel() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: [Q] and [F] from the car. FireHook is the aimed shot; UseTool is everything else Q does. */
	void UseTool(class ADCCRover* Car);
	void FireHook(class ADCCRover* Car, FVector Dir);
	void CycleTool(class ADCCRover* Car);
	/** A player truck hit something solid. Speed is cm/s at the contact. Plating scales it. */
	void NoteImpact(class ADCCRover* Car, float SpeedCms);

	/** Streets are indices -StreetRadius..StreetRadius. The minimap reads these.
	 *  2026-11-14: a bigger grid, and wider asphalt (RoadHalf). Towers stay inside the block. */
	static constexpr float StreetSpacing = 24000.0f;
	static constexpr int32 StreetRadius = 3;
	static constexpr float MapHalf = 86000.0f;
	static constexpr float RoadHalf = 1800.0f;
	/** Tower footprint and how far its centre sits from the middle of the block. The sidewalk is what is left. */
	static constexpr float TowerFit = 6400.0f;
	static constexpr float TowerInset = 4600.0f;
	static constexpr float ShiftSeconds = 180.0f;

	/** Relative to the arena origin. 0 and 1 are the two street stations. */
	static FVector2D GasOffset(int32 Index);
	static FVector2D YardOffset();

protected:
	virtual void BuildLocal() override;
	virtual void ServerBegin() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void ClientTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;

private:
	struct FTruck
	{
		TWeakObjectPtr<ADCCRover> Car;
		FString Name;
		bool bPlayer = false;
		/** 0 stock, 1 runner, 2 net, 3 winch, 4 tank. Rivals stay 0 and pull harder in code. */
		int32 Role = 0;
		float Fuel = 100.0f;
		float Ledger = 48.0f;
		float Score = 0.0f;
		int32 Nabs = 0;
		int32 Fines = 0;
		int32 Tickets = 0;
		float BestNab = 0.0f;
		int32 Place = 0;
		/** Index into Marks, or -1. */
		int32 HookedId = -1;
		float BasePace = 0.52f;
		double HitUntil = 0.0;
		double NextTell = 0.0;
		double NetReadyAt = 0.0;
		double NextBark = 0.0;
		bool bDebt = false;
		bool bHunting = false;
		bool bLimpTold = false;
		/** 100 sound, 0 wrecked: owners, shots and rams take it, a gas station gives it back (2026-11-14). */
		float Hull = 100.0f;
		bool bRepairing = false;
		double NextHurtTell = 0.0;
		/** Chop Shop levels (DCCChopShop), read from the driver's profile when they are seated. */
		int32 ChopPlating = 0, ChopTank = 0, ChopEngine = 0, ChopLine = 0, ChopPit = 0;
		/** Spare cans bought from Jan. One dumps itself when the tank hits empty. */
		int32 Cans = 0;
		int32 Route = 0;
		TArray<FName> Tools;
		int32 Selected = 0;
		/** Traffic: the crossing it is heading for, and the one it came from (TickTraffic). */
		FIntPoint To = FIntPoint(0, 0), From = FIntPoint(0, 0);
	};

	struct FMark
	{
		TWeakObjectPtr<ADCCRover> Car;
		FString Name;
		/** 0 parked alone, 1 boxed in, 2 driving, 3 spooks and leaves the ground. */
		int32 Kind = 0;
		/** 0 clean, 100 wrecked. The drop pays less as this climbs. */
		float Damage = 0.0f;
		float BasePace = 0.46f;
		double HookedAt = -1.0;
		double RespawnAt = 0.0;
		int32 Route = 0;
		/** The truck that owns the line. One line per mark. */
		TWeakObjectPtr<ADCCRover> Tower;
		bool bDocked = false;
		bool bAlerted = false;
		/** 0 on the street, 1 at hover height. Kind 3 only, and only once it spooks. */
		float Hover = 0.0f;
		/** How many scar warnings this mark has already given its tower. */
		int32 ScarTold = 0;
		TArray<TWeakObjectPtr<ADCCRover>> Blockers;
		/** A driver who stays in and shoots at the truck that has the line (2026-11-14). */
		bool bShootsBack = false;
		double NextShot = 0.0;
		/** The once-a-shift coach. Heavy on the bumper, pays PayMul, does not come back. */
		bool bHeavy = false;
		bool bRetired = false;
		bool bHeavyTold = false;
		float PayMul = 1.0f;
	};

	/** An owner who came out of the building to fight for the car (Arcade/DCCRallyThugs.cpp). */
	struct FThug
	{
		TWeakObjectPtr<class ADCCRallyThug> Actor;
		TWeakObjectPtr<ADCCRover> Target;
		double GiveUpAt = 0.0;
		double NextHit = 0.0;
		/** When it is removed, once it has been knocked flat or given up. 0 while it is still angry. */
		double GoneAt = 0.0;
	};
	TArray<FThug> Thugs;
	/** The two gas station roofs, hidden on this machine while its own truck is under one (the chase camera is above it). */
	TArray<TWeakObjectPtr<AActor>> Canopies;
	/** Server: the angry owners and the drivers who shoot back. Called from ServerTick. */
	void TickThugs(float DeltaSeconds);
	/** Server: roll for trouble when a hook lands on a mark. */
	void RollTrouble(int32 Slot, FTruck& Truck);
	/** bYellOnly: they come out screaming and do not swing or shoot. */
	void SpawnOwner(int32 Slot, FTruck& Truck, bool bYellOnly);
	/** Server: knock some hull off a player's truck. At 0 it is wrecked and drops its tow. */
	void HurtTruck(FTruck& Truck, float Amount, const TCHAR* Why);
	/** Everyone sees and hears it. HitKind 0 punch, 1 shot that hit, 2 shot that missed. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_RallyHit(FVector_NetQuantize From, FVector_NetQuantize To, uint8 HitKind);
	/** Everyone near At hears an owner line. The row is a Voice Studio id (RR_Pryce_01 and friends). */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RallyYell(FName Speaker, FName Row, FVector_NetQuantize At);

	struct FBeacon
	{
		TWeakObjectPtr<AActor> Beam;
		TWeakObjectPtr<ATextRenderActor> Sign;
	};

	FVector Crossing(int32 Ix, int32 Iy) const;
	void FillRoute(int32 Which, TArray<FVector>& Out) const;
	FTransform GridSlot(int32 Index) const;
	FVector ParkSpot(int32 Slot) const;
	FVector Bumper(const ADCCRover* Truck) const;
	bool InYard(const ADCCRover* Car) const;
	void DressCity();
	void SeatPlayers();
	void SpawnStaff();
	void SpawnMark(int32 Slot);
	/** One heavy coach per shift, after the flag. Worth more. A stronger engine tows it faster. */
	void SpawnBonus();
	void TickCity(float DeltaSeconds);
	void Release(int32 Slot, bool bTell);
	void Deliver(int32 Slot);
	void AlertMark(int32 Slot, FTruck* Who);
	void NoteDebt(FTruck& Truck, float Before);
	void ThrowNet(FTruck& Truck, const FVector& At);
	void BuyRole(FTruck& Truck, int32 RoleId);
	void SyncKit(FTruck& Truck);
	void Tell(ADCCRover* Car, const FString& Message);
	FTruck* TruckFor(ADCCRover* Car);
	int32 MarkIndex(const ADCCRover* Car) const;
	float HookLine(const FTruck& Truck) const;
	float HookMul(const FTruck& Truck) const;
	ATextRenderActor* Label(const FVector& At, float Yaw, const FString& Text, const FColor& Colour, float Size);

	TArray<FTruck> Trucks;
	TArray<FMark> Marks;
	TSet<TWeakObjectPtr<APlayerController>> Seated;
	/** "name|role" while that truck is standing in the bay, so a bay pays once per visit. */
	TSet<FString> BayInside;

	UPROPERTY(Replicated)
	TArray<FDCCRallyBoot> Nets;

	bool bPaid = false;
	bool bGo = false;
	double GoAt = 0.0;
	double ShiftEnd = 0.0;
	int32 NextNetId = 1;
	int32 NamesUsed = 0;
	/** 0 parked, 1 boxed, 2 driving, 3 skittish. Climbs on a timer through the shift. */
	int32 Wave = 0;

	TMap<TWeakObjectPtr<ADCCRover>, FBeacon> Beacons;
	TMap<int32, TWeakObjectPtr<AActor>> NetVisuals;

	// 2026-11-13 (Claude, taking Repo Rally over from Grok):
	friend class FDCCRallyAmbience; // Arcade/DCCRallyAmbience.cpp: city sound, boost pads, the song
	/** Server clock when the shift ends; 0 before the flag. The song is timed to end on it. */
	UPROPERTY(Replicated)
	double ShiftEndsAt = 0.0;
	/** Boost pads on the streets and the skyway descents, built locally with the city. */
	TArray<FBox> BoostPads;
	bool bWarned30 = false;
	/** Dispatch on the truck radio, if the line is recorded (ADCCRover::Client_RallyRadio). */
	void Radio(ADCCRover* Car, const TCHAR* Row);
	/** Night sky, ramps, skyways and boost pads: added to the city in DressCity. */
	void BuildStreetFun();
	/** Courtyards, the gap between the towers, the yard fence. Off the lanes, and nothing here collides. */
	void DressStreetLife();
	/** Shopfront rows, wall signs, AC units, junk and overhead wires, instanced (DCCRallyClutter.cpp, 2026-11-14). */
	void DressUrbanClutter();
	/** NPC Forge people walking the sidewalks, local to each machine; they hurry when a truck comes close. */
	void SpawnWalkers();
	void TickWalkers(float DeltaSeconds);
	struct FWalker
	{
		TWeakObjectPtr<class USkeletalMeshComponent> Body;
		FVector Line = FVector::ZeroVector;
		FVector Along = FVector::ForwardVector;
		float Half = 0.0f;
		float D = 0.0f;
		float Dir = 1.0f;
		float Pace = 150.0f;
		bool bHurry = false;
	};
	TArray<FWalker> Walkers;
	UPROPERTY(Transient)
	TObjectPtr<class UAnimSequence> WalkAnim;
	UPROPERTY(Transient)
	TObjectPtr<class UAnimSequence> JogAnim;
	/** Traffic drives the grid: right-hand lane, a new turn at every crossing, never a U-turn. Server. */
	void TickTraffic();
	bool bBriefed = false;
	bool bBonusSpawned = false;
public:
	/** For the HUD's briefing and low-fuel arrow. */
	double GetShiftEndsAt() const { return ShiftEndsAt; }
	double GetNow() const { return ServerTime(); }
	/** A mark's own colour, from its plate (2026-11-14): its outline, briefing marker and hook lock share it. */
	static FLinearColor MarkColour(const FString& Plate);
private:
	/** Each mark's edge glow (M_MarkGlow as an overlay material), this machine only. */
	TMap<TWeakObjectPtr<ADCCRover>, TWeakObjectPtr<class UMaterialInstanceDynamic>> MarkGlows;
public:
	bool IsShiftOn() const { return ShiftEndsAt > 0.0; }
	/** Dev check (DCC.RallyTrouble): "brawler"/"shooter" spawns an owner by the first truck, "hurt" takes 30 hull. */
	void DebugTrouble(const FString& What);

	// The board, the briefing and the shift report (2026-11-14, "nice full menus, clear organization").
	// Filled on the server every tick in RefreshBoard; the HUD draws them (UI/DCCHUD_RallyMenus.cpp).
	/** Player trucks, best first. */
	UPROPERTY(Replicated)
	TArray<FDCCRallyCrewRow> Crew;
	UPROPERTY(Replicated)
	TArray<FDCCRallyMarkRow> Board;
	/** Server clock when the flag drops (the briefing counts down to it). */
	UPROPERTY(Replicated)
	double FlagAt = 0.0;
	/** 0 parked, 1 boxed, 2 running, 3 skittish: what the city is throwing at you now. */
	UPROPERTY(Replicated)
	int32 RallyWave = 0;
private:
	void RefreshBoard();
};
/** Neon Mile: endless synthwave highway. Distance before a crash, per player. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeNeon : public ADCCArcadeArena
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
public:
	virtual FName GetGameId() const override { return TEXT("Neon"); }
protected:
	virtual void ServerBegin() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void ClientTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;
	virtual void BuildLocal() override;
private:
	/** Tickets paid out for the runs still going at the buzzer. */
	bool bPaid = false;
	UPROPERTY(Replicated)
	int32 Seed = 0;

	/** The striped retro sun at the end of the road; follows the leading car. */
	TArray<TWeakObjectPtr<AActor>> SunStripes;
	TArray<FVector> SunOffsets;

	struct FChunk
	{
		TArray<TObjectPtr<AActor>> Actors;
		/** Boost pads in this stretch (world space). Built on every machine from the seed. */
		TArray<FBox> Pads;
	};
	void BuildChunk(int32 Index);
	void BuildBoostPad(FChunk& Chunk, const FVector& At);
	void BuildScenery(int32 Index, const FVector& O);
	void CheckBoostPads();

	/** Server: keeps moving traffic flowing up the road ahead of every car. */
	void UpdateTraffic();
	bool HasTrafficIn(int32 ChunkIndex) const;
	void SeedTraffic(int32 ChunkIndex);

	TArray<TWeakObjectPtr<class ADCCNeonTrafficCar>> Traffic;
	TMap<int32, double> LastSeeded;
	TMap<TWeakObjectPtr<ADCCRover>, double> PadCooldown;
	float TrafficTimer = 0.0f;
	void SeatPlayers();
	void Crash(ADCCRover* Car, const TCHAR* Why);

	TMap<int32, FChunk> Chunks;
	TSet<TWeakObjectPtr<APlayerController>> Seated;
	TMap<TWeakObjectPtr<ADCCRover>, double> CrashCooldown;
	double StartedAt = 0.0;
	float RefreshTimer = 0.0f;
	static constexpr float ChunkLength = 2000.0f;
	// Wide lanes: the rover handles loosely, so it needs room to wander (2026-11-13).
	static constexpr float RoadHalfWidth = 1150.0f;
	static constexpr float LaneWidth = 680.0f;
	/** Traffic cruises at about 50 km/h; the rover does about 200, 400 boosting. */
	static constexpr float TrafficSpeed = 1400.0f;
	static constexpr float TimeLimit = 150.0f;
};
