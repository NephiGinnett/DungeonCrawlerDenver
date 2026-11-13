#include "Progression/DCCCrowdCrawler.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Deliberately mundane. The joke in this setting is that the dungeon is full
	// of ordinary people from Denver who were having a normal week, so the crowd
	// should read as a queue at the DMV rather than as a fantasy tavern.
	static const TCHAR* CrowdFirstNames[] = {
		TEXT("Dale"), TEXT("Marisol"), TEXT("Trent"), TEXT("Bev"), TEXT("Omar"),
		TEXT("Kelli"), TEXT("Rich"), TEXT("Yolanda"), TEXT("Pete"), TEXT("Shauna"),
		TEXT("Curtis"), TEXT("Nadia"), TEXT("Doug"), TEXT("Priya"), TEXT("Wes"),
		TEXT("Loretta"), TEXT("Sam"), TEXT("Aggie"), TEXT("Hector"), TEXT("Jo")
	};

	// A crawler's handle, not their surname. The System AI assigns these and it
	// is not trying to be flattering.
	static const TCHAR* CrowdTags[] = {
		TEXT("the Punctual"), TEXT("of Accounts Payable"), TEXT("Unsponsored"),
		TEXT("Still Optimistic"), TEXT("Second Shift"), TEXT("No Fixed Abode"),
		TEXT("Provisional"), TEXT("of Lakewood"), TEXT("Recently Promoted"),
		TEXT("Under Review"), TEXT("the Reasonable"), TEXT("Pending"),
		TEXT("of Unit 4B"), TEXT("Formerly Retail"), TEXT("the Unremarkable")
	};
}

ADCCCrowdCrawler::ADCCCrowdCrawler()
{
	// Never ticks. A crowd that thinks is a crowd that costs, and this one has
	// nothing to think about.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Blocks movement so the party cannot walk through the crowd, but is
	// invisible to every combat and interaction query. Nothing that fights, aims
	// or talks should ever find one of these.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	// Single-node, like every other character in this project - there is no
	// AnimBlueprint for the shared skeleton.
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(Mesh);
	Nameplate->SetRelativeLocation(FVector(0.0f, 0.0f, 190.0f));
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetVerticalAlignment(EVRTA_TextCenter);
	Nameplate->SetWorldSize(16.0f);
	Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Nameplate->SetMobility(EComponentMobility::Movable);
	Nameplate->SetTextRenderColor(FColor(150, 160, 175));

	// Defaults so a hand-placed crowd member works with no configuration. All on
	// the shared crawler skeleton, so any of them can play any idle in the pool.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshA(
		TEXT("/Game/DCC/Characters/OfficeAirplane/SK_OfficeAirplane_Rebound.SK_OfficeAirplane_Rebound"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshB(
		TEXT("/Game/DCC/Characters/ArmsOut/SK_ArmsOut_Rebound.SK_ArmsOut_Rebound"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshC(
		TEXT("/Game/DCC/Characters/DiceWinged/SK_DiceWinged_Rebound.SK_DiceWinged_Rebound"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshD(
		TEXT("/Game/DCC/Characters/HockeyWings/SK_HockeyWings.SK_HockeyWings"));

	for (const auto* Finder : { &MeshA, &MeshB, &MeshC, &MeshD })
	{
		if (Finder->Succeeded())
		{
			MeshPool.Add(Finder->Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleA(
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_Alert.A_DCC_OA_Alert"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleB(
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_BoxingWarmup.A_DCC_OA_BoxingWarmup"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleC(
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AirSquat.A_DCC_AO_AirSquat"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleD(
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AgreeGesture.A_DCC_AO_AgreeGesture"));

	for (const auto* Finder : { &IdleA, &IdleB, &IdleC, &IdleD })
	{
		if (Finder->Succeeded())
		{
			IdlePool.Add(Finder->Object);
		}
	}
}

void ADCCCrowdCrawler::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCCrowdCrawler, Seed);
}

void ADCCCrowdCrawler::SetSeed(int32 NewSeed)
{
	Seed = NewSeed;
	ApplyAppearance();
}

void ADCCCrowdCrawler::OnRep_Seed()
{
	ApplyAppearance();
}

void ADCCCrowdCrawler::BeginPlay()
{
	Super::BeginPlay();

	// A hand-placed crowd member seeds from where it stands. That keeps it the
	// same person every launch, which a random seed would not - and a bar whose
	// regulars change identity on every restart is worse than one with none.
	if (Seed == 0)
	{
		const FVector Location = GetActorLocation();
		Seed = GetTypeHash(FIntVector(Location)) | 1;
	}

	ApplyAppearance();
}

FText ADCCCrowdCrawler::GenerateName(FRandomStream& Stream) const
{
	const int32 FirstCount = UE_ARRAY_COUNT(CrowdFirstNames);
	const int32 TagCount = UE_ARRAY_COUNT(CrowdTags);

	const FString First = CrowdFirstNames[Stream.RandRange(0, FirstCount - 1)];

	// Two thirds get a tag. Everyone having one would read as a naming scheme
	// rather than as the AI labelling people it finds mildly interesting.
	if (Stream.FRand() < 0.66f)
	{
		return FText::FromString(FString::Printf(TEXT("%s %s"),
			*First, CrowdTags[Stream.RandRange(0, TagCount - 1)]));
	}

	return FText::FromString(First);
}

void ADCCCrowdCrawler::ApplyAppearance()
{
	if (!Mesh || Seed == 0)
	{
		return;
	}

	// One stream, seeded once. Every cosmetic decision below draws from it in a
	// fixed order, which is what makes the same seed produce the same person on
	// every machine.
	FRandomStream Stream(Seed);

	if (!MeshPool.IsEmpty())
	{
		const int32 Index = Stream.RandRange(0, MeshPool.Num() - 1);
		if (MeshPool[Index])
		{
			Mesh->SetSkeletalMesh(MeshPool[Index]);
		}
	}

	// Height and build varied separately, so two crowd members sharing a mesh
	// still differ in silhouette - which is the thing the eye actually catches.
	const float Height = Stream.FRandRange(MinScale, MaxScale);
	const float Width = Height * Stream.FRandRange(0.94f, 1.06f);
	Mesh->SetRelativeScale3D(FVector(Width, Width, Height));

	// Facing. A crowd all pointing the same way reads as a chorus line.
	SetActorRotation(FRotator(0.0f, Stream.FRandRange(0.0f, 360.0f), 0.0f));

	if (!IdlePool.IsEmpty())
	{
		const int32 Index = Stream.RandRange(0, IdlePool.Num() - 1);
		if (UAnimationAsset* Idle = IdlePool[Index])
		{
			Mesh->PlayAnimation(Idle, /*bLooping=*/true);

			// Phase and speed offset. Without these, twenty people playing the
			// same loop breathe in perfect unison and the whole room reads as one
			// object - the single most common way a crowd gives itself away.
			Mesh->SetPlayRate(Stream.FRandRange(0.85f, 1.15f));
			Mesh->SetPosition(Stream.FRandRange(0.0f, 1.0f), /*bFireNotifies=*/false);
		}
	}

	// Tint, applied to every slot through dynamic instances. Small offsets only:
	// this should read as different clothing under the same bad lighting.
	if (TintStrength > 0.0f)
	{
		const FLinearColor Tint(
			1.0f + Stream.FRandRange(-TintStrength, TintStrength),
			1.0f + Stream.FRandRange(-TintStrength, TintStrength),
			1.0f + Stream.FRandRange(-TintStrength, TintStrength));

		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateAndSetMaterialInstanceDynamic(Slot))
			{
				// Named parameters that most imported materials will not have. A
				// missing parameter is a silent no-op rather than an error, so
				// this degrades to "no tint" rather than to a broken material.
				Dynamic->SetVectorParameterValue(TEXT("Tint"), Tint);
				Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Tint);
				Dynamic->SetVectorParameterValue(TEXT("Color"), Tint);
			}
		}
	}

	if (Nameplate)
	{
		Nameplate->SetText(GenerateName(Stream));
	}
}
