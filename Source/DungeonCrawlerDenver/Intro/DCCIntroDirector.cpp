#include "Intro/DCCIntroDirector.h"

#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ADCCIntroDirector::ADCCIntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	DoorTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorTrigger"));
	SetRootComponent(DoorTrigger);

	// Wide enough to be impossible to squeeze past, since crossing it is the
	// only way the intro can ever end on its own.
	DoorTrigger->SetBoxExtent(FVector(120.0f, 160.0f, 160.0f));
	DoorTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DoorTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	DoorTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DoorTrigger->SetGenerateOverlapEvents(true);

	IntroCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("IntroCamera"));
	IntroCamera->SetupAttachment(DoorTrigger);
}

void ADCCIntroDirector::BeginPlay()
{
	Super::BeginPlay();

	DoorTrigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCIntroDirector::HandleDoorOverlap);

	if (HasAuthority() && bRunOnBeginPlay)
	{
		BeginIntro();
	}
}

void ADCCIntroDirector::BeginIntro()
{
	if (!HasAuthority() || Phase != EDCCIntroPhase::Idle)
	{
		return;
	}

	Multicast_EnterPhase(EDCCIntroPhase::Waking);
}

void ADCCIntroDirector::SkipIntro()
{
	if (!HasAuthority())
	{
		return;
	}

	bCollapseTriggered = true;
	Multicast_EnterPhase(EDCCIntroPhase::Idle);

	TeleportAllCrawlersToDungeon();
}

void ADCCIntroDirector::Multicast_EnterPhase_Implementation(EDCCIntroPhase NewPhase)
{
	Phase = NewPhase;
	PhaseElapsed = 0.0f;

	ApplyPhase(NewPhase);
}

APlayerController* ADCCIntroDirector::GetLocalController() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetFirstPlayerController() : nullptr;
}

ADCCCharacterBase* ADCCIntroDirector::GetLocalCrawler() const
{
	const APlayerController* PC = GetLocalController();
	return PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
}

void ADCCIntroDirector::ApplyPhase(EDCCIntroPhase NewPhase)
{
	APlayerController* PC = GetLocalController();
	if (!PC)
	{
		return;
	}

	switch (NewPhase)
	{
	case EDCCIntroPhase::Waking:
		// The camera becomes this actor's, not the pawn's, so the pawn can stay
		// standing in its capsule the whole time - laying a Character down
		// properly means fighting the capsule and the movement component for a
		// shot that lasts three seconds.
		PC->SetViewTarget(this);

		// SetCinematicMode's own bHidePlayer is deliberately false: the bedroom
		// is first person for these few seconds and there is no body to hide,
		// but a later art pass may want to see the crawler's own arms.
		PC->SetCinematicMode(/*bInCinematicMode=*/true, /*bHidePlayer=*/false,
			/*bAffectsHUD=*/true, /*bAffectsMovement=*/true, /*bAffectsTurning=*/true);

		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(
				1.0f, 0.0f, FadeInSeconds, FLinearColor::Black, /*bShouldFadeAudio=*/true);
		}
		break;

	case EDCCIntroPhase::Rising:
		// Nothing to apply - Tick animates the camera. Kept as an explicit case
		// so the phase list reads as complete rather than as having a hole in it.
		break;

	case EDCCIntroPhase::Exploring:
		ReleaseToPlayer(/*BlendSeconds=*/0.6f);
		break;

	case EDCCIntroPhase::Street:
		// Control stays with the player through the street beat. They walked out
		// of their own front door; taking the camera away at that exact moment
		// would read as a punishment for doing the thing they were asked to do.
		break;

	case EDCCIntroPhase::Collapsing:
		PC->SetViewTarget(this);
		PC->SetCinematicMode(true, false, true, true, true);

		if (PC->PlayerCameraManager)
		{
			// Held black at the end rather than faded back up: the next thing
			// the player sees is the Queue, and the cut should happen behind
			// the curtain.
			PC->PlayerCameraManager->StartCameraFade(
				0.0f, 1.0f, CollapseSeconds, FLinearColor::Black, true, /*bHoldWhenFinished=*/true);
		}
		break;

	case EDCCIntroPhase::Idle:
	default:
		ReleaseToPlayer(0.0f);
		break;
	}
}

void ADCCIntroDirector::TeleportAllCrawlersToDungeon() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Index = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
		if (!Crawler)
		{
			continue;
		}

		// A ring rather than a random scatter - the party arrived together and
		// should read as arriving together, just not stacked on one coordinate.
		const float Angle = FMath::DegreesToRadians(Index * (360.0f / 5.0f));
		const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * ArrivalSpreadRadius;

		Crawler->SetActorLocationAndRotation(DungeonArrivalLocation + Offset, DungeonArrivalRotation);
		++Index;
	}
}

void ADCCIntroDirector::ReleaseToPlayer(float BlendSeconds)
{
	APlayerController* PC = GetLocalController();
	if (!PC)
	{
		return;
	}

	PC->SetCinematicMode(false, false, true, true, true);

	if (APawn* Pawn = PC->GetPawn())
	{
		PC->SetViewTargetWithBlend(Pawn, BlendSeconds);
	}
}

void ADCCIntroDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Phase == EDCCIntroPhase::Idle)
	{
		return;
	}

	PhaseElapsed += DeltaSeconds;

	switch (Phase)
	{
	case EDCCIntroPhase::Waking:
		if (IntroCamera)
		{
			IntroCamera->SetRelativeLocation(SleepingCameraOffset);
			IntroCamera->SetRelativeRotation(FRotator(SleepingCameraPitch, 0.0f, 0.0f));
		}

		if (HasAuthority() && PhaseElapsed >= LyingSeconds)
		{
			Multicast_EnterPhase(EDCCIntroPhase::Rising);
		}
		break;

	case EDCCIntroPhase::Rising:
		if (IntroCamera)
		{
			// Eased rather than linear: sitting up is fast at the start and
			// slow at the top, and a constant-rate rise reads like an elevator.
			const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(RiseSeconds, 0.1f), 0.0f, 1.0f);
			const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

			IntroCamera->SetRelativeLocation(
				FMath::Lerp(SleepingCameraOffset, StandingCameraOffset, Eased));

			IntroCamera->SetRelativeRotation(
				FRotator(FMath::Lerp(SleepingCameraPitch, 0.0f, Eased), 0.0f, 0.0f));
		}

		if (HasAuthority() && PhaseElapsed >= RiseSeconds)
		{
			Multicast_EnterPhase(EDCCIntroPhase::Exploring);
		}
		break;

	case EDCCIntroPhase::Exploring:
		// Open-ended on purpose. The player leaves when they open the door, and
		// there is no timer pushing them - a cold open that hurries you is a
		// cold open you resent.
		break;

	case EDCCIntroPhase::Street:
		if (HasAuthority() && PhaseElapsed >= StreetSeconds)
		{
			Multicast_EnterPhase(EDCCIntroPhase::Collapsing);
		}
		break;

	case EDCCIntroPhase::Collapsing:
		if (IntroCamera)
		{
			// Hand-rolled shake rather than a UCameraShakeBase asset: a shake
			// class is another reflected UCLASS and another asset to author,
			// for a curve that is four lines here. Ramps up rather than
			// starting at full amplitude, so the first tremor is something you
			// notice rather than something that hits you.
			const float Ramp = FMath::Clamp(PhaseElapsed / FMath::Max(CollapseSeconds, 0.1f), 0.0f, 1.0f);
			const float Amplitude = CollapseShakeAmplitude * Ramp;

			const float Time = PhaseElapsed * 34.0f;

			const FVector Shake(
				FMath::Sin(Time * 1.7f) * Amplitude,
				FMath::Sin(Time * 2.3f) * Amplitude,
				FMath::Sin(Time * 3.1f) * Amplitude * 0.6f);

			IntroCamera->SetRelativeLocation(StandingCameraOffset + Shake);

			IntroCamera->SetRelativeRotation(FRotator(
				FMath::Sin(Time * 1.1f) * Ramp * 4.0f,
				FMath::Sin(Time * 0.9f) * Ramp * 4.0f,
				FMath::Sin(Time * 1.9f) * Ramp * 6.0f));
		}

		if (HasAuthority() && PhaseElapsed >= CollapseSeconds)
		{
			TeleportAllCrawlersToDungeon();

			Multicast_EnterPhase(EDCCIntroPhase::Idle);

			// The System AI's first words in the entire game, and the first
			// sound of any kind since the title. Everything before this beat is
			// deliberately silent - see the class comment.
			ADCCGameState::Log(this, EDCCLogCategory::Announcement, NSLOCTEXT("DCC",
				"IntroCollapse",
				"Good morning. Your building has been repossessed, along with the rest of it. "
				"Please remain calm: you have been pre-approved for a complimentary orientation."));

			if (APlayerController* PC = GetLocalController())
			{
				if (PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->StartCameraFade(
						1.0f, 0.0f, 2.0f, FLinearColor::Black, true);
				}
			}
		}
		break;

	default:
		break;
	}
}

void ADCCIntroDirector::HandleDoorOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& Sweep)
{
	if (!HasAuthority() || bCollapseTriggered || Phase != EDCCIntroPhase::Exploring)
	{
		return;
	}

	const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(OtherActor);
	if (!Crawler || !Crawler->IsCrawler())
	{
		return;
	}

	bCollapseTriggered = true;
	Multicast_EnterPhase(EDCCIntroPhase::Street);
}
