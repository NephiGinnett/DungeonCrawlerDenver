#include "Arcade/DCCArcadeFrontDoor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/DCCHUD.h"

namespace
{
	// The crawler stands here, turned to the camera (ClientTick keeps it turned:
	// a start rotation alone was not enough, something turns the pawn after spawn).
	const FVector StandAt(0.0f, 0.0f, 100.0f);
}

ADCCArcadeArena::FSky ADCCArcadeFrontDoor::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -30.0f;
	Sky.SunIntensity = 1.5f;
	Sky.SunColour = FLinearColor(1.0f, 0.8f, 0.7f);
	Sky.SkyIntensity = 0.3f;
	Sky.FogColour = FLinearColor(0.12f, 0.06f, 0.18f);
	Sky.FogDensity = 0.05f;
	return Sky;
}

void ADCCArcadeFrontDoor::GetStartTransforms(TArray<FTransform>& Out) const
{
	Out.Add(FTransform(FRotator(0.0f, 180.0f, 0.0f), GetActorLocation() + StandAt));
}

void ADCCArcadeFrontDoor::BuildLocal()
{
	const FVector O = GetActorLocation();

	// A round-ish stage on a dark floor, a back wall to catch the light.
	Block(O + FVector(0, 0, -20), FVector(6000, 6000, 40), FLinearColor(0.03f, 0.02f, 0.05f));
	Block(O + FVector(0, 0, 4), FVector(420, 420, 8), FLinearColor(0.35f, 0.12f, 0.45f), 45.0f);
	Block(O + FVector(0, 0, 8), FVector(300, 300, 8), FLinearColor(0.10f, 0.07f, 0.14f), 45.0f);
	Block(O + FVector(700, 0, 500), FVector(40, 3000, 1000), FLinearColor(0.07f, 0.04f, 0.10f));
	Block(O + FVector(680, 0, 40), FVector(10, 3000, 6), FLinearColor(1.0f, 0.45f, 0.9f));

	Sign(O + FVector(670, 0, 620), 180.0f, TEXT("NOW ENTERING THE SAFE ROOM"), FColor(255, 120, 230), 48.0f);

	// Key light from the camera side, a warm rim from behind, a cool fill.
	Light(O + FVector(-350, -120, 320), FLinearColor(1.0f, 0.92f, 0.85f), 9000.0f, 900.0f);
	Light(O + FVector(300, 180, 260), FLinearColor(1.0f, 0.5f, 0.85f), 7000.0f, 700.0f);
	Light(O + FVector(-100, 350, 120), FLinearColor(0.45f, 0.6f, 1.0f), 3000.0f, 600.0f);
}

void ADCCArcadeFrontDoor::ClientTick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->GetPawn())
	{
		return;
	}

	if (!StageCamera)
	{
		// Shifted to the side and looking straight down the stage, so the
		// crawler stands in the right half of the frame (the menu is on the
		// left), looking back at the player.
		const FVector From = GetActorLocation() + FVector(-470.0f, -60.0f, 130.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		StageCamera = World->SpawnActor<ACameraActor>(From, FRotator(-4.0f, 0.0f, 0.0f), Params);
		if (StageCamera)
		{
			StageCamera->GetCameraComponent()->SetFieldOfView(55.0f);
			StageCamera->GetCameraComponent()->bConstrainAspectRatio = false;
			LocalActors.Add(StageCamera);
		}
	}

	// Face the camera, every frame. The bench orbits its own camera, so this
	// only fixes the body's facing, never the view.
	if (StageCamera)
	{
		APawn* Pawn = PC->GetPawn();
		const FVector ToCamera = StageCamera->GetActorLocation() - Pawn->GetActorLocation();
		const FRotator Facing(0.0f, ToCamera.Rotation().Yaw, 0.0f);
		if (!Pawn->GetActorRotation().Equals(Facing, 0.5f))
		{
			Pawn->SetActorRotation(Facing);
			PC->SetControlRotation(Facing);
		}
	}

	// Possession resets the view target to the pawn, so keep claiming it,
	// except while the pose bench (opened from the menu) has its own camera.
	const ADCCHUD* Hud = Cast<ADCCHUD>(PC->GetHUD());
	if (Hud && Hud->IsGearBenchOpen())
	{
		return;
	}
	if (StageCamera && PC->GetViewTarget() != StageCamera)
	{
		PC->SetViewTarget(StageCamera);
	}
}
