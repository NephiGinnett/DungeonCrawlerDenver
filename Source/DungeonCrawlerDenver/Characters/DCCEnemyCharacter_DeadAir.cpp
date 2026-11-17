#include "Characters/DCCEnemyCharacter_DeadAir.h"

#include "Arcade/DCCArcadeHaunt.h"
#include "Characters/DCCForgeBody.h"
#include "Characters/DCCCreatureLook.h"
#include "Animation/AnimSequence.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stats/DCCStatsComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	ADCCArcadeHaunt* FindMine(const UWorld* World)
	{
		for (TActorIterator<ADCCArcadeHaunt> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	void Scores(ADCCCharacterBase* Character, int32 Str, int32 Con, int32 Dex)
	{
		if (UDCCStatsComponent* Stats = Character ? Character->Stats.Get() : nullptr)
		{
			Stats->AbilityScores.SetScore(EDCCAbility::Strength, Str);
			Stats->AbilityScores.SetScore(EDCCAbility::Constitution, Con);
			Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, Dex);
		}
	}

	void Speed(ADCCCharacterBase* Character, float Walk)
	{
		if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
		{
			Movement->MaxWalkSpeed = Walk;
		}
	}
}

// ---- the lamp-eater ------------------------------------------------------------------------------------------------

ADCCEnemyCharacter_LampEater::ADCCEnemyCharacter_LampEater()
{
	DisplayName = NSLOCTEXT("DCC", "LampEaterName", "Lamp-eater");
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(4), 1.05f);
	Scores(this, 4, 4, 3);
	WeaponDamageDie = 6;
	AttackRange = 190.0f;
	AttackCooldownSeconds = 1.5f;
	AggroRadius = 2600.0f; // it sees a lamp from a long way off
	Speed(this, 300.0f);   // a creep, not a run: you can always walk away from it
}

void ADCCEnemyCharacter_LampEater::BeginPlay()
{
	Super::BeginPlay();
	DCCCreatureLook::Hologram(this, FLinearColor(0.32f, 0.08f, 0.6f, 1.0f), 5.0f);
	DCCCreatureLook::Aura(this, FLinearColor(0.4f, 0.15f, 0.9f), 500.0f, 260.0f, 110.0f);
}

void ADCCEnemyCharacter_LampEater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Brighter when it is hunting, nearly gone when it has lost you.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (UMaterialInstanceDynamic* Spirit = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(0)))
		{
			const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const float Base = GetCurrentTarget() ? 6.0f : 1.5f;
			Spirit->SetScalarParameterValue(TEXT("Emissive Intensity"), Base + 1.5f * FMath::Sin(T * 3.1f + GetUniqueID()));
		}
	}
}

bool ADCCEnemyCharacter_LampEater::AcceptsTarget(const ADCCCharacterBase* Candidate) const
{
	// It only eats light: crawlers with their lamp on. Anyone else (and anyone in the dark) it does not see.
	const APlayerState* State = Candidate ? Candidate->GetPlayerState() : nullptr;
	if (!State)
	{
		return false;
	}
	if (!Mine.IsValid())
	{
		Mine = FindMine(GetWorld());
	}
	return !Mine.IsValid() || !Mine->IsLampOff(State->GetPlayerName());
}

// ---- the Tommyknocker ----------------------------------------------------------------------------------------------

ADCCEnemyCharacter_Tommyknocker::ADCCEnemyCharacter_Tommyknocker()
{
	DisplayName = NSLOCTEXT("DCC", "TommyknockerName", "Tommyknocker");
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(7), 0.55f);
	Scores(this, 2, 2, 4);
	WeaponDamageDie = 4;
	AttackRange = 150.0f;
	AttackCooldownSeconds = 1.3f;
	AggroRadius = 0.0f;
	// It never comes for you: it stays where it was put and brings the roof down instead. Hit it and it hits back.
	bPassiveUntilProvoked = true;
	Speed(this, 140.0f);
}

void ADCCEnemyCharacter_Tommyknocker::BeginPlay()
{
	Super::BeginPlay();
	DCCCreatureLook::Tint(this, FLinearColor(0.55f, 0.42f, 0.3f), FLinearColor(1.0f, 0.8f, 0.5f), 0.35f);
	DCCCreatureLook::Aura(this, FLinearColor(1.0f, 0.7f, 0.35f), 220.0f, 160.0f, 60.0f); // a candle stub: you glimpse it in your lamp
	NextKnockAt = GetWorld() ? GetWorld()->GetTimeSeconds() + FMath::FRandRange(8.0f, 16.0f) : 0.0f;
}

void ADCCEnemyCharacter_Tommyknocker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || !IsAlive())
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now < NextKnockAt)
	{
		return;
	}
	NextKnockAt = Now + FMath::FRandRange(18.0f, 26.0f); // the mine drops its own rocks too; the two together were a lot
	if (!Mine.IsValid())
	{
		Mine = FindMine(World);
	}
	if (!Mine.IsValid())
	{
		return;
	}
	// The nearest crawler it can hear, within a room or two.
	ADCCCharacterBase* Mark = nullptr;
	float Best = FMath::Square(1700.0f);
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Crawler = *It;
		if (!Crawler || !Crawler->IsAlive() || !Cast<APlayerController>(Crawler->GetController()))
		{
			continue;
		}
		const float D = FVector::DistSquared(Crawler->GetActorLocation(), GetActorLocation());
		if (D < Best)
		{
			Best = D;
			Mark = Crawler;
		}
	}
	if (Mark && Mine->QueueRock(Mark->GetActorLocation(), 3.5))
	{
		SetActorRotation(FRotator(0.0f, (Mark->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0f));
	}
}

// ---- slag dogs -----------------------------------------------------------------------------------------------------

ADCCEnemyCharacter_SlagDog::ADCCEnemyCharacter_SlagDog()
{
	DisplayName = NSLOCTEXT("DCC", "SlagDogName", "Slag Dog");
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SlimeMesh(TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Slime_SK.Slime_SK"));
	if (SlimeMesh.Succeeded() && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(SlimeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_Walk_ANIM.Slime_Walk_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Run(TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_Run_ANIM.Slime_Run_ANIM"));
	if (Walk.Succeeded()) { WalkAnimation = Walk.Object; }
	if (Run.Succeeded()) { RunAnimation = Run.Object; }
	// Balance (2026-11-16): three of them used to deal ~15 damage a second between them, a full health bar in eight
	// seconds. Now about 7, so a pack is a fight you can win or run from, not a sentence.
	Scores(this, 2, 2, 6);
	WeaponDamageDie = 4;
	AttackRange = 160.0f;
	AttackCooldownSeconds = 1.3f;
	AggroRadius = 2200.0f;
	Speed(this, 640.0f); // faster than a crawler walks: you fight these or you run hard
}

void ADCCEnemyCharacter_SlagDog::BeginPlay()
{
	Super::BeginPlay();
	// Soot-black, with a furnace glow along its edges.
	DCCCreatureLook::Tint(this, FLinearColor(0.08f, 0.06f, 0.05f), FLinearColor(1.0f, 0.42f, 0.08f), 1.0f);
	DCCCreatureLook::Aura(this, FLinearColor(1.0f, 0.4f, 0.1f), 900.0f, 240.0f, 50.0f);
}

// ---- the shift boss ------------------------------------------------------------------------------------------------

ADCCEnemyCharacter_ShiftBoss::ADCCEnemyCharacter_ShiftBoss()
{
	DisplayName = NSLOCTEXT("DCC", "ShiftBossName", "The Shift Boss");
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(10), 1.45f);
	Scores(this, 8, 10, 2);
	WeaponDamageDie = 10;
	AttackRange = 270.0f;
	AttackCooldownSeconds = 2.2f;
	AggroRadius = 2000.0f;
	Speed(this, 250.0f); // slow: the danger is being cornered by him, not caught
}

void ADCCEnemyCharacter_ShiftBoss::BeginPlay()
{
	Super::BeginPlay();
	DCCCreatureLook::Tint(this, FLinearColor(0.6f, 0.45f, 0.4f), FLinearColor(1.0f, 0.2f, 0.12f), 0.5f);
	// His lamp burns red: you see it coming down the drift before you see him.
	DCCCreatureLook::Aura(this, FLinearColor(1.0f, 0.12f, 0.06f), 6000.0f, 900.0f, 230.0f);
}
