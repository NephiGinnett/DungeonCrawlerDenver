#include "Characters/DCCEnemyCharacter.h"

#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Items/DCCPickupVariants.h"
#include "Items/DCCPickup.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Components/WidgetComponent.h"
#include "Combat/DCCGameState.h"
#include "Rooms/DCCEncounter.h"
#include "UI/DCCEventLogTypes.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/**
	 * A single-number "how good is this item" heuristic, so an ally can compare
	 * a candidate on the ground against whatever it already has equipped.
	 *
	 * Not a real evaluation of fit for a build or a fight - just enough signal
	 * to stop an ally standing next to a dropped weapon with its fists out.
	 */
	int32 ScoreEquipmentDef(const FDCCEquipmentDef& Def)
	{
		return Def.BonusStrength + Def.BonusIntelligence + Def.BonusConstitution
			+ Def.BonusDexterity + Def.BonusCharisma + Def.WeaponDamageDie + Def.EvadeBonus;
	}
}

void ADCCEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCEnemyCharacter, bIsAlly);
}

ADCCEnemyCharacter::ADCCEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Zero means "wait until looted" - see LootRemains, which starts the despawn
	// once the body is empty.
	//
	// This was 8 seconds, from before bodies carried anything. The moment loot
	// moved onto the corpse that became a bug: the party had eight seconds to
	// find and search a body before it deleted itself and the loot with it.
	// Cleanup still happens, it is just driven by the body being emptied rather
	// than by a timer that knows nothing about whether anyone got there.
	CorpseLifeSpan = 0.0f;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->MaxWalkSpeed = 400.0f;
	}

	// A default worth having rather than an empty array nobody remembers to fill.
	// Roughly half of anything hostile leaves pocket change, which keeps a cleared
	// room worth walking back through without turning gold into a guaranteed wage.
	// Override per-Blueprint for anything that should pay better - a Debt Eater
	// carrying the till is a different entry, not a bigger number here.
	{
		FDCCLootDrop Coins;
		Coins.PickupClass = ADCCPickup_Coins::StaticClass();
		Coins.Chance = 0.55f;
		Coins.MinGold = 3;
		Coins.MaxGold = 12;
		LootTable.Add(Coins);
	}

	bUseControllerRotationYaw = false;
}

void ADCCEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Retires the Blueprint-authored HealthBarWidget (WBP_EnemyHealthBar) on
	// every machine. It was a Screen Space widget component with no distance
	// cull and a per-frame Blueprint Tick recomputing its fill - a full
	// compositing pass for every enemy in the level regardless of range, which
	// is what made health bars show everywhere and cost real frame time. The
	// HUD's DrawNearbyHealthBars now does this job as one bounded Canvas pass.
	//
	// HIDDEN, NOT DESTROYED. This previously called DestroyComponent, which threw
	// five "HealthBarWidget is not valid (pending kill or garbage)" errors on
	// every PIE start: the Blueprint EventGraph on each subclass still holds a
	// reference and calls SetVisibility on it, and destroying the component out
	// from under Blueprint code that has not been edited to match is a lie the
	// Blueprint has no way to detect.
	//
	// Hiding costs nothing the destroy saved. An invisible component is skipped
	// by the renderer, and with its tick off the per-frame Blueprint recompute is
	// gone too - which was the actual expense. The reference simply stays valid,
	// so the Blueprint's own SetVisibility(false) is a harmless no-op instead of
	// an error.
	if (UWidgetComponent* LegacyHealthBar = FindComponentByClass<UWidgetComponent>())
	{
		LegacyHealthBar->SetVisibility(false);
		LegacyHealthBar->SetHiddenInGame(true);
		LegacyHealthBar->SetComponentTickEnabled(false);
	}

	// Only the server thinks. Clients would otherwise run a second, divergent
	// copy of this AI against their own view of the world.
	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
	}
	else
	{
		// Allies get the survivability cushion here rather than in the constructor,
		// because bIsAlly is a per-instance flag - the placed actor decides, and
		// only the server is authoritative about it.
		if (bIsAlly && Stats)
		{
			Stats->SetIsAllyCharacter(true);
		}

		LogCastingReadiness();

		// A passive guard is the only thing that needs to hear about violence
		// it didn't start itself - see bPassiveUntilProvoked's own comment.
		if (bPassiveUntilProvoked)
		{
			ADCCCharacterBase::OnAnyAttackAttempted.AddUObject(this, &ADCCEnemyCharacter::HandleNearbyAttack);
			bBoundToAttackDelegate = true;
		}
	}
}

void ADCCEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bBoundToAttackDelegate)
	{
		ADCCCharacterBase::OnAnyAttackAttempted.RemoveAll(this);
		bBoundToAttackDelegate = false;
	}

	Super::EndPlay(EndPlayReason);
}

void ADCCEnemyCharacter::LogCastingReadiness() const
{
	// Diagnostic for "enemies never cast". The failure is silent by design -
	// CanActivate returns NotEnoughMana or UnknownSkill and the AI simply falls
	// through to melee - so the only way to tell WHICH of the several possible
	// causes is in play is to print the inputs once, per enemy, at spawn.
	//
	// Reads the values off the live actor rather than the class, because the
	// placed instance is what actually plays: a Blueprint CDO and the copy
	// serialised into the level have disagreed before, and that cost a session.
	if (!Stats || !Skills)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Cast] %s: missing %s component"),
			*GetActorNameOrLabel(), Stats ? TEXT("Skills") : TEXT("Stats"));
		return;
	}

	const int32 IntScore = Stats->GetEnhanced().GetScore(EDCCAbility::Intelligence);

	FString SkillReport;
	for (const FDCCKnownSkill& Known : Skills->KnownSkills)
	{
		FDCCSkillDef Def;
		if (Skills->FindSkill(Known.SkillId, Def))
		{
			SkillReport += FString::Printf(TEXT("%s(cost %d%s) "),
				*Known.SkillId.ToString(), Def.ManaCost,
				Def.ManaCost > Stats->MaxManaPoints ? TEXT(" UNAFFORDABLE") : TEXT(""));
		}
		else
		{
			SkillReport += FString::Printf(TEXT("%s(NOT IN TABLE) "), *Known.SkillId.ToString());
		}
	}

	if (Skills->KnownSkills.IsEmpty())
	{
		SkillReport = TEXT("(knows nothing)");
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[DCC Cast] %s  INT=%d  mana=%d/%d  bUseSkills=%s  table=%s  skills: %s"),
		*GetActorNameOrLabel(),
		IntScore,
		Stats->CurrentManaPoints, Stats->MaxManaPoints,
		bUseSkills ? TEXT("true") : TEXT("false"),
		Skills->SkillTable ? *Skills->SkillTable->GetName() : TEXT("NONE"),
		*SkillReport);
}

ADCCCharacterBase* ADCCEnemyCharacter::FindNearestHostileTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ADCCCharacterBase* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	// Every combatant, filtered by faction. This replaced a player-controller
	// scan: that version could only ever hunt the human player, so a friendly NPC
	// had nothing to attack and mobs ignored allies standing next to them.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Candidate = *It;
		if (!Candidate || !Candidate->IsAlive() || !IsHostileTo(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Candidate;
		}
	}

	if (Best && BestDistanceSquared <= FMath::Square(AggroRadius))
	{
		return Best;
	}

	return nullptr;
}

ADCCCharacterBase* ADCCEnemyCharacter::GetFollowLeaderThrottled()
{
	UWorld* World = GetWorld();
	if (!bIsAlly || !World)
	{
		return nullptr;
	}

	const float Now = World->GetTimeSeconds();

	// A leader that died or was destroyed must not be held onto until the next
	// scheduled scan, so validity is checked every frame even though the search
	// is not.
	if (CachedLeader && (!IsValid(CachedLeader) || !CachedLeader->IsAlive()))
	{
		CachedLeader = nullptr;
		NextLeaderScanTime = 0.0f;
	}

	if (Now >= NextLeaderScanTime)
	{
		// Staggered by a random fraction of the interval so a room full of allies
		// spreads its scans across frames instead of spiking on one.
		NextLeaderScanTime = Now + LeaderScanInterval * FMath::FRandRange(0.85f, 1.15f);
		CachedLeader = FindFollowLeader();
	}

	return CachedLeader;
}

void ADCCEnemyCharacter::TryAutoEquipNearbyGear()
{
	if (!bIsAlly || !HasAuthority() || !Equipment)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Here = GetActorLocation();
	const float RadiusSquared = FMath::Square(GearScanRadius);

	for (TActorIterator<ADCCPickup> It(World); It; ++It)
	{
		ADCCPickup* Candidate = *It;

		// A held pickup is in someone's hand and not available to grab, and an
		// unset EquipmentItemId means this is a crate or a coin, not gear.
		if (!Candidate || Candidate->IsHeld() || Candidate->EquipmentItemId.IsNone())
		{
			continue;
		}

		if (FVector::DistSquared(Here, Candidate->GetActorLocation()) > RadiusSquared)
		{
			continue;
		}

		FDCCEquipmentDef CandidateDef;
		if (!Equipment->FindItem(Candidate->EquipmentItemId, CandidateDef))
		{
			continue;
		}

		const FName CurrentId = Equipment->GetEquippedId(CandidateDef.Slot);

		int32 CurrentScore = 0;
		if (!CurrentId.IsNone())
		{
			FDCCEquipmentDef CurrentDef;
			if (Equipment->FindItem(CurrentId, CurrentDef))
			{
				CurrentScore = ScoreEquipmentDef(CurrentDef);
			}
		}

		if (ScoreEquipmentDef(CandidateDef) <= CurrentScore)
		{
			continue;
		}

		// The same entry point a player's own pickup uses - it equips straight
		// into the slot and consumes itself. See ADCCPickup::OnPickedUp's
		// comment on EquipmentItemId for why nothing more is needed here.
		Candidate->OnPickedUp(this);

		ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
			NSLOCTEXT("DCC", "LogAllyGearUp", "{0} picks up {1}."),
			FText::FromString(GetActorNameOrLabel()), CandidateDef.DisplayName));

		// One item per scan - the next scan re-evaluates from the new baseline
		// rather than an ally emptying a whole chest into itself in one frame.
		return;
	}
}

ADCCCharacterBase* ADCCEnemyCharacter::FindFollowLeader() const
{
	UWorld* World = GetWorld();
	if (!bIsAlly || !World)
	{
		return nullptr;
	}

	ADCCCharacterBase* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Candidate = *It;

		// Player-controlled only. IsCrawler() is true for allies too, and letting
		// allies lead each other produces a conga line that never reaches anyone.
		if (!Candidate || Candidate == this || !Candidate->IsAlive() || !Candidate->IsPlayerControlled())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Candidate;
		}
	}

	return Best;
}

bool ADCCEnemyCharacter::ShouldAllyEngage(const ADCCCharacterBase* Hostile, const ADCCCharacterBase* Leader) const
{
	if (!Hostile)
	{
		return false;
	}

	// Close enough to be an immediate problem for this ally.
	if (FVector::DistSquared(GetActorLocation(), Hostile->GetActorLocation()) <= FMath::Square(AllyThreatRadius))
	{
		return true;
	}

	// Or close enough to be an immediate problem for whoever we are escorting -
	// an ally that only defends itself is not an escort.
	if (Leader && FVector::DistSquared(Leader->GetActorLocation(), Hostile->GetActorLocation())
		<= FMath::Square(AllyThreatRadius))
	{
		return true;
	}

	// The "just when aggroed" case: this hostile has already committed to
	// someone on our side, so the fight exists whether we join it or not.
	if (bEngageWhenPartyAggroed)
	{
		if (const ADCCEnemyCharacter* HostileAI = Cast<ADCCEnemyCharacter>(Hostile))
		{
			const ADCCCharacterBase* TheirTarget = HostileAI->GetCurrentTarget();

			// IsCrawler() covers the player and every ally, which is exactly the
			// set of people we would step in for.
			if (TheirTarget && TheirTarget->IsAlive() && TheirTarget->IsCrawler())
			{
				return true;
			}
		}
	}

	return false;
}

void ADCCEnemyCharacter::SetHoldingPosition(bool bHold)
{
	bHoldingPosition = bHold;

	// Stop immediately rather than after the current path finishes - "hold
	// position" that takes another ten metres to take effect is not an order.
	if (bHold)
	{
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			AIController->StopMovement();
		}
		bIsChasing = false;
	}
}

void ADCCEnemyCharacter::TickFollowLeader(ADCCCharacterBase* Leader, AAIController* AIController)
{
	// Ordered to stay put. Combat is handled before this is ever reached, so a
	// held ally still defends itself - it just stops trailing the party.
	if (bHoldingPosition)
	{
		return;
	}

	if (!AIController)
	{
		return;
	}

	// No leader is a REASON TO STOP, not a reason to do nothing. Returning early
	// here left any outstanding move order running, which is half of why allies
	// walked forever - the other half was the speed, below.
	if (!Leader)
	{
		if (bIsChasing)
		{
			AIController->StopMovement();
			bIsChasing = false;
		}

		SetMovementSpeedOverride(0.0f);
		return;
	}

	const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Leader->GetActorLocation());

	// Hysteresis. Stopping and re-engaging at the same radius makes an ally
	// twitch on the boundary forever; they now settle at FollowDistance and do
	// not set off again until the leader is meaningfully further away.
	const float ResumeDistance = FollowDistance * FollowResumeMultiplier;

	if (DistanceSquared <= FMath::Square(FollowDistance))
	{
		// Unconditional, not gated on bIsChasing. That flag can disagree with the
		// controller - combat sets and clears it on its own paths - and when it
		// did, nothing ever issued the stop and the ally kept walking.
		AIController->StopMovement();
		bIsChasing = false;

		SetMovementSpeedOverride(0.0f);
		return;
	}

	if (!bIsChasing && DistanceSquared <= FMath::Square(ResumeDistance))
	{
		// Inside the dead band: already settled, not yet far enough to chase.
		return;
	}

	// Escorting is a walk, not a sprint. Without this the derived speed lands
	// above RunSpeedThreshold and every ally plays the run cycle whenever it
	// moves at all - which reads as the whole party permanently fleeing.
	SetMovementSpeedOverride(FollowWalkSpeed);

	// Re-issued when idle for the same reason the chase is: the leader keeps
	// moving, so a single MoveToActor finishes somewhere they no longer are.
	if (!bIsChasing || AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		AIController->MoveToActor(Leader, FollowDistance * 0.8f);
		bIsChasing = true;
	}
}

bool ADCCEnemyCharacter::TryTickFirstAid(AAIController* AIController)
{
	if (!bIsAlly || !bHasFirstAid || !HasAuthority())
	{
		return false;
	}

	// Already channelling - BeginCastLock already stopped movement, so there is
	// nothing to do but hold the tick so TickFollowLeader does not issue a
	// competing move order out from under the revive.
	if (bIsCasting)
	{
		return true;
	}

	// Drop a commitment that stopped being valid - revived by someone faster,
	// died outright, or simply gone (level streaming, destroyed, etc.).
	if (FirstAidTarget && (!IsValid(FirstAidTarget) || !FirstAidTarget->IsDowned()))
	{
		FirstAidTarget = nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Only scan for someone NEW on the throttled interval. Once committed, keep
	// walking toward the same patient every tick regardless of the scan timer -
	// otherwise a momentarily-closer patient appearing mid-approach would
	// flicker the target and the ally would never actually arrive at either.
	if (!FirstAidTarget)
	{
		const float Now = World->GetTimeSeconds();
		if (Now < NextFirstAidScanTime)
		{
			return false;
		}

		NextFirstAidScanTime = Now + FirstAidScanInterval * FMath::FRandRange(0.85f, 1.15f);

		float BestDistanceSquared = FMath::Square(FirstAidSearchRadius);
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			ADCCCharacterBase* Candidate = *It;
			if (!Candidate || Candidate == this || !Candidate->IsDowned() || IsHostileTo(Candidate))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
			if (DistanceSquared <= BestDistanceSquared)
			{
				FirstAidTarget = Candidate;
				BestDistanceSquared = DistanceSquared;
			}
		}
	}

	if (!FirstAidTarget)
	{
		return false;
	}

	if (FVector::DistSquared(GetActorLocation(), FirstAidTarget->GetActorLocation()) <= FMath::Square(FirstAidRange))
	{
		if (AIController)
		{
			AIController->StopMovement();
		}

		BeginReviveChannel(FirstAidTarget);
		FirstAidTarget = nullptr;
		return true;
	}

	if (AIController)
	{
		SetMovementSpeedOverride(FollowWalkSpeed);

		if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			AIController->MoveToActor(FirstAidTarget, FirstAidRange * 0.8f);
		}
	}

	return true;
}

bool ADCCEnemyCharacter::TryUseSkill(ADCCCharacterBase* Target)
{
	if (!bUseSkills || !Skills || !Target || !HasAuthority())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if ((World->GetTimeSeconds() - LastSkillAttemptTime) < SkillAttemptInterval)
	{
		return false;
	}

	const float DistanceToTarget = FVector::Dist(GetActorLocation(), Target->GetActorLocation());

	FName BestSkillId = NAME_None;
	float BestRange = -1.0f;

	for (const FDCCKnownSkill& Known : Skills->KnownSkills)
	{
		FDCCSkillDef Skill;
		if (!Skills->FindSkill(Known.SkillId, Skill) || Skill.bIsPassive)
		{
			continue;
		}

		// CanActivate only range-checks SingleTarget skills, so cones and areas
		// need checking here or an enemy would happily breathe fire from across
		// the room and hit nothing.
		if (Skill.Targeting != EDCCSkillTargeting::Self && DistanceToTarget > Skill.Range)
		{
			continue;
		}

		if (Skills->CanActivate(Known.SkillId, Target) != EDCCSkillFailure::None)
		{
			continue;
		}

		if (Skill.Range > BestRange)
		{
			BestRange = Skill.Range;
			BestSkillId = Known.SkillId;
		}
	}

	if (BestSkillId.IsNone())
	{
		return false;
	}

	// Face the target first: cast effects spawn along the caster's forward
	// vector, so a spell thrown by someone facing away looks like a bug.
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (!ToTarget.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}

	LastSkillAttemptTime = World->GetTimeSeconds();
	CastHoldUntilTime = World->GetTimeSeconds() + CastHoldSeconds;

	// Rooted for the cast: stop dead now, and Tick refuses to move or re-path
	// until the hold expires.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}
	bIsChasing = false;

	PlayOneShotAnimation(CastAnimation, CastHoldSeconds);

	Skills->TryActivateSkill(BestSkillId, Target);

	return true;
}

void ADCCEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !IsAlive())
	{
		return;
	}

	// The pause is a planning phase for the whole table, so the dungeon stops
	// thinking too. Without this, enemies would keep closing and swinging while
	// the players are frozen deciding what to do.
	if (const ADCCGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADCCGameState>() : nullptr)
	{
		if (GameState->IsCombatPaused())
		{
			return;
		}
	}

	AAIController* AIController = Cast<AAIController>(GetController());

	// Independent of combat targeting on purpose - an ally gears up whether or
	// not it currently has a fight, same as it keeps escorting either way.
	if (bIsAlly)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextGearScanTime)
		{
			NextGearScanTime = Now + GearScanInterval * FMath::FRandRange(0.85f, 1.15f);
			TryAutoEquipNearbyGear();
		}
	}

	// Null for every mob, and for an ally with nobody to escort - so all the
	// escort logic below collapses to the original behaviour in those cases.
	ADCCCharacterBase* Leader = GetFollowLeaderThrottled();

	// Drop a target that died or wandered past the leash before looking for a new one.
	if (CurrentTarget)
	{
		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());

		bool bDropTarget = !CurrentTarget->IsAlive() || DistanceSquared > FMath::Square(LeashRadius);

		// An ally is also leashed to its leader, not just to its target. Without
		// this a mob that backs away tows the whole escort off the map.
		//
		// A leash break also starts a re-engage cooldown, which is the half that
		// was missing: dropping the target alone did nothing, because the very
		// next scan 0.35s later re-acquired the same enemy from the same spot
		// and the ally simply turned round and charged again. The result was an
		// escort permanently sprinting between its leader and a fight it never
		// arrived at - "they never stop running and never attack".
		if (!bDropTarget && Leader)
		{
			const float FromLeaderSquared = FVector::DistSquared(GetActorLocation(), Leader->GetActorLocation());
			if (FromLeaderSquared > FMath::Square(FollowLeashRadius))
			{
				bDropTarget = true;

				if (const UWorld* World = GetWorld())
				{
					ReengageAllowedTime = World->GetTimeSeconds() + ReengageCooldownSeconds;
				}
			}
		}

		// And it stops caring the moment the target stops being a threat worth
		// leaving the party for - otherwise "engage when aggroed" would be a
		// one-way door and the ally would never come home.
		if (!bDropTarget && bIsAlly && !ShouldAllyEngage(CurrentTarget, Leader))
		{
			bDropTarget = true;
		}

		if (bDropTarget)
		{
			CurrentTarget = nullptr;
			bIsChasing = false;

			if (AIController)
			{
				AIController->StopMovement();
			}
		}
	}

	if (!CurrentTarget)
	{
		// Throttled for the same reason the leader scan is. An enemy with nothing
		// to fight was previously re-scanning the entire world every single frame,
		// which is the most expensive thing an idle actor could possibly do.
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now < NextTargetScanTime)
		{
			if (bIsAlly)
			{
				TickFollowLeader(Leader, AIController);
			}

			return;
		}

		NextTargetScanTime = Now + TargetScanInterval * FMath::FRandRange(0.85f, 1.15f);

		// Still walking off a leash break: head home rather than picking the
		// same fight again. Self-defence is unaffected, because anything that
		// actually reaches the ally lands inside AllyThreatRadius and gets
		// taken as a target the moment the cooldown lapses.
		if (bIsAlly && Now < ReengageAllowedTime)
		{
			TickFollowLeader(Leader, AIController);
			return;
		}

		// A passive guard never auto-acquires by proximity - the only way it
		// ever gets a target is HandleNearbyAttack. Skipping the scan here
		// rather than filtering its result is what lets it go back to
		// patrolling on its own once that target is lost: nothing re-arms.
		ADCCCharacterBase* Candidate = bPassiveUntilProvoked ? nullptr : FindNearestHostileTarget();

		// Mobs take whatever they find. Allies hold formation unless the fight
		// has actually come to the party.
		if (Candidate && bIsAlly && !ShouldAllyEngage(Candidate, Leader))
		{
			Candidate = nullptr;
		}

		CurrentTarget = Candidate;
		bIsChasing = false;
	}

	if (!CurrentTarget)
	{
		// Nothing worth fighting. An ally checks for someone to save before
		// falling back on escorting; a passive guard patrols; a mob idles
		// exactly as it always did.
		if (bIsAlly)
		{
			if (!TryTickFirstAid(AIController))
			{
				TickFollowLeader(Leader, AIController);
			}
		}
		else if (bPassiveUntilProvoked)
		{
			TickPatrol(AIController);
		}

		return;
	}

	// Mid-cast: hold position. Facing still tracks the target so the spell does
	// not fire off into empty space, but no movement is issued.
	if (GetWorld()->GetTimeSeconds() < CastHoldUntilTime)
	{
		const FVector ToTargetHeld = CurrentTarget->GetActorLocation() - GetActorLocation();
		if (!ToTargetHeld.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.0f, ToTargetHeld.Rotation().Yaw, 0.0f));
		}
		return;
	}

	// Skills come before melee and before movement. A caster whose spell outranges
	// its fists should stand and use it rather than closing first - and stopping
	// the chase here is what actually produces that behaviour.
	if (TryUseSkill(CurrentTarget))
	{
		if (bIsChasing && AIController)
		{
			AIController->StopMovement();
			bIsChasing = false;
		}

		return;
	}

	// Equipment can raise reach well past the melee AttackRange property, so
	// every distance decision below reads the EFFECTIVE value. Mixing the two
	// is what made ranged fighters walk into melee - see the RANGED STANDOFF
	// note on the header.
	const float EffectiveRange = GetEffectiveAttackRange();
	const float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());

	// "Ranged" is not a flag anyone sets - it is simply whether the thing in
	// your hands reaches further than your arms do. Pick up a bow and you
	// start kiting; drop it and you stop, with no per-character retuning.
	const bool bIsRanged = EffectiveRange > (AttackRange + 1.0f);

	if (IsInAttackRange(CurrentTarget))
	{
		if (bIsChasing && AIController)
		{
			AIController->StopMovement();
			bIsChasing = false;
		}

		// Face the target so the swing reads correctly even though movement
		// rotation is no longer driving us.
		const FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator Facing(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			SetActorRotation(Facing);
		}

		// PerformAttack enforces its own cooldown, so calling every frame is safe.
		// Shooting happens BEFORE any repositioning below, so giving ground never
		// costs a shot - a kiter that stopped firing while backing up would be
		// strictly worse than one that just stood still.
		PerformAttack(CurrentTarget);

		// Something has closed inside comfortable distance: give ground, still
		// firing. Gated on the move being idle so this issues one retreat order
		// rather than a fresh one every frame, which would cancel itself
		// continuously and leave the character twitching in place.
		if (bIsRanged && AIController
			&& DistanceToTarget < EffectiveRange * RangedRetreatFraction
			&& AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			const FVector Away = (GetActorLocation() - CurrentTarget->GetActorLocation()).GetSafeNormal2D();
			if (!Away.IsNearlyZero())
			{
				const float Backpedal = FMath::Max(
					EffectiveRange * RangedStandoffFraction - DistanceToTarget, 0.0f);

				AIController->MoveToLocation(GetActorLocation() + Away * Backpedal, 40.0f);
			}
		}

		return;
	}

	// Full speed once there is something to fight - the escort walk is only for
	// trailing the party.
	SetMovementSpeedOverride(0.0f);

	// A ranged fighter stops at its preferred standoff; a melee one still closes
	// to half its reach exactly as before, because for melee EffectiveRange and
	// AttackRange are the same number.
	const float StopDistance = bIsRanged
		? EffectiveRange * RangedStandoffFraction
		: EffectiveRange * 0.5f;

	if (AIController && !bIsChasing)
	{
		// Requires a NavMeshBoundsVolume covering the room; without one this is
		// a no-op and the enemy will stand still.
		AIController->MoveToActor(CurrentTarget, StopDistance);
		bIsChasing = true;
	}
	else if (AIController && bIsChasing)
	{
		// Re-issue when the path finished but we are still out of range, which
		// happens whenever the target keeps moving.
		if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			AIController->MoveToActor(CurrentTarget, StopDistance);
		}
	}
}

void ADCCEnemyCharacter::TickPatrol(AAIController* AIController)
{
	if (!AIController || PatrolPoints.IsEmpty())
	{
		return;
	}

	const bool bMovingNow = AIController->GetMoveStatus() != EPathFollowingStatus::Idle;
	if (bMovingNow)
	{
		bWasPatrolMoving = true;
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// The move that just finished this frame: start the pause clock rather
	// than immediately setting off again. Distinguishing "just arrived" from
	// "still resting from a previous arrival" is the whole reason this is a
	// bool rather than just checking GetMoveStatus() == Idle - that alone is
	// also true before the very first move ever starts.
	if (bWasPatrolMoving)
	{
		bWasPatrolMoving = false;
		PatrolWaitUntil = Now + PatrolWaitSeconds;
		return;
	}

	if (Now < PatrolWaitUntil)
	{
		return;
	}

	CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();
	AIController->MoveToLocation(PatrolPoints[CurrentPatrolIndex], PatrolPointTolerance);
}

void ADCCEnemyCharacter::HandleNearbyAttack(ADCCCharacterBase* Attacker, ADCCCharacterBase* Target)
{
	// HasAuthority() is implicit - this is only ever bound from the
	// server-only branch of BeginPlay, and OnAnyAttackAttempted is only ever
	// broadcast from the server side of PerformAttack.
	if (!bPassiveUntilProvoked || CurrentTarget || !IsAlive() || !Attacker || Attacker == this)
	{
		return;
	}

	// Only react to violence close enough to have actually seen or heard it -
	// otherwise every passive guard on the floor piles onto a fight three
	// rooms away the instant anyone swings anywhere.
	const bool bNearAttacker =
		FVector::DistSquared(GetActorLocation(), Attacker->GetActorLocation()) <= FMath::Square(ViolenceWatchRadius);
	const bool bNearTarget = Target &&
		FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(ViolenceWatchRadius);

	if (!bNearAttacker && !bNearTarget)
	{
		return;
	}

	// Always the attacker, never the target - the guard is breaking up a
	// fight, not picking a side in whatever it was about.
	CurrentTarget = Attacker;
	bIsChasing = false;
}

void ADCCEnemyCharacter::OnDamageDealt(ADCCCharacterBase* Target, int32 Damage)
{
	Super::OnDamageDealt(Target, Damage);

	if (!HasAuthority() || GoldStolenPerHit <= 0 || !Target || !Target->Stats)
	{
		return;
	}

	const int32 Taken = Target->Stats->StealGold(GoldStolenPerHit);
	if (Taken <= 0)
	{
		// A party with nothing left to take starves the boss. That is a real and
		// intentional outcome, not a failure worth logging every swing.
		return;
	}

	// Resolved lazily rather than at spawn: waves summon minions without knowing
	// which spawn was flagged as the boss, so the first bite is what looks it up.
	if (!LeechBeneficiary)
	{
		if (const ADCCEncounter* Encounter = Cast<ADCCEncounter>(GetOwner()))
		{
			LeechBeneficiary = Encounter->GetBoss();
		}
	}

	if (LeechBeneficiary && LeechBeneficiary->IsAlive() && LeechBeneficiary->Stats)
	{
		const int32 Healed = FMath::RoundToInt32(Taken * SummonerHealPerGold);
		if (Healed > 0)
		{
			LeechBeneficiary->Stats->Heal(Healed);
		}
	}

	ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
		NSLOCTEXT("DCC", "RepossessionNip", "{0} takes {1} gold from {2}. It does not go far."),
		FText::FromString(GetActorNameOrLabel()),
		FText::AsNumber(Taken),
		FText::FromString(Target->GetActorNameOrLabel())));
}
