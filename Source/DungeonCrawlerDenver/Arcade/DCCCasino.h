#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCCasino.generated.h"

class ADCCArcadeLobby;
class ADCCCharacterBase;
class APlayerState;
class APointLight;
class ATextRenderActor;
class UStaticMeshComponent;

/**
 * Casino Cascade, the room east of the Safe Room (2026-11-20).
 *
 * Five games and a bar, each against a wall, with the middle of the floor left
 * open. Walk up to a game and press E to sit. Looking at the dealer still
 * opens rules, and Bud still pours. A hand starts with whoever sat down. The
 * rest of the Safe Room keeps voting. Anyone in a hand stands still with dots
 * over their head, so the room reads them as busy rather than as a vote that
 * has not happened. The wheel, the roulette ball, the skee ball and the puck
 * move on every machine from the replicated hand, so the room can watch a result.
 *
 * Skee-Ball and air hockey pay tickets. Roulette, blackjack and the wheel
 * stake 5, and a broke crawler plays on the house.
 */

USTRUCT()
struct FDCCCasinoSeatNet
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	/** 255 means no bet yet. Roulette 10/11/12, wheel 20-25, hockey stores the shot. */
	UPROPERTY()
	uint8 Choice = 255;

	UPROPERTY()
	uint8 Card0 = 0;
	UPROPERTY()
	uint8 Card1 = 0;
	UPROPERTY()
	uint8 Card2 = 0;
	UPROPERTY()
	uint8 Card3 = 0;
	UPROPERTY()
	uint8 Card4 = 0;

	/** In the hand that is already dealing. Someone who sits mid-hand waits. */
	UPROPERTY()
	uint8 bInHand = 0;

	UPROPERTY()
	uint8 bDone = 0;

	/** Skee: points this set. Hockey: shot error times 100. */
	UPROPERTY()
	int16 Score = 0;

	/** What this hand actually took. 0 means the house comped it. */
	UPROPERTY()
	int16 Stake = 0;

	/** Skee: balls already rolled. */
	UPROPERTY()
	uint8 Rolls = 0;

	/** Points on the ball that just left the hand. Paired with BallSerial so a repeated 10 still plays. */
	UPROPERTY()
	uint8 LastBall = 0;

	/** Counts every skee roll, including the one that closes a set. */
	UPROPERTY()
	uint8 BallSerial = 0;
};

USTRUCT()
struct FDCCCasinoTableNet
{
	GENERATED_BODY()

	/** 0 empty, 1 people are sitting, 2 the hand is live, 3 the result is up. */
	UPROPERTY()
	uint8 Phase = 0;

	/** Whose blackjack turn, or 255. */
	UPROPERTY()
	uint8 Turn = 255;

	UPROPERTY()
	double EndsAt = 0.0;

	/** Roulette number, wheel wedge, or unused. */
	UPROPERTY()
	int32 Result = -1;

	UPROPERTY()
	uint8 Dealer0 = 0;
	UPROPERTY()
	uint8 Dealer1 = 0;
	UPROPERTY()
	uint8 Dealer2 = 0;
	UPROPERTY()
	uint8 Dealer3 = 0;
	UPROPERTY()
	uint8 Dealer4 = 0;

	UPROPERTY()
	uint8 bShowDealer = 0;

	/** Air hockey, first to 3. Seats 0-2 are gold, 3-5 are blue. */
	UPROPERTY()
	uint8 ScoreA = 0;
	UPROPERTY()
	uint8 ScoreB = 0;

	UPROPERTY()
	FString Line;
};

/** One slot machine on the casino's north wall (2026-11-21). A new Serial spins its reels on every screen. */
USTRUCT()
struct FDCCCasinoSlotNet
{
	GENERATED_BODY()

	UPROPERTY()
	FString Player;

	/** The symbol each reel stops on (0 cherries ... 5 sevens). */
	UPROPERTY()
	uint8 Reel0 = 0;
	UPROPERTY()
	uint8 Reel1 = 0;
	UPROPERTY()
	uint8 Reel2 = 0;

	UPROPERTY()
	uint8 Serial = 0;

	/** Tickets this pull pays, paid when the last reel stops. */
	UPROPERTY()
	int16 Pay = 0;
};

/** A Mother Lode scratchcard. The server deals it whole; scratching only uncovers it (2026-11-21). */
USTRUCT()
struct FDCCScratchNet
{
	GENERATED_BODY()

	UPROPERTY()
	FString Owner;

	/** Nine cells, each an index into the prize amounts. Three alike win that amount. */
	UPROPERTY()
	TArray<uint8> Cells;

	UPROPERTY()
	int16 Prize = 0;

	UPROPERTY()
	uint8 Serial = 0;

	UPROPERTY()
	uint8 bClaimed = 0;
};

/** Floor staff. Play, rules, and the bar list. They do not fight. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCCasinoStaff : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	ADCCCasinoStaff();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 0-4 a table, 5 the bar. */
	UPROPERTY(ReplicatedUsing = OnRep_Role)
	uint8 Post = 0;

	void SayRules();

protected:
	virtual void BuildScript() override;

private:
	UFUNCTION()
	void OnRep_Role();

	void ApplyLook();
	bool bDressed = false;
	/** 255 until the first dress. A late Post replication can change the outfit once. */
	uint8 AppliedPost = 255;
	TArray<TObjectPtr<UStaticMeshComponent>> Worn;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCCasinoCascade : public AActor
{
	GENERATED_BODY()

public:
	ADCCCasinoCascade();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static ADCCCasinoCascade* Get(UWorld* World);

	/** Carpet, ceiling, the east door, and the casino room. Called from the Safe Room build. */
	static void Dress(ADCCArcadeLobby* Lobby);

	static constexpr int32 TableCount = 5;
	static constexpr int32 SeatCount = 6;
	static constexpr int32 Stake = 5;

	/** Reels and scratchcards (2026-11-21). Four slot machines, then the card machine at index SlotCount. */
	static constexpr int32 SlotCount = 4;
	static constexpr int32 SlotStake = 2;
	static constexpr int32 CardPrice = 2;
	/** The machine (0 to SlotCount-1), the card machine (SlotCount), or -1 when nothing is in front of you. */
	int32 SlotNear(const FVector& At) const;
	void PullSlot(ADCCCharacterBase* Who, int32 Machine);
	void BuyCard(ADCCCharacterBase* Who);
	void ClaimCard(ADCCCharacterBase* Who);
	const FDCCScratchNet* CardFor(const APlayerState* State) const;
	static int32 CardAmount(int32 Index);

	/** The moving mark. Salt is the seat for skee, and 0 for the shared hockey puck. */
	static double Mark(double Now, int32 Salt);

	bool IsSeated(const ADCCCharacterBase* Who) const;
	bool IsSeatedState(const APlayerState* State) const;

	/** The game within arm's reach, or -1 out in the aisle. Press E there to sit. */
	int32 ClosestTable(const FVector& At) const;

	/** Inside the pit, including the doorway overlap. The lobby's own music stays outside. */
	bool Contains(const FVector& At) const;

	void Join(ADCCCharacterBase* Who, int32 Table);
	void Choose(ADCCCharacterBase* Who, int32 ChoiceId);
	void Leave(ADCCCharacterBase* Who);
	void Buy(ADCCCharacterBase* Who, int32 Drink);

	/** Local menu. The buttons are the same list the panel draws. */
	void LocalScroll(const ADCCCharacterBase* Who, int32 Delta);
	void LocalConfirm(ADCCCharacterBase* Who);
	int32 LocalIndex(const ADCCCharacterBase* Who) const;
	void FillOptions(const ADCCCharacterBase* Who, TArray<FString>& Labels, TArray<int32>& Ids) const;

	const TArray<FDCCCasinoTableNet>& GetTables() const { return Tables; }
	const TArray<FDCCCasinoSeatNet>& GetSeats() const { return Seats; }
	double EstimatedNow() const;

	static constexpr int32 DrinkCount = 3;
	static void DrinkInfo(int32 Index, FString& Label, int32& Price, FString& Spoken);

	struct FSpot
	{
		FVector At = FVector::ZeroVector;
		FRotator Face = FRotator::ZeroRotator;
	};

	struct FRoom
	{
		FVector Center = FVector::ZeroVector;
		float HalfX = 0.0f;
		float HalfY = 0.0f;
		FVector Tables[TableCount];
		FSpot Seats[TableCount][SeatCount];
		FSpot Staff[6];
		FVector Door = FVector::ZeroVector;
	};

	static void DenBounds(float& L, float& W);
	static FRoom RoomFor(const FVector& LobbyOrigin);
	/** Floor middle of a machine (Index SlotCount is the card machine). Players stand 190 south of it. */
	static FVector MachineAt(const FRoom& R, int32 Index);

private:
	UPROPERTY(Replicated)
	TArray<FDCCCasinoTableNet> Tables;

	UPROPERTY(Replicated)
	TArray<FDCCCasinoSeatNet> Seats;

	UPROPERTY(ReplicatedUsing = OnRep_Clock)
	double ServerClock = 0.0;

	UPROPERTY(Replicated)
	TArray<FDCCCasinoSlotNet> Slots;

	UPROPERTY(Replicated)
	TArray<FDCCScratchNet> Scratchcards;

	UFUNCTION()
	void OnRep_Clock();

	double ClockOffset = 0.0;

	TMap<TWeakObjectPtr<const ADCCCharacterBase>, int32> MenuIndex;
	TSet<TWeakObjectPtr<ADCCCharacterBase>> Held;

	FRoom Room() const { return RoomFor(GetActorLocation()); }
	void EnsureArrays();
	void SpawnStaff();
	void ServerTickGame();
	void BuildFloor();
	void AnimateFloor(float DeltaSeconds);
	ADCCArcadeLobby* FindLobby() const;

	bool bFloorBuilt = false;
	bool bVisualCaught = false;

	struct FSpin
	{
		float From = 0.0f;
		float To = 0.0f;
		float Age = 0.0f;
		float Duration = 4.2f;
		bool bLive = false;
	};

	FSpin WheelSpin;
	FSpin RouletteSpinAnim;
	float WheelAngle = 0.0f;
	float RouletteAngle = 0.0f;
	/** The slice the wheel is landing on (0-11; slice j pays like j % 6). */
	int32 WheelSlice = 0;
	int32 WheelSpins = 0;
	int32 LastPeg = MIN_int32;
	float FlapperKick = 0.0f;
	int32 LastBounce = -1;
	bool bWheelShowWas = false;
	int32 RouletteShownPocket = -1;

	/** Roulette and the wheel pay out when the spin stops, not when the server rolls (2026-11-21). */
	struct FPendingPay
	{
		TWeakObjectPtr<ADCCCharacterBase> Who;
		int32 Pay = 0;
		FString Text;
	};
	TArray<FPendingPay> PendingPays[TableCount];
	FString PendingLine[TableCount];
	double RevealAt[TableCount] = {};

	// Reels (server).
	double SlotBusy[SlotCount] = {};
	double SlotRevealAt[SlotCount] = {};
	FPendingPay SlotOwed[SlotCount];
	bool SpendFor(ADCCCharacterBase* Who, int32 Price);

	// Reels and levers (every screen).
	AActor* SlotSymbolActors[SlotCount][3][6] = {};
	AActor* SlotLevers[SlotCount][2] = {};
	AActor* SlotTops[SlotCount] = {};
	ATextRenderActor* SlotSigns[SlotCount] = {};
	float ReelAngle[SlotCount][3] = {};
	float ReelFrom[SlotCount][3] = {};
	float ReelTo[SlotCount][3] = {};
	float SlotAge[SlotCount] = {};
	bool SlotLive[SlotCount] = {};
	uint8 SlotSeen[SlotCount] = {};
	AActor* WheelLever[2] = {};
	float WheelLeverAge = 99.0f;
	float BallT[SeatCount] = {};
	int32 BallPts[SeatCount] = {};
	uint8 SeenSerial[SeatCount] = {};
	uint8 SeenPhase[TableCount] = {};
	int32 SeenResult[TableCount] = {};
	uint8 SeenScoreA = 0;
	uint8 SeenScoreB = 0;
	float MalletT[2] = {};
	uint8 SeenMallet[2] = {};
	float PuckHit = 0.0f;
	/** 1 sends the puck to the north goal, -1 to the south goal. */
	int32 PuckSide = 0;

	AActor* PrizeDisc = nullptr;
	/** Four bands per slice, narrow at the hub and wide at the rim. Twelve slices, each pay twice. */
	AActor* PrizeParts[12][4] = {};
	ATextRenderActor* WheelLabels[12] = {};
	AActor* WheelPegs[12] = {};
	AActor* WheelBulbs[24] = {};
	AActor* WheelFlapper = nullptr;
	AActor* RouletteDisc = nullptr;
	AActor* RoulettePockets[37] = {};
	AActor* RouletteFrets[37] = {};
	AActor* RouletteCross[2] = {};
	AActor* RouletteBallActor = nullptr;
	AActor* Puck = nullptr;
	AActor* Mallets[2] = {};
	AActor* SkeeBalls[SeatCount] = {};
	AActor* SkeeRings[SeatCount][5] = {};
	AActor* Cards[7][5] = {};
	AActor* Chase[10] = {};
	ATextRenderActor* Marquees[TableCount] = {};
	APointLight* PitLights[TableCount] = {};
	int32 FindSeat(const APlayerState* State, int32& Table, int32& Seat) const;
	bool Near(const FRoom& Layout, int32 Table, const FVector& At) const;
	void Sit(ADCCCharacterBase* Who, const FRoom& Layout, int32 Table, int32 Seat);
	void Release(ADCCCharacterBase* Who);
	int32 Occupied(int32 Table) const;
	void StartHand(int32 Table);
	void FinishShow(int32 Table);
	void ResolveHockey(int32 Table);
	void FinishBlackjack(int32 Table);
	void AdvanceBlackjack(int32 Table);
	int32 TakeStake(ADCCCharacterBase* Who, FDCCCasinoSeatNet& Seat);
	void Give(ADCCCharacterBase* Who, int32 Amount, const FString& Why);
	uint8 DrawRank() const;
	void SetSeatCard(FDCCCasinoSeatNet& Seat, int32 Index, uint8 Rank);
	void SetDealerCard(FDCCCasinoTableNet& Table, int32 Index, uint8 Rank);
};

/** One card rank, 1 (ace) through 13. 0 is an empty slot. */
int32 DCCCasinoHandValue(uint8 A, uint8 B, uint8 C, uint8 D, uint8 E);
FString DCCCasinoCardName(uint8 Rank);
FString DCCCasinoHandText(uint8 A, uint8 B, uint8 C, uint8 D, uint8 E, bool bHideHole);
bool DCCCasinoRedPocket(int32 Number);
