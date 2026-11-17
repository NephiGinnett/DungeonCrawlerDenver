#include "Characters/DCCEnemyCharacter_Corporate.h"

#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Skills/DCCSkillTypes.h"
#include "Rooms/DCCEncounter.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Characters/DCCForgeBody.h"

// =========================================================================
// THE INTERN POOL
// =========================================================================

ADCCEnemyCharacter_InternManager::ADCCEnemyCharacter_InternManager()
{
	DisplayName = NSLOCTEXT("DCC", "InternManagerName", "Regional Intake Manager");

	// An NPC Forge person (2026-11-16): the old cartoon minion body looked like someone else's character.
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(2), 1.05f);

	if (Stats)
	{
		Stats->Level = 3;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 6);
	}

	// Personally harmless. Hangs back and lets the pipeline do the work, which
	// is the joke and also the mechanic - closing on it is the whole solution,
	// so it must never be the thing threatening you.
	WeaponDamageDie = 4;
	AttackCooldownSeconds = 3.5f;
	AttackRange = 200.0f;
	AggroRadius = 1800.0f;

	InternClass = ADCCEnemyCharacter_Intern::StaticClass();
}

void ADCCEnemyCharacter_InternManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(InternTimer, this,
			&ADCCEnemyCharacter_InternManager::SpawnIntern,
			FMath::Max(1.0f, InternIntervalSeconds), true);
	}
}

void ADCCEnemyCharacter_InternManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Killing the manager is the win condition, so the pipeline has to stop
	// with it. Clearing on EndPlay rather than on death specifically, so a
	// streamed-out or destroyed manager cannot leave a timer running.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InternTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void ADCCEnemyCharacter_InternManager::SpawnIntern()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !InternClass || !IsAlive())
	{
		return;
	}

	// Weak pointers, so anything already destroyed simply drops out of the
	// count without needing a death delegate per intern.
	LiveInterns.RemoveAll([](const TWeakObjectPtr<ADCCEnemyCharacter>& P)
	{
		return !P.IsValid() || !P->IsAlive();
	});

	if (LiveInterns.Num() >= FMath::Max(1, MaxLiveInterns))
	{
		return;
	}

	const FVector2D Offset = FMath::RandPointInCircle(InternSpawnRadius);
	const FVector SpawnLocation = GetActorLocation() + FVector(Offset.X, Offset.Y, 0.0f);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADCCEnemyCharacter* Intern = World->SpawnActor<ADCCEnemyCharacter>(
		InternClass, SpawnLocation, GetActorRotation(), Params);
	if (!Intern)
	{
		return;
	}

	LiveInterns.Add(Intern);

	// Without this the encounter never counts them and can report itself clear
	// with interns still standing - the exact Balloon Payment bug from the
	// 2026-08-22 pass.
	//
	// A direct owner lookup rather than a search, copying
	// UDCCBossComponent::GetOwningEncounter: the encounter sets itself as the
	// spawn Owner of everything in its waves. A manager placed by hand has no
	// owner, which is fine - its interns simply are not part of a clear count,
	// because there is no encounter to clear.
	if (ADCCEncounter* Encounter = Cast<ADCCEncounter>(GetOwner()))
	{
		Encounter->RegisterSpawnedEnemy(Intern);
	}
}

ADCCEnemyCharacter_Intern::ADCCEnemyCharacter_Intern()
{
	DisplayName = NSLOCTEXT("DCC", "InternName", "Unpaid Intern");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> InternMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Mushroom_SK.Mushroom_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (InternMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(InternMesh.Object);
		}
		MeshComponent->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.8f));
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Mushroom/AngryVersion/Mushroom_walkFWDAngry_ANIM.Mushroom_walkFWDAngry_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Mushroom/AngryVersion/Mushroom_runFWDAngry_ANIM.Mushroom_runFWDAngry_ANIM"));
	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded())  { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 1;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 1);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 1);
	}

	// Dies to almost anything. The cost of an intern is the second it takes to
	// kill it, repeated forever - not the danger.
	WeaponDamageDie = 3;
	AttackCooldownSeconds = 1.4f;
	AttackRange = 150.0f;
	AggroRadius = 1600.0f;
}

// =========================================================================
// THE NOTARY
// =========================================================================

ADCCEnemyCharacter_Notary::ADCCEnemyCharacter_Notary()
{
	DisplayName = NSLOCTEXT("DCC", "NotaryName", "The Notary");

	// An NPC Forge person (2026-11-16): the old cartoon minion body looked like someone else's character.
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(8), 1.0f);

	if (Stats)
	{
		Stats->Level = 4;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 3);
	}

	WeaponDamageDie = 8;
	AttackCooldownSeconds = 2.0f;
	AttackRange = 220.0f;
	AggroRadius = 1500.0f;
}

int32 ADCCEnemyCharacter_Notary::ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker)
{
	// No seal authored, or the seal is already broken: an ordinary enemy.
	if (!Seal || !Seal->IsAlive())
	{
		return Damage;
	}

	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastSealNoticeTime >= SealNoticeIntervalSeconds)
		{
			LastSealNoticeTime = Now;
			if (ADCCGameState* GameState = World->GetGameState<ADCCGameState>())
			{
				GameState->AddLogEntry(EDCCLogCategory::Combat,
					NSLOCTEXT("DCC", "NotarySealed",
						"The blow is witnessed, recorded, and disallowed. The Notary's seal is "
						"still intact - and the seal is not on the Notary."));
			}
		}
	}

	return 0;
}

// =========================================================================
// ESCALATION SPECIALIST
// =========================================================================

ADCCEnemyCharacter_EscalationSpecialist::ADCCEnemyCharacter_EscalationSpecialist()
{
	DisplayName = NSLOCTEXT("DCC", "EscalationName", "Escalation Specialist");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> EscMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Cactus_SK.Cactus_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (EscMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(EscMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Cactus/Cactus_WalkFWD_ANIM.Cactus_WalkFWD_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Cactus/Cactus_RunFWD_ANIM.Cactus_RunFWD_ANIM"));
	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded())  { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 4;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 5);
	}

	BaseWeaponDamageDie = 6;
	WeaponDamageDie = BaseWeaponDamageDie;
	AttackCooldownSeconds = 2.0f;
	AttackRange = 200.0f;
	AggroRadius = 1600.0f;
}

void ADCCEnemyCharacter_EscalationSpecialist::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	BaseWeaponDamageDie = WeaponDamageDie;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Bound to every crawler present at spawn. Deliberately not re-scanned
	// afterwards: a crawler who joins mid-fight not feeding the escalation is a
	// far smaller problem than a per-tick sweep over every character in the
	// level, and this enemy is authored into a room the party is already in.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Character = *It;
		if (!Character || !Character->IsCrawler())
		{
			continue;
		}

		if (UDCCSkillComponent* CrawlerSkills = Character->FindComponentByClass<UDCCSkillComponent>())
		{
			CrawlerSkills->OnSkillActivated.AddDynamic(
				this, &ADCCEnemyCharacter_EscalationSpecialist::HandleSkillActivated);
		}
	}
}

void ADCCEnemyCharacter_EscalationSpecialist::HandleSkillActivated(
	UDCCSkillComponent* CastingSkills, const FDCCSkillResult& Result)
{
	if (!HasAuthority() || !IsAlive() || Stacks >= MaxStacks)
	{
		return;
	}

	// A failed activation still counts. The rule the party has to learn is
	// "stop reaching for skills", not "stop landing them" - and a fizzle that
	// escalated nothing would teach the wrong lesson while still costing mana.
	if (!CastingSkills)
	{
		return;
	}

	if (const AActor* Caster = CastingSkills->GetOwner())
	{
		if (FVector::Dist(GetActorLocation(), Caster->GetActorLocation()) > EscalationRadius)
		{
			return;
		}
	}

	++Stacks;
	WeaponDamageDie = BaseWeaponDamageDie + DamagePerStack * Stacks;

	if (UWorld* World = GetWorld())
	{
		if (ADCCGameState* GameState = World->GetGameState<ADCCGameState>())
		{
			GameState->AddLogEntry(EDCCLogCategory::Combat, FText::Format(
				NSLOCTEXT("DCC", "EscalationStack",
					"The Escalation Specialist makes a note. This has now been escalated {0} "
					"time(s), and it is getting worse each time somebody escalates it."),
				FText::AsNumber(Stacks)));
		}
	}
}
