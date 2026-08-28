#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCCharacterBase.h"
#include "DCCEnemyCharacter.generated.h"

/**
 * A hostile combatant.
 *
 * The AI here is deliberately the simplest thing that produces a real fight:
 * acquire the nearest living player inside the aggro radius, close to melee
 * range, swing on cooldown. No behaviour tree, because a tree would need asset
 * authoring before the first encounter could be played at all, and this layer
 * is meant to be replaced by StateTree once encounters need real tactics.
 *
 * Runs on the server only. Clients see position through movement replication
 * and outcomes through the stats component.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter : public ADCCCharacterBase
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** How far this enemy will notice a player from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float AggroRadius = 1500.0f;

	/**
	 * Once aggroed, how far the target may get before this enemy gives up.
	 * Larger than AggroRadius so enemies do not visibly flicker in and out of
	 * pursuit at the boundary.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float LeashRadius = 3000.0f;

	/**
	 * Whether this enemy will use its known skills instead of only swinging.
	 *
	 * On by default, but it does nothing unless the character actually knows a
	 * skill - so a plain melee enemy needs no configuration to stay plain.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI")
	bool bUseSkills = true;

	/**
	 * Minimum seconds between skill attempts, on top of each skill's own cooldown.
	 *
	 * Without this a caster empties its entire hotbar on the first frame it sees
	 * a target, which is both unreadable and unsurvivable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float SkillAttemptInterval = 2.5f;

	// -------------------------------------------------------------------------
	// RANGED STANDOFF
	//
	// A fighter whose weapon outreaches its own body should use that reach
	// instead of walking into arm's length to use it. Before this existed the
	// chase always closed to `AttackRange * 0.5` - the RAW property - while the
	// in-range test asked GetEffectiveAttackRange(), which equipment can raise.
	// Hand Scott a 600-unit lacrosse stick and the two disagreed by 400 units:
	// he could fire from 600 and his feet kept taking him to 210 anyway, every
	// time, which is exactly "ranged allies never keep their distance".
	//
	// Both numbers are fractions of the EFFECTIVE range, so an item that
	// changes reach moves the whole behaviour with it rather than needing the
	// AI retuned per weapon.
	// -------------------------------------------------------------------------

	/** Where a ranged fighter prefers to stand, as a fraction of its effective reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Ranged", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RangedStandoffFraction = 0.8f;

	/**
	 * Closer than this fraction of reach and a ranged fighter gives ground.
	 *
	 * Deliberately well below RangedStandoffFraction: the gap between the two
	 * is a dead band, and without one an archer jitters back and forth on the
	 * boundary instead of shooting - the same reason the escort walk has
	 * FollowResumeMultiplier.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Ranged", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RangedRetreatFraction = 0.45f;

	/**
	 * Seconds a leash-dropped ally must spend heading home before it may pick
	 * a new fight.
	 *
	 * Without this, an ally that hit FollowLeashRadius dropped its target and
	 * re-acquired the same one on the very next scan - 0.35s later - so it
	 * bounced between "charge" and "come back" forever and never stood still
	 * long enough to swing. That reads as "allies just always run at enemies
	 * and never attack", which is precisely what it was.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Escort", meta = (ClampMin = "0.0"))
	float ReengageCooldownSeconds = 2.5f;

	/**
	 * Seconds a caster stands still after casting, rooted in place.
	 *
	 * Casting on the move reads as nothing happening at all - the spell effect
	 * slides along behind a running character and the moment is lost. Holding
	 * still also gives the party a readable window: you can see who is casting
	 * and go and interrupt them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float CastHoldSeconds = 1.5f;

	UFUNCTION(BlueprintPure, Category = "DCC|AI")
	ADCCCharacterBase* GetCurrentTarget() const { return CurrentTarget; }

	// -------------------------------------------------------------------------
	// PASSIVE / PATROL - a guard, not a mob
	//
	// "There for appearances, keeps the place safe" (Zoe's own words for the
	// bar guard): normally ignores everyone and walks a patrol loop, and only
	// turns hostile once violence actually happens nearby - never by simply
	// noticing a player exist, which is what every other mob does. Off by
	// default, so nothing about an ordinary mob's behaviour changes.
	// -------------------------------------------------------------------------

	/**
	 * On: this character never auto-acquires a target by proximity - see the
	 * skipped FindNearestHostileTarget call in Tick. The only way it ever gets
	 * a CurrentTarget is HandleNearbyAttack, below. Once that target is lost
	 * (dies, or leashes away - the same top-of-Tick logic every enemy already
	 * has), it goes back to patrolling rather than picking a new fight on its
	 * own, because nothing here ever re-arms FindNearestHostileTarget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Guard")
	bool bPassiveUntilProvoked = false;

	/**
	 * How close a real attack (see ADCCCharacterBase::OnAnyAttackAttempted)
	 * has to land - to the attacker or to the target - before this guard
	 * treats it as a disturbance worth responding to. Deliberately a
	 * "watching the room" radius rather than AggroRadius, which this
	 * character never uses while passive.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Guard", meta = (ClampMin = "0.0"))
	float ViolenceWatchRadius = 900.0f;

	/**
	 * World-space points this character walks between while passive and
	 * unprovoked. Empty means "stand still" - a guard does not need a patrol
	 * to be a guard. Authored per-instance once placed; there is no in-editor
	 * path generator here, on purpose, the same reason the encounter system
	 * uses hand-placed waves instead of procedural ones.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Guard")
	TArray<FVector> PatrolPoints;

	/** How close counts as "arrived" at a patrol point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Guard", meta = (ClampMin = "1.0"))
	float PatrolPointTolerance = 80.0f;

	/** Seconds spent standing at each patrol point before moving to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Guard", meta = (ClampMin = "0.0"))
	float PatrolWaitSeconds = 3.0f;

	// -------------------------------------------------------------------------
	// ESCORT BEHAVIOUR - allies only
	//
	// Everything below is ignored unless bIsAlly is true. A mob's job is to find
	// the nearest crawler and commit to it; an ally's job is to stay with the
	// party and only break off for a fight that is actually happening. Sharing
	// one AI for both made allies sprint at anything they could see the moment
	// they spawned, which is how a four-person escort ends up strung out across
	// three rooms with the player alone in the middle of them.
	// -------------------------------------------------------------------------

	/** How close an ally tries to stay to the crawler it is escorting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Escort", meta = (ClampMin = "0.0"))
	float FollowDistance = 400.0f;

	/**
	 * Multiple of FollowDistance at which a settled ally sets off again.
	 *
	 * The dead band between the two is what stops an escort twitching on the
	 * boundary: stop at 400, do not resume until 640.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Escort", meta = (ClampMin = "1.0"))
	float FollowResumeMultiplier = 1.6f;

	/**
	 * Speed while escorting. Below RunSpeedThreshold on purpose, so following
	 * plays the walk cycle instead of the run.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Escort", meta = (ClampMin = "0.0"))
	float FollowWalkSpeed = 260.0f;

	/**
	 * Past this far from their leader an ally abandons its target and comes back.
	 *
	 * This is the leash that actually matters for allies - LeashRadius measures
	 * from the ally to its target, which does nothing to stop the ally itself
	 * being dragged halfway across the floor by a fleeing mob.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Escort", meta = (ClampMin = "0.0"))
	float FollowLeashRadius = 1600.0f;

	/**
	 * How close a hostile must be - to the ally or to its leader - before the
	 * ally treats it as an immediate threat and engages without being provoked.
	 *
	 * Deliberately much tighter than AggroRadius. Outside this, an ally will only
	 * engage a hostile that is already fighting someone on the party's side.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Escort", meta = (ClampMin = "0.0"))
	float AllyThreatRadius = 700.0f;

	/**
	 * Whether allies join a fight the moment any hostile aggroes onto the party,
	 * even one further away than AllyThreatRadius.
	 *
	 * This is the "just when aggroed" half. On, because an ally that watches a
	 * mob chew on the player from twenty metres away reads as broken rather than
	 * as disciplined.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Escort")
	bool bEngageWhenPartyAggroed = true;

	// -------------------------------------------------------------------------
	// GEARING UP - allies only
	//
	// An ally that walks past a dropped weapon and never picks it up reads as
	// oblivious, the same way "allies never fight" did before AttackAnimation
	// was wired. Deliberately upgrades-only rather than "grab anything free
	// hands can hold" - an ally comparing a nearby item against what it
	// already has and only swapping in a strict improvement means it never
	// undresses itself for something worse, which a first-match rule would
	// happily do if a plain item turned up before a better one.
	// -------------------------------------------------------------------------

	/** How far an ally will notice gear worth picking up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Gear", meta = (ClampMin = "0.0"))
	float GearScanRadius = 500.0f;

	/** Seconds between gear scans - the same throttling reason the target and leader scans use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|Gear", meta = (ClampMin = "0.0"))
	float GearScanInterval = 3.0f;

	// -------------------------------------------------------------------------
	// FIRST AID - allies only, and only the ones flagged for it
	//
	// Off by default: not every ally should abandon a fight to go play medic,
	// and a party that never designated a healer should not get one for free.
	// CJ carries this by default (ADCCAlly_HockeyWings) - a caster is the
	// natural pick, and "the one with the mana pool also knows first aid"
	// reads as a choice rather than an arbitrary flag on one ally.
	//
	// Deliberately does NOT use the skill/mana system - it reuses the exact
	// same BeginReviveChannel a player's own "Help Them Up" already runs, so
	// there is one revive mechanic in the game, not two that can drift apart.
	// -------------------------------------------------------------------------

	/** Whether this ally will notice a downed party member and go help on its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|FirstAid")
	bool bHasFirstAid = false;

	/** How far this ally will notice someone downed. Generous - missing a call for help should not be subtle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|FirstAid", meta = (ClampMin = "0.0"))
	float FirstAidSearchRadius = 2000.0f;

	/** How close the ally must actually be to start the revive channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|FirstAid", meta = (ClampMin = "0.0"))
	float FirstAidRange = 150.0f;

	/** Seconds between first-aid scans - same throttling reasoning as every other AI scan here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI|FirstAid", meta = (ClampMin = "0.0"))
	float FirstAidScanInterval = 1.0f;

	/**
	 * Ordered to stand still via the interaction menu's "Hold position".
	 *
	 * Suppresses escorting only, never self-defence: a held ally still fights
	 * what walks into it. An order to wait is not an order to die politely.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|AI|Escort")
	bool bHoldingPosition = false;

	virtual void SetHoldingPosition(bool bHold) override;

	/**
	 * Flips this character to the crawlers' side: an ally NPC rather than a mob.
	 *
	 * One flag rather than a separate ally class, because the behaviour is
	 * identical - find the nearest hostile, close, swing. Only the definition of
	 * "hostile" changes, and IsHostileTo already derives that from IsCrawler.
	 *
	 * Allies do count as room occupants, so a room full of friendly NPCs can arm
	 * an encounter. That is deliberate: the party's own NPCs walking into a trap
	 * should spring it.
	 */
	// Replicated: without it, only the server (and a listen-server host, which
	// shares the server's own actors) ever sees the true value. Every remote
	// client falls back to the C++ default of false - IsCrawler() then reads
	// false too, so anything client-side that keys off allegiance (nameplate
	// colour, the world-space health bar tint) shows an ally as hostile even
	// though the server-driven behaviour - targeting, damage, who fights whom -
	// was always correct. That split (right behaviour, wrong colour) is exactly
	// what this was seen doing before it was marked Replicated.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "DCC|AI")
	bool bIsAlly = false;

	/** Dungeon-side unless bIsAlly flips it. Drives the whole faction check. */
	virtual bool IsCrawler() const override { return bIsAlly; }

	// -------------------------------------------------------------------------
	// REPOSSESSION NIP
	//
	// The Debt Eaters' actual threat. Their damage is trivial and always will be -
	// what makes them dangerous is that every bite converts the party's gold into
	// the boss's health, so ignoring them to focus the boss is a losing trade even
	// though it looks like the obvious play.
	// -------------------------------------------------------------------------

	/** Gold taken from the target on every landed hit. Zero disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Leech", meta = (ClampMin = "0"))
	int32 GoldStolenPerHit = 0;

	/**
	 * Hit points the summoner gains per gold stolen.
	 *
	 * Separate from the amount taken so the exchange rate is tunable: at 1.0 a
	 * broke party neuters the boss entirely, which is a legitimate and funny
	 * outcome the fight should be allowed to have.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Leech", meta = (ClampMin = "0.0"))
	float SummonerHealPerGold = 1.0f;

	/**
	 * Who the stolen gold feeds. Resolved from the encounter's boss when unset.
	 *
	 * Held as a soft reference rather than set at spawn because minions are
	 * summoned by waves that do not know which spawn was flagged as the boss.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Leech")
	TObjectPtr<ADCCCharacterBase> LeechBeneficiary;

protected:
	/** Applies Repossession Nip. Server only, called from PerformAttack. */
	virtual void OnDamageDealt(ADCCCharacterBase* Target, int32 Damage) override;

public:

protected:
	virtual void BeginPlay() override;

	/**
	 * Nearest living hostile inside AggroRadius, or null.
	 *
	 * Asks IsHostileTo rather than looking for a PlayerController, which is what
	 * makes allies work at all: a friendly NPC hunts mobs, a mob hunts crawlers
	 * and their allies, and neither needs to know which is player-controlled.
	 */
	ADCCCharacterBase* FindNearestHostileTarget() const;

	/**
	 * One line per enemy at spawn: Intelligence, mana pool, skill table, and the
	 * cost of everything it knows. Exists because a caster that cannot afford its
	 * own spell behaves identically to one that has no spell at all.
	 */
	void LogCastingReadiness() const;

	/**
	 * The crawler this ally is escorting: the nearest player-controlled one.
	 *
	 * Player-controlled specifically, not merely IsCrawler - otherwise four
	 * allies pick each other as leaders and the whole group follows itself
	 * around in a circle while the actual player walks off alone.
	 *
	 * Returns null for mobs, and for an ally with no player in the level.
	 */
	ADCCCharacterBase* FindFollowLeader() const;

	/**
	 * Whether an ally should break off and fight this hostile.
	 *
	 * True when the hostile is an immediate threat (inside AllyThreatRadius of
	 * the ally or of its leader) or, with bEngageWhenPartyAggroed, when it has
	 * already picked a fight with someone on the party's side. Everything else
	 * is left alone - which is the whole point, and what stops an ally charging
	 * an idle mob across the room the instant it comes into view.
	 */
	bool ShouldAllyEngage(const ADCCCharacterBase* Hostile, const ADCCCharacterBase* Leader) const;

	/**
	 * Walks an ally back towards its leader, or stands it down when close enough.
	 *
	 * Split out of Tick because it is the ally's whole idle behaviour, and Tick
	 * was already long enough to hide a bug in.
	 */
	void TickFollowLeader(ADCCCharacterBase* Leader, class AAIController* AIController);

	/**
	 * Server only. Casts the longest-range currently-usable skill at Target.
	 *
	 * Returns true if something was cast, which the caller treats as "this turn
	 * is spent" - so a caster that just threw a spell does not also close to
	 * melee in the same frame.
	 *
	 * Longest range wins deliberately: it makes casters hang back and use the
	 * reach their spells give them, instead of walking into melee to cast a
	 * ranged attack, which looks broken even when the numbers are right.
	 */
	bool TryUseSkill(ADCCCharacterBase* Target);

	/**
	 * Server only, ally-only. If a downed party member is within
	 * FirstAidSearchRadius, moves to them and starts BeginReviveChannel once
	 * in range. Returns true when it has claimed this tick for first aid (so
	 * the caller does not also try to chase a combat target the same frame).
	 *
	 * Deliberately only runs when this ally has no CurrentTarget - an ally
	 * already mid-fight does not break off to play medic, the same
	 * "self-defence is unaffected" line HoldPosition draws for combat.
	 */
	bool TryTickFirstAid(class AAIController* AIController);

	/**
	 * Walks between PatrolPoints, pausing PatrolWaitSeconds at each. Called
	 * from Tick's idle branch only when bPassiveUntilProvoked and there is no
	 * CurrentTarget - a provoked guard falls through to the normal chase/swing
	 * path below exactly like any other enemy.
	 */
	void TickPatrol(class AAIController* AIController);

	/**
	 * Bound to ADCCCharacterBase::OnAnyAttackAttempted when bPassiveUntilProvoked,
	 * in BeginPlay. Server only - the delegate is only ever broadcast there.
	 * Ignores everything except a genuine disturbance: already fighting,
	 * dead, no attacker, or too far away from both the attacker and the
	 * target all fall through without doing anything.
	 */
	void HandleNearbyAttack(ADCCCharacterBase* Attacker, ADCCCharacterBase* Target);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> CurrentTarget;

	/** Set while a move order is outstanding, so we do not re-issue it every frame. */
	bool bIsChasing = false;

	// -------------------------------------------------------------------------
	// PATROL STATE - see TickPatrol
	// -------------------------------------------------------------------------

	int32 CurrentPatrolIndex = -1;

	/** True on the frame after the patrol move order was last issued and still running. */
	bool bWasPatrolMoving = false;

	/** World time before which a just-arrived patrol stands still. */
	float PatrolWaitUntil = 0.0f;

	/** Set true once HandleNearbyAttack is bound, so EndPlay only unbinds what BeginPlay actually bound. */
	bool bBoundToAttackDelegate = false;

	// -------------------------------------------------------------------------
	// SCAN THROTTLING
	//
	// FindFollowLeader and FindNearestHostileTarget both walk every character in
	// the world with a TActorIterator. Running either per enemy per frame is
	// O(enemies x actors) every tick - with a couple of hundred actors in the
	// level that is thousands of visits a frame for information that changes
	// slowly. Both are now cached and refreshed on a stagger.
	//
	// The stagger matters as much as the interval: without a per-actor offset
	// every enemy would re-scan on the same frame and turn a steady cost into a
	// periodic hitch.
	// -------------------------------------------------------------------------

	/** Seconds between full world scans for a follow leader. */
	UPROPERTY(EditAnywhere, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float LeaderScanInterval = 0.5f;

	/** Seconds between full world scans for a new target. */
	UPROPERTY(EditAnywhere, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float TargetScanInterval = 0.35f;

	float NextFirstAidScanTime = 0.0f;

	/**
	 * Whoever this ally has committed to reaching. Kept across ticks so an
	 * approach is not abandoned mid-walk the instant the scan timer lapses and
	 * a different, momentarily-closer patient appears - that would flicker the
	 * ally between two targets and never actually arrive at either.
	 */
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> FirstAidTarget;

	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> CachedLeader;

	float NextLeaderScanTime = 0.0f;
	float NextTargetScanTime = 0.0f;

	/** Cached leader, rescanned at most every LeaderScanInterval. */
	ADCCCharacterBase* GetFollowLeaderThrottled();

	/**
	 * Server only, allies only. Scans within GearScanRadius for equipment
	 * pickups worth wearing and equips the first strict upgrade found.
	 *
	 * "Upgrade" is a single-number heuristic - ability bonuses plus weapon die
	 * plus Evade bonus, summed - not a real simulation of what the party would
	 * actually choose. Good enough to stop an ally standing next to a dropped
	 * sword with a fist, not a substitute for a player's own judgement about
	 * NameTagDave's Charisma bonus mattering more than a d8 in this fight.
	 */
	void TryAutoEquipNearbyGear();

	float NextGearScanTime = 0.0f;

	/** World seconds of the last skill attempt. Server-side only. */
	float LastSkillAttemptTime = TNumericLimits<float>::Lowest();

	/** World time until which this character is rooted mid-cast. */
	float CastHoldUntilTime = 0.0f;

	/** World time before which a leash-dropped ally may not take a new target. */
	float ReengageAllowedTime = 0.0f;
};
