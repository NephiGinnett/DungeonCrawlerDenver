#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCLootStage.generated.h"

class UPointLightComponent;
class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * The loot box screen's little photo studio (2026-11-16, Zoe: "an actual chest that shakes and then glows instead of
 * the icon, and similarly show the actual items awarded instead of just an icon representing them"). A real chest
 * and a few item booths, far below the world, each with its own camera writing to a texture the HUD draws. Local to
 * this machine: nothing here is replicated or seen by anyone else.
 *
 *  - The idle chest: closed, turning slowly. The row of boxes to choose from.
 *  - The reveal chest: shakes harder and harder, glows in the box's colour, then bursts open (PoseReveal).
 *  - Booths: one item each, photographed from its real model (Photo).
 */
UCLASS(NotPlaceable, Transient)
class DUNGEONCRAWLERDENVER_API ADCCLootStage : public AActor
{
	GENERATED_BODY()

public:
	ADCCLootStage();
	virtual void Tick(float DeltaSeconds) override;

	/** The cameras run only while the loot screen is open. */
	void SetActive(bool bActive);

	/** The reveal chest at T seconds into an opening that bursts at BurstAt, glowing in Colour. */
	void PoseReveal(float T, float BurstAt, const FLinearColor& Colour);

	UTextureRenderTarget2D* IdleImage() const { return IdleTarget; }
	UTextureRenderTarget2D* RevealImage() const { return RevealTarget; }
	UTextureRenderTarget2D* BoothImage(int32 Slot) const { return BoothTargets.IsValidIndex(Slot) ? BoothTargets[Slot].Get() : nullptr; }

	static constexpr int32 BoothCount = 6;
	/** Puts Mesh in booth Slot, framed to fit, and returns that booth's picture. Null clears the booth. */
	UTextureRenderTarget2D* Photo(int32 Slot, UStaticMesh* Mesh);

private:
	void Frame(USceneCaptureComponent2D* Camera, UStaticMeshComponent* Subject, float Margin, UPointLightComponent* Key = nullptr) const;
	USceneCaptureComponent2D* MakeCamera(const TCHAR* Name, UTextureRenderTarget2D*& OutTarget, int32 Size);

	UPROPERTY() TObjectPtr<USceneComponent> Root;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> IdleChest;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> IdleCamera;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> IdleTarget;
	UPROPERTY() TObjectPtr<UPointLightComponent> IdleLight;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> RevealChest;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RevealChestOpen;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> RevealCamera;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> RevealTarget;
	UPROPERTY() TObjectPtr<UPointLightComponent> RevealKey;
	UPROPERTY() TObjectPtr<UPointLightComponent> RevealGlow;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BoothMeshes;
	UPROPERTY() TArray<TObjectPtr<USceneCaptureComponent2D>> BoothCameras;
	UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> BoothTargets;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> BoothLights;

	struct FBackdrop { TObjectPtr<USceneCaptureComponent2D> Camera; TObjectPtr<UStaticMeshComponent> Plane; bool bWall; };
	TArray<FBackdrop> Backdrops;
	FVector RevealChestHome = FVector::ZeroVector;
	float IdleSpin = 0.0f;
	bool bActive = false;
};
