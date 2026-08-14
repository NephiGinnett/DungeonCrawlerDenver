#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCCCarryComponent.generated.h"

class ADCCPickup;
class ADCCCharacterBase;
class UDCCCarryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCCarryChanged, UDCCCarryComponent*, Carry, ADCCPickup*, Pickup);

/**
 * Lets a character pick up, drop and throw props.
 *
 * Same authority shape as skills and attacks: a client may ASK to lift or throw
 * something, but whether it is close enough, light enough, and how hard it flies
 * are all decided on the server. A modified client can request a throw of the
 * heaviest object in the level and still be refused by its own Strength score.
 *
 * On ADCCCharacterBase rather than on the player controller, so that enemies can
 * use it too - a brute that hurls a crate at the party is the same code path,
 * and the DM client inspects it the same way.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCCarryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** How far the character can reach to lift something. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (ClampMin = "0.0"))
	float PickupReach = 250.0f;

	/**
	 * Socket on the character mesh to hold the prop at.
	 *
	 * Falls back to an offset from the capsule when the socket does not exist, so
	 * this works on the stock third-person mannequin without anyone having to
	 * author a socket before the first test.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FName HoldSocketName = TEXT("hand_rSocket");

	/** Where the prop sits when no socket matched. Relative to the character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FVector FallbackHoldOffset = FVector(80.0f, 0.0f, 40.0f);

	/** Upward tilt on a throw, in degrees, so props arc rather than firing flat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float ThrowPitchDegrees = 12.0f;

	/** What this character is currently holding, or null. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CarriedPickup, Category = "Carry")
	TObjectPtr<ADCCPickup> CarriedPickup;

	/** Fires on server and on every client that receives the change. */
	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FDCCCarryChanged OnCarryChanged;

	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsCarrying() const { return CarriedPickup != nullptr; }

	/** Heaviest thing this character could lift right now, per the ruleset. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	float GetCarryMassLimit() const;

	/** Nearest liftable pickup inside PickupReach that is light enough, or null. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	ADCCPickup* FindBestPickupInReach() const;

	/**
	 * Safe to call from a client - routes to the server rather than resolving
	 * locally. Picks up if empty-handed, drops if already carrying, so one key
	 * covers both without the player having to track which.
	 */
	UFUNCTION(BlueprintCallable, Category = "Carry")
	void TryPickupOrDrop();

	/** Safe to call from a client. Does nothing when not carrying. */
	UFUNCTION(BlueprintCallable, Category = "Carry")
	void TryThrow();

protected:
	UFUNCTION(Server, Reliable)
	void Server_PickupOrDrop();

	UFUNCTION(Server, Reliable)
	void Server_Throw();

	UFUNCTION()
	void OnRep_CarriedPickup();

	/** Server only. */
	void DoPickup(ADCCPickup* Pickup);
	void DoDrop();

	/** Attaches or detaches the held prop. Runs on every machine. */
	void ApplyAttachment();

private:
	ADCCCharacterBase* GetOwningCharacter() const;
};
