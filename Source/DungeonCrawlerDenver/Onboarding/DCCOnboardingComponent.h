#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Onboarding/DCCOnboardingTypes.h"
#include "DCCOnboardingComponent.generated.h"

class UAudioComponent;
class USkeletalMesh;
class UDataTable;

/**
 * Who this crawler is (CJ, Scott...), how far through the first ten minutes
 * they have got, and which parts of the HUD they have been issued.
 *
 * Lives on every ADCCCharacterBase, enemies included, because that is one
 * line in one constructor against a second class hierarchy for players. On
 * anything that is not a player it never leaves its defaults and costs
 * nothing.
 *
 * WHEN THERE IS NO ONBOARDING IN THE LEVEL, NOTHING IS LOCKED. Gating only
 * switches on when an ADCCOnboardingSettings actor is placed. Every existing
 * test level, sandbox and PIE habit keeps the full HUD, so this cannot
 * quietly take the health bar away from a session that never asked for it.
 *
 * Server decides stages and unlocks. The System window queue is client-side
 * presentation only: the server sends a finished payload by RPC, and the
 * client queues, voices and dismisses it on its own. Nothing about a pop-up
 * is worth a round trip.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCOnboardingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCOnboardingComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Identity ------------------------------------------------------------

	/**
	 * Which crawler this is: "CJ", "Scott", "Marcel", "Vaughn", "Kyle", "Zoe".
	 * Set by ADCCGameMode from the ?Crawler= join option, or from whichever
	 * bedroom PlayerStart the player was given. None means "unidentified",
	 * and an unidentified crawler may pick up anyone's personal item, so a
	 * solo PIE test is never stuck.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CrawlerId, EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	FName CrawlerId;

	/**
	 * Each crawler's own body. When CrawlerId is set (or arrives on a client) the character swaps to the matching
	 * mesh, so CJ looks like CJ instead of everyone wearing the player Blueprint's default. Soft, so nothing
	 * loads until it's worn; editable, so a new model needs no code. The animation slots stay as they are: the
	 * shared clips already play on every crawler mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onboarding|Looks")
	TMap<FName, TSoftObjectPtr<USkeletalMesh>> CrawlerMeshes;

	/** Put this crawler's body on the owning character. Safe to call repeatedly, on any machine. */
	void ApplyCrawlerLook();

	// --- Stage and unlocks ------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Onboarding")
	EDCCOnboardingStage GetStage() const { return Stage; }

	UFUNCTION(BlueprintPure, Category = "Onboarding")
	bool HasReached(EDCCOnboardingStage Query) const { return Stage >= Query; }

	UFUNCTION(BlueprintPure, Category = "Onboarding")
	int32 GetUnlockedFeatures() const { return UnlockedFeatures; }

	UFUNCTION(BlueprintPure, Category = "Onboarding")
	bool IsFeatureUnlocked(EDCCHudFeature Feature) const { return DCCHud::Has(UnlockedFeatures, Feature); }

	/** Forward only. Asking for an earlier stage is ignored, so triggers can be sloppy about order. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Onboarding")
	void AdvanceTo(EDCCOnboardingStage NewStage);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Onboarding")
	void UnlockFeatures(int32 FeatureMask);

	/** Server: look a row up in DT_DCCSystemWindows, apply its unlocks, send it to this crawler's screen. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Onboarding")
	bool ShowWindowRow(FName RowName);

	/** Server: send a window that is not in any table (rolled encounter intros, mostly). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Onboarding")
	void ShowWindow(const FDCCSystemWindowPayload& Window);

	/**
	 * Server: advance a stage and show a window after a delay. Used for the
	 * World Dungeon announcement, which should land a few seconds after the
	 * personal item rather than stacked on top of it.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Onboarding")
	void AdvanceLater(EDCCOnboardingStage NewStage, FName WindowRow, float DelaySeconds);

	/** Server: a one-line toast on this crawler's screen only. */
	void NotifyOwner(const FText& Message);

	/** Server, from ADCCGameMode: carry identity and progress onto a respawned pawn. */
	void ApplyRestoredState(FName InCrawlerId, bool bHasProgress, EDCCOnboardingStage InStage, int32 InFeatures);

	// --- Client-side window queue --------------------------------------------

	/**
	 * The window currently on screen, if any. Non-const because the first
	 * frame a window becomes active is also when its voice line starts.
	 */
	const FDCCSystemWindowPayload* GetActiveWindow();

	/** Seconds since the active window opened. Drives the unroll animation. */
	float GetActiveWindowAge() const;

	int32 GetQueuedWindowCount() const { return PendingWindows.Num(); }

	/** True when something was dismissed, so the E press can stop there. */
	bool DismissActiveWindow();

	/** Client: show a row once per session (the first time a menu page opens). */
	void ShowLocalTutorialOnce(FName RowName);

	/** Where the rows live. Soft, so a table imported mid-session is picked up without a restart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	TSoftObjectPtr<UDataTable> SystemWindowTable;

	/** Below this, E is ignored, so mashing through a fight can't skip a window unread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "0.0"))
	float MinimumWindowSeconds = 0.75f;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Stage, BlueprintReadOnly, Category = "Onboarding")
	EDCCOnboardingStage Stage = EDCCOnboardingStage::Complete;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Onboarding")
	int32 UnlockedFeatures = DCCHud::AllFeatures;

	UFUNCTION()
	void OnRep_Stage();

	UFUNCTION()
	void OnRep_CrawlerId();

	UFUNCTION(Client, Reliable)
	void Client_ShowSystemWindow(const FDCCSystemWindowPayload& Window);

	UFUNCTION(Client, Reliable)
	void Client_Notify(const FText& Message);

	void EnqueueLocal(const FDCCSystemWindowPayload& Window);
	bool BuildPayload(FName RowName, FDCCSystemWindowPayload& OutPayload, int32& OutUnlocks) const;

private:
	TArray<FDCCSystemWindowPayload> PendingWindows;
	float ActiveSince = -1.0f;
	TWeakObjectPtr<UAudioComponent> ActiveVoice;
	TSet<FName> ShownLocalTutorials;
};
