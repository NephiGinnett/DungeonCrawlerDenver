#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Bench/DCCBenchData.h"
#include "DCCGearBenchWidget.generated.h"

class ACameraActor;
class ADCCCharacterBase;
class ADCCHUD;
class UAnimSequence;
template <typename OptionType> class SComboBox;

/**
 * The gear bench (F3): line up how a crawler holds things, and save it.
 *
 * Borrowed in spirit from The Familiar's pose bench: an orbit camera on the
 * model, play or step through any animation, nudge numbers until it looks
 * right, save, and the game uses the numbers from then on (Bench/DCCBenchData).
 *
 *   Body       swap to any crawler's body (host or solo).
 *   Animation  any clip on that body's skeleton: play, pause, scrub, step, speed.
 *   Gear       equip any item from DT_DCCEquipment (host or solo).
 *   Nudge      move / rotate / scale one of:
 *                - the item's grip, on every body
 *                - the item's grip, on this body only
 *                - a socket on this body (moves everything held there)
 *                - a bone on this body (on top of every animation)
 *
 * Right-drag orbits, middle-drag raises and lowers the view, the wheel zooms.
 * The HUD ticks it (TickBench) so the camera follows without relying on
 * native UUserWidget ticking.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCGearBenchWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<ADCCHUD> Hud;

	/** Takes over the camera and the crawler's animation. */
	void Begin(ADCCCharacterBase* InCrawler);
	/** Gives them back. */
	void End();
	void TickBench(float DeltaSeconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class ETarget : uint8 { GripAll, GripBody, Socket, Bone };

	USkeletalMeshComponent* GetBodyComponent() const;
	FString GetBodyPath() const;

	void RefreshLists();
	void RefreshAnimOptions();
	void SelectBody(FName CrawlerId);
	void SelectAnim(int32 Index);
	void SelectItem(FName ItemId);
	void SetTarget(ETarget NewTarget);
	void LoadEdit();
	void ApplyEdit();
	void ResetEdit();
	void SaveAll();
	/** What Save writes: the entry being edited right now (kind, mesh path, name). */
	void CurrentEntry(const TCHAR*& Kind, FString& MeshPath, FName& Name) const;
	/** Entries changed but not saved ("kind|mesh|name"), so Quit can warn about the rest. */
	TSet<FString> UnsavedEntries;
	FString EntryKey() const;

	/** F3, Esc or the Close button. As the standalone tool this quits, after a warning if anything is unsaved. */
	void RequestClose();
	bool IsTool() const;
	bool bDirty = false;
	bool bQuitArmed = false;
	void TogglePlay();
	void Step(float Seconds);
	float GetScrub() const;
	void SetScrub(float Fraction);
	FText GetTargetLabel() const;

	TWeakObjectPtr<ADCCCharacterBase> Crawler;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentAnim;

	float Yaw = 0.0f;
	float Pitch = -8.0f;
	float Distance = 260.0f;
	float FocusHeight = 40.0f;
	bool bOrbiting = false;
	bool bPanning = false;

	bool bPlaying = true;
	float PlayRate = 1.0f;

	ETarget Target = ETarget::GripAll;
	FName SelectedItem;
	FName SelectedSocket = TEXT("hand_rSocket");
	FName SelectedBone;
	DCCBench::FNudge Edit;
	FString Status;

	// Combo sources (Slate holds pointers into these).
	TArray<TSharedPtr<FString>> BodyOptions;
	TArray<TSharedPtr<FString>> AnimOptions;
	TArray<FSoftObjectPath> AnimPaths;
	TArray<FSoftObjectPath> AllAnimPaths;
	TArray<TSharedPtr<FString>> ItemOptions;
	TArray<FName> ItemIds;
	TArray<TSharedPtr<FString>> SocketOptions;
	TArray<TSharedPtr<FString>> BoneOptions;
	FString AnimFilter;

	// Gear list (2026-11-13): a category ("All", "Main Hand"... "Hats & cosmetics"),
	// a name search, sorted A-Z. Arcade cosmetics are listed as "Cosmetic.<Id>".
	FString ItemCategory = TEXT("All");
	FString ItemFilter;
	void RebuildItemOptions();
	static bool IsCosmeticKey(FName Key) { return Key.ToString().StartsWith(TEXT("Cosmetic.")); }
	/** "ON THIS BODY": everything the body has on (gear and cosmetics), with Nudge and Remove. Click the body to open it. */
	bool bShowWorn = false;
	TSharedPtr<class SVerticalBox> WornBox;
	void RebuildWornList();
	void RemoveWorn(FName Key);
	/** A left click that did not drag: if it lands on the body, open the list. */
	void TryClickBody();
	float DragDistance = 0.0f;

	/** Cosmetics tried on at the bench, one per slot. */
	TArray<FName> BenchCosmetics;

	TSharedPtr<SComboBox<TSharedPtr<FString>>> BodyCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> AnimCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ItemCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> BoneCombo;
};
