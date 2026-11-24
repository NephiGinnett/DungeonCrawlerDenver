#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Arcade/DCCCosmetics.h"
#include "DCCWalletComponent.generated.h"

class APlayerState;
class UDCCEquipmentComponent;
class UStaticMeshComponent;

/** One worn cosmetic, replicated so every machine can draw it. */
USTRUCT()
struct FDCCWornCosmetic
{
	GENERATED_BODY()

	UPROPERTY()
	FName Slot;

	UPROPERTY()
	FName Id;
};

/**
 * A crawler's arcade wallet (2026-11-13): tickets, the cosmetics they own, and
 * what they are wearing. On every ADCCCharacterBase; only player-controlled
 * ones ever load anything.
 *
 * The truth lives in the host's arcade profile (DCCProfile, keyed by player
 * name). This component mirrors it for the HUD and draws the worn cosmetics on
 * every machine. Buying and wearing are requests (Server_*), checked on the host
 * against the profile, so a client can never hand itself tickets.
 *
 * Also carries the end-of-round vote (Server_ArcadeVote), because it is a
 * player-owned component the server will accept an RPC from in every game.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCWalletComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(Replicated)
	int32 Tickets = 0;

	UPROPERTY(Replicated)
	TArray<FName> Owned;

	UPROPERTY(ReplicatedUsing = OnRep_Worn)
	TArray<FDCCWornCosmetic> Worn;

	// --- Fourteener kit (mirrored from ADCCArcadeClimb for the HUD) ---------
	UPROPERTY(Replicated)
	TArray<FName> ClimbTools;

	/** "Chalk Bag 45 s   Crampons   Hard Hat" */
	UPROPERTY(Replicated)
	FString ClimbBoosts;

	UPROPERTY(Replicated)
	TArray<FName> CarriedArtifacts;

	/** Fourteener hunger, 0-100 (100 = fed). -1 outside Fourteener. Caps max stamina. */
	UPROPERTY(Replicated)
	float ClimbHunger = -1.0f;

	// --- Trophy vault (from the profile) ----------------------------------
	UPROPERTY(Replicated)
	TArray<FName> TrophyIds;

	UPROPERTY(Replicated)
	TArray<int32> TrophyCounts;

	int32 TrophyCount(FName Id) const { const int32 i = TrophyIds.IndexOfByKey(Id); return i == INDEX_NONE ? 0 : TrophyCounts[i]; }

	// --- The Chop Shop (2026-11-14): scrap and truck upgrade levels, from the profile ------
	UPROPERTY(Replicated)
	int32 Scrap = 0;

	/** One level per DCCChopShop::All() entry, in that order. */
	UPROPERTY(Replicated)
	TArray<uint8> ChopLevels;

	int32 ChopLevel(FName UpgradeId) const;

	/** Buy the next level of a truck upgrade with scrap. */
	UFUNCTION(Server, Reliable)
	void Server_BuyChop(FName UpgradeId);

	/** Profile sync (DCCProfile::MergeFromClient): a visiting crawler's copy of their profile, and the copy they last got. */
	UFUNCTION(Server, Reliable)
	void Server_SyncProfile(const TArray<uint8>& Current, const TArray<uint8>& Base);
	/** The host's merged copy, for the visitor's own PC. */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveProfile(const TArray<uint8>& Merged);

	/** Fourteener: buy a climbing item at Fort Tiner's Trading Post with tickets. It goes in this round's kit. */
	UFUNCTION(Server, Reliable)
	void Server_BuySupply(FName ItemId);

	/** Spray tags (DCCSpray): paint this crawler's picture where they are looking. The picture travels with the
	 *  request because it lives on the painter's own PC. From the truck, ADCCRover::Server_Spray does the same. */
	UFUNCTION(Server, Reliable)
	void Server_Spray(FVector At, FVector Normal, FVector ViewDir, const TArray<uint8>& RGB);

	/** Dead Air: send the cage up early (everyone still standing must agree). */
	UFUNCTION(Server, Reliable)
	void Server_CageVote(bool bUp);

	/** PRIMETIME's couch (2026-11-23): 0 Charm, 1 Banter, 2 Defy. Only the guest in the hot seat is heard. */
	UFUNCTION(Server, Reliable)
	void Server_PrimetimeAnswer(uint8 Answer);

	/** Starter kits (Session/DCCStarterKits): the host offers the choice once, the player picks one. */
	UFUNCTION(Client, Reliable)
	void Client_OfferStarterKit();
	UFUNCTION(Server, Reliable)
	void Server_ChooseStarterKit(int32 Index);

	/** Server: pay a player scrap (the driving games), keep the profile and wallet in step, and tell them. */
	static void AwardScrap(APlayerState* PlayerState, int32 Amount, const FString& Why);

	// --- Mine Outfitters (2026-11-15): Dead Air's credits ------------------------------------
	UPROPERTY(Replicated)
	int32 MineCredits = 0;

	static void AwardCredits(APlayerState* PlayerState, int32 Amount, const FString& Why);

	/** Dead Air: throw a lightstick from Eye along Dir. The lightstick is a belt slot. The server checks you have one left. */
	UFUNCTION(Server, Reliable)
	void Server_ThrowLightstick(FVector Eye, FVector Dir);

	// --- Crawler password (2026-11-14) ----------------------------------------
	/** Change this crawler's password. Hashes only (DCCProfile::HashPassword); the host checks the old one. */
	UFUNCTION(Server, Reliable)
	void Server_ChangePassword(const FString& OldHash, const FString& NewHash);

	/** What the last password change said, and whether it worked. Local. */
	FString PasswordMessage;
	bool bPasswordChanged = false;

	/** The belt grappling hook (Fourteener). */
	UFUNCTION(Server, Reliable)
	void Server_Grapple();

	/** A pull at a summoning tube's podium in the Den (2026-11-17, DCCSummons): 100 tickets, one friend comes down. */
	UFUNCTION(Server, Reliable)
	void Server_Summon(uint8 Tube);
	/** Server: the pull itself. bFree skips the tickets and the podium check (dev tests, DCC.SummonFree). */
	void DoSummon(uint8 Tube, bool bFree);
	/** Who came down, for this player's reveal banner. */
	UFUNCTION(Client, Reliable)
	void Client_SummonResult(FName Friend, uint8 Tier, uint8 Variant, FName Weapon);

	/**
	 * The ten-pull (2026-11-22, Zoe: "Like a real gacha where a batch of 10 guarantees a rare or higher and you can just
	 * select which ones you keep and the rest get evaporated"). Ten friends for the price of nine, at least one Rare or
	 * better. They wait in the tube while the player picks up to their free ally slots. The rest evaporate.
	 */
	UFUNCTION(Server, Reliable)
	void Server_SummonTen(uint8 Tube);
	/** Server: the ten-pull itself. bFree skips the tickets and the podium (DCC.SummonTenFree). */
	void DoSummonTen(uint8 Tube, bool bFree);
	/** Which of the ten to keep, by index. Everything else evaporates. */
	UFUNCTION(Server, Reliable)
	void Server_KeepSummons(const TArray<uint8>& Keep);
	/** A title under the name on the nameplate, earned off the arcade (OPEN MIC from OPEN MIC WEEK). Everyone sees it. */
	UPROPERTY(Replicated)
	FString NameTitle;

	// --- The Hot Mic (2026-11-23): Epic, only for the chosen voice of Hiss Cranson (Voice Drop). A belt item: press it to
	// go live for 8 s; the next spell in that time hits 40% harder over a 30% wider area and plays its holder's clip
	// (SourceArt/Voice/NPC/_Clips/<name>.wav, from Voice Drop) for everyone. 45 s between uses.
	static const FName HotMicId;
	/** Server: send this player their profile again (an item or title just arrived). */
	void RefreshFromProfile();
	UFUNCTION(Server, Reliable)
	void Server_HotMic();
	/** Server clock. Live until then (the next spell is amplified), ready again at HotMicReadyAt. */
	UPROPERTY(Replicated)
	double HotMicLiveUntil = 0.0;
	UPROPERTY(Replicated)
	double HotMicReadyAt = 0.0;
	/** Server: true (once) if the Hot Mic is live now; it is spent by asking. */
	bool ConsumeHotMic();

	/** Fame (Arcade/DCCAudience, 2026-11-23): this player's progress on the audience track, for their own HUD. */
	UPROPERTY(Replicated)
	int32 Fame = 0;
	UPROPERTY(Replicated)
	int32 FameLevel = 0;
	/** A Fame level was reached: the toast, with the box it paid. */
	UFUNCTION(Client, Reliable)
	void Client_FameLevel(int32 Level, uint8 Tier);

	/** The ten, for this player's pick screen, and how many they can keep. */
	UFUNCTION(Client, Reliable)
	void Client_SummonTen(const TArray<FName>& Friends, const TArray<uint8>& Tiers, const TArray<uint8>& Variants, const TArray<FName>& Weapons, int32 Slots);
	/** Server: the ten waiting in the tube. */
	TArray<FName> TenFriends, TenWeapons;
	TArray<uint8> TenTiers, TenVariants;
	int32 TenTube = -1;
	/** The server threw your hook: it is back in Seconds. The HUD counts down from here, on this PC's own clock. */
	UFUNCTION(Client, Reliable)
	void Client_GrappleThrown(float Seconds);
	/** This PC's clock: when the hook is ready again. */
	float GrappleReadyLocal = 0.0f;

	/** Sell one artifact at the Prize Counter (those with a ticket value). */
	UFUNCTION(Server, Reliable)
	void Server_SellTrophy(FName ArtifactId);

	/** Server: re-read the trophy vault into TrophyIds/TrophyCounts. */
	void RefreshTrophies();

	/** Bounties (DCCBounties), one bit each in DCCBounties::All() order: finished, and paid. Yours only. */
	UPROPERTY(Replicated)
	int32 BountiesDone = 0;
	UPROPERTY(Replicated)
	int32 BountiesPaid = 0;
	bool BountyDone(FName Id) const;
	bool BountyPaid(FName Id) const;
	/** Server: re-read the bounties; tells the player about any that just finished. */
	void RefreshBounties();
	/** Pay a finished bounty (the BOUNTIES tab's CLAIM). */
	UFUNCTION(Server, Reliable)
	void Server_ClaimBounty(FName BountyId);

	bool Owns(FName Id) const { return Owned.Contains(Id); }
	FName WornIn(FName Slot) const;

	UFUNCTION(Server, Reliable)
	void Server_Buy(FName CosmeticId);

	UFUNCTION(Server, Reliable)
	void Server_Wear(FName CosmeticId);

	UFUNCTION(Server, Reliable)
	void Server_TakeOff(FName Slot);

	/** End-of-round choice in an arcade game: true to play again, false for the Den. */
	UFUNCTION(Server, Reliable)
	void Server_ArcadeVote(bool bPlayAgain);

	/** Server: re-read tickets, cosmetics and outfit from the profile (spawn, crawler switch). */
	void LoadFromProfile();

	/**
	 * Server: pay a player tickets, keep the profile and their live wallet in step,
	 * and tell them why ("+120 tickets: summit!").
	 */
	static void AwardTickets(APlayerState* PlayerState, int32 Amount, const FString& Why);

	/** The last thing the shop said to this player (bought, too poor...). Local. */
	FString ShopMessage;

	/** Draws Worn as cosmetics on the owner. Also used for NPCs, which wear things without a wallet. */
	void ApplyOutfit(const TArray<FName>& CosmeticIds);

private:
	UFUNCTION()
	void OnRep_Worn();

	/** One thing on your head (2026-11-15): Head gear going on takes the cosmetic hat off. Server. */
	UFUNCTION()
	void OnGearChanged(UDCCEquipmentComponent* Changed);
	/** The other way round: a cosmetic hat going on puts Head gear in the bag. Server. */
	void StowHeadGear();

	UFUNCTION(Client, Reliable)
	void Client_Message(const FString& Message, bool bNotice);

	UFUNCTION(Client, Reliable)
	void Client_PasswordResult(bool bChanged, const FString& NewHash);

public:
	/** Dead Air: switch your helmet lamp on or off (L). Off walks you through firedamp safely, in the dark. */
	UFUNCTION(Server, Reliable)
	void Server_SetMineLamp(bool bOn);

	/** Server: a notice on the owner's screen. */
	void Tell(const FString& Message) { Client_Message(Message, true); }

private:

	FString ProfileKey() const;
	void PushToClients(const FString& Key);
	void RebuildVisuals();
	void UpdateVisuals();

	struct FLivePart
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		DCCCosmetics::FPart Part;
		DCCCosmetics::EAnchor Anchor = DCCCosmetics::EAnchor::HeadTop;
		bool bSpins = false;
		/** "Cosmetic.<Id>": the gear bench's grip key for this item (Bench/DCCBenchData.h). */
		FName BenchKey;
	};
	TArray<FLivePart> LiveParts;

	// The anchor bones' reference-pose rotations (component space), so a hat can
	// turn by how far the head has turned from rest. Re-read when the body changes.
	TWeakObjectPtr<const class USkeletalMesh> RefAsset;
	FName HeadBone, BackBone;
	FQuat RefHead = FQuat::Identity, RefBack = FQuat::Identity;

	FTimerHandle LoadRetry;
	FTimerHandle SyncTimer;
	int32 SyncAttempts = 0;
	bool bProfileSent = false;
	int32 LoadAttempts = 0;
	FString LoadedKey;
private:
	FTimerHandle BountyTimer;
	bool bBountiesRead = false;
};
