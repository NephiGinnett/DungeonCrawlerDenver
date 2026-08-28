#include "Intro/DCCIntroDirector.h"

#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

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

	CollapseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CollapseCamera"));
	CollapseCamera->SetupAttachment(DoorTrigger);
	CollapseCamera->SetActive(false);

	// A placeholder establishing shot - pulled back and slightly elevated, so
	// there is something to look at the moment this is placed rather than a
	// camera buried inside the doorway. Zoe repositions this by hand once the
	// skyline is actually placed, same as everywhere else in this project
	// that gets nudged into its final spot after the fact.
	CollapseCamera->SetRelativeLocation(FVector(-1800.0f, 0.0f, 900.0f));
	CollapseCamera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));

	// Dam_city ships both the default skyline building meshes and this smoke
	// system, so the two already look like they belong together - see
	// Docs/the-collapse-cutscene.md. A soft path assignment rather than
	// ConstructorHelpers::FObjectFinder because DustEffect is a
	// TSoftObjectPtr, resolved with LoadSynchronous() only when the collapse
	// actually starts, not at editor startup.
	DustEffect = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/Dam_city/FX/NS_Smoke.NS_Smoke")));
}

void ADCCIntroDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCIntroDirector, ReadyForCollapse);
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

	// Called from here rather than from inside ApplyPhase: ApplyPhase bails
	// out immediately on a machine with no local PlayerController (a
	// dedicated server), but the building cache it primes has to exist on
	// every machine so Tick's per-frame animation - which does run
	// everywhere, unconditionally - has identical data to work from.
	if (NewPhase == EDCCIntroPhase::Collapsing)
	{
		BeginCollapseSequence();
	}

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
		// Hand off from the first-person IntroCamera to the pulled-back
		// establishing shot - see CollapseCamera's own comment for why this
		// needs to be a second camera component rather than just re-aiming
		// IntroCamera in place.
		if (IntroCamera)
		{
			IntroCamera->SetActive(false);
		}
		if (CollapseCamera)
		{
			CollapseCamera->SetActive(true);
		}

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

void ADCCIntroDirector::BeginCollapseSequence()
{
	NextCrashSoundIndex = 0;

	if (CollapseCamera)
	{
		CollapseCameraBaseLocation = CollapseCamera->GetRelativeLocation();
		CollapseCameraBaseRotation = CollapseCamera->GetRelativeRotation();
	}

	const auto CacheBuilding = [](FDCCCollapsingBuilding& Building)
	{
		if (!Building.BuildingActor)
		{
			return;
		}

		Building.CachedScale = Building.BuildingActor->GetActorScale3D();
		Building.CachedLocation = Building.BuildingActor->GetActorLocation();
		Building.CachedRotation = Building.BuildingActor->GetActorRotation();
		Building.bDustSpawned = false;

		// Deterministic from the building's own placement rather than
		// FMath::Rand - every machine, including a dedicated server that
		// never renders any of this, derives the exact same tumble axis with
		// nothing to replicate.
		FRandomStream Stream(static_cast<int32>(GetTypeHash(Building.CachedLocation)));
		Building.TumbleAxis = Stream.VRand();
	};

	for (FDCCCollapsingBuilding& Building : SkylineBuildings)
	{
		CacheBuilding(Building);
	}

	for (FDCCCollapsingBuilding& Building : HeroBuildingChunks)
	{
		CacheBuilding(Building);
	}
}

float ADCCIntroDirector::GetBuildingStaggerDelay(const FDCCCollapsingBuilding& Building) const
{
	const float DistanceMeters = FVector::Dist(GetActorLocation(), Building.CachedLocation) / 100.0f;
	return DistanceMeters * CollapseStaggerSecondsPerMeter + Building.ExtraDelaySeconds;
}

void ADCCIntroDirector::TickCollapsingBuildings()
{
	const auto AnimateBuilding = [this](FDCCCollapsingBuilding& Building)
	{
		AActor* Actor = Building.BuildingActor;
		if (!Actor)
		{
			return;
		}

		const float Local = PhaseElapsed - GetBuildingStaggerDelay(Building);
		if (Local <= 0.0f)
		{
			// Not this one's turn yet - stays standing, exactly as placed.
			return;
		}

		const float Alpha = FMath::Clamp(Local / BuildingCollapseDuration, 0.0f, 1.0f);
		const float Eased = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f);

		if (!Building.bDustSpawned)
		{
			Building.bDustSpawned = true;

			if (UNiagaraSystem* Dust = DustEffect.LoadSynchronous())
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					GetWorld(), Dust, Building.CachedLocation, FRotator::ZeroRotator);
			}
		}

		if (Building.bIsHeroChunk)
		{
			// A rigid chunk tumbling and dropping, not squashing - it is a
			// separate piece of a modular building, not the whole skyline.
			const FVector FallOffset(0.0f, 0.0f, -HeroChunkFallDistance * Eased);

			const FQuat TumbleQuat(Building.TumbleAxis.GetSafeNormal(),
				FMath::DegreesToRadians(HeroChunkTumbleDegrees * Eased));

			Actor->SetActorLocationAndRotation(
				Building.CachedLocation + FallOffset,
				TumbleQuat * Building.CachedRotation.Quaternion());
		}
		else
		{
			// The cheap trick: squash toward the ground rather than actually
			// fracturing anything. Assumes the mesh's pivot sits at its base,
			// which every building kit used for the default skyline does -
			// see Docs/the-collapse-cutscene.md if a future mesh needs a
			// location compensation this does not provide.
			FVector NewScale = Building.CachedScale;
			NewScale.Z = FMath::Lerp(Building.CachedScale.Z, Building.CachedScale.Z * 0.04f, Eased);
			Actor->SetActorScale3D(NewScale);
		}
	};

	for (FDCCCollapsingBuilding& Building : SkylineBuildings)
	{
		AnimateBuilding(Building);
	}

	for (FDCCCollapsingBuilding& Building : HeroBuildingChunks)
	{
		AnimateBuilding(Building);
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
		// Runs on every machine unconditionally, including a dedicated
		// server that never renders any of it - see BeginCollapseSequence's
		// own comment on why the cache it relies on is primed the same way.
		TickCollapsingBuildings();

		while (NextCrashSoundIndex < CollapseCrashSounds.Num()
			&& PhaseElapsed >= NextCrashSoundIndex * CrashSoundStaggerSeconds)
		{
			if (USoundBase* Sound = CollapseCrashSounds[NextCrashSoundIndex].LoadSynchronous())
			{
				UGameplayStatics::PlaySound2D(this, Sound);
			}
			++NextCrashSoundIndex;
		}

		if (CollapseCamera)
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

			CollapseCamera->SetRelativeLocation(CollapseCameraBaseLocation + Shake);

			CollapseCamera->SetRelativeRotation(CollapseCameraBaseRotation + FRotator(
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

	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(OtherActor);
	if (!Crawler || !Crawler->IsCrawler() || ReadyForCollapse.Contains(Crawler))
	{
		return;
	}

	ReadyForCollapse.Add(Crawler);

	if (ReadyForCollapse.Num() < GetExpectedPartySize())
	{
		// Not everyone has reached the exit yet. This crawler waits -
		// ADCCHUD reads ReadyForCollapse straight off this replicated array,
		// no RPC required, and is otherwise free to keep exploring; nothing
		// about the wait needs to hold their movement or camera.
		return;
	}

	bCollapseTriggered = true;
	Multicast_EnterPhase(EDCCIntroPhase::Street);
}

int32 ADCCIntroDirector::GetExpectedPartySize() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? FMath::Max(GS->PlayerArray.Num(), 1) : 1;
}
