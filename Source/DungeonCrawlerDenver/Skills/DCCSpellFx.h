#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCSpellFx.generated.h"

class ADCCCharacterBase;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FDCCSkillDef;

/**
 * A spell's show (2026-11-22, Zoe: "Do the spells have different animations / spell effects?"). The packs have about
 * six usable particle systems, so every spell shared them. This builds each spell's own effect out of glowing shapes
 * (an emissive material) that move, grow and fade: a beam, a bolt, a stamp that slams down, a fan, shockwave rings,
 * a cage, a dome, hail, an orbit, pillars of light, confetti. Each spell has its colours, its shape, a comic-book word
 * over the caster, and its own cast clip.
 *
 * Cosmetic, local to each machine, never replicated: every machine runs it from UDCCSkillComponent::PlaySkillEffects.
 */
UCLASS(NotPlaceable, Transient)
class DUNGEONCRAWLERDENVER_API ADCCSpellFx : public AActor
{
	GENERATED_BODY()

public:
	ADCCSpellFx();
	virtual void Tick(float DeltaSeconds) override;

	/** Play the show for a cast. Hit is who it landed on (may be empty: a miss still shows). False if the spell has no look. */
	static bool Play(UWorld* World, ADCCCharacterBase* Caster, const FDCCSkillDef& Skill, const TArray<AActor*>& Hit);

	/** The cast animation for a spell (a path under Shared/AnimsFixed), or null for the default cast. */
	static const TCHAR* CastClipFor(FName SkillId);

private:
	struct FPiece
	{
		UStaticMeshComponent* Mesh = nullptr;
		UMaterialInstanceDynamic* Mid = nullptr;
		FVector P0 = FVector::ZeroVector, P1 = FVector::ZeroVector;
		FVector S0 = FVector::OneVector, S1 = FVector::OneVector;
		FRotator Rot = FRotator::ZeroRotator;
		float T0 = 0.0f, T1 = 0.5f;
		FLinearColor Colour = FLinearColor::White;
		float Glow = 6.0f;
		// Orbit instead of a straight line: around P0 at Radius, from angle A0, Spin radians a second, rising RiseZ.
		bool bOrbit = false;
		float Radius = 0.0f, A0 = 0.0f, Spin = 0.0f, RiseZ = 0.0f;
	};
	struct FFlash
	{
		UPointLightComponent* Light = nullptr;
		float T0 = 0.0f, T1 = 0.3f, Peak = 20000.0f;
	};
	struct FWord
	{
		UTextRenderComponent* Text = nullptr;
		FVector P0 = FVector::ZeroVector;
		float T0 = 0.0f, T1 = 1.0f;
	};
	TArray<FPiece> Pieces;
	TArray<FFlash> Flashes;
	TArray<FWord> Words;
	float Age = 0.0f;
	float Life = 0.5f;

	FPiece& AddPiece(const TCHAR* Shape, const FVector& P0, const FVector& P1, const FVector& S0, const FVector& S1,
		float T0, float T1, const FLinearColor& Colour, float Glow = 6.0f, const FRotator& Rot = FRotator::ZeroRotator);
	void AddFlash(const FVector& At, const FLinearColor& Colour, float T0, float T1, float Peak, float Radius);
	void AddWord(const FString& Text, const FVector& At, const FLinearColor& Colour, float T0, float T1, float Size);

	// The shapes.
	void Beam(const FVector& From, const FVector& To, const FLinearColor& A, const FLinearColor& B, float Start, bool bJitter);
	void Bolt(const FVector& From, const FVector& To, const FLinearColor& A, const FLinearColor& B, float Start, float Size);
	void Burst(const FVector& At, const FLinearColor& Colour, float Start, float Size);
	void Stamp(const FVector& At, const FLinearColor& A, const FLinearColor& B, float Start);
	void Fan(const FVector& From, const FVector& Dir, float Range, float HalfAngle, const FLinearColor& A, const FLinearColor& B, int32 Count);
	void Ring(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B, float Start, int32 Pulses);
	/** A thin glowing circle from radius From to To. */
	void RimAt(const FVector& At, float From, float To, float T0, float T1, const FLinearColor& Colour);
	void Cage(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B);
	void Dome(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B);
	void Rain(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B, int32 Count, const TArray<FVector>& Spots);
	void Orbit(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B, int32 Count, float Rise, float Seconds);
	void Pillar(const FVector& At, const FLinearColor& A, const FLinearColor& B, float Start, float Height);
	void Fountain(const FVector& At, int32 Count, float Start);
};
