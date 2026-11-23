#include "Characters/DCCSynthAudio.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

namespace DCCSynth
{
	void PlayPcm(UWorld* World, const TArray<int16>& Pcm, int32 Rate, const FVector* At, float Volume, float Pitch, USoundAttenuation* Att)
	{
		if (!World || World->GetNetMode() == NM_DedicatedServer || Pcm.IsEmpty() || Rate <= 0)
		{
			return;
		}
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(Rate);
		Wave->NumChannels = 1;
		Wave->Duration = float(Pcm.Num()) / Rate;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		UAudioComponent* Voice = At
			? UGameplayStatics::SpawnSoundAtLocation(World, Wave, *At, FRotator::ZeroRotator, Volume, Pitch, 0.0f, Att, nullptr, true)
			: UGameplayStatics::SpawnSound2D(World, Wave, Volume, Pitch, 0.0f, nullptr, false, true);
		if (Voice)
		{
			Voice->StopDelayed(Wave->Duration / FMath::Max(0.1f, Pitch) + 0.05f);
		}
	}

	USoundAttenuation* Falloff(float Inner, float Distance)
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
		Att->AddToRoot();
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0, 0);
		Att->Attenuation.FalloffDistance = Distance;
		return Att;
	}
}

namespace
{
	constexpr int32 StepRate = 22050;

	/** A soft scuff: a short, low, dull thud with a little grit on top. */
	const TArray<TArray<int16>>& StepPcm()
	{
		static TArray<TArray<int16>> Steps;
		if (Steps.IsEmpty())
		{
			for (int32 n = 0; n < 5; ++n)
			{
				FRandomStream R(600 + n);
				TArray<float> M;
				M.SetNumZeroed(int32(0.11f * StepRate));
				float Low = 0.0f, Grit = 0.0f;
				const float Hz = R.FRandRange(70.0f, 105.0f);
				for (int32 i = 0; i < M.Num(); ++i)
				{
					const float T = float(i) / StepRate;
					Low += (R.FRandRange(-1.0f, 1.0f) - Low) * 0.06f;
					Grit += (R.FRandRange(-1.0f, 1.0f) - Grit) * 0.5f;
					M[i] = (Low * 3.0f + 0.35f * FMath::Sin(2.0f * PI * Hz * T)) * FMath::Exp(-T / 0.03f)
						+ Grit * 0.12f * FMath::Exp(-T / 0.012f);
				}
				float Max = 0.0001f;
				for (float V : M) { Max = FMath::Max(Max, FMath::Abs(V)); }
				TArray<int16>& Out = Steps.AddDefaulted_GetRef();
				Out.SetNumUninitialized(M.Num());
				for (int32 i = 0; i < M.Num(); ++i) { Out[i] = int16(M[i] / Max * 0.6f * 32000.0f); }
			}
		}
		return Steps;
	}

	struct FStepState
	{
		FVector Last = FVector::ZeroVector;
		float Walked = 0.0f;
		bool bInit = false;
	};
}

namespace DCCFootsteps
{
	void Tick(ADCCCharacterBase* Character, float DeltaSeconds)
	{
		UWorld* World = Character ? Character->GetWorld() : nullptr;
		if (!World || World->GetNetMode() == NM_DedicatedServer || !Character->IsAlive())
		{
			return;
		}
		static TMap<TWeakObjectPtr<ADCCCharacterBase>, FStepState> States;
		if (States.Num() > 400)
		{
			for (auto It = States.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
		}
		FStepState& S = States.FindOrAdd(Character);
		const FVector Here = Character->GetActorLocation();
		if (!S.bInit)
		{
			S.bInit = true;
			S.Last = Here;
			return;
		}
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		const float Moved = FVector::Dist2D(Here, S.Last);
		S.Last = Here;
		if (!Move || !Move->IsMovingOnGround() || Moved > 400.0f) // a teleport is not a step
		{
			S.Walked = 0.0f;
			return;
		}
		const APlayerController* PC = World->GetFirstPlayerController();
		const APawn* Me = PC ? PC->GetPawn() : nullptr;
		FVector Ear = Me ? Me->GetActorLocation() : Here;
		if (PC && PC->PlayerCameraManager)
		{
			Ear = PC->PlayerCameraManager->GetCameraLocation();
		}
		if (FVector::DistSquared(Here, Ear) > FMath::Square(2400.0f))
		{
			return;
		}
		S.Walked += Moved;
		const float Height = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		const float Stride = FMath::Clamp(Height * 1.55f, 110.0f, 320.0f);
		if (S.Walked < Stride)
		{
			return;
		}
		S.Walked = 0.0f;
		static USoundAttenuation* Att = DCCSynth::Falloff(200.0f, 1600.0f);
		const float Big = FMath::Clamp(Height / 90.0f, 0.6f, 2.5f);
		const bool bMine = Character == Me;
		const FVector Feet = Here - FVector(0, 0, Height);
		const TArray<TArray<int16>>& Steps = StepPcm();
		// Quiet: your own steps sit under everything else, other people's a little more present.
		DCCSynth::PlayPcm(World, Steps[FMath::RandRange(0, Steps.Num() - 1)], StepRate, &Feet,
			(bMine ? 0.16f : 0.24f) * FMath::Min(1.8f, Big), FMath::FRandRange(0.92f, 1.08f) / FMath::Sqrt(Big), Att);
	}
}
