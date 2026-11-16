#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCRallyThug.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * A Repo Rally car owner who does not take the repo quietly (2026-11-14, Zoe: "a chance for enemies to
 * shoot at you or fight you"). When a hook lands, the owner may come out of the building. A Yeller
 * just screams. A Brawler runs at the truck and beats on it. A Shooter stops at range and fires.
 * The fighters damage the truck's HULL (repaired by sitting at a gas station).
 *
 * What they scream is a Voice Studio line (Pryce, Nia, Holt in impound_lines.json). The plate picks
 * the line, so the same car always has the same owner. Text shows even before anyone has recorded it.
 *
 * Server-driven: ADCCArcadeKart::TickThugs moves it and decides when it swings or fires. It replicates
 * its movement and a Mood, and every machine plays the matching mannequin clip. Driving into one at
 * speed knocks it flat. A shot is a Multicast, so every machine sees the tracer and hears the bang.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCRallyThug : public AActor
{
	GENERATED_BODY()

public:
	ADCCRallyThug();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	enum : uint8 { Brawler = 0, Shooter = 1, Yeller = 2 };
	enum : uint8 { Run = 0, Attack = 1, Down = 2, Stand = 3 };

	/** Brawler, Shooter, or Yeller. Set before FinishSpawning. */
	UPROPERTY(Replicated)
	uint8 Kind = Brawler;

	/** The mark's plate, so the HUD can say whose car this was. */
	UPROPERTY(Replicated)
	FString Plate;

	/**
	 * This machine only. The scream, picked from the plate, and when the caption comes down.
	 * Not replicated: every machine derives the same line from Plate. Empty until TryShout.
	 */
	FString ShoutText;
	double ShoutUntil = 0.0;

	/** Picks the line and plays the recording, once Plate is set. Safe to call every frame. */
	void TryShout();

	/**
	 * The scream for this plate, from the Voice Studio file (Pryce, Nia, Holt).
	 * The same plate always returns the same line. Text is never empty.
	 */
	static void YellFor(const FString& Plate, FString& OutSpeaker, FString& OutRow, FString& OutText);

	UPROPERTY(ReplicatedUsing = OnRep_Mood)
	uint8 Mood = Run;

	/** Server. */
	void SetMood(uint8 NewMood);

	/** Where the muzzle is now (Shooter), or the fists (Brawler). */
	FVector MuzzleLocation() const;

	/**
	 * This machine only: a shot (tracer, muzzle flash, bang) or a punch (a thud and a spark).
	 * ADCCArcadeKart::Multicast_RallyHit calls it on every machine. Kind 0 punch, 1 shot that hit, 2 shot that missed.
	 */
	static void PlayHit(UWorld* World, const FVector& From, const FVector& To, uint8 HitKind);

protected:
	UFUNCTION()
	void OnRep_Mood();

	void ApplyMood();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Gun;

	/** True once BeginPlay put an NPC Forge person in the body (their own skeleton and retargeted clips). */
	bool bForgeBody = false;

	bool bShouted = false;
};
