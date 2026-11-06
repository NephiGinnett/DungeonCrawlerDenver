#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Onboarding/DCCOnboardingTypes.h"
#include "DCCOnboardingActors.generated.h"

class ADCCCharacterBase;
class ADCCEnemyCharacter;
class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UArrowComponent;
class UDataTable;
class UMaterialInterface;
class UTextRenderComponent;

// ---------------------------------------------------------------------------
// The pieces of the first ten minutes, all placed by hand in the level:
//
//   ADCCOnboardingSettings  - one per level. Its presence is what turns
//                             onboarding on (see UDCCOnboardingComponent).
//   ADCCPersonalItem        - the one thing in each bedroom that belongs to
//                             that crawler. Walking into it picks it up.
//   ADCCSkyBeacon           - the giant arrow over the dungeon entrance.
//                             Each player only sees it once the System has
//                             made its announcement to them.
//   ADCCDungeonEntrance     - the doorway. Sends each crawler to their own
//                             ADCCSoloTrial, so everyone fights alone.
//   ADCCSoloTrial           - one solo room. Rolls a random encounter, then
//                             opens two exits: the safe room, or onward.
//   ADCCOnboardingTrigger   - a general-purpose volume: "when a crawler at
//                             stage X walks in, show these windows, unlock
//                             these HUD parts, move them to stage Y". The
//                             safe room's whole tutorial is a few of these.
//
// All of it is server-authoritative. Clients only ever see the results:
// replicated stages, and System windows sent to them by RPC.
// ---------------------------------------------------------------------------

namespace DCCOnboarding
{
	/** Shared by every actor here: teleport a crawler and turn their camera to match. */
	void TeleportCrawler(ADCCCharacterBase* Crawler, const FVector& Location, const FRotator& Rotation);
}

UCLASS(meta = (DisplayName = "DCC Onboarding Settings"))
class DUNGEONCRAWLERDENVER_API ADCCOnboardingSettings : public AActor
{
	GENERATED_BODY()

public:
	ADCCOnboardingSettings();

	/** Where every crawler starts. Waking for the real thing; later stages for testing a single beat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onboarding")
	EDCCOnboardingStage StartingStage = EDCCOnboardingStage::Waking;

	/** HUD issued at the start. 0 is the design: wake up with nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Onboarding", meta = (Bitmask, BitmaskEnum = "/Script/DungeonCrawlerDenver.EDCCHudFeature"))
	int32 StartingFeatures = 0;
};

UCLASS(meta = (DisplayName = "DCC Personal Item"))
class DUNGEONCRAWLERDENVER_API ADCCPersonalItem : public AActor
{
	GENERATED_BODY()

public:
	ADCCPersonalItem();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Personal Item")
	TObjectPtr<USphereComponent> PickupSphere;

	/** Set this to the item's mesh in the Details panel (a board game box, a lacrosse stick). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Personal Item")
	TObjectPtr<UStaticMeshComponent> ItemMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Personal Item")
	TObjectPtr<UPointLightComponent> Glow;

	/** Whose it is. Must match the crawler's CrawlerId ("CJ", "Scott"...). None means anyone's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personal Item")
	FName OwnerCrawlerId;

	/** DT_DCCEquipment row equipped on pickup. Lacrosse stick is a weapon, the board game a trinket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personal Item")
	FName EquipmentItemId;

	/** System window shown on pickup. Empty uses "PersonalItem_<OwnerCrawlerId>", then "PersonalItem". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personal Item")
	FName PickupWindowRow;

	/** The planet-wide announcement, this many seconds after the pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personal Item")
	FName AnnouncementWindowRow = TEXT("WorldDungeonAnnouncement");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personal Item", meta = (ClampMin = "0.0"))
	float AnnouncementDelaySeconds = 4.0f;

	/** A gentle bob and spin, so it reads as "the thing to pick up" with no HUD to point at it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personal Item")
	float BobHeight = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personal Item")
	float SpinDegreesPerSecond = 25.0f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(ReplicatedUsing = OnRep_Taken)
	bool bTaken = false;

	UFUNCTION()
	void OnRep_Taken();

private:
	FVector MeshRestLocation = FVector::ZeroVector;
	float RefusalCooldownUntil = 0.0f;
};

UCLASS(meta = (DisplayName = "DCC Sky Beacon"))
class DUNGEONCRAWLERDENVER_API ADCCSkyBeacon : public AActor
{
	GENERATED_BODY()

public:
	ADCCSkyBeacon();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beacon")
	TObjectPtr<USceneComponent> ArrowRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> Shaft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beacon")
	TObjectPtr<UPointLightComponent> Light;

	/** The local player sees the arrow once they reach this stage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	EDCCOnboardingStage RevealAtStage = EDCCOnboardingStage::Announced;

	/** ...and stops seeing it once they are through the door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	EDCCOnboardingStage HideAtStage = EDCCOnboardingStage::InDungeon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	FLinearColor Colour = FLinearColor(1.0f, 0.78f, 0.1f);

	/** Optional: a proper emissive material. Without one the arrow uses the engine's flat shape material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	TObjectPtr<UMaterialInterface> ArrowMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float BobHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float BobSeconds = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float SpinDegreesPerSecond = 30.0f;

	/** Fade in over this many seconds when revealed, instead of popping into existence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon", meta = (ClampMin = "0.0"))
	float RevealSeconds = 2.0f;

protected:
	virtual void BeginPlay() override;

private:
	bool ShouldShowForLocalPlayer() const;
	float RevealAlpha = 0.0f;
};

UCLASS(meta = (DisplayName = "DCC Onboarding Trigger"))
class DUNGEONCRAWLERDENVER_API ADCCOnboardingTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADCCOnboardingTrigger();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onboarding")
	TObjectPtr<UBoxComponent> Volume;

	/** Only fires for crawlers at or past this stage... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	EDCCOnboardingStage MinimumStage = EDCCOnboardingStage::Waking;

	/** ...and before this one. Complete means "no upper bound". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	EDCCOnboardingStage BeforeStage = EDCCOnboardingStage::Complete;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	bool bAdvanceStage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (EditCondition = "bAdvanceStage"))
	EDCCOnboardingStage AdvanceToStage = EDCCOnboardingStage::SafeRoom;

	/** Shown in order. Each row's own UnlockFeatures applies as it is sent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	TArray<FName> WindowRows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (Bitmask, BitmaskEnum = "/Script/DungeonCrawlerDenver.EDCCHudFeature"))
	int32 UnlockFeatures = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	bool bOncePerCrawler = true;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	TSet<TWeakObjectPtr<ADCCCharacterBase>> AlreadyFired;
};

UCLASS(meta = (DisplayName = "DCC Solo Trial"))
class DUNGEONCRAWLERDENVER_API ADCCSoloTrial : public AActor
{
	GENERATED_BODY()

public:
	ADCCSoloTrial();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<USceneComponent> Root;

	/** Where the crawler lands. Its arrow is the way they face. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<UArrowComponent> ArrivalPoint;

	/** Enemies spawn in a ring around this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<USceneComponent> SpawnCentre;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<UBoxComponent> SafeRoomExit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<UStaticMeshComponent> SafeRoomExitVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<UTextRenderComponent> SafeRoomExitLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<UBoxComponent> ContinueExit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<UStaticMeshComponent> ContinueExitVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trial")
	TObjectPtr<UTextRenderComponent> ContinueExitLabel;

	/** DT_DCCRandomEncounters. Soft so a table imported mid-session works without a restart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial")
	TSoftObjectPtr<UDataTable> EncounterTable;

	/** Testing: always roll this row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial")
	FName ForcedEncounterRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial", meta = (ClampMin = "0.0"))
	float SpawnRadius = 650.0f;

	/** Breathing room between landing and the enemies arriving, so the welcome window can be read. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial", meta = (ClampMin = "0.0"))
	float SpawnDelaySeconds = 3.0f;

	/** The shared safe room. Anything with a transform: a TargetPoint, the room's DCCRoom actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial")
	TObjectPtr<AActor> SafeRoomDestination;

	/** "Keep going alone." Leave empty and the onward exit never opens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial")
	TObjectPtr<AActor> ContinueDestination;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial")
	FName ClearedWindowRow = TEXT("FirstFightWon");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial")
	FName ContinueWindowRow = TEXT("ContinueAlone");

	UFUNCTION(BlueprintPure, Category = "Trial")
	bool IsFree() const;

	UFUNCTION(BlueprintPure, Category = "Trial")
	ADCCCharacterBase* GetOccupant() const { return Occupant; }

	/** Server: take this crawler in, roll their fight. */
	void BeginFor(ADCCCharacterBase* Crawler);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleEnemyDied(ADCCCharacterBase* Character);

	UFUNCTION()
	void HandleSafeRoomExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleContinueExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(ReplicatedUsing = OnRep_ExitsOpen)
	bool bExitsOpen = false;

	UFUNCTION()
	void OnRep_ExitsOpen();

private:
	void SpawnRolledEncounter();
	bool RollEncounter(FDCCRandomEncounterRow& OutRow) const;
	void OpenExits();
	void Release();
	void LeaveTo(ADCCCharacterBase* Crawler, AActor* Destination);

	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> Occupant;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCEnemyCharacter>> LivingEnemies;

	FTimerHandle SpawnTimer;
};

UCLASS(meta = (DisplayName = "DCC Dungeon Entrance"))
class DUNGEONCRAWLERDENVER_API ADCCDungeonEntrance : public AActor
{
	GENERATED_BODY()

public:
	ADCCDungeonEntrance();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Entrance")
	TObjectPtr<UBoxComponent> Doorway;

	/** One per player, ideally. Rooms far enough apart that enemies in one never aggro on the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance")
	TArray<TObjectPtr<ADCCSoloTrial>> Trials;

	/** If every trial is busy (a sixth player), they go straight here instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance")
	TObjectPtr<AActor> OverflowDestination;

	/** The isekai explainer, shown on arrival. Several rows read as several pages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance")
	TArray<FName> EntryWindowRows = { TEXT("WelcomeCrawler"), TEXT("TheRules") };

	/** Too early: the dungeon is not open to you yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance")
	EDCCOnboardingStage MinimumStage = EDCCOnboardingStage::Announced;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	float RefusalCooldownUntil = 0.0f;
};
