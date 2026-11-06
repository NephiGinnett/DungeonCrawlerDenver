#include "Onboarding/DCCOnboardingActors.h"

#include "Onboarding/DCCOnboardingComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** The crawler behind an overlap, or null for anything that is not a living, player-controlled one. */
	ADCCCharacterBase* PlayerCrawlerFrom(AActor* Actor)
	{
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Actor);
		if (!Crawler || !Crawler->IsPlayerControlled() || !Crawler->Stats || !Crawler->Stats->IsAlive())
		{
			return nullptr;
		}
		return Crawler;
	}

	UDCCOnboardingComponent* OnboardingOf(ADCCCharacterBase* Crawler)
	{
		return Crawler ? Crawler->FindComponentByClass<UDCCOnboardingComponent>() : nullptr;
	}

}

void DCCOnboarding::TeleportCrawler(ADCCCharacterBase* Crawler, const FVector& Location, const FRotator& Rotation)
{
	if (!Crawler)
	{
		return;
	}

	const FRotator Yaw(0.0f, Rotation.Yaw, 0.0f);
	Crawler->TeleportTo(Location, Yaw, /*bIsATest=*/false, /*bNoCheck=*/true);

	// Control rotation belongs to the owning client; setting it here alone
	// would be overwritten on the next input packet.
	if (APlayerController* PC = Cast<APlayerController>(Crawler->GetController()))
	{
		PC->SetControlRotation(Yaw);
		PC->ClientSetRotation(Yaw, /*bResetCamera=*/true);
	}
}

// ---------------------------------------------------------------------------
// ADCCOnboardingSettings
// ---------------------------------------------------------------------------

ADCCOnboardingSettings::ADCCOnboardingSettings()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	SetActorHiddenInGame(true);
}

// ---------------------------------------------------------------------------
// ADCCPersonalItem
// ---------------------------------------------------------------------------

ADCCPersonalItem::ADCCPersonalItem()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(110.0f);
	PickupSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(PickupSphere);

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(PickupSphere);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(PickupSphere);
	Glow->SetIntensity(1800.0f);
	Glow->SetAttenuationRadius(260.0f);
	Glow->SetLightColor(FLinearColor(0.35f, 0.85f, 1.0f));
	Glow->SetCastShadows(false);
}

void ADCCPersonalItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCPersonalItem, bTaken);
}

void ADCCPersonalItem::BeginPlay()
{
	Super::BeginPlay();
	MeshRestLocation = ItemMesh->GetRelativeLocation();
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ADCCPersonalItem::HandleOverlap);
}

void ADCCPersonalItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bTaken)
	{
		return;
	}

	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ItemMesh->SetRelativeLocation(MeshRestLocation + FVector(0.0f, 0.0f, FMath::Sin(Time * 2.0f) * BobHeight));
	ItemMesh->AddRelativeRotation(FRotator(0.0f, SpinDegreesPerSecond * DeltaSeconds, 0.0f));
}

void ADCCPersonalItem::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bTaken)
	{
		return;
	}

	ADCCCharacterBase* Crawler = PlayerCrawlerFrom(OtherActor);
	UDCCOnboardingComponent* Onboarding = OnboardingOf(Crawler);
	if (!Onboarding)
	{
		return;
	}

	const bool bIsTheirs = OwnerCrawlerId.IsNone() || Onboarding->CrawlerId.IsNone() || Onboarding->CrawlerId == OwnerCrawlerId;
	if (!bIsTheirs)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= RefusalCooldownUntil)
		{
			RefusalCooldownUntil = Now + 3.0f;
			Onboarding->NotifyOwner(FText::Format(
				NSLOCTEXT("DCC", "PersonalItemNotYours", "That belongs to {0}. You know better than to touch it."),
				FText::FromName(OwnerCrawlerId)));
		}
		return;
	}

	bTaken = true;
	OnRep_Taken();

	if (Crawler->Equipment && !EquipmentItemId.IsNone() && !Crawler->Equipment->Equip(EquipmentItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Onboarding] %s: could not equip '%s' - is it in DT_DCCEquipment?"),
			*GetName(), *EquipmentItemId.ToString());
	}

	Onboarding->AdvanceTo(EDCCOnboardingStage::ItemFound);

	const FName SpecificRow(*FString::Printf(TEXT("PersonalItem_%s"), *OwnerCrawlerId.ToString()));
	const bool bShown =
		(!PickupWindowRow.IsNone() && Onboarding->ShowWindowRow(PickupWindowRow)) ||
		(!OwnerCrawlerId.IsNone() && Onboarding->ShowWindowRow(SpecificRow)) ||
		Onboarding->ShowWindowRow(TEXT("PersonalItem"));
	(void)bShown;

	Onboarding->AdvanceLater(EDCCOnboardingStage::Announced, AnnouncementWindowRow, AnnouncementDelaySeconds);
}

void ADCCPersonalItem::OnRep_Taken()
{
	if (bTaken)
	{
		ItemMesh->SetVisibility(false);
		Glow->SetVisibility(false);
		PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorTickEnabled(false);
	}
}

// ---------------------------------------------------------------------------
// ADCCSkyBeacon
// ---------------------------------------------------------------------------

ADCCSkyBeacon::ADCCSkyBeacon()
{
	PrimaryActorTick.bCanEverTick = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	ArrowRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArrowRoot"));
	ArrowRoot->SetupAttachment(RootComponent);
	// Twenty metres of arrow. The point is that it is ridiculous.
	ArrowRoot->SetRelativeScale3D(FVector(20.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(ArrowRoot);
	Shaft->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	Shaft->SetRelativeScale3D(FVector(0.35f, 0.35f, 1.4f));
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shaft->CastShadow = false;
	if (CylinderMesh.Succeeded()) { Shaft->SetStaticMesh(CylinderMesh.Object); }

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(ArrowRoot);
	// The engine cone points up; flip it so the arrow points at the door.
	Head->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	Head->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
	Head->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.9f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->CastShadow = false;
	if (ConeMesh.Succeeded()) { Head->SetStaticMesh(ConeMesh.Object); }

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(ArrowRoot);
	Light->SetIntensity(80000.0f);
	Light->SetAttenuationRadius(6000.0f);
	Light->SetCastShadows(false);
}

void ADCCSkyBeacon::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* Base = ArrowMaterial;
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	if (Base)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), Colour);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), Colour * 8.0f);
		Shaft->SetMaterial(0, MID);
		Head->SetMaterial(0, MID);
	}

	Light->SetLightColor(Colour);
	RevealAlpha = 0.0f;
	ArrowRoot->SetVisibility(false, /*bPropagateToChildren=*/true);
}

bool ADCCSkyBeacon::ShouldShowForLocalPlayer() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	const UDCCOnboardingComponent* Onboarding = OnboardingOf(Crawler);
	if (!Onboarding)
	{
		return false;
	}

	return Onboarding->GetStage() >= RevealAtStage && Onboarding->GetStage() < HideAtStage;
}

void ADCCSkyBeacon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Per-viewer by design: each player's beacon appears when THEIR
	// announcement lands, so it is read locally and never replicated.
	const bool bWant = ShouldShowForLocalPlayer();
	const float Rate = RevealSeconds > 0.0f ? DeltaSeconds / RevealSeconds : 1.0f;
	RevealAlpha = FMath::Clamp(RevealAlpha + (bWant ? Rate : -Rate), 0.0f, 1.0f);

	const bool bVisible = RevealAlpha > 0.0f;
	if (ArrowRoot->IsVisible() != bVisible)
	{
		ArrowRoot->SetVisibility(bVisible, true);
	}
	if (!bVisible)
	{
		return;
	}

	const float Time = GetWorld()->GetTimeSeconds();
	const float Bob = BobSeconds > 0.0f ? FMath::Sin(Time * UE_TWO_PI / BobSeconds) * BobHeight : 0.0f;

	// Grows in from nothing rather than fading, because the flat shape
	// material has no opacity to fade.
	const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, RevealAlpha, 3.0f);
	ArrowRoot->SetRelativeLocation(FVector(0.0f, 0.0f, Bob));
	ArrowRoot->SetRelativeScale3D(FVector(20.0f * Eased));
	ArrowRoot->AddRelativeRotation(FRotator(0.0f, SpinDegreesPerSecond * DeltaSeconds, 0.0f));
	Light->SetIntensity(80000.0f * Eased);
}

// ---------------------------------------------------------------------------
// ADCCOnboardingTrigger
// ---------------------------------------------------------------------------

ADCCOnboardingTrigger::ADCCOnboardingTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(FVector(200.0f, 200.0f, 150.0f));
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(Volume);
}

void ADCCOnboardingTrigger::BeginPlay()
{
	Super::BeginPlay();
	Volume->OnComponentBeginOverlap.AddDynamic(this, &ADCCOnboardingTrigger::HandleOverlap);
}

void ADCCOnboardingTrigger::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	ADCCCharacterBase* Crawler = PlayerCrawlerFrom(OtherActor);
	UDCCOnboardingComponent* Onboarding = OnboardingOf(Crawler);
	if (!Onboarding)
	{
		return;
	}

	if (bOncePerCrawler && AlreadyFired.Contains(Crawler))
	{
		return;
	}

	const EDCCOnboardingStage Stage = Onboarding->GetStage();
	const bool bInRange = Stage >= MinimumStage &&
		(BeforeStage == EDCCOnboardingStage::Complete || Stage < BeforeStage);
	if (!bInRange)
	{
		return;
	}

	AlreadyFired.Add(Crawler);

	if (bAdvanceStage)
	{
		Onboarding->AdvanceTo(AdvanceToStage);
	}
	Onboarding->UnlockFeatures(UnlockFeatures);
	for (const FName& Row : WindowRows)
	{
		Onboarding->ShowWindowRow(Row);
	}
}

// ---------------------------------------------------------------------------
// ADCCSoloTrial
// ---------------------------------------------------------------------------

ADCCSoloTrial::ADCCSoloTrial()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ArrivalPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalPoint"));
	ArrivalPoint->SetupAttachment(Root);
	ArrivalPoint->SetRelativeLocation(FVector(-700.0f, 0.0f, 100.0f));
	ArrivalPoint->ArrowSize = 3.0f;

	SpawnCentre = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnCentre"));
	SpawnCentre->SetupAttachment(Root);
	SpawnCentre->SetRelativeLocation(FVector(300.0f, 0.0f, 100.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cylinder = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	auto MakeExit = [this, Cylinder](const TCHAR* BoxName, const TCHAR* VisualName, const TCHAR* LabelName,
		const FVector& Where, const FText& Label, const FColor& Colour,
		TObjectPtr<UBoxComponent>& OutBox, TObjectPtr<UStaticMeshComponent>& OutVisual, TObjectPtr<UTextRenderComponent>& OutLabel)
	{
		OutBox = CreateDefaultSubobject<UBoxComponent>(BoxName);
		OutBox->SetupAttachment(Root);
		OutBox->SetRelativeLocation(Where);
		OutBox->SetBoxExtent(FVector(90.0f, 90.0f, 120.0f));
		OutBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

		// A tall glowing pillar is the whole affordance: no HUD exists yet to point at it.
		OutVisual = CreateDefaultSubobject<UStaticMeshComponent>(VisualName);
		OutVisual->SetupAttachment(OutBox);
		OutVisual->SetRelativeScale3D(FVector(1.4f, 1.4f, 6.0f));
		OutVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
		OutVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutVisual->CastShadow = false;
		if (Cylinder) { OutVisual->SetStaticMesh(Cylinder); }

		OutLabel = CreateDefaultSubobject<UTextRenderComponent>(LabelName);
		OutLabel->SetupAttachment(OutBox);
		OutLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 520.0f));
		OutLabel->SetHorizontalAlignment(EHTA_Center);
		OutLabel->SetWorldSize(48.0f);
		OutLabel->SetText(Label);
		OutLabel->SetTextRenderColor(Colour);
	};

	MakeExit(TEXT("SafeRoomExit"), TEXT("SafeRoomExitVisual"), TEXT("SafeRoomExitLabel"),
		FVector(1100.0f, -350.0f, 100.0f), NSLOCTEXT("DCC", "TrialSafeRoomExit", "SAFE ROOM"), FColor(90, 220, 255),
		SafeRoomExit, SafeRoomExitVisual, SafeRoomExitLabel);

	MakeExit(TEXT("ContinueExit"), TEXT("ContinueExitVisual"), TEXT("ContinueExitLabel"),
		FVector(1100.0f, 350.0f, 100.0f), NSLOCTEXT("DCC", "TrialContinueExit", "ONWARD"), FColor(255, 190, 60),
		ContinueExit, ContinueExitVisual, ContinueExitLabel);

	EncounterTable = TSoftObjectPtr<UDataTable>(
		FSoftObjectPath(TEXT("/Game/DCC/Onboarding/DT_DCCRandomEncounters.DT_DCCRandomEncounters")));
}

void ADCCSoloTrial::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCSoloTrial, bExitsOpen);
}

void ADCCSoloTrial::BeginPlay()
{
	Super::BeginPlay();

	SafeRoomExit->OnComponentBeginOverlap.AddDynamic(this, &ADCCSoloTrial::HandleSafeRoomExit);
	ContinueExit->OnComponentBeginOverlap.AddDynamic(this, &ADCCSoloTrial::HandleContinueExit);
	OnRep_ExitsOpen();
}

bool ADCCSoloTrial::IsFree() const
{
	return !IsValid(Occupant) || !Occupant->Stats || !Occupant->Stats->IsAlive();
}

void ADCCSoloTrial::BeginFor(ADCCCharacterBase* Crawler)
{
	if (!HasAuthority() || !Crawler)
	{
		return;
	}

	Occupant = Crawler;
	bExitsOpen = false;
	OnRep_ExitsOpen();

	DCCOnboarding::TeleportCrawler(Crawler, ArrivalPoint->GetComponentLocation(), ArrivalPoint->GetComponentRotation());

	if (UDCCOnboardingComponent* Onboarding = OnboardingOf(Crawler))
	{
		Onboarding->AdvanceTo(EDCCOnboardingStage::InDungeon);
	}

	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ADCCSoloTrial::SpawnRolledEncounter, FMath::Max(0.01f, SpawnDelaySeconds), false);
}

bool ADCCSoloTrial::RollEncounter(FDCCRandomEncounterRow& OutRow) const
{
	UDataTable* Table = EncounterTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Onboarding] %s has no encounter table (expected DT_DCCRandomEncounters)."), *GetName());
		return false;
	}

	if (!ForcedEncounterRow.IsNone())
	{
		if (const FDCCRandomEncounterRow* Forced = Table->FindRow<FDCCRandomEncounterRow>(ForcedEncounterRow, TEXT("DCCSoloTrial")))
		{
			OutRow = *Forced;
			return true;
		}
	}

	TArray<FDCCRandomEncounterRow*> Rows;
	Table->GetAllRows<FDCCRandomEncounterRow>(TEXT("DCCSoloTrial"), Rows);

	float Total = 0.0f;
	for (const FDCCRandomEncounterRow* Row : Rows)
	{
		Total += (Row && Row->Spawns.Num() > 0) ? FMath::Max(0.0f, Row->Weight) : 0.0f;
	}
	if (Total <= 0.0f)
	{
		return false;
	}

	float Pick = FMath::FRandRange(0.0f, Total);
	for (const FDCCRandomEncounterRow* Row : Rows)
	{
		if (!Row || Row->Spawns.Num() == 0)
		{
			continue;
		}
		Pick -= FMath::Max(0.0f, Row->Weight);
		if (Pick <= 0.0f)
		{
			OutRow = *Row;
			return true;
		}
	}

	OutRow = *Rows.Last();
	return true;
}

void ADCCSoloTrial::SpawnRolledEncounter()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsValid(Occupant))
	{
		return;
	}

	FDCCRandomEncounterRow Row;
	if (!RollEncounter(Row))
	{
		// No table yet should not strand anyone in an empty room.
		OpenExits();
		return;
	}

	if (UDCCOnboardingComponent* Onboarding = OnboardingOf(Occupant))
	{
		FDCCSystemWindowPayload Window;
		Window.Header = NSLOCTEXT("DCC", "EncounterHeader", "WARNING  -  HOSTILES DETECTED");
		Window.Title = Row.EncounterName;
		Window.Body = Row.IntroLine;
		Window.Style = EDCCSystemWindowStyle::Warning;
		Window.Voice = Row.IntroVoice;
		Onboarding->ShowWindow(Window);
	}

	int32 Total = 0;
	for (const FDCCRandomEncounterSpawn& Spawn : Row.Spawns)
	{
		Total += Spawn.EnemyClass ? FMath::Max(1, Spawn.Count) : 0;
	}

	const FVector Centre = SpawnCentre->GetComponentLocation();
	const FRotator FaceArrival = (ArrivalPoint->GetComponentLocation() - Centre).GetSafeNormal2D().Rotation();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Index = 0;
	for (const FDCCRandomEncounterSpawn& Spawn : Row.Spawns)
	{
		if (!Spawn.EnemyClass)
		{
			continue;
		}

		for (int32 i = 0; i < FMath::Max(1, Spawn.Count); ++i, ++Index)
		{
			// A loose arc facing the crawler, not a perfect circle around the
			// centre: nothing should spawn behind the player on their first fight.
			const float Spread = Total > 1 ? FMath::Lerp(-70.0f, 70.0f, static_cast<float>(Index) / (Total - 1)) : 0.0f;
			const FVector Offset = FaceArrival.RotateVector(FRotator(0.0f, Spread, 0.0f).RotateVector(FVector(-SpawnRadius * 0.35f, 0.0f, 0.0f)));
			const FVector Location = Centre + Offset + FVector(FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-60.0f, 60.0f), 0.0f);

			// Deferred so the renamed DisplayName is set before BeginPlay and
			// travels in the first replication bunch: clients build the
			// nameplate from it on their own BeginPlay.
			const FTransform SpawnTransform(FaceArrival, Location);
			ADCCEnemyCharacter* Enemy = World->SpawnActorDeferred<ADCCEnemyCharacter>(
				Spawn.EnemyClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
			if (!Enemy)
			{
				continue;
			}

			if (!Spawn.DisplayNameOverride.IsEmpty())
			{
				Enemy->DisplayName = Spawn.DisplayNameOverride;
			}

			Enemy->FinishSpawning(SpawnTransform);

			if (Enemy->Stats)
			{
				Enemy->Stats->Level = Spawn.Level;
				Enemy->Stats->RefreshDerivedStats(/*bRefillToFull=*/true);
			}

			Enemy->OnCharacterDied.AddDynamic(this, &ADCCSoloTrial::HandleEnemyDied);
			LivingEnemies.Add(Enemy);
		}
	}

	if (LivingEnemies.Num() == 0)
	{
		OpenExits();
	}
}

void ADCCSoloTrial::HandleEnemyDied(ADCCCharacterBase* Character)
{
	LivingEnemies.RemoveAll([Character](const TObjectPtr<ADCCEnemyCharacter>& Enemy)
	{
		return !IsValid(Enemy) || Enemy == Character || !Enemy->Stats || !Enemy->Stats->IsAlive();
	});

	if (LivingEnemies.Num() == 0)
	{
		OpenExits();
	}
}

void ADCCSoloTrial::OpenExits()
{
	if (bExitsOpen)
	{
		return;
	}

	bExitsOpen = true;
	OnRep_ExitsOpen();

	if (UDCCOnboardingComponent* Onboarding = OnboardingOf(Occupant))
	{
		Onboarding->AdvanceTo(EDCCOnboardingStage::FirstFightWon);
		Onboarding->ShowWindowRow(ClearedWindowRow);
	}
}

void ADCCSoloTrial::OnRep_ExitsOpen()
{
	const bool bContinueAvailable = ContinueDestination != nullptr;

	SafeRoomExitVisual->SetVisibility(bExitsOpen);
	SafeRoomExitLabel->SetVisibility(bExitsOpen);
	ContinueExitVisual->SetVisibility(bExitsOpen && bContinueAvailable);
	ContinueExitLabel->SetVisibility(bExitsOpen && bContinueAvailable);
}

void ADCCSoloTrial::LeaveTo(ADCCCharacterBase* Crawler, AActor* Destination)
{
	if (!Destination)
	{
		return;
	}

	DCCOnboarding::TeleportCrawler(Crawler, Destination->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), Destination->GetActorRotation());
	Release();
}

void ADCCSoloTrial::Release()
{
	Occupant = nullptr;
	LivingEnemies.Reset();
	bExitsOpen = false;
	OnRep_ExitsOpen();
}

void ADCCSoloTrial::HandleSafeRoomExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ADCCCharacterBase* Crawler = PlayerCrawlerFrom(OtherActor);
	if (!HasAuthority() || !bExitsOpen || !Crawler || Crawler != Occupant)
	{
		return;
	}

	// The safe room's own ADCCOnboardingTrigger does the welcome and the HUD.
	LeaveTo(Crawler, SafeRoomDestination);
}

void ADCCSoloTrial::HandleContinueExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ADCCCharacterBase* Crawler = PlayerCrawlerFrom(OtherActor);
	if (!HasAuthority() || !bExitsOpen || !Crawler || Crawler != Occupant || !ContinueDestination)
	{
		return;
	}

	// Skipping the safe room still finishes onboarding: nobody should play
	// the whole floor without a health bar because they were brave.
	if (UDCCOnboardingComponent* Onboarding = OnboardingOf(Crawler))
	{
		Onboarding->ShowWindowRow(ContinueWindowRow);
		Onboarding->AdvanceTo(EDCCOnboardingStage::Complete);
	}

	LeaveTo(Crawler, ContinueDestination);
}

// ---------------------------------------------------------------------------
// ADCCDungeonEntrance
// ---------------------------------------------------------------------------

ADCCDungeonEntrance::ADCCDungeonEntrance()
{
	PrimaryActorTick.bCanEverTick = false;

	Doorway = CreateDefaultSubobject<UBoxComponent>(TEXT("Doorway"));
	Doorway->SetBoxExtent(FVector(120.0f, 250.0f, 200.0f));
	Doorway->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(Doorway);
}

void ADCCDungeonEntrance::BeginPlay()
{
	Super::BeginPlay();
	Doorway->OnComponentBeginOverlap.AddDynamic(this, &ADCCDungeonEntrance::HandleOverlap);
}

void ADCCDungeonEntrance::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	ADCCCharacterBase* Crawler = PlayerCrawlerFrom(OtherActor);
	UDCCOnboardingComponent* Onboarding = OnboardingOf(Crawler);
	if (!Onboarding)
	{
		return;
	}

	if (Onboarding->GetStage() < MinimumStage)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= RefusalCooldownUntil)
		{
			RefusalCooldownUntil = Now + 3.0f;
			Onboarding->NotifyOwner(NSLOCTEXT("DCC", "EntranceNotYet", "The doorway does not open. Not yet. You haven't been invited."));
		}
		return;
	}

	// Past the trial already (a respawn, or onboarding switched off): not our business.
	if (Onboarding->GetStage() >= EDCCOnboardingStage::InDungeon)
	{
		return;
	}

	for (const FName& Row : EntryWindowRows)
	{
		Onboarding->ShowWindowRow(Row);
	}

	for (ADCCSoloTrial* Trial : Trials)
	{
		if (Trial && Trial->IsFree())
		{
			Trial->BeginFor(Crawler);
			return;
		}
	}

	// More players than trial rooms. Better a skipped fight than a stuck player.
	UE_LOG(LogTemp, Warning, TEXT("[DCC Onboarding] %s: every solo trial is busy; sending %s to the overflow."),
		*GetName(), *GetNameSafe(Crawler));
	Onboarding->AdvanceTo(EDCCOnboardingStage::FirstFightWon);
	if (OverflowDestination)
	{
		DCCOnboarding::TeleportCrawler(Crawler, OverflowDestination->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f),
			OverflowDestination->GetActorRotation());
	}
}
