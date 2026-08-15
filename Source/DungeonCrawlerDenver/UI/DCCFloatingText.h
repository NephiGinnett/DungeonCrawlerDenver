#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCFloatingText.generated.h"

class UTextRenderComponent;

// -----------------------------------------------------------------------------
// FLOATING COMBAT TEXT
//
// Damage and healing numbers that rise off a character and fade.
//
// Built on UTextRenderComponent rather than a UMG widget on purpose. A widget
// ticker needs a WBP asset authored through the UMG toolset, which is fiddly to
// drive over MCP - its own docs require list_properties before every set and warn
// that guessed property names fail silently. TextRender needs no asset at all, so
// this works the moment it compiles. Swapping to UMG later only changes this
// class, not any call site.
//
// PURELY COSMETIC AND NEVER REPLICATED. Each machine spawns its own, because the
// event that drives it (UDCCStatsComponent::OnHealthChanged) already fires on
// every machine - the server through ApplyDamage, clients through OnRep. Sending
// these over the wire would duplicate them.
// -----------------------------------------------------------------------------

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCFloatingText : public AActor
{
	GENERATED_BODY()

public:
	ADCCFloatingText();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Spawns a number at a world location. Returns null on a dedicated server or
	 * when the amount is zero, so callers do not need to guard either case.
	 *
	 * Positive Delta reads as healing, negative as damage - the same sign
	 * convention OnHealthChanged already uses, so callers pass it straight through.
	 */
	static ADCCFloatingText* Spawn(
		const UObject* WorldContext,
		const FVector& Location,
		int32 Delta,
		bool bOnCrawler);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FloatingText")
	TObjectPtr<UTextRenderComponent> Text;

	/** How long the number lives before it is destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FloatingText", meta = (ClampMin = "0.1"))
	float LifeSeconds = 1.2f;

	/** Units per second the number drifts upward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FloatingText")
	float RiseSpeed = 90.0f;

protected:
	virtual void BeginPlay() override;

private:
	/** Seconds since spawn, for the rise and fade curves. */
	float ElapsedSeconds = 0.0f;

	FLinearColor BaseColor = FLinearColor::White;
};
