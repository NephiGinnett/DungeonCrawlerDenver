#include "Arcade/DCCSupplyChest.h"
#include "Items/DCCChestSound.h"

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCClimbItems.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

namespace
{
	UStaticMesh* ChestMesh(bool bOpen)
	{
		return LoadObject<UStaticMesh>(nullptr, bOpen
			? TEXT("/Game/UnrealBucket/Library/Various/Chest/SM_Chest_Open.SM_Chest_Open")
			: TEXT("/Game/UnrealBucket/Library/Various/Chest/SM_Chest.SM_Chest"));
	}

	/** Scales a mesh to Height cm tall, sitting on the actor's origin. */
	void FitMesh(UStaticMeshComponent* Component, UStaticMesh* Mesh, float Height)
	{
		if (!Component || !Mesh)
		{
			return;
		}
		Component->SetStaticMesh(Mesh);
		const FBox Bounds = Mesh->GetBoundingBox();
		const float Scale = Height / FMath::Max(1.0f, Bounds.GetSize().Z);
		Component->SetRelativeScale3D(FVector(Scale));
		Component->SetRelativeLocation(FVector(-Bounds.GetCenter().X * Scale, -Bounds.GetCenter().Y * Scale, -Bounds.Min.Z * Scale));
	}
}

ADCCSupplyChest::ADCCSupplyChest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = true;
	bAlwaysRelevant = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(170.0f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Trigger;

	Closed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Closed"));
	Closed->SetupAttachment(Trigger);
	Closed->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Open = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Open"));
	Open->SetupAttachment(Trigger);
	Open->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Open->SetVisibility(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Trigger);
	Glow->SetRelativeLocation(FVector(0, 0, 120));
	Glow->SetAttenuationRadius(450.0f);
	Glow->SetCastShadows(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetRelativeLocation(FVector(0, 0, 150));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(18.0f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADCCSupplyChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCSupplyChest, Kind);
	DOREPLIFETIME(ADCCSupplyChest, ArtifactId);
	DOREPLIFETIME(ADCCSupplyChest, LootedBy);
}

void ADCCSupplyChest::BeginPlay()
{
	Super::BeginPlay();
	const float Height = Kind == EDCCChestKind::Outfitter ? 130.0f : 85.0f;
	FitMesh(Closed, ChestMesh(false), Height);
	FitMesh(Open, ChestMesh(true), Height);
	if (!Closed->GetStaticMesh())
	{
		// No chest pack: a plain box still reads as "a thing to open".
		FitMesh(Closed, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")), 70.0f);
	}

	const DCCClimbItems::FItem* Artifact = Kind == EDCCChestKind::Artifact ? DCCClimbItems::Find(ArtifactId) : nullptr;
	Glow->SetLightColor(Artifact ? FLinearColor(1.0f, 0.8f, 0.3f) : FLinearColor(0.6f, 0.85f, 1.0f));
	Glow->SetIntensity(Artifact ? 9000.0f : Kind == EDCCChestKind::Outfitter ? 2500.0f : 3500.0f);
	const DCCClimbItems::FItem* Fixed = Kind == EDCCChestKind::Supply && !ArtifactId.IsNone() ? DCCClimbItems::Find(ArtifactId) : nullptr;
	Label->SetText(FText::FromString(Kind == EDCCChestKind::Outfitter ? TEXT("OUTFITTER'S CRATE\none of everything")
		: Artifact ? TEXT("? ? ?") : Fixed && Fixed->Food > 0.0f ? TEXT("FOOD") : TEXT("SUPPLIES")));
	Label->SetTextRenderColor(Artifact ? FColor(255, 214, 120) : FColor(200, 230, 255));

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCSupplyChest::HandleOverlap);
}

void ADCCSupplyChest::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(OtherActor);
	APlayerState* PlayerState = Crawler ? Crawler->GetPlayerState() : nullptr;
	if (!HasAuthority() || !PlayerState || !Crawler->IsPlayerControlled() || LootedBy.Contains(PlayerState->GetPlayerName()))
	{
		return;
	}
	ADCCArcadeClimb* Arena = nullptr;
	for (TActorIterator<ADCCArcadeClimb> It(GetWorld()); It; ++It)
	{
		Arena = *It;
		break;
	}
	if (!Arena)
	{
		return;
	}
	LootedBy.Add(PlayerState->GetPlayerName());
	switch (Kind)
	{
	case EDCCChestKind::Outfitter:
		Arena->GiveFullKit(Crawler);
		break;
	case EDCCChestKind::Artifact:
		Arena->GiveItem(Crawler, ArtifactId, false);
		break;
	default:
	{
		// A supply chest with an item set (a food cache) gives that; otherwise one at random.
		const TArray<FName> Pool = DCCClimbItems::ChestPool();
		Arena->GiveItem(Crawler, !ArtifactId.IsNone() ? ArtifactId : Pool[FMath::RandRange(0, Pool.Num() - 1)], false);
		break;
	}
	}
}

void ADCCSupplyChest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Open for whoever on this machine has looted it, closed for everyone else.
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const bool bOpen = PC && PC->PlayerState && LootedBy.Contains(PC->PlayerState->GetPlayerName());
	if (bOpen != bShownOpen)
	{
		if (bOpen && !bShownOpen)
		{
			DCCChestSound::PlayOpen(this, GetActorLocation());
		}
		bShownOpen = bOpen;
		const bool bHasOpenMesh = Open->GetStaticMesh() != nullptr;
		Closed->SetVisibility(!bOpen || !bHasOpenMesh);
		Open->SetVisibility(bOpen && bHasOpenMesh);
		Glow->SetIntensity(bOpen ? 0.0f : Glow->Intensity);
		Label->SetVisibility(!bOpen);
	}
	// Labels turn to whoever is looking.
	if (PC && PC->PlayerCameraManager && Label->IsVisible())
	{
		Label->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation()).Rotation());
	}
}
