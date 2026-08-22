#include "Progression/DCCCrowdSpawner.h"

#include "Progression/DCCCrowdCrawler.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

ADCCCrowdSpawner::ADCCCrowdSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->SetBoxExtent(FVector(600.0f, 600.0f, 200.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetHiddenInGame(true);

	CrowdClass = ADCCCrowdCrawler::StaticClass();
}

void ADCCCrowdSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Server only. The crawlers replicate down with their seeds, so a client
	// generating its own would produce a second, different crowd in the same
	// room - and both would be visible.
	if (HasAuthority())
	{
		PopulateCrowd();
	}
}

void ADCCCrowdSpawner::PopulateCrowd()
{
	UWorld* World = GetWorld();
	if (!World || !CrowdClass || CrowdSize <= 0 || !Bounds)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	const FVector Extent = Bounds->GetScaledBoxExtent();

	// Derived from position when unset, so this room's crowd is the same people
	// every session rather than re-rolling on each launch.
	const int32 EffectiveSeed = CrowdSeed != 0
		? CrowdSeed
		: (GetTypeHash(FIntVector(Origin)) | 1);

	FRandomStream Stream(EffectiveSeed);

	TArray<FVector> Placed;
	Placed.Reserve(CrowdSize);

	const float SpacingSquared = FMath::Square(MinSpacing);

	// Bounded attempts rather than looping until full: a small box with a large
	// CrowdSize is a authoring mistake that should place what fits and stop, not
	// hang the level load.
	const int32 MaxAttempts = CrowdSize * 24;
	int32 Attempts = 0;

	while (Placed.Num() < CrowdSize && Attempts < MaxAttempts)
	{
		++Attempts;

		FVector Candidate(
			Origin.X + Stream.FRandRange(-Extent.X, Extent.X),
			Origin.Y + Stream.FRandRange(-Extent.Y, Extent.Y),
			Origin.Z);

		// Rejection sampling. Uniform random points cluster, and a crowd with
		// three people inside each other next to a bare patch reads as broken
		// rather than as busy.
		bool bTooClose = false;
		for (const FVector& Existing : Placed)
		{
			if (FVector::DistSquared2D(Existing, Candidate) < SpacingSquared)
			{
				bTooClose = true;
				break;
			}
		}

		if (bTooClose)
		{
			continue;
		}

		if (bSnapToGround)
		{
			FHitResult Hit;
			const FVector TraceStart = Candidate + FVector(0.0f, 0.0f, Extent.Z);
			const FVector TraceEnd = Candidate - FVector(0.0f, 0.0f, Extent.Z + 2000.0f);

			FCollisionQueryParams Params;
			Params.AddIgnoredActor(this);

			if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
			{
				Candidate.Z = Hit.Location.Z;
			}
			else
			{
				// Nothing underneath - a hole, or the box hanging off the edge of
				// the floor. Skip rather than leave somebody standing on air.
				continue;
			}
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnParams.Owner = this;

		ADCCCrowdCrawler* Crawler = World->SpawnActor<ADCCCrowdCrawler>(
			CrowdClass, Candidate, FRotator::ZeroRotator, SpawnParams);

		if (!Crawler)
		{
			continue;
		}

		// Each gets its own seed from the shared stream, so the crowd as a whole
		// is reproducible while no two members are the same person.
		Crawler->SetSeed(Stream.RandRange(1, TNumericLimits<int32>::Max() - 1));

		Placed.Add(Candidate);
	}

	UE_LOG(LogTemp, Log, TEXT("[DCC Crowd] %s placed %d of %d requested."),
		*GetActorNameOrLabel(), Placed.Num(), CrowdSize);
}
