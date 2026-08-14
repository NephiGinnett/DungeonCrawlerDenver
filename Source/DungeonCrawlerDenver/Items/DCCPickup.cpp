#include "Items/DCCPickup.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ADCCPickup::ADCCPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	// Needed for HandleMeshHit to fire at all; physics bodies do not report hits
	// unless asked to, and the throw damage rule depends entirely on that event.
	Mesh->SetNotifyRigidBodyCollision(true);
}

void ADCCPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCPickup, bIsHeld);
}

void ADCCPickup::BeginPlay()
{
	Super::BeginPlay();

	if (Mesh)
	{
		Mesh->SetMassOverrideInKg(NAME_None, FMath::Max(0.1f, MassKilograms), true);
		Mesh->OnComponentHit.AddDynamic(this, &ADCCPickup::HandleMeshHit);
	}
}

void ADCCPickup::OnPickedUp(ADCCCharacterBase* Carrier)
{
	if (!HasAuthority() || !Carrier)
	{
		return;
	}

	// A pickup that is still mid-throw must not keep its old attribution once
	// somebody has caught it out of the air.
	bThrowIsLive = false;
	ThrownByCharacter = nullptr;

	bIsHeld = true;
	ApplyHeldState();
}

void ADCCPickup::OnDropped()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsHeld = false;
	ApplyHeldState();

	// Dropped, not thrown: no attribution and no damage. Letting go of a crate
	// onto an enemy's head should not read as an attack.
	bThrowIsLive = false;
	ThrownByCharacter = nullptr;
}

void ADCCPickup::OnThrown(const FVector& Velocity, ADCCCharacterBase* Thrower)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsHeld = false;
	ApplyHeldState();

	ThrownByCharacter = Thrower;
	bThrowIsLive = true;

	if (Mesh)
	{
		Mesh->SetPhysicsLinearVelocity(Velocity);

		// A little spin, purely so a thrown crate reads as thrown rather than as
		// sliding through the air.
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector(0.0f, 180.0f, 0.0f));
	}
}

void ADCCPickup::OnRep_IsHeld()
{
	ApplyHeldState();
}

void ADCCPickup::ApplyHeldState()
{
	if (!Mesh)
	{
		return;
	}

	if (bIsHeld)
	{
		// Collision off entirely while carried: a simulating body parented to a
		// moving character fights the character's own capsule and shoves the
		// carrier around the level.
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// Only the server simulates. Clients receive the resulting transform
		// through replicated movement, so that every machine agrees on where the
		// crate landed rather than each running its own divergent physics.
		Mesh->SetSimulatePhysics(HasAuthority());
	}

	BP_OnCarryStateChanged(bIsHeld);
}

void ADCCPickup::HandleMeshHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority() || !bThrowIsLive || !OtherActor || OtherActor == ThrownByCharacter)
	{
		return;
	}

	ADCCCharacterBase* Victim = Cast<ADCCCharacterBase>(OtherActor);
	if (!Victim || !Victim->IsAlive())
	{
		// Hit the wall or the floor. The throw is spent either way - otherwise a
		// crate that bounced off a pillar would still be armed when it reached
		// someone standing behind it.
		bThrowIsLive = false;
		ThrownByCharacter = nullptr;
		return;
	}

	// Spend the throw before resolving, so a single impact cannot re-enter.
	bThrowIsLive = false;

	ADCCCharacterBase* Thrower = ThrownByCharacter;
	ThrownByCharacter = nullptr;

	UDCCStatsComponent* ThrowerStats = Thrower ? Thrower->Stats : nullptr;
	UDCCStatsComponent* VictimStats = Victim->Stats;
	if (!ThrowerStats || !VictimStats)
	{
		return;
	}

	UDCCRuleset* Ruleset = ThrowerStats->GetRuleset();
	if (!Ruleset)
	{
		return;
	}

	const float ImpactSpeed = Mesh ? Mesh->GetPhysicsLinearVelocity().Size() : 0.0f;

	const int32 Damage = Ruleset->GetThrowDamage(
		ThrowerStats->AbilityScores,
		ImpactDamageDie,
		ImpactSpeed);

	if (Damage > 0)
	{
		VictimStats->ApplyDamage(Damage);
	}
}
