#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skills/DCCSkillTypes.h"
#include "DCCSkillComponent.generated.h"

class ADCCCharacterBase;
class UDCCStatsComponent;
class UDCCSkillComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCSkillActivated, UDCCSkillComponent*, Skills, const FDCCSkillResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCCSkillAdvanced, UDCCSkillComponent*, Skills, FName, SkillId, int32, NewRank);

/**
 * A character's known skills, and the single place they are spent.
 *
 * Server-authoritative in the same shape as the rest of the game: a client may
 * ASK to use a skill, but cost, cooldown, targeting and the roll are all decided
 * here on the server, and the client learns the outcome. A modified client can
 * request anything it likes and still cannot cast what it cannot afford.
 *
 * The component resolves the archetypes with unambiguous mechanics itself
 * (Damage, Heal) and hands everything else to Blueprint through OnSkillEffect,
 * which is the deliberate split: mechanical resolution stays deterministic in
 * C++, while the "anything is possible" half stays authorable by the DM.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCSkillComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Skills this character knows, with their Ranks.
	 *
	 * Replicated so the owning client's HUD can show ranks, costs and
	 * greying-out without asking the server what it is allowed to press.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Skills")
	TArray<FDCCKnownSkill> KnownSkills;

	/**
	 * The skill library this character draws its starting kit from.
	 *
	 * Skills live in a DataTable rather than being authored per character so that
	 * adding a skill mid-session is one new row plus one name, with no recompile
	 * and no copy of the same numbers pasted onto five different enemies. That
	 * matters because the tabletop game invents skills at the table.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills")
	TObjectPtr<UDataTable> SkillTable;

	/**
	 * Row names granted from SkillTable when play begins.
	 *
	 * Names rather than embedded definitions, so retuning a skill's damage means
	 * editing one table row and every character that knows it is retuned too.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills")
	TArray<FName> StartingSkillIds;

	/** Rank each starting skill is granted at. Enemies can be tuned by raising this alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills", meta = (ClampMin = "0"))
	int32 StartingSkillRank = 1;

	/**
	 * Server only. Grants everything in StartingSkillIds from SkillTable.
	 *
	 * Public so a level-up, a sponsor drop or the DM client can re-run it after
	 * changing the list, rather than this being a one-shot at BeginPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	int32 GrantStartingSkills();

	/** Fires on the server and on every client that receives the activation. */
	UPROPERTY(BlueprintAssignable, Category = "Skills")
	FDCCSkillActivated OnSkillActivated;

	/**
	 * Fires when a skill gains a Rank. The announcement moment - the System AI
	 * telling the crawler they have got better at something is exactly the beat
	 * the tone bible wants, so it is surfaced rather than left implicit.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Skills")
	FDCCSkillAdvanced OnSkillAdvanced;

	/** Looks the definition up in SkillTable. Works for skills this character does not know. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	bool FindSkill(FName SkillId, FDCCSkillDef& OutSkill) const;

	UFUNCTION(BlueprintPure, Category = "Skills")
	bool IsSkillKnown(FName SkillId) const;

	/** This character's Rank in the skill, or 0 if untrained. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	int32 GetSkillRank(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category = "Skills")
	bool HasAdvancementMark(FName SkillId) const;

	/**
	 * Server only. Rolls advancement for every marked skill, then clears all
	 * marks - including on skills that failed their roll, exactly as the
	 * published rule specifies.
	 *
	 * Called when an encounter is cleared. That is this game's stand-in for the
	 * tabletop's "every two hours of play / end of each floor", chosen because it
	 * is a beat the players already feel as an ending and it keeps progress
	 * visible within a single session.
	 *
	 * Returns how many skills gained a Rank.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	int32 RunSkillAdvancement();

	/** Seconds until usable again, or 0 if ready. Valid on server and owning client. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetRemainingCooldown(FName SkillId) const;

	/** Full pre-flight check. Use this to drive HUD state. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	EDCCSkillFailure CanActivate(FName SkillId, AActor* Target) const;

	/**
	 * Ask to use a skill. Safe to call from a client - it routes to the server
	 * rather than resolving locally. This is what input and the phone HUD call.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void TryActivateSkill(FName SkillId, AActor* Target);

	/**
	 * Server only. Grants a skill at runtime - sponsor drops, level ups, DM fiat,
	 * or earning it by having attempted it untrained.
	 *
	 * Granting a skill already known raises it to InitialRank if that is higher,
	 * rather than failing or stacking a duplicate entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	bool GrantSkill(FName SkillId, int32 InitialRank = 1);

	UFUNCTION(BlueprintCallable, Category = "Skills")
	bool RemoveSkill(FName SkillId);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void Server_ActivateSkill(FName SkillId, AActor* Target);

	/** Server only. Does the actual work once the request has been validated. */
	FDCCSkillResult ActivateSkillInternal(const FDCCSkillDef& Skill, AActor* Target);

	/** Everyone the skill lands on, given its targeting mode. */
	void GatherTargets(const FDCCSkillDef& Skill, AActor* Target, TArray<ADCCCharacterBase*>& OutTargets) const;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SkillActivated(const FDCCSkillResult& Result);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SkillAdvanced(FName SkillId, int32 NewRank);

	/**
	 * Server only. Marks a skill as used so it is eligible for advancement.
	 *
	 * Marks on attempt, not on success - failing at something is how you learn it,
	 * and the published rule is explicit that both outcomes count.
	 */
	void MarkSkillUsed(FName SkillId);

	/** Finds this character's entry for a skill, or null. */
	FDCCKnownSkill* FindKnownSkill(FName SkillId);
	const FDCCKnownSkill* FindKnownSkill(FName SkillId) const;

	/**
	 * Archetypes this component does not resolve itself land here, with the
	 * definition and the gathered targets already worked out. Implement in
	 * Blueprint per EffectTag.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skills", meta = (DisplayName = "On Skill Effect"))
	void BP_OnSkillEffect(const FDCCSkillDef& Skill, const TArray<AActor*>& Targets);

	/** Cosmetic hook - montages, VFX, sound. Runs on every machine. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skills", meta = (DisplayName = "On Skill Played"))
	void BP_OnSkillPlayed(const FDCCSkillResult& Result);

private:
	ADCCCharacterBase* GetOwningCharacter() const;
	UDCCStatsComponent* GetOwnerStats() const;

	/**
	 * Cooldown end times keyed by skill, in world seconds.
	 *
	 * Server-side truth. Not replicated as a map because the owning client can
	 * derive its own copy from the activation it just saw, and a stale HUD
	 * cooldown is a far smaller problem than per-frame map replication.
	 */
	TMap<FName, float> CooldownEndTimes;

	/**
	 * Skills attempted untrained since the last advancement roll.
	 *
	 * Server-side only. This is how a crawler *earns* a skill they were never
	 * given: try something you cannot do, and one of those attempts may stick.
	 * Deliberately cleared alongside the advancement marks, because the published
	 * rule is explicit that untrained attempts cannot be banked between sessions.
	 */
	TSet<FName> UntrainedAttempts;
};
