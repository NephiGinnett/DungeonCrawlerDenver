#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCCRadiationAura.generated.h"

class ADCCCharacterBase;

/**
 * A standing hazard that hurts anyone who lingers near it.
 *
 * Built as a component rather than baked into an enemy because the mechanic is
 * about *place*, not about who is standing there: a leaking barrel, a cracked
 * reactor housing and the Diving Pool all want this and none of them are
 * characters. The Nuclear Family are simply the first things to carry one around.
 *
 * The design intent is a pressure that punishes standing still rather than one
 * that punishes being hit. Every other threat in this game rewards closing to
 * melee and staying there; this one makes that the wrong answer, which is what
 * gives a room built around it a different shape to fight in.
 *
 * DOSE, NOT DAMAGE. Exposure accumulates while you are inside the radius and
 * decays when you leave, and the damage scales with accumulated dose. Walking
 * through is nearly free; holding ground is not. A flat damage-per-second aura
 * would be the same decision every second, which is no decision at all.
 *
 * Server-authoritative. The aura ticks and applies damage only with authority;
 * clients see hit points change through replication like everything else.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCRadiationAura : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCRadiationAura();

	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** How far the contamination reaches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation", meta = (ClampMin = "0.0"))
	float Radius = 550.0f;

	/**
	 * Dose gained per second inside the radius, and lost per second outside it.
	 *
	 * Decay is deliberately slower than gain - backing off should relieve the
	 * pressure without instantly erasing the decision to have stood there.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation", meta = (ClampMin = "0.0"))
	float DoseGainPerSecond = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation", meta = (ClampMin = "0.0"))
	float DoseDecayPerSecond = 0.5f;

	/** Dose is capped here, so a long fight cannot become unsurvivable arithmetic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation", meta = (ClampMin = "1.0"))
	float MaxDose = 8.0f;

	/**
	 * Dose below which nothing happens at all.
	 *
	 * The grace period is what makes walking through the room free and makes
	 * standing in it a choice. Without it the aura would be an invisible tax on
	 * movement and players would learn to hate the room rather than respect it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation", meta = (ClampMin = "0.0"))
	float SafeDose = 2.0f;

	/** Hit points per tick, per point of dose above SafeDose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation", meta = (ClampMin = "0"))
	int32 DamagePerDose = 1;

	/** Seconds between damage applications. Not every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation", meta = (ClampMin = "0.1"))
	float DamageInterval = 1.0f;

	/**
	 * Multiplier applied to everything above, raised by the owner.
	 *
	 * The Nuclear Family use this for grief: each one that dies makes the rest
	 * worse. Kept as a plain scalar so anything else can escalate an aura without
	 * this component knowing why.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Radiation")
	float Intensity = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Radiation")
	void SetIntensity(float NewIntensity) { Intensity = FMath::Max(0.0f, NewIntensity); }

	/** Accumulated dose for one character, 0-MaxDose. Drives the HUD warning. */
	UFUNCTION(BlueprintPure, Category = "Radiation")
	float GetDoseFor(const ADCCCharacterBase* Character) const;

	/**
	 * Highest dose any aura in the world is currently applying to this character.
	 *
	 * Static because the HUD asks the question from the other direction - "how
	 * irradiated am I" - and should not have to find the aura to ask it.
	 */
	UFUNCTION(BlueprintPure, Category = "Radiation")
	static float GetTotalDoseOn(const ADCCCharacterBase* Character);

private:
	/** Per-character exposure. Server-side only; nothing here is replicated. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<ADCCCharacterBase>, float> Doses;

	float TimeUntilDamage = 0.0f;
};
