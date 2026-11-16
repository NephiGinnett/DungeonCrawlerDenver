#include "Items/DCCPickup.h"
#include "Onboarding/DCCOnboardingComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Items/DCCInventoryComponent.h"
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

	// Both are usually set once, immediately after a server-side spawn (a
	// rolled corpse or chest drop overriding the class default) - without
	// replicating them a remote client's copy of this actor never learns what
	// it actually is, which is harmless for GoldValue (the server alone pays
	// out) but was a real bug for EquipmentItemId: ADCCPickup_Equipment reads
	// it in BeginPlay to pick a ground mesh, and a client that never received
	// the real value would show the wrong prop forever.
	DOREPLIFETIME(ADCCPickup, EquipmentItemId);
	DOREPLIFETIME(ADCCPickup, GoldValue);
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

	// Gear goes straight into the pack rather than joining the carry/throw loop
	// or auto-equipping itself - changed 2026-08-30 (see DISPATCH.md). It used
	// to equip on contact with no player choice involved at all, which reads
	// as "I picked it up and it vanished" the moment the slot already held
	// something. UDCCEquipmentComponent::EquipItem is now the one place gear
	// actually equips, called from the Inventory page on request. Consumes
	// itself the moment it is recorded, so it never falls through to the gold
	// path below even if someone also gave it a GoldValue for sell-back.
	if (!EquipmentItemId.IsNone())
	{
		// Captured first: a successful stow destroys this actor.
		const FText ItemName = DisplayName.IsEmpty() ? FText::FromName(EquipmentItemId) : DisplayName;
		const FText Who = Carrier->DisplayName.IsEmpty() ? FText::FromString(GetNameSafe(Carrier)) : Carrier->DisplayName;
		if (Carrier->Inventory && Carrier->Inventory->StowPickup(this))
		{
			// In the feed too, so a pickup is never silent (2026-11-13).
			ADCCGameState::Log(Carrier, EDCCLogCategory::Experience, FText::Format(
				NSLOCTEXT("DCC", "LogGearStowed", "{0} stows {1}."), Who, ItemName));
			return;
		}

		// Pack full: leave it exactly where it was rather than destroying it
		// for nothing - same "nothing is ever lost to a failed stow" rule
		// StowPickup itself already follows. And say so, rather than nothing.
		if (Carrier->Onboarding)
		{
			Carrier->Onboarding->NotifyOwner(FText::Format(
				NSLOCTEXT("DCC", "GearPackFull", "Your bag is full. {0} stays on the floor."), ItemName));
		}
		bIsHeld = false;
		ApplyHeldState();
		return;
	}

	// Pocket change pays out and disappears; anything valuable enough to be worth
	// carrying stays in your hands until the exit. See bBanksOnPickup.
	if (bBanksOnPickup && GoldValue > 0 && !bValueClaimed)
	{
		ClaimValue(Carrier);
		Destroy();
	}
}

int32 ADCCPickup::ClaimValue(ADCCCharacterBase* Claimant)
{
	if (!HasAuthority() || bValueClaimed || GoldValue <= 0 || !Claimant)
	{
		return 0;
	}

	UDCCStatsComponent* ClaimantStats = Claimant->Stats;
	if (!ClaimantStats)
	{
		return 0;
	}

	bValueClaimed = true;
	ClaimantStats->AddGold(GoldValue);

	ADCCGameState::Log(this, EDCCLogCategory::Experience, FText::Format(
		NSLOCTEXT("DCC", "LogLootClaimed", "{0} banks {1} — {2} gold."),
		FText::FromString(GetNameSafe(Claimant)), DisplayName, FText::AsNumber(GoldValue)));

	return GoldValue;
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
		ThrowerStats->GetEnhanced(),
		ImpactDamageDie,
		ImpactSpeed);

	if (Damage > 0)
	{
		VictimStats->ApplyDamage(Damage);
		Thrower->NoteDamageDealtTo(Victim, Damage);
	}
}
