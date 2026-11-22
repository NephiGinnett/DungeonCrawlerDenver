#pragma once

#include "CoreMinimal.h"
#include "Arcade/DCCArcadeArena.h"
#include "DCCArcadeHaunt.generated.h"

class ADCCCharacterBase;
class ADCCEnemyCharacter;
class APointLight;
class UDCCDungeonGrid;
class USpotLightComponent;

/** One player's tape in Dead Air, replicated so every HUD can show it. */
USTRUCT()
struct FDCCFootageEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FString Player;

	/** Score banked on tape so far. Becomes views at upload. */
	UPROPERTY()
	float Footage = 0.0f;

	/** Seconds of tape left. */
	UPROPERTY()
	float Tape = 0.0f;

	/** How good the shot is right now (0 when not recording). Drives the viewfinder meter. */
	UPROPERTY()
	float LiveShot = 0.0f;

	UPROPERTY()
	bool bRecording = false;

	/** After the clock: uploaded from the van, or left behind. */
	UPROPERTY()
	int32 Views = -1;

	/** The note this player is reading off a body (2026-11-15), shown until NoteUntil (server clock). */
	UPROPERTY()
	FString NoteTitle;

	UPROPERTY()
	FString NoteText;

	UPROPERTY()
	double NoteUntil = 0.0;

	/** The kit this player bought at the Mine Outfitters (DCCChopShop, bMine), loaded at the start of the round. */
	UPROPERTY()
	bool bKit = false;
	UPROPERTY()
	uint8 KitAir = 0;
	UPROPERTY()
	uint8 KitLamp = 0;
	UPROPERTY()
	uint8 KitHat = 0;
	UPROPERTY()
	uint8 KitHarness = 0;
	UPROPERTY()
	uint8 KitGas = 0;
	/** Lightsticks left this round. */
	UPROPERTY()
	int32 Sticks = 0;
	/** Air in the tank, 0..1. Drains below the depth your tanks reach, refills above it. */
	UPROPERTY()
	float Air = 1.0f;
};

/**
 * One post on the Scroller (2026-11-15): a crew member's uploaded tape, shown at the end of the round on a fake
 * social feed (the phone-style feed idea from The Familiar's scroller, rebuilt in the game). The "video" is that
 * player's best moment, replayed from the round's recording. Ranked by popularity; the rank pays.
 */
USTRUCT()
struct FDCCReelClip
{
	GENERATED_BODY()

	/** Whose camera (one; the array is kept for a shared shot). */
	UPROPERTY()
	TArray<FString> Cams;

	UPROPERTY()
	int32 Likes = 0;

	UPROPERTY()
	int32 Comments = 0;

	UPROPERTY()
	int32 Shares = 0;

	/** 1 is trending. */
	UPROPERTY()
	int32 Rank = 0;

	UPROPERTY()
	int32 Tickets = 0;

	UPROPERTY()
	int32 Credits = 0;

	/** Server clock: the stretch of the round this clip replays. */
	UPROPERTY()
	double From = 0.0;

	UPROPERTY()
	double To = 0.0;

	UPROPERTY()
	FString Title;

	/** Views that made it out of the mine. Zero still gets a post. */
	UPROPERTY()
	int32 Views = 0;

	/** The explanation card: ghosts, monsters, traps, notes, stories, views, likes, tickets. */
	UPROPERTY()
	FString Card;
};

/** A thrown lightstick: where it was thrown from, where it landed, and when (server clock). */
USTRUCT()
struct FDCCLightstick
{
	GENERATED_BODY()

	UPROPERTY()
	FVector From = FVector::ZeroVector;

	UPROPERTY()
	FVector At = FVector::ZeroVector;

	UPROPERTY()
	double Time = 0.0;
};

/**
 * Something that happened in the mine (2026-11-15): a plank giving way, a runaway cart, a powder blast, a firedamp
 * flash, a lamp snuffed. Replicated as kind, where and when, so every machine plays the same moment.
 */
USTRUCT()
struct FDCCMineEvent
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	/** 1 plank break, 2 cart run, 3 powder blast, 4 firedamp flash, 5 lamp snuffed. */
	UPROPERTY()
	uint8 Kind = 0;

	/** Which trap (plank, cart, crate or pocket index), or the ghost for a snuff. */
	UPROPERTY()
	int32 Index = 0;

	UPROPERTY()
	FVector At = FVector::ZeroVector;

	/** Server clock. */
	UPROPERTY()
	double Time = 0.0;

	/** A player, for a snuffed lamp. */
	UPROPERTY()
	FString Who;

	/** A cart run's direction: true runs from B back to A. */
	UPROPERTY()
	bool bReverse = false;
};

/** A rock coming down from the roof of the mine. Replicated as where and when; every machine draws the fall. */
USTRUCT()
struct FDCCMineRock
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	/** On the floor, where it lands. */
	UPROPERTY()
	FVector At = FVector::ZeroVector;

	/** Server clock. Three knocks when it appears; it falls at this time. */
	UPROPERTY()
	double FallsAt = 0.0;
};

/**
 * Dead Air (2026-11-13; moved underground 2026-11-15): an original found-footage minigame. The party is a
 * ghost-hunting crew filming the Lucky Tuesday, a silver mine that runs under the mountain, for a paranormal
 * stream. The shift that went down in 1893 never came up. V starts and stops your body cam. What is in frame
 * earns footage:
 *
 *   an anomaly (a miner's lamp with no miner)               small
 *   a ghost miner (there, then not)                         more
 *   the thing in the deepest stope                          big
 *   a monster                                               good
 *   a monster going for someone                             great
 *   a teammate getting hurt, or down, on camera             the best (the audience is like that)
 *
 * Centred, close and unobstructed shots score more. Tape is limited. When the clock runs out, footage uploads
 * only for crew standing in the cage at the top of the shaft; anyone else's stays in the mine. Dying halves your
 * tape. Views are recorded per player (DCCProfile "Haunt.BestViews").
 *
 * The mine is the dungeon generator's layout (UDCCDungeonGrid) from a replicated seed: timbered drifts with rails,
 * stopes with carts and spoil, lanterns that gutter, helmet lamps on everyone, and knocking in the rock.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeHaunt : public ADCCArcadeArena
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	TArray<FDCCFootageEntry> Footage;

	UPROPERTY(Replicated)
	float TimeLeft = 0.0f;

	const FDCCFootageEntry* FindEntry(const FString& Player) const;

	virtual FName GetGameId() const override { return TEXT("Haunt"); }
	virtual FString GetTip() const override { return TEXT("Press 1 to film. Be back in the cage before time runs out."); }
	virtual UDCCDungeonGrid* GetChaseGrid() const override { return Grid; }
	/** A tag on the cage makes the stream: footage, once a round each, and an achievement. */
	virtual void OnTagged(class ADCCCharacterBase* Painter, const FVector& At) override;
	virtual void OnWorldSound(class USoundBase* Sound, const FVector& At) override;
	virtual void OnAttackSeen(class ADCCCharacterBase* Attacker, class ADCCCharacterBase* Target, bool bHit) override;
	/** Dev (DCC.HauntEnd): the clock runs out now, so the reel can be checked without a five-minute round. Server. */
	void DebugEndRound()
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Haunt] the clock runs out (dev); %d sound(s) logged this round"), SoundLog.Num());
		EndsAt = ServerTime();
	}
	/** Footage for a spray or an emote while the body cam is rolling. */
	void GrantFootage(const FString& Name, float Amount);

	/** Dev (DCC.MineGo): every player to stope RoomIndex, or (bDrift) to a straight stretch of tunnel, facing along it.
	 *  RoomIndex -1 faces the first ghost miner; -2 stands next to the first body with a note. */
	void DebugGoTo(int32 RoomIndex, bool bDrift);

	/** Notes found this round (indices into the placed bodies), and stories finished. Replicated for the HUD. */
	UPROPERTY(Replicated)
	TArray<int32> NotesRead;

	UPROPERTY(Replicated)
	TArray<int32> StoriesDone;

	// ---- the end of the round: data extraction, then the highlight reel (2026-11-15) ----------------------------------
	/** 0 playing, 1 extracting data (bars over the heads in the cage), 2 the highlight reel, 3 done. */
	UPROPERTY(Replicated)
	uint8 ReelPhase = 0;
	UPROPERTY(Replicated)
	double ExtractStart = 0.0;
	UPROPERTY(Replicated)
	double ReelStart = 0.0;
	UPROPERTY(Replicated)
	TArray<FDCCReelClip> Reel;
	/** Crew whose tape made it into the cage. */
	UPROPERTY(Replicated)
	TArray<FString> Uploaders;
	static constexpr double ExtractSeconds = 6.0;
	/** 12400 -> "12.4K", the way a feed counts. */
	static FString CompactCount(int32 N)
	{
		return N >= 1000000 ? FString::Printf(TEXT("%.1fM"), N / 1000000.0f) : N >= 1000 ? FString::Printf(TEXT("%.1fK"), N / 1000.0f) : FString::FromInt(N);
	}
	/** Seconds each post plays before the feed scrolls on by itself (anyone can flick sooner). */
	static constexpr double PostSeconds = 10.0;
	/** The post this machine's viewer is on, and since when (server clock). Local. */
	int32 FeedIndex = 0;
	double FeedSince = 0.0;
	/** How long a post plays before the feed moves on (the trailer shortens it). */
	double FeedAutoSeconds = PostSeconds;
	/** Which clip is on now and how far into it; false when the reel is not playing. */
	bool CurrentClip(int32& OutIndex, double& OutPlayTime, double& OutIntoClip) const;
	/** The cameras' pictures for the HUD (one per camera in the current clip). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextureRenderTarget2D>> ReelTargets;
	/** Server clock, for the HUD. */
	double NowServer() const { return ServerTime(); }

	/** How deep a point is: 0 at the cage, 1 at the farthest reach of the workings. */
	float DepthAt(const FVector& Location) const;
	/** Server, from UDCCWalletComponent::Server_ThrowLightstick (G). */
	void ThrowLightstick(const FString& Player, const FVector& Eye, const FVector& Dir);
	UPROPERTY(Replicated)
	TArray<FDCCLightstick> Lightsticks;
	/** Distance to the nearest firedamp pocket's edge (the gas meter). */
	float NearestGas(const FVector& Location) const;

	/** Traps (2026-11-15, Docs/dead-air-mine-plan.md section 1). Public for the HUD and the lamp key. */
	bool IsBadAir(const FVector& Location) const;
	bool IsInFiredamp(const FVector& Location) const;
	bool IsLampOff(const FString& Player) const;
	/** Server: a rockfall at At (on the floor), with the three knocks first. The Tommyknocker calls this. Refused in
	 *  the cage room. */
	bool QueueRock(const FVector& At, double Delay = 3.5);
	/** Server, from UDCCWalletComponent::Server_SetMineLamp (the L key). */
	void SetLamp(const FString& Player, bool bOn);
	/** Seconds the local player has been in bad air (the HUD's canary). */
	float LocalBadAirSeconds = 0.0f;
	/** The ledge for the knockers: where it is, and whether a crust has been left. */
	FVector LedgeAt = FVector::ZeroVector;
	UPROPERTY(Replicated)
	bool bCrustLeft = false;

	/** Which stories are in the mine this round (indices into DCCMineNotes::Stories()). Same on every machine. */
	TArray<int32> StoriesHere;
	int32 NotesInStory(int32 Story) const;
	int32 NotesReadInStory(int32 Story) const;

	// ---- the cage (2026-11-16, Zoe: "There needs to be a physical cage we crawl in and out of, and it needs to be a safe
	// space from enemies including aoe. ... They need to be able to go back early if they're all out and a round needs
	// to end and close up cleanly when all players have died.") ----------------------------------------------------
	/** Inside the cage: nothing hurts you there, and it is where tapes must be when the round ends. */
	bool IsInCage(const FVector& Location) const { return InVan(Location); }
	/** Everyone still standing is in the cage (after the crew has been out): the send-it-up vote is open. */
	UPROPERTY(Replicated)
	bool bAllInCage = false;
	UPROPERTY(Replicated)
	int32 CageVotesUp = 0;
	UPROPERTY(Replicated)
	int32 CageCrew = 0;
	/** Server, from UDCCWalletComponent::Server_CageVote. */
	void VoteCageUp(APlayerController* PC, bool bUp);

	static constexpr float TapeSeconds = 120.0f;
	static constexpr float RoundSeconds = 300.0f;
	static constexpr float ViewsPerPoint = 137.0f;

protected:
	friend class ADCCHauntTrailerDirector;
	virtual void PrepareServer() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void ClientTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;

private:
	void EnsureGrid();
	void BuildHouse();
	void SpawnMonsters();
	void Score(float DeltaSeconds);
	void Wrap();
	bool InVan(const FVector& Location) const;
	FDCCFootageEntry& EntryFor(const FString& Player);

	UPROPERTY(Replicated)
	int32 Seed = 0;

	/** The whole first room (the cage stands in its middle). Traps and drifts keep out of it. */
	FBox FoyerBox = FBox(ForceInit);
	bool InFoyer(const FVector& Location) const { return FoyerBox.IsValid && FoyerBox.IsInsideXY(Location); }
	FVector CageCenter = FVector::ZeroVector;
	float CageHalf = 400.0f;
	/** Which way the cage's gate faces (toward the way out of the room). */
	FVector GateDir = FVector(1, 0, 0);
	TSet<FString> CageVoters;
	/** Tape warnings already given this round, per player: 1 at 30 s, 2 at 10 s, 3 full. */
	TMap<FString, int32> TapeWarned;
	bool bCrewLeftCage = false;
	double AllDownSince = 0.0;

	/**
	 * F.I.A.T. companions playing Dead Air themselves (2026-11-21): each walks the mine on its own, films what it
	 * finds (the same scoring as a player's camera), and makes for the cage when the clock, the tape or its health
	 * says so. Its footage uploads under its own name; its player gets the tickets.
	 */
	struct FMineCompanion
	{
		FString Name;
		FString Boss;
		/** The anomaly it is after (an index into Anomalies), or -1. */
		int32 Goal = -1;
		/** Filming the goal until then; 0 until it has arrived. */
		double FilmUntil = 0.0;
		bool bHome = false;
		/** What its camera points at, for Score. */
		FVector Subject = FVector::ZeroVector;
		TSet<int32> Filmed;
		/** Closest it has got to the goal, and when, so a shot it cannot reach is given up. */
		float BestDist = BIG_NUMBER;
		double BestAt = 0.0;
		/** Running from a monster (to the cage, filming over its shoulder) until then. */
		double FleeUntil = 0.0;
		/** Filming a monster since then; after a few seconds it leaves the monsters alone for a while. */
		double BeastSince = 0.0;
		double BeastsOffUntil = 0.0;
	};
	TMap<TWeakObjectPtr<class ADCCSummonedAlly>, FMineCompanion> MineCrew;
	void TickMineCrew();
	void CrewSay(const FMineCompanion& Mate, const FString& Line);
	void BuildCage();
	/** Dead Air's dressing passes: textures, props, filler (Arcade/DCCHauntDressing.cpp). */
	void DressDetail();
	/** The mine's sound: bed, drips, creaks, rumbles, taps, and the song very low (Arcade/DCCHauntAudio.cpp). */
	void TickMineAudio(float DeltaSeconds);
	void TickCage();

	UPROPERTY(Transient)
	TObjectPtr<UDCCDungeonGrid> Grid;

	struct FAnomaly
	{
		FVector Location;
		float Value = 4.0f;
		TWeakObjectPtr<AActor> Visual;
		/** A ghost miner (a Forge body that flickers and fades when you get close), not a drifting lamp. */
		bool bGhost = false;
		float Phase = 0.0f;
	};
	/** The mine dressing (2026-11-15): timbers, rails, carts, rock, the cage. Every machine, from the seed. */
	void DressMine();
	/**
	 * Corridors are dug two cells wide. True for the lower (or left) cell of a straight two-wide stretch, with the
	 * middle of the tunnel (between the two cells) in Centre: where rails, carts, planks and gas go.
	 */
	bool CorridorSpan(int32 X, int32 Y, FVector& Centre, bool& bAlongX) const;
	/** Rock-lined walls, cave-stone floor, and the marks later visitors left on Mount Fourteener. */
	void DressCave();
	AActor* SpawnGhostMiner(const FVector& Feet, float Yaw, int32 BodyIndex);
	/** The bodies (2026-11-15): where, which note, and the local visual. Placed from the seed on every machine. */
	struct FBody
	{
		int32 Note = 0;
		FVector At = FVector::ZeroVector;
		TWeakObjectPtr<AActor> Visual;
	};
	/** Note indices this machine's player has already walked up to, so the outline stays off. */
	TSet<int32> SeenBodies;
	void RefreshBodyGlow();
	TArray<FBody> Bodies;
	void PlaceBodies();
	AActor* SpawnBody(const FVector& Feet, float Yaw, int32 BodyIndex);
	/** Server: who has read what, so a note is shown to each player once. */
	TMap<FString, TSet<int32>> ReadBy;
	void ReadNotes();
	/** Local: payoff ghosts already shown. */
	TSet<int32> PayoffsShown;
	void ShowPayoff(int32 Story);

	/** Rockfalls, each warned by three knocks (the tommyknockers' warning, Docs/dead-air-mine-plan.md trap 3). */
	UPROPERTY(Replicated)
	TArray<FDCCMineRock> Rocks;
	int32 NextRockId = 1;
	double NextRockAt = 0.0;
	TSet<int32> RocksLanded;
	TSet<int32> RocksKnocked;
	TMap<int32, TWeakObjectPtr<AActor>> RockVisuals;
	void TickRocksServer();
	void TickRocksLocal();

	// ---- the reel (DCCHauntReel.cpp) ----------------------------------------------------------------------------------
	/** Server: every shot each player took, while recording, and what was best in frame. */
	struct FShotMark { double T = 0.0; float Shot = 0.0f; uint8 Label = 0; FVector Where = FVector::ZeroVector; };
	TMap<FString, TArray<FShotMark>> ShotLog;
	void BuildReel();
	void TickReelServer();
	static FString LabelTitle(uint8 Label);

	/** Every machine records the round as it saw it, ten times a second: bodies, and each crew member's camera. */
	struct FRecBody { TWeakObjectPtr<class ADCCCharacterBase> Who; FVector Loc = FVector::ZeroVector; float Yaw = 0.0f; float Speed = 0.0f; bool bAlive = true; };
	struct FRecCam { FString Name; FVector Loc = FVector::ZeroVector; FRotator Rot = FRotator::ZeroRotator; };
	struct FRecFrame { double T = 0.0; TArray<FRecBody> Bodies; TArray<FRecCam> Cams; };
	TArray<FRecFrame> Recording;

	/**
	 * The round's sound, for the reel (2026-11-22, Zoe: "make sure that our game properly captures audio and includes
	 * it in the dead air filming"). Like Recording, every machine keeps its own: what played, where, how loud, when.
	 * The reel plays back what falls inside each clip, placed as the clip's camera heard it, over the mine's own air.
	 */
	struct FRecSound
	{
		double T = 0.0;
		/** A generated sound (knock, cart, blast, bell): its samples live as long as the game. */
		const TArray<int16>* Pcm = nullptr;
		int32 Rate = 22050;
		/** Or a sound asset (a character's attack). */
		TWeakObjectPtr<class USoundBase> Asset;
		FVector At = FVector::ZeroVector;
		float Volume = 1.0f;
		float Pitch = 1.0f;
		float Falloff = 3500.0f;
		bool b2D = false;
	};
	TArray<FRecSound> SoundLog;
	void NoteSound(const TArray<int16>& Pcm, int32 Rate, const FVector& At, float Volume, float Falloff, bool b2D = false);
	/** Play a logged sound now, moved from where the clip's camera heard it to where the viewer is listening. */
	void PlayReelSound(const FRecSound& Sound);
	/** Where a point in the clip (seen from its camera) sits for this machine's listener. */
	FVector ReelToListener(const FVector& At) const;
	/** The clip camera this frame (the first one, on a MULTICAM post), and whether there is one. */
	FTransform ReelEar = FTransform::Identity;
	bool bReelEarValid = false;
	int32 ReelSoundClip = -1;
	double ReelSoundCursor = 0.0;
	double NextRecordAt = 0.0;
	void RecordFrame();
	void TickReelLocal(float DeltaSeconds);
	void TickRecLights();
	void EndReelLocal();
	/** Local stand-ins for the characters during the reel, keyed by the character they copy. */
	TMap<TWeakObjectPtr<class ADCCCharacterBase>, TWeakObjectPtr<AActor>> Puppets;
	TWeakObjectPtr<AActor> ReelRig;
	TArray<TWeakObjectPtr<class USceneCaptureComponent2D>> ReelCaptures;
	TArray<TWeakObjectPtr<class USpotLightComponent>> ReelLamps;
	int32 ReelPlayedClip = -1;
	double ReelLastPlayTime = 0.0;
	TSet<int32> ReelEventsPlayed;
	bool bReelHidRealCast = false;
	/** The blinking red light over everyone who is recording. Local. */
	TMap<TWeakObjectPtr<class ADCCCharacterBase>, TWeakObjectPtr<class UPointLightComponent>> RecLights;
	TMap<TWeakObjectPtr<class ADCCCharacterBase>, TWeakObjectPtr<class UStaticMeshComponent>> RecBulbs;
	TWeakObjectPtr<AActor> ExtractGlow;

	// ---- the Outfitters' kit (2026-11-15) ----------------------------------------------------------------------------
	TArray<float> DepthMap;
	void BuildDepth();
	void LoadKit(FDCCFootageEntry& Entry, APlayerController* PC);
	FDCCFootageEntry* KitOf(const APawn* Pawn);
	/** Crew damage from rock, carts and blasts, after the hard hat. */
	void HitCrew(class ADCCCharacterBase* Crawler, float Fraction, int32 Min, bool bHat, bool bGas);
	FVector OutfitPad = FVector::ZeroVector;
	bool bWasOnOutfitPad = false;
	TMap<int32, TWeakObjectPtr<AActor>> StickVisuals;
	TSet<int32> StickLit;

	/** Short-lived effects (a blast's fire, the dust before a rockfall) and when to stop them. Local. */
	struct FBurst { TWeakObjectPtr<class UNiagaraComponent> Comp; double Until = 0.0; };
	TArray<FBurst> Bursts;
	void Burst(const TCHAR* SystemPath, const FVector& At, double Seconds, float Scale = 1.0f);
	bool bCrustShown = false;

	// ---- traps ----------------------------------------------------------------------------------------------------
	UPROPERTY(Replicated)
	TArray<FDCCMineEvent> Events;
	UPROPERTY(Replicated)
	TArray<FString> LampsOff;
	int32 NextEventId = 1;
	TSet<int32> EventsPlayed;
	void AddEvent(uint8 Kind, int32 Index, const FVector& At, const FString& Who = FString(), bool bReverse = false);
	bool EventActive(uint8 Kind, int32 Index, double Within, const FDCCMineEvent** Out = nullptr) const;
	void PlaceTraps();
	void TickTrapsServer(float DeltaSeconds);
	void TickTrapsLocal(float DeltaSeconds);
	void Blast(int32 Crate);
	void OnDamageLanded(ADCCCharacterBase* Dealer, ADCCCharacterBase* Target, int32 Amount);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Rotten planks over a winze, and the sump level they drop you into. */
	struct FPlank { FVector At; bool bBroken = false; TArray<TWeakObjectPtr<AActor>> Boards; TMap<FString, float> StoodFor; };
	TArray<FPlank> Planks;
	FVector SumpStart = FVector::ZeroVector;
	FVector SumpLadder = FVector::ZeroVector;
	TMap<FString, FVector> SumpReturn;

	/** A runaway ore cart on a straight run of rails. */
	struct FCartRun { FVector A; FVector B; TWeakObjectPtr<AActor> Cart; bool bAtB = false; TSet<FString> HitThisRun; };
	TArray<FCartRun> Carts;
	double NextCartAt = 0.0;

	/** Old powder crates. */
	struct FCrate { FVector At; bool bGone = false; TArray<TWeakObjectPtr<AActor>> Visuals; };
	TArray<FCrate> Crates;

	/** Firedamp pockets: lit lamps burn blue inside, and four seconds of a lit lamp sets it off. */
	struct FPocket { FVector At; float Radius = 650.0f; float Fuse = 0.0f; double QuietUntil = 0.0; };
	TArray<FPocket> Pockets;

	/** Blackdamp: one stope with bad air. */
	int32 BadAirRoom = -1;
	TMap<FString, float> BadAirFor;

	/** The canary's ghost (a small warm light) leading toward the next unread note, when The Canary is down here. */
	TWeakObjectPtr<AActor> Canary;
	FVector CanaryPos = FVector::ZeroVector;

	/** The knockers' ledge. */
	TMap<FString, float> LedgeFor;

	/** Lamp-snuffer. */
	double NextSnuffAt = 0.0;

	/** The cage bell. It lies (rings early) until The Bell at 4:15 is finished; then it rings honestly at 30 s. */
	UPROPERTY(Replicated)
	int32 BellRings = 0;
	int32 BellsHeard = 0;
	double NextLieAt = 0.0;
	double LieUntil = 0.0;
	bool bHonestBellRung = false;
	TArray<FAnomaly> Anomalies;
	TArray<TWeakObjectPtr<APointLight>> RoomLights;
	TArray<float> RoomLightBase;
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<USpotLightComponent>> Flashlights;

	TArray<TWeakObjectPtr<ADCCEnemyCharacter>> Monsters;
	TMap<TWeakObjectPtr<ADCCCharacterBase>, int32> LastHealth;
	TMap<TWeakObjectPtr<ADCCCharacterBase>, double> LastHurtAt;
	/** Where a monster last stood while alive, and a few seconds of its death for the reel. */
	TMap<TWeakObjectPtr<ADCCEnemyCharacter>, FVector> MonsterLiveAt;
	TArray<TPair<FVector, double>> DeathFlashes;
	TSet<TWeakObjectPtr<ADCCCharacterBase>> Penalised;

	FBox VanBox = FBox(ForceInit);
	/** Who has tagged the cage this round. */
	TSet<FString> CageTaggers;
	bool bShiftBossDown = false;
	bool bHouseBuilt = false;
	bool bMonstersSpawned = false;
	bool bWrapped = false;
	double EndsAt = 0.0;
	float ScoreTimer = 0.0f;

	static constexpr int32 GridSize = 22;
	static constexpr float TileSize = 440.0f; // corridors are two cells wide (UDCCDungeonGrid::Dig), so 8.8 m
	static constexpr float WallHeight = 480.0f;
};
