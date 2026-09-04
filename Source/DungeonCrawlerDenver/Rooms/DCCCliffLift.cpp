#include "Rooms/DCCCliffLift.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ADCCCliffLift::ADCCCliffLift()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	SetRootComponent(PlatformMesh);
	PlatformMesh->SetMobility(EComponentMobility::Movable);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PlatformMesh->SetCollisionObjectType(ECC_WorldDynamic);
	PlatformMesh->SetCollisionResponseToAllChannels(ECR_Block);

	// Idle at either end costs nothing worth ticking for - only the rise itself does.
	SetActorTickEnabled(false);
}

void ADCCCliffLift::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCCliffLift, bIsRising);
	DOREPLIFETIME(ADCCCliffLift, bHasRisen);
	DOREPLIFETIME(ADCCCliffLift, RiseStartServerTime);
}

void ADCCCliffLift::BeginPlay()
{
	Super::BeginPlay();

	StartZ = GetActorLocation().Z;
}

void ADCCCliffLift::RequestRaise()
{
	if (!HasAuthority() || bIsRising || bHasRisen)
	{
		return;
	}

	bIsRising = true;
	RiseStartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// The server does not receive its own OnRep.
	OnRep_Rising();
}

void ADCCCliffLift::OnRep_Rising()
{
	if (bIsRising)
	{
		SetActorTickEnabled(true);
	}
}

void ADCCCliffLift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsRising || !GetWorld())
	{
		return;
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - RiseStartServerTime;
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(RaiseDurationSeconds, 0.01f), 0.0f, 1.0f);

	FVector Location = GetActorLocation();
	Location.Z = FMath::Lerp(StartZ, StartZ + RaiseHeight, Alpha);
	SetActorLocation(Location, /*bSweep=*/true);

	if (Alpha >= 1.0f)
	{
		// Every machine reaches this independently once its own Lerp finishes, so
		// the tick can just stop itself locally - only the server needs to touch
		// the replicated state, which then settles bIsRising=false everywhere else
		// on its own.
		SetActorTickEnabled(false);

		if (HasAuthority())
		{
			bIsRising = false;
			bHasRisen = true;
		}
	}
}
