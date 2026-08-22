#include "Rooms/DCCFloorExit.h"

#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCBossComponent.h"
#include "Combat/DCCGameState.h"
#include "Progression/DCCPopulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Items/DCCCarryComponent.h"
#include "Items/DCCPickup.h"
#include "Stats/DCCStatsComponent.h"
#include "Quests/DCCQuestComponent.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCCFloorExit::ADCCFloorExit()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	SetRootComponent(Frame);

	// Scenery: the trigger does the work, and a blocking frame would let a crawler
	// wedge the Strongbox in the doorway.
	Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FrameMesh(
		TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
	if (FrameMesh.Succeeded())
	{
		Frame->SetStaticMesh(FrameMesh.Object);
	}

	// A doorway-shaped slab until there is real art for it.
	Frame->SetRelativeScale3D(FVector(0.4f, 3.0f, 4.5f));

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Frame);
	Trigger->SetBoxExtent(FVector(120.0f, 160.0f, 220.0f));

	// Overlap only, and only against pawns: a thrown crate must not open the floor.
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Undo the frame's non-uniform scale, or the trigger inherits it and ends up
	// a wildly different shape from the extents set above.
	Trigger->SetRelativeScale3D(FVector(2.5f, 1.0f / 3.0f, 1.0f / 4.5f));

	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Frame);
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetWorldSize(60.0f);
	Sign->SetRelativeScale3D(FVector(2.5f, 1.0f / 3.0f, 1.0f / 4.5f));
	Sign->SetRelativeLocation(FVector(0.0f, 0.0f, 0.62f));
}

void ADCCFloorExit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCFloorExit, bLocked);
}

void ADCCFloorExit::BeginPlay()
{
	Super::BeginPlay();

	bLocked = bRequiresBossDefeat;
	ApplyLockedVisuals();

	if (!HasAuthority())
	{
		return;
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCFloorExit::HandleTriggerBeginOverlap);

	if (bRequiresBossDefeat)
	{
		// Polled rather than event-driven because the boss does not exist yet -
		// it arrives as an encounter wave. Half a second is far below the
		// threshold anyone would notice on a door opening, and the check is a
		// short iteration over boss components, of which there is at most one.
		GetWorldTimerManager().SetTimer(BossWatchTimer, this,
			&ADCCFloorExit::CheckBossState, 0.5f, true);
	}
}

void ADCCFloorExit::CheckBossState()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !bLocked)
	{
		return;
	}

	bool bLivingBoss = false;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->FindComponentByClass<UDCCBossComponent>())
		{
			continue;
		}

		// Seen once is enough to arm the latch, dead or alive - otherwise a boss
		// that dies between two polls would leave the exit locked forever.
		bBossHasAppeared = true;

		const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(Actor);
		if (!Character || Character->IsAlive())
		{
			bLivingBoss = true;
			break;
		}
	}

	if (!bBossHasAppeared || bLivingBoss)
	{
		return;
	}

	bLocked = false;
	GetWorldTimerManager().ClearTimer(BossWatchTimer);
	ApplyLockedVisuals();

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogExitUnlocked",
			"The lien on Floor {0} is discharged. The exit is now compliant with fire code and may be used."),
		FText::AsNumber(FloorNumber)));
}

void ADCCFloorExit::OnRep_Locked()
{
	ApplyLockedVisuals();
}

void ADCCFloorExit::ApplyLockedVisuals()
{
	if (!Sign)
	{
		return;
	}

	Sign->SetText(bLocked
		? NSLOCTEXT("DCC", "ExitLocked", "EXIT — CLOSED FOR MAINTENANCE")
		: NSLOCTEXT("DCC", "ExitOpen", "EXIT"));

	Sign->SetTextRenderColor(bLocked ? FColor(200, 40, 40) : FColor(70, 230, 90));
}

void ADCCFloorExit::HandleTriggerBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*Sweep*/)
{
	if (!HasAuthority() || bLocked)
	{
		return;
	}

	ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(OtherActor);
	if (!Character || !Character->IsAlive() || !Character->IsCrawler())
	{
		return;
	}

	UseExit(Character);
}

void ADCCFloorExit::UseExit(ADCCCharacterBase* Character)
{
	if (!Character)
	{
		return;
	}

	// Anything that deferred its value until the exit gets paid out here. This is
	// the only place the Strongbox is ever worth anything, which is what made
	// carrying it a real decision rather than a free bonus.
	if (UDCCCarryComponent* Carry = Character->FindComponentByClass<UDCCCarryComponent>())
	{
		if (ADCCPickup* Carried = Carry->CarriedPickup)
		{
			if (Carried->GoldValue > 0 && !Carried->bValueClaimed)
			{
				Carried->ClaimValue(Character);
				Carried->Destroy();
			}
		}
	}

	if (bFloorCompleted)
	{
		return;
	}
	bFloorCompleted = true;

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogFloorExit",
			"{0} has left Floor {1}. Your table is ready on the floor below. It will not be ready when you arrive."),
		FText::FromString(GetNameSafe(Character)), FText::AsNumber(FloorNumber)));

	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::ClearFloor,
		FName(*FString::FromInt(FloorNumber)));

	// The rest of the dungeon does not survive the floor either. This is the only
	// place the global death toll moves, and that is deliberate: a number that
	// drips down continuously becomes wallpaper, whereas one that lurches at a
	// floor boundary is the single most affecting statistic in the game.
	//
	// Fractional rather than absolute, because "300,000 died" stops meaning
	// anything once only 90,000 remain.
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDCCPopulationSubsystem* Population =
			GameInstance->GetSubsystem<UDCCPopulationSubsystem>())
		{
			Population->ApplyAttritionFraction(FloorAttritionFraction, FText::GetEmpty());

			ADCCGameState::Log(this, EDCCLogCategory::Announcement,
				Population->BuildPopulationAnnouncement());
		}
	}
}
