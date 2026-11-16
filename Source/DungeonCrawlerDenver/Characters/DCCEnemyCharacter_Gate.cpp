#include "Characters/DCCEnemyCharacter_Gate.h"

#include "Characters/DCCForgeBody.h"
#include "Combat/DCCBossComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stats/DCCStatsComponent.h"

namespace
{
	/** The character-shade overlay (it multiplies what is behind it), pushed toward a colour: a gate's touch. */
	void TintBody(ADCCCharacterBase* Character, const FLinearColor& Tint, float Rim)
	{
		USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
		UMaterialInterface* Shade = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DCC/Materials/M_CharacterShade.M_CharacterShade"));
		if (!Body || !Shade)
		{
			return;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Shade, Character);
		Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Tint.R, Tint.G, Tint.B, 1.0f));
		Mid->SetVectorParameterValue(TEXT("Edge"), FLinearColor::White);
		Mid->SetScalarParameterValue(TEXT("RimStrength"), Rim);
		Body->SetOverlayMaterial(Mid);
	}

	/** A light that rides along on a character. */
	void Aura(ADCCCharacterBase* Character, const FLinearColor& Colour, float Intensity, float Radius, float Height)
	{
		if (!Character || !Character->GetMesh())
		{
			return;
		}
		UPointLightComponent* Glow = NewObject<UPointLightComponent>(Character);
		Glow->SetupAttachment(Character->GetRootComponent());
		Glow->SetRelativeLocation(FVector(0, 0, Height));
		Glow->SetLightColor(Colour);
		Glow->SetIntensity(Intensity);
		Glow->SetAttenuationRadius(Radius);
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
	}
}

// ---- the Gatekeeper ------------------------------------------------------------------------------------------------

ADCCEnemyCharacter_Gatekeeper::ADCCEnemyCharacter_Gatekeeper()
{
	DisplayName = NSLOCTEXT("DCC", "GatekeeperName", "The Gatekeeper");
	// A worker from the Forge, half again as big as anyone, in the gate's colour.
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(10), 1.5f);
	if (Stats)
	{
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 9);
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 7);
	}
	WeaponDamageDie = 8;
	AttackRange = 280.0f;
	AttackCooldownSeconds = 1.9f;
	AggroRadius = 2600.0f;

	Boss = CreateDefaultSubobject<UDCCBossComponent>(TEXT("Boss"));
	Boss->ChargeHealthThreshold = 0.5f;
	Boss->ChargeDurationSeconds = 6.0f;
	Boss->ChargeBaseDamage = 5;
	Boss->ChargeDamagePerMinion = 4;
	Boss->VulnerableSecondsAfterCharge = 6.0f;
	Boss->LienGold = 25;
	Boss->ChargeStartLine = NSLOCTEXT("DCC", "GatekeeperCharge", "The Gatekeeper raises both hands. The gate is listening to it. Hit it before it finishes.");
	Boss->ChargeLandedLine = NSLOCTEXT("DCC", "GatekeeperChargeLanded", "The gate answers. Everything in the room feels it.");
	Boss->ChargeFizzledLine = NSLOCTEXT("DCC", "GatekeeperFizzle", "Interrupted. The gate is not used to being interrupted.");
	Boss->LienLine = NSLOCTEXT("DCC", "GatekeeperLien", "The gate keeps a little of everyone who passes.");
}

void ADCCEnemyCharacter_Gatekeeper::BeginPlay()
{
	Super::BeginPlay();
	TintBody(this, FLinearColor(0.72f, 0.5f, 1.0f), 0.6f);
	Aura(this, FLinearColor(0.6f, 0.35f, 1.0f), 5000.0f, 650.0f, 120.0f);
}

// ---- a Champion ----------------------------------------------------------------------------------------------------

ADCCEnemyCharacter_GateChampion::ADCCEnemyCharacter_GateChampion()
{
	DisplayName = NSLOCTEXT("DCC", "ChampionName", "The Champion");
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(7), 1.2f);
	if (Stats)
	{
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 7);
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 5);
	}
	WeaponDamageDie = 8;
	AttackRange = 230.0f;
	AttackCooldownSeconds = 1.4f;
	AggroRadius = 2200.0f;
}

void ADCCEnemyCharacter_GateChampion::BeginPlay()
{
	Super::BeginPlay();
	TintBody(this, FLinearColor(1.0f, 0.82f, 0.45f), 0.55f);
	Aura(this, FLinearColor(1.0f, 0.75f, 0.3f), 3000.0f, 450.0f, 140.0f);
}

FString ADCCEnemyCharacter_GateChampion::RollName(int32 Seed)
{
	// Champions of the gate: whoever came through before you and decided to stay.
	static const TCHAR* First[] = {
		TEXT("Dale"), TEXT("Marisol"), TEXT("Big Terry"), TEXT("Wendell"), TEXT("Priya"), TEXT("Gus"), TEXT("Rhonda"), TEXT("Two-Hat Ernie"),
	};
	static const TCHAR* Title[] = {
		TEXT("Who Stayed"), TEXT("of the Last Gate"), TEXT("Undefeated Since Tuesday"), TEXT("the Rank-Climber"),
		TEXT("Employee of the Month"), TEXT("Still Waiting for a Ride"), TEXT("the Unrefunded"), TEXT("of Aisle Nine"),
	};
	FRandomStream R(Seed);
	return FString::Printf(TEXT("%s %s"), First[R.RandRange(0, UE_ARRAY_COUNT(First) - 1)], Title[R.RandRange(0, UE_ARRAY_COUNT(Title) - 1)]);
}

// ---- Gate Shades ---------------------------------------------------------------------------------------------------

ADCCEnemyCharacter_GateShade::ADCCEnemyCharacter_GateShade()
{
	DisplayName = NSLOCTEXT("DCC", "GateShadeName", "Gate Shade");
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(9), 0.92f);
	if (Stats)
	{
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 5);
	}
	WeaponDamageDie = 4;
	AttackRange = 170.0f;
	AttackCooldownSeconds = 1.1f;
	AggroRadius = 2400.0f;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = 520.0f;
	}
}

void ADCCEnemyCharacter_GateShade::BeginPlay()
{
	Super::BeginPlay();
	// See-through violet, in the same hologram material as Dead Air's ghosts, but the gate's colour.
	USkeletalMeshComponent* Body = GetMesh();
	UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cyberpunk_City/Materials/Master/M_hologram.M_hologram"));
	if (Body && Holo)
	{
		UMaterialInstanceDynamic* Spirit = UMaterialInstanceDynamic::Create(Holo, this);
		Spirit->SetVectorParameterValue(TEXT("Emissive Color"), FLinearColor(0.75f, 0.35f, 1.0f, 1.0f));
		Spirit->SetScalarParameterValue(TEXT("Emissive Intensity"), 9.0f);
		Spirit->SetScalarParameterValue(TEXT("Tiling Hologram"), 10.0f);
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			Body->SetMaterial(Slot, Spirit);
		}
		// Something must sit in the overlay slot, or the arena's own shade would be put on top of the glow.
		TintBody(this, FLinearColor(1.0f, 1.0f, 1.0f), 0.0f);
	}
	Aura(this, FLinearColor(0.55f, 0.3f, 1.0f), 900.0f, 280.0f, 100.0f);
}

void ADCCEnemyCharacter_GateShade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// A slow shimmer, so a crowd of them does not pulse in step.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (UMaterialInstanceDynamic* Spirit = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(0)))
		{
			const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			Spirit->SetScalarParameterValue(TEXT("Emissive Intensity"), 8.0f + 3.0f * FMath::Sin(T * 2.3f + GetUniqueID() * 0.7f));
		}
	}
}
