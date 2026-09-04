#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dialogue/DCCDialogueTypes.h"
#include "DCCDialogueNPC.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class USphereComponent;
class UTextRenderComponent;
class UAnimationAsset;
class UDataTable;
class ADCCCharacterBase;

/**
 * Someone who talks and does not fight.
 *
 * Deliberately NOT derived from ADCCCharacterBase. That class assumes combat all the
 * way down - it carries stats, skills, favor, carry, inventory and achievement
 * components, it is hunted by IsHostileTo, and it is counted as a room occupant that
 * can arm an encounter. A bartender inheriting all of that would be a combatant we
 * had to keep remembering to exclude, in every system, forever.
 *
 * A plain AActor with a mesh and a trigger is a smaller, safer thing, and nothing
 * that fights can see it.
 *
 * The speech bubble is a UTextRenderComponent rather than UMG, matching the
 * nameplates and quest markers: no asset to author, works the moment it compiles,
 * and it is legible in world space where the conversation actually is.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCDialogueNPC : public AActor
{
	GENERATED_BODY()

public:
	ADCCDialogueNPC();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** Where this NPC's lines come from. Rows are matched on SpeakerId. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<UDataTable> DialogueTable;

	/** Matches FDCCDialogueLine::SpeakerId. Nothing is said without one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName SpeakerId;

	/** Shown above the head when nobody is listening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DisplayName;

	/** Fallback duration for lines that do not set their own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (ClampMin = "0.5"))
	float DefaultLineSeconds = 5.0f;

	/** Gap between lines, so a monologue does not read as a wall of text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (ClampMin = "0.0"))
	float PauseBetweenLinesSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (ClampMin = "50.0"))
	float TalkRadius = 400.0f;

	/**
	 * Whether the NPC starts over once it has said everything.
	 *
	 * On by default (changed 2026-08-30 - see DISPATCH.md). The line-index state
	 * this drives is per-actor, not per-player, and dialogue also auto-advances
	 * ambiently just from a player standing nearby (HandleTalkOverlapBegin/Tick) -
	 * with five players and no cross-session save data, a finite non-looping
	 * script gets walked to its end within minutes of play just from people
	 * wandering past, and then goes permanently silent with zero feedback:
	 * TryPlayerInteract's "if (HasMoreToSay() || bLoopWhenExhausted)" guard means
	 * E stops doing anything at all, which reads exactly like a broken E key
	 * rather than "this NPC has nothing left to say." A one-time story beat that
	 * really should go quiet for good can still set this false per-instance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bLoopWhenExhausted = true;

	/**
	 * Quests this NPC can hand out - offered through ADCCCharacterBase's own
	 * Talk/Shop/Quests/Leave menu (EDCCNPCInteractionOption::Quests), not
	 * through TryPlayerInteract or the ambient script. Deliberately on the
	 * base class rather than ADCCVendorNPC - a guild mentor handing out floor
	 * objectives needs this with no shop attached, so gating it behind being
	 * a vendor would be wrong for that case.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Quests")
	TArray<FName> OfferedQuestIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<UCapsuleComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** The proximity that starts them talking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<USphereComponent> TalkTrigger;

	/** The speech bubble, and the resting nameplate between lines. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<UTextRenderComponent> Speech;

	/** Looping idle. Played through AnimationSingleNode, like the enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Animation")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	/** Optional gesture, played once as each line begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Animation")
	TObjectPtr<UAnimationAsset> TalkAnimation;

	/** Colour of the speech text. Warm by default - these are the friendly ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FLinearColor SpeechColour = FLinearColor(0.95f, 0.90f, 0.70f);

	/** Says the next unsaid line immediately, if there is one. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SayNextLine();

	/** Everything this speaker has left to say, in table order. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void GetRemainingLines(TArray<FDCCDialogueLine>& OutLines) const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool HasMoreToSay() const;

	/**
	 * Advances the script one line, same as walking up already does via
	 * HandleTalkOverlapBegin - but on demand, and a line already showing gets
	 * skipped ahead rather than waited out. Purely local, like the rest of
	 * this class - see BeginPlay's comment on why nothing here is
	 * server-authoritative.
	 *
	 * NOT what a player's E press calls any more (changed 2026-08-30, see
	 * DISPATCH.md) - E now opens ADCCCharacterBase's own Talk/Shop/Leave menu
	 * (NPCInteractionTarget), whose Talk row calls SayNextLine() directly so
	 * that virtual dispatch to ADCCVendorNPC's override below cannot make
	 * "Talk" secretly open the shop instead. This is left as a plain
	 * BlueprintCallable entry point for anything else that wants to make an
	 * NPC say its next line programmatically (a trigger, a cutscene beat).
	 *
	 * Virtual, and takes the interactor, so ADCCVendorNPC can still override
	 * it for whatever else calls it - the base implementation ignores the
	 * parameter, since ambient dialogue has nobody in particular to address.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor);

protected:
	UFUNCTION()
	void HandleTalkOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void HandleTalkOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Collects this speaker's rows from the table, in order, hints last. */
	void BuildScript();

	void ShowLine(const FDCCDialogueLine& Line);

	/** Falls back to the display name, or hides the bubble entirely. */
	void ShowIdleText();

private:
	/** This speaker's lines, ordered. Built once at BeginPlay. */
	UPROPERTY(Transient)
	TArray<FDCCDialogueLine> Script;

	/** How far through Script we are. Never rewinds unless bLoopWhenExhausted. */
	int32 NextLineIndex = 0;

	/** Seconds left on the current line, or on the gap after it. */
	float LineTimeRemaining = 0.0f;

	bool bSpeaking = false;

	/**
	 * Someone is close enough to be spoken to.
	 *
	 * A count rather than a bool: with five players, one walking away must not
	 * silence a conversation the other four are still standing in.
	 */
	int32 ListenerCount = 0;
};
