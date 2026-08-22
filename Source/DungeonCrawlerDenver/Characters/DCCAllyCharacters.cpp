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
}

ADCCAlly_OfficeAirplane::ADCCAlly_OfficeAirplane()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Game/DCC/Characters/OfficeAirplane/SK_OfficeAirplane_Shared.SK_OfficeAirplane_Shared"));
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
		TEXT("/Game/DCC/Characters/ArmsOut/SK_ArmsOut_Shared.SK_ArmsOut_Shared"));
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
		TEXT("/Game/DCC/Characters/DiceWinged/SK_DiceWinged_Shared.SK_DiceWinged_Shared"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (MeshAsset.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(MeshAsset.Object);
		}
	}

	DCCCrawlerAnimSet::Apply(*this);
}
