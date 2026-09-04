#include "Characters/DCCEnemyCharacter_Mine.h"

#include "Stats/DCCStatsComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

// =========================================================================
// LEGACY SYSTEM
// =========================================================================

ADCCEnemyCharacter_LegacySystem::ADCCEnemyCharacter_LegacySystem()
{
	DisplayName = NSLOCTEXT("DCC", "LegacySystemName", "Legacy System");

	// TurtleShell reused at scale - it already reads as armoured and slow, and
	// nothing else installed reads as "a machine nobody has maintained".
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ShellMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/TurtleShell_SK.TurtleShell_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (ShellMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(ShellMesh.Object);
		}
		// Visibly larger than anything else on the floor, so "do not try to
		// stab this" is communicated before the first swing bounces off.
		MeshComponent->SetRelativeScale3D(FVector(1.6f, 1.6f, 1.6f));
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/TurtleShell/TurtleShell_Walk_ANIM.TurtleShell_Walk_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/TurtleShell/TurtleShell_Run_ANIM.TurtleShell_Run_ANIM"));

	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded())  { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 4;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 2);
	}

	// Deliberately NOT a high Constitution wall. Its defence is the immunity,
	// not a health bar - once the party works out the answer it should die at
	// a reasonable pace rather than turning into a throwing marathon.
	WeaponDamageDie = 10;
	AttackCooldownSeconds = 3.0f;
	AttackRange = 220.0f;
	AggroRadius = 1300.0f;
}

int32 ADCCEnemyCharacter_LegacySystem::ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker)
{
	// Tell them why nothing is happening. A silent no-op here would read
	// exactly like the unset-AttackAnimation bug in CLAUDE.md - a mechanic
	// working perfectly and looking broken.
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastDeflectNoticeTime >= DeflectNoticeIntervalSeconds)
		{
			LastDeflectNoticeTime = Now;
			if (ADCCGameState* GameState = World->GetGameState<ADCCGameState>())
			{
				GameState->AddLogEntry(EDCCLogCategory::Combat,
					NSLOCTEXT("DCC", "LegacySystemDeflect",
						"The blow glances off the Legacy System without leaving a mark. It is not "
						"armoured so much as indifferent. Hitting it is not going to work - "
						"try throwing something at it."));
			}
		}
	}

	return 0;
}

// =========================================================================
// RECONCILIATION
// =========================================================================

ADCCEnemyCharacter_Reconciliation::ADCCEnemyCharacter_Reconciliation()
{
	DisplayName = NSLOCTEXT("DCC", "ReconciliationName", "Reconciliation");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BeholderMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Beholder_SK.Beholder_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (BeholderMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(BeholderMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Beholder/Beholder_WalkFWD_ANIM.Beholder_WalkFWD_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Beholder/Beholder_Run_ANIM.Beholder_Run_ANIM"));

	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded())  { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 4;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 3);
	}

	// Low Constitution on purpose: once the party solves the coordination
	// problem, the pair should fold fast. The difficulty is the trick, not the
	// grind - a tanky version of this would punish working it out.
	WeaponDamageDie = 6;
	AttackCooldownSeconds = 2.0f;
	AttackRange = 700.0f;
	AggroRadius = 1600.0f;
}

void ADCCEnemyCharacter_Reconciliation::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() || Twin)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Adopt the nearest unpaired sibling. Nearest rather than first-found so a
	// wave spawning four of them pairs them off sensibly by position instead of
	// by iteration order, which would cross-pair opposite corners of the room
	// and make "hit both at once" impossible rather than merely hard.
	ADCCEnemyCharacter_Reconciliation* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ADCCEnemyCharacter_Reconciliation> It(World); It; ++It)
	{
		ADCCEnemyCharacter_Reconciliation* Other = *It;
		if (!Other || Other == this || Other->Twin)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}

	if (Best)
	{
		// Symmetric, so whichever of the two runs BeginPlay second finds itself
		// already paired and does nothing.
		Twin = Best;
		Best->Twin = this;
	}
}

int32 ADCCEnemyCharacter_Reconciliation::ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker)
{
	if (!HasAuthority())
	{
		return Damage;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Damage;
	}

	const float Now = World->GetTimeSeconds();
	LastHitTime = Now;

	// A lone survivor of the pair has nothing to reconcile against and just
	// dies normally - otherwise killing the first one would make the second
	// unkillable, which is a bug rather than a mechanic.
	if (!Twin || !Twin->IsAlive())
	{
		return Damage;
	}

	// The twin was struck moments ago: the books do not balance and the damage
	// stands, on BOTH of them.
	if (Now - Twin->GetLastHitTime() <= SimultaneousWindowSeconds)
	{
		return Damage;
	}

	// Otherwise the entry is reversed and the loss is booked against the other
	// side of the ledger, which is to say: the twin heals for exactly what
	// this one just lost.
	HealTwin(Damage);

	if (ADCCGameState* GameState = World->GetGameState<ADCCGameState>())
	{
		GameState->AddLogEntry(EDCCLogCategory::Combat,
			NSLOCTEXT("DCC", "ReconciliationBalanced",
				"The wound closes as fast as it opens - the other one absorbs the entry and the books balance. "
				"They will have to be hit together."));
	}

	return 0;
}

void ADCCEnemyCharacter_Reconciliation::HealTwin(int32 Amount)
{
	if (!HasAuthority() || !Twin || Amount <= 0)
	{
		return;
	}

	if (UDCCStatsComponent* TwinStats = Twin->FindComponentByClass<UDCCStatsComponent>())
	{
		// Negative damage is this project's existing heal path (see the
		// consumable effect application in DCCCharacterBase); ApplyDamage
		// clamps at max HP, so this can never overheal.
		TwinStats->ApplyDamage(-Amount);
	}
}
