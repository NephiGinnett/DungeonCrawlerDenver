#include "Characters/DCCEnemyCharacter_Voldemortgage.h"

#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Combat/DCCBossComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_Voldemortgage::ADCCEnemyCharacter_Voldemortgage()
{
	DisplayName = NSLOCTEXT("DCC", "VoldemortgageName", "Lord Voldemortgage");

	// Regenerated model, landed 2026-08-21 (SK_Voldemortgage2) - a second Meshy
	// pass on the same character, swapped in over the first import. Scaled up
	// regardless of which mesh this is: he floats above the pool per the floor
	// design, and should read as the biggest thing in the room.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BossMesh(
		TEXT("/Game/DCC/Characters/Voldemortgage/SK_Voldemortgage2.SK_Voldemortgage2"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (BossMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(BossMesh.Object);
		}

		MeshComponent->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));
	}

	// Str 3 / Dex 3 deliberate: never physically dangerous, never pretends to
	// be. Con 7 buys the fight enough length for Opening Statement -> Late
	// Notice -> Balloon Payment -> Aftermath to actually play out. Int 8 pays
	// for summoning eight Debt Eaters across the fight (3 then 5).
	if (Stats)
	{
		Stats->Level = 4;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 8);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 7);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 6);
	}

	WeaponDamageDie = 8;
	AttackCooldownSeconds = 2.2f;
	AttackRange = 1100.0f;
	AggroRadius = 3000.0f;

	Boss = CreateDefaultSubobject<UDCCBossComponent>(TEXT("Boss"));

	// Per-charge tuning shared by all three acts of the Balloon Payment below -
	// ChargeHealthThreshold/ChargeDurationSeconds are NOT set here on purpose:
	// this fight uses PhaseThresholds (a three-charge escalation) instead of the
	// single-threshold fallback, and PhaseThresholds overrides both once set.
	// See "THE BALLOON PAYMENT" further down for where those two are actually
	// decided - setting them here too would just be a value the component never
	// reads, which is exactly the dead-write confusion this comment replaced.
	Boss->ChargeBaseDamage = 6;
	Boss->ChargeDamagePerMinion = 8;
	Boss->VulnerableSecondsAfterCharge = 6.0f;
	Boss->LienGold = 25;

	// Straightening his tie and beginning the charge in the same breath - one of
	// his own barks, since it already fits the escalation exactly.
	Boss->ChargeStartLine = NSLOCTEXT("DCC", "VoldemortgageChargeStart",
		"Lord Voldemortgage straightens his tie. \"This is a courtesy contact. The next one will not be.\"");
	Boss->ChargeLandedLine = NSLOCTEXT("DCC", "VoldemortgageChargeLanded",
		"The balance comes due, in full, immediately.");
	Boss->ChargeFizzledLine = NSLOCTEXT("DCC", "VoldemortgageFizzle",
		"Minimum Payment received. Lord Voldemortgage notes that this will not be forgotten.");
	Boss->LienLine = NSLOCTEXT("DCC", "VoldemortgageLien",
		"The debt does not close on death. It transfers.");

	// The remaining barks - "Let's talk about your... payment history.",
	// "I have your file. I have everyone's file.", "You are not in trouble. You
	// are in arrears. There is a difference and it is worse." - are written down
	// in Docs/DT_DCCEnemies.json for whoever wires up idle chatter or the AI
	// narrator pass. Not implemented here: there is no idle-bark system in the
	// project yet, and adding one was not asked for.

	// NOT DCCCrawlerAnimSet::Apply - that set is authored against
	// SK_DCCCrawler_Skeleton, and SK_Voldemortgage2 has its own skeleton
	// (SK_Voldemortgage2_Skeleton, auto-created on import). Assigning a
	// shared-skeleton clip to a mismatched skeleton is exactly the
	// SkeletonToCompactPose.IsValidIndex ensure hit documented on
	// DCCCrawlerAnimSet.h.
	//
	// WalkAnimation/RunAnimation/HitReactionAnimation are deliberately UNSET
	// here, unlike the first import. SK_Voldemortgage2 came out of a different
	// rigging tool (its armature is named "UniRigArmature", not Meshy's usual
	// "Armature") and shipped with no companion animation file at all, so
	// A_Voldemortgage_Walking/Running/HitReact - authored against the FIRST
	// skeleton - would just get silently refused ("Wrong rig", the same log
	// line the shared-skeleton mismatch produces) rather than play. Same
	// accepted cosmetic gap as Attack/Cast/Climb/FistBump below: idle pose
	// instead of a walk cycle until a real clip exists for this skeleton.

	// Set here rather than on a Blueprint because this class has none - so unlike
	// the ally characters, a C++ loadout is the authoritative one and actually
	// reaches the spawned actor.
	//
	// Both are Intelligence spells he cannot be talked out of: House Rules opens
	// a crawler's file and makes them take more damage, Closing Time is the
	// room-wide hit that pairs with his charge. Together they give the fight a
	// caster's rhythm instead of a boss who only ever walks over and swings.
	if (Skills)
	{
		Skills->StartingSkillIds = { TEXT("HouseRules"), TEXT("ClosingTime") };
		Skills->StartingSkillRank = 3;
	}

	// ------------------------------------------------------------------
	// THE BALLOON PAYMENT - three acts
	//
	// 75%: Opening Statement. One balloon, a trickle of collectors, mostly
	//      a lesson in what the mechanic IS.
	// 45%: Late Notice. Same shape, thicker swarm, and by now the party knows
	//      it has to choose between the adds and the channel.
	// 20%: Final Demand. Starts with the floor already busy.
	//
	// Ascending health thresholds are read in order, so they must be listed
	// highest first - the component takes the next unused one each time.
	// ------------------------------------------------------------------
	Boss->PhaseThresholds = { 0.75f, 0.45f, 0.20f };

	// Shorter than the old single nine-second charge, because there are three of
	// them now and the fight should not become mostly waiting.
	Boss->ChargeDurationSeconds = 7.0f;

	Boss->ChargeLiftHeight = 650.0f;
	Boss->ChargeDamageResistance = 0.6f;
	Boss->BalloonMinScale = 0.5f;
	Boss->BalloonMaxScale = 6.0f;

	Boss->MinionClass = ADCCEnemyCharacter_DebtEater::StaticClass();
	Boss->MinionSpawnInterval = 2.2f;
	Boss->MinionsPerPhaseBonus = 1;
	Boss->MinionSpawnRadius = 800.0f;

	// A sphere for now - swap the mesh for real art when it exists. Collision off
	// so it never blocks a shot the player thought was going to land on him.
	BalloonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BalloonMesh"));
	BalloonMesh->SetupAttachment(GetRootComponent());
	BalloonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BalloonBaseHeight));
	BalloonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BalloonMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		BalloonMesh->SetStaticMesh(Sphere.Object);
	}

	// Sits above the balloon's own base height - TickBalloon-equivalent logic
	// in Tick() below re-lifts it to clear the balloon as it grows, same as
	// BalloonMesh's own relative-location update does for itself.
	ChannelingLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChannelingLabel"));
	ChannelingLabel->SetupAttachment(GetRootComponent());
	ChannelingLabel->SetRelativeLocation(FVector(0.0f, 0.0f, BalloonBaseHeight + 120.0f));
	ChannelingLabel->SetHorizontalAlignment(EHTA_Center);
	ChannelingLabel->SetVerticalAlignment(EVRTA_TextBottom);
	ChannelingLabel->SetWorldSize(48.0f);
	ChannelingLabel->SetTextRenderColor(FColor(255, 90, 80));
	ChannelingLabel->SetText(FText::GetEmpty());
	ChannelingLabel->SetVisibility(false);
}

void ADCCEnemyCharacter_Voldemortgage::BeginPlay()
{
	Super::BeginPlay();

	// He floats above the pool per the floor design - nothing was actually
	// making that true before this. On the default ground-based movement mode,
	// gravity pulled him down every tick while the AI/nav projection tried to
	// put him back on a walkable surface, which is exactly what reads as
	// "bouncing" rather than floating. Flying with zero gravity is the whole
	// fix: nothing left to fight.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Flying);
		Movement->GravityScale = 0.0f;
		Movement->MaxFlySpeed = 300.0f;
		Movement->BrakingDecelerationFlying = 2000.0f;
	}
}

void ADCCEnemyCharacter_Voldemortgage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!BalloonMesh || !Boss)
	{
		return;
	}

	// Driven off the replicated ChargeProgress, so the balloon inflates on every
	// client rather than only on the server that owns the timer.
	const bool bCharging = Boss->IsCharging();

	BalloonMesh->SetVisibility(bCharging);

	float BalloonTopHeight = BalloonBaseHeight;

	if (bCharging)
	{
		const float Scale = FMath::Lerp(
			Boss->BalloonMinScale, Boss->BalloonMaxScale, Boss->GetChargeProgress());

		BalloonMesh->SetRelativeScale3D(FVector(Scale));

		// Rises with its own size so it never clips down through his head as it
		// grows.
		BalloonTopHeight = BalloonBaseHeight + Scale * 40.0f;
		BalloonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BalloonTopHeight));
	}

	if (ChannelingLabel)
	{
		ChannelingLabel->SetVisibility(bCharging);

		if (bCharging)
		{
			// Default sphere radius is 50 units, so half the current scale is
			// roughly the balloon's own top edge - clearing it the same way
			// BalloonMesh clears his head above.
			ChannelingLabel->SetRelativeLocation(
				FVector(0.0f, 0.0f, BalloonTopHeight + BalloonMesh->GetRelativeScale3D().Z * 50.0f + 30.0f));

			ChannelingLabel->SetText(Boss->CurrentPhase <= 1
				? NSLOCTEXT("DCC", "VoldemortgageChanneling1", "CHANNELING - BALLOON PAYMENT")
				: NSLOCTEXT("DCC", "VoldemortgageChanneling2", "CHANNELING - BALLOON PAYMENT (AGAIN)"));
		}
	}
}
