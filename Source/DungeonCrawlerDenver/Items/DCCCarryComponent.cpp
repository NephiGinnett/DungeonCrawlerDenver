#include "Items/DCCCarryComponent.h"

#include "Items/DCCPickup.h"
#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Quests/DCCQuestComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

UDCCCarryComponent::UDCCCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDCCCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCCarryComponent, CarriedPickup);
}

ADCCCharacterBase* UDCCCarryComponent::GetOwningCharacter() const
{
	return Cast<ADCCCharacterBase>(GetOwner());
}

float UDCCCarryComponent::GetCarryMassLimit() const
{
	const ADCCCharacterBase* Character = GetOwningCharacter();
	UDCCStatsComponent* Stats = Character ? Character->Stats : nullptr;
	if (!Stats)
	{
		return 0.0f;
	}

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	if (!Ruleset)
	{
		return 0.0f;
	}

	return Ruleset->GetCarryMassLimit(Stats->GetEnhanced());
}

ADCCPickup* UDCCCarryComponent::FindBestPickupInReach() const
{
	const ADCCCharacterBase* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return nullptr;
	}

	const float MassLimit = GetCarryMassLimit();
	const FVector Origin = Character->GetActorLocation();

	ADCCPickup* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (TActorIterator<ADCCPickup> It(World); It; ++It)
	{
		ADCCPickup* Candidate = *It;
		if (!Candidate || !Candidate->bCanBePickedUp || Candidate->IsHeld())
		{
			continue;
		}

		// Strength gate. Skipped rather than picked-and-refused so that a crawler
		// standing between a heavy crate and a light one always gets the one they
		// can actually lift.
		if (Candidate->MassKilograms > MassLimit)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistanceSquared > FMath::Square(PickupReach))
		{
			continue;
		}

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Candidate;
		}
	}

	return Best;
}

void UDCCCarryComponent::TryPickupOrDrop()
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		Server_PickupOrDrop_Implementation();
		return;
	}

	Server_PickupOrDrop();
}

void UDCCCarryComponent::Server_PickupOrDrop_Implementation()
{
	const ADCCCharacterBase* Character = GetOwningCharacter();
	if (!Character || !Character->IsAlive())
	{
		return;
	}

	if (IsCarrying())
	{
		DoDrop();
		return;
	}

	// Target selection is repeated server-side rather than accepting the client's
	// nomination, for the same reason attacks are: otherwise a modified client
	// picks up a crate from across the room.
	if (ADCCPickup* Pickup = FindBestPickupInReach())
	{
		DoPickup(Pickup);
	}
}

void UDCCCarryComponent::DoPickup(ADCCPickup* Pickup)
{
	if (!Pickup || IsCarrying())
	{
		return;
	}

	// Reported by class so a quest can ask for a particular prop - "recover the
	// generator core" - while an empty TargetId counts anything liftable.
	const FName PickupClassName = Pickup->GetClass() ? Pickup->GetClass()->GetFName() : NAME_None;

	ADCCCharacterBase* Character = GetOwningCharacter();

	// Pocket change banks and vanishes; it must never be diverted into a
	// container first. Without this test, equipping a water bottle silently
	// turned every coin on the floor into an inventory item that had to be
	// fished back out to be worth anything - see ADCCPickup::bBanksOnPickup,
	// which exists precisely so small money skips the carry loop entirely.
	const bool bIsPocketChange = Pickup->bBanksOnPickup && Pickup->GoldValue > 0;

	// Small or liquid-tagged items disappear into an equipped container (a
	// water bottle, say) instead of filling the one carry slot a crate would -
	// scooping up something bottle-sized should not cost you both hands.
	if (!bIsPocketChange && Character && Character->Equipment
		&& Character->Equipment->TryStoreInAnyContainer(Pickup))
	{
		UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::Collect, PickupClassName);
		return;
	}

	CarriedPickup = Pickup;
	Pickup->OnPickedUp(Character);

	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::Collect, PickupClassName);

	ApplyAttachment();
	OnCarryChanged.Broadcast(this, Pickup);
}

void UDCCCarryComponent::DoDrop()
{
	ADCCPickup* Pickup = CarriedPickup;
	if (!Pickup)
	{
		return;
	}

	CarriedPickup = nullptr;

	// Detach before handing physics back, so the body does not inherit the
	// character's transform as a teleport on the frame it starts simulating.
	Pickup->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Pickup->OnDropped();

	OnCarryChanged.Broadcast(this, nullptr);
}

ADCCPickup* UDCCCarryComponent::ReleaseCarried()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCCarryComponent::ReleaseCarried called on a client; ignoring."));
		return nullptr;
	}

	ADCCPickup* Pickup = CarriedPickup;
	if (!Pickup)
	{
		return nullptr;
	}

	CarriedPickup = nullptr;
	Pickup->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	OnCarryChanged.Broadcast(this, nullptr);

	return Pickup;
}

void UDCCCarryComponent::TryThrow()
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		Server_Throw_Implementation();
		return;
	}

	Server_Throw();
}

void UDCCCarryComponent::Server_Throw_Implementation()
{
	ADCCCharacterBase* Character = GetOwningCharacter();
	ADCCPickup* Pickup = CarriedPickup;
	if (!Character || !Character->IsAlive() || !Pickup)
	{
		return;
	}

	UDCCStatsComponent* Stats = Character->Stats;
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	if (!Stats || !Ruleset)
	{
		return;
	}

	const float Speed = Ruleset->GetThrowSpeed(Stats->GetEnhanced(), Pickup->MassKilograms);

	// Throw along the character's facing, tilted up so props arc. Using control
	// rotation would aim at wherever the camera happens to point, which for a
	// third-person camera is frequently the floor.
	FRotator ThrowRotation = Character->GetActorRotation();
	ThrowRotation.Pitch += ThrowPitchDegrees;

	const FVector Velocity = ThrowRotation.Vector() * Speed;

	CarriedPickup = nullptr;

	Pickup->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Pickup->OnThrown(Velocity, Character);

	OnCarryChanged.Broadcast(this, nullptr);
}

void UDCCCarryComponent::OnRep_CarriedPickup()
{
	ApplyAttachment();
	OnCarryChanged.Broadcast(this, CarriedPickup);
}

void UDCCCarryComponent::ApplyAttachment()
{
	ADCCCharacterBase* Character = GetOwningCharacter();
	if (!Character || !CarriedPickup)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();

	// Prefer the hand socket, but fall back to the capsule with an offset when it
	// is missing. The stock mannequin has no such socket out of the box, and
	// failing to attach at all would leave the prop floating at the origin.
	if (CharacterMesh && CharacterMesh->DoesSocketExist(HoldSocketName))
	{
		CarriedPickup->AttachToComponent(
			CharacterMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			HoldSocketName);
		return;
	}

	CarriedPickup->AttachToComponent(
		Character->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	CarriedPickup->SetActorRelativeLocation(FallbackHoldOffset);
}
