#include "Rooms/DCCOathgate.h"

#include "Rooms/DCCEncounter.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Characters/DCCEnemyCharacter_Oathgate.h"
#include "Quests/DCCQuestComponent.h"
#include "Quests/DCCQuestTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADCCOathgate::ADCCOathgate()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// A working default rather than an empty array every placement has to fill
	// in first - all three are pure C++ classes (no Blueprint path to guess at
	// or get wrong), so a freshly placed pair is immediately testable. Clear
	// this on whichever gate of a pair is meant to be the exit-only side.
	EnemyPool = {
		ADCCEnemyCharacter_DebtEater::StaticClass(),
		ADCCEnemyCharacter_FinePrint::StaticClass(),
		ADCCEnemyCharacter_Arbiter::StaticClass(),
	};

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(100.0f, 160.0f, 160.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Trigger);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetGenerateOverlapEvents(false);
}

void ADCCOathgate::BeginPlay()
{
	Super::BeginPlay();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCOathgate::HandleTriggerOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &ADCCOathgate::HandleTriggerEndOverlap);
}

void ADCCOathgate::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() < NextUsableTime)
	{
		return;
	}

	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(OtherActor);
	if (!Crawler || !Crawler->IsAlive() || !Crawler->IsCrawler())
	{
		return;
	}

	// Set before the prompt goes out, not after - two players overlapping the
	// same frame must both see the debounce, not just whichever one is
	// processed second. Governs how soon the PROMPT can reappear now, not the
	// teleport itself - walking in, deciding, and walking back in again inside
	// this window just does not re-show it.
	NextUsableTime = World->GetTimeSeconds() + ReuseDelaySeconds;

	// No longer an instant activation - arms the local "Enter the gate?"
	// confirm prompt on whichever client this crawler belongs to instead. See
	// RequestActivation for what used to happen here directly.
	Crawler->Client_ShowOathgatePrompt(this);
}

void ADCCOathgate::HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(OtherActor);
	if (!Crawler)
	{
		return;
	}

	// Gate null closes the prompt - see Client_ShowOathgatePrompt_Implementation.
	// Harmless to send if they already confirmed one way or the other; the
	// prompt is closed by then and this just clears an already-null pointer.
	Crawler->Client_ShowOathgatePrompt(nullptr);
}

void ADCCOathgate::RequestActivation(ADCCCharacterBase* Requester)
{
	if (!Requester || !Requester->IsAlive() || !Requester->IsCrawler())
	{
		return;
	}

	ActivateFor(Requester);
}

void ADCCOathgate::ActivateFor(ADCCCharacterBase* Requester)
{
	UWorld* World = GetWorld();
	if (!World || !Requester)
	{
		return;
	}

	// Fired on either gate of a pair, so "stepped through the Oathgate" is true
	// the moment either side is used - reaching the arena and reaching the way
	// back both count.
	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::ReachLocation, QuestObjectiveTag, 1);

	if (!PairedGate)
	{
		// A gate with nowhere to go is still a valid thing to stand near - it
		// just does nothing. Quieter than refusing to compile a level that has
		// one unpaired for testing.
		return;
	}

	// Gathered around the Requester's OWN position, not the gate's - "their
	// party" means whoever is near them right now, the same escort-radius idea
	// FollowDistance/FollowLeashRadius already use for allies.
	TArray<ADCCCharacterBase*> Party;
	Party.Add(Requester);

	const float RadiusSquared = FMath::Square(PartyGatherRadius);
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Candidate = *It;
		if (!Candidate || Candidate == Requester || !Candidate->IsAlive())
		{
			continue;
		}

		// Not IsCrawler()-only: a room full of enemies auto-arms via occupancy
		// elsewhere in this project, so being explicit that only NON-hostile
		// company gets pulled through the gate matters here too.
		if (Candidate->IsHostileTo(Requester))
		{
			continue;
		}

		if (FVector::DistSquared(Requester->GetActorLocation(), Candidate->GetActorLocation()) <= RadiusSquared)
		{
			Party.Add(Candidate);
		}
	}

	if (!EnemyPool.IsEmpty())
	{
		SpawnArenaEncounter(PairedGate->GetActorLocation());
	}

	const FVector Destination = PairedGate->GetActorLocation();
	const FRotator DestinationRotation = PairedGate->GetActorRotation();

	for (int32 Index = 0; Index < Party.Num(); ++Index)
	{
		// A ring rather than a stack, same reasoning as
		// ADCCIntroDirector::TeleportAllCrawlersToDungeon - the party arrived
		// together and should read as arriving together, not merged into one
		// capsule.
		const float Angle = FMath::DegreesToRadians(Index * (360.0f / FMath::Max(1, Party.Num())));
		const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * ArrivalSpreadRadius;

		Party[Index]->SetActorLocationAndRotation(Destination + Offset, DestinationRotation);
	}
}

void ADCCOathgate::SpawnArenaEncounter(const FVector& Origin) const
{
	UWorld* World = GetWorld();
	if (!World || EnemyPool.IsEmpty())
	{
		return;
	}

	FActorSpawnParameters Params;
	ADCCEncounter* Encounter = World->SpawnActor<ADCCEncounter>(Origin, FRotator::ZeroRotator, Params);
	if (!Encounter)
	{
		return;
	}

	Encounter->FloorNumber = 1;

	const int32 Count = FMath::RandRange(FMath::Min(MinEnemies, MaxEnemies), MaxEnemies);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FDCCEnemySpawn Spawn;
		Spawn.EnemyClass = EnemyPool[FMath::RandRange(0, EnemyPool.Num() - 1)];
		Spawn.Level = SpawnLevel;

		FVector2D Dir2D = FMath::RandPointInCircle(1.0f).GetSafeNormal();
		Spawn.RelativeLocation = FVector(Dir2D.X, Dir2D.Y, 0.0f) * FMath::FRandRange(150.0f, EncounterScatterRadius);

		Encounter->Enemies.Add(Spawn);
	}

	// No ADCCRoom wraps this encounter - there is nothing to reconcile
	// occupancy and call TriggerEncounter for it, so this starts it directly.
	// Encounter->Enemies is treated as wave 0 whenever Waves is left empty.
	Encounter->StartEncounter();
}
