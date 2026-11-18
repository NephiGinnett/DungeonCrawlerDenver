#include "Characters/DCCAllyCharacters.h"

#include "Characters/DCCCrawlerAnimSet.h"
#include "Characters/DCCCreatureLook.h"
#include "Arcade/DCCSummons.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"

ADCCAlly_HockeyWings::ADCCAlly_HockeyWings()
{
	// No mesh override needed - SK_HockeyWings was already on the shared
	// skeleton before this session's consolidation even started.
	DCCCrawlerAnimSet::Apply(*this);

	// CJ is the party's designated healer, per Zoe's own call - a caster is
	// the natural pick, and this is what makes "the one with the mana pool
	// also knows first aid" a real, checkable fact rather than a headcanon.
	bHasFirstAid = true;
}

ADCCAlly_OfficeAirplane::ADCCAlly_OfficeAirplane()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Game/DCC/Characters/OfficeAirplane/SK_OfficeAirplane_Rebound.SK_OfficeAirplane_Rebound"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (MeshAsset.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(MeshAsset.Object);
		}
	}

	DCCCrawlerAnimSet::Apply(*this);
}

ADCCAlly_ArmsOut::ADCCAlly_ArmsOut()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Game/DCC/Characters/ArmsOut/SK_ArmsOut_Rebound.SK_ArmsOut_Rebound"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (MeshAsset.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(MeshAsset.Object);
		}
	}

	DCCCrawlerAnimSet::Apply(*this);
}

ADCCAlly_DiceWinged::ADCCAlly_DiceWinged()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Game/DCC/Characters/DiceWinged/SK_DiceWinged_Rebound.SK_DiceWinged_Rebound"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (MeshAsset.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(MeshAsset.Object);
		}
	}

	DCCCrawlerAnimSet::Apply(*this);
}

ADCCAlly_Vaughn::ADCCAlly_Vaughn()
{
	// _Shared, like the other three below - see this class's header comment
	// on why the original "own skeleton, Compatible-marked" plan was replaced.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Game/DCC/Characters/Vaughn/SK_Vaughn_Rebound.SK_Vaughn_Rebound"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (MeshAsset.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(MeshAsset.Object);
		}
	}

	// Same shared skeleton as ArmsOut/OfficeAirplane/DiceWinged now, so this
	// reaches him for real - confirmed, not the speculative call it used to be.
	DCCCrawlerAnimSet::Apply(*this);
}

// ---- Summoned from the Den's tubes (2026-11-17) ----------------------------------------------------------------------

ADCCSummonedAlly::ADCCSummonedAlly()
{
	// Set up like Gate Run's code-spawned creatures (DCCForgeBody): feet on the floor, facing forward, and the crawler
	// clips played one at a time. Without single-clip mode it stood in a T-pose.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Body->SetRelativeLocation(FVector(0.0f, 0.0f, -Capsule->GetUnscaledCapsuleHalfHeight()));
		}
	}
	DCCCrawlerAnimSet::Apply(*this);
	bIsAlly = true;
	bReplicates = true;
}

void ADCCSummonedAlly::BeginPlay()
{
	Super::BeginPlay();
	ApplySummonLook();
}

void ADCCSummonedAlly::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCSummonedAlly, Friend);
	DOREPLIFETIME(ADCCSummonedAlly, Tier);
	DOREPLIFETIME(ADCCSummonedAlly, Variant);
	DOREPLIFETIME(ADCCSummonedAlly, SummonerName);
}

void ADCCSummonedAlly::OnRep_Summon()
{
	ApplySummonLook();
}

void ADCCSummonedAlly::ApplySummonLook()
{
	if (bSummonLookApplied || Friend.IsNone())
	{
		return;
	}
	bSummonLookApplied = true;
	// The regular model with an overlay on top (Zoe: "just the regular models but with a shiny overlay"). A Shiny
	// wears a glowing shimmer in its rarity's colour; a Shadow goes near black with lit edges and a violet light. Every
	// other ally carries a faint light of its rarity, so you can tell whose side it is on in the dark (Dead Air).
	const FLinearColor Colour = DCCSummons::TierColour(Tier);
	if (Variant == DCCSummons::Shiny)
	{
		// Louder (2026-11-17, Zoe: "make the shiny variant more pronounced"): a bright shimmer, a strong light in the
		// rarity's colour, and a halo of white-gold light over the head.
		DCCCreatureLook::Shimmer(this, FMath::Lerp(Colour, FLinearColor(1.0f, 0.95f, 0.7f), 0.5f), 14.0f);
		DCCCreatureLook::Aura(this, Colour, 9000.0f, 520.0f, 110.0f);
		DCCCreatureLook::Aura(this, FLinearColor(1.0f, 0.92f, 0.65f), 5000.0f, 260.0f, 230.0f);
	}
	else if (Variant == DCCSummons::Shadow)
	{
		DCCCreatureLook::Tint(this, FLinearColor(0.03f, 0.015f, 0.06f), FLinearColor::White, 0.55f);
		DCCCreatureLook::Aura(this, FLinearColor(0.55f, 0.2f, 1.0f), 1800.0f, 320.0f, 110.0f);
	}
	else if (Tier > 0)
	{
		DCCCreatureLook::Aura(this, Colour, 700.0f, 220.0f, 110.0f);
	}
}
