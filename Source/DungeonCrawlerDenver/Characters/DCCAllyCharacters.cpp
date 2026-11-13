#include "Characters/DCCAllyCharacters.h"

#include "Characters/DCCCrawlerAnimSet.h"
#include "Components/SkeletalMeshComponent.h"
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
