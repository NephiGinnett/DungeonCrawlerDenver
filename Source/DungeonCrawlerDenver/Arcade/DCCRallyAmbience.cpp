#include "Arcade/DCCRallyAmbience.h"

#include "Arcade/DCCArcadeGames.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"
#include "Vehicles/DCCRover.h"

namespace
{
	const int32 Rate = 22050;

	void ToPcm(const TArray<float>& In, TArray<int16>& Out, float Peak)
	{
		float Max = 0.0001f;
		for (float V : In) { Max = FMath::Max(Max, FMath::Abs(V)); }
		Out.SetNumUninitialized(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			Out[i] = int16(FMath::Clamp(In[i] / Max * Peak, -1.0f, 1.0f) * 32000.0f);
		}
	}

	/** A car going past: filtered noise swelling and falling, with a tyre hiss and an engine hum. */
	void PassBy(TArray<int16>& Out)
	{
		FRandomStream R(31);
		const float Len = 1.8f;
		TArray<float> M;
		M.SetNumZeroed(int32(Len * Rate));
		float A = 0.0f, Ph = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / Rate;
			const float Env = FMath::Exp(-FMath::Square((T - 0.8f) / 0.32f));
			A += (R.FRandRange(-1.0f, 1.0f) - A) * 0.12f;
			const float Hz = FMath::Lerp(140.0f, 95.0f, FMath::Clamp((T - 0.6f) / 0.5f, 0.0f, 1.0f)); // the drop as it passes
			Ph += 2.0f * PI * Hz / Rate;
			M[i] = (A * 2.5f + 0.5f * FMath::Sin(Ph) + 0.25f * FMath::Sin(Ph * 2.0f)) * Env;
		}
		ToPcm(M, Out, 0.75f);
	}

	/** A two-tone horn. */
	void Horn(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		const float Len = R.FRandRange(0.25f, 0.7f);
		const float F1 = R.FRandRange(330.0f, 420.0f), F2 = F1 * 1.26f;
		TArray<float> M;
		M.SetNumZeroed(int32(Len * Rate));
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / Rate;
			const float Env = FMath::Min(1.0f, T / 0.02f) * FMath::Min(1.0f, (Len - T) / 0.04f);
			float V = 0.0f;
			for (int32 k = 1; k <= 5; ++k)
			{
				V += (FMath::Sin(2.0f * PI * F1 * k * T) + FMath::Sin(2.0f * PI * F2 * k * T)) / float(k);
			}
			M[i] = V * Env;
		}
		ToPcm(M, Out, 0.6f);
	}

	/** A distant siren: a slow wail. */
	void Siren(TArray<int16>& Out)
	{
		const float Len = 4.5f;
		TArray<float> M;
		M.SetNumZeroed(int32(Len * Rate));
		float Ph = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / Rate;
			const float Hz = 700.0f + 350.0f * FMath::Sin(2.0f * PI * 0.45f * T);
			Ph += 2.0f * PI * Hz / Rate;
			const float Env = FMath::Min(1.0f, T / 0.6f) * FMath::Min(1.0f, (Len - T) / 0.8f);
			M[i] = (FMath::Sin(Ph) + 0.3f * FMath::Sin(Ph * 2.0f)) * Env;
		}
		ToPcm(M, Out, 0.5f);
	}

	struct FBed
	{
		FRandomStream R{ 7 };
		float A = 0.0f, B = 0.0f;
		double T = 0.0;
	};

	USoundWaveProcedural* Wave(UWorld* World, const TArray<int16>& Samples)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(Rate);
		W->NumChannels = 1;
		W->Duration = float(Samples.Num()) / Rate;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		W->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		return W;
	}

	USoundAttenuation* Falloff(float Inner, float Range)
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0, 0);
		Att->Attenuation.FalloffDistance = Range;
		return Att;
	}

	struct FState
	{
		TWeakObjectPtr<UAudioComponent> Bed;
		TStrongObjectPtr<USoundAttenuation> Near, Far;
		TArray<int16> PassPcm, SirenPcm;
		TArray<TArray<int16>> HornPcm;
		TMap<TWeakObjectPtr<ADCCRover>, double> PassedAt;
		TMap<TWeakObjectPtr<ADCCRover>, double> PadCooldown;
		TArray<TWeakObjectPtr<AActor>> GasBeams;
		double Started = -1.0, NextHorn = 0.0, NextSiren = 0.0;
	};
	TMap<TWeakObjectPtr<ADCCArcadeKart>, FState> GStates;
}

void FDCCRallyAmbience::Tick(ADCCArcadeKart* Arena, float DeltaSeconds)
{
	UWorld* World = Arena ? Arena->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	for (auto It = GStates.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
	FState& S = GStates.FindOrAdd(Arena);
	const double Now = World->GetTimeSeconds();

	// --- boost pads (driver's machine and server) -------------------------------------
	for (TActorIterator<ADCCRover> It(World); It; ++It)
	{
		ADCCRover* Car = *It;
		if (!Car->GetPlayerState() || !(Car->IsLocallyControlled() || Arena->HasAuthority()))
		{
			continue;
		}
		if (const double* Until = S.PadCooldown.Find(Car); Until && Now < *Until)
		{
			continue;
		}
		const FVector At = Car->GetActorLocation();
		for (const FBox& Pad : Arena->BoostPads)
		{
			if (Pad.IsInsideXY(At) && At.Z > Pad.Min.Z - 200.0f && At.Z < Pad.Max.Z + 300.0f)
			{
				Car->GrantBoost();
				S.PadCooldown.Add(Car, Now + 1.2);
				break;
			}
		}
	}

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (S.Started < 0.0)
	{
		S.Started = Now;
		S.NextHorn = Now + 4.0;
		S.NextSiren = Now + 25.0;
		S.Near.Reset(Falloff(300.0f, 3500.0f));
		S.Far.Reset(Falloff(1500.0f, 22000.0f));
		PassBy(S.PassPcm);
		Siren(S.SirenPcm);
		for (int32 h = 0; h < 4; ++h)
		{
			Horn(S.HornPcm.AddDefaulted_GetRef(), 50 + h);
		}
	}
	APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FVector Ear = Me ? Me->GetActorLocation() : Arena->GetActorLocation();

	// --- gas stations light up when you are low on fuel (2026-11-13) ----------------------------
	{
		const ADCCRover* MyCar = Cast<ADCCRover>(Me);
		const bool bLow = MyCar && MyCar->RallyFuel >= 0.0f && MyCar->RallyFuel < 35.0f;
		if (S.GasBeams.IsEmpty())
		{
			for (int32 g = 0; g < 2; ++g)
			{
				const FVector2D Off = ADCCArcadeKart::GasOffset(g);
				S.GasBeams.Add(Arena->Shape(TEXT("Cylinder"), Arena->GetActorLocation() + FVector(Off.X, Off.Y, 4000.0f), FVector(260, 260, 8000),
					FLinearColor(0.2f, 1.0f, 0.4f), FRotator::ZeroRotator, false));
			}
		}
		const float Pulse = 0.75f + 0.25f * FMath::Sin(float(Now) * 5.0f);
		for (const TWeakObjectPtr<AActor>& Beam : S.GasBeams)
		{
			if (AActor* B = Beam.Get())
			{
				B->SetActorHiddenInGame(!bLow);
				B->SetActorScale3D(FVector(2.6f * Pulse, 2.6f * Pulse, 80.0f));
			}
		}
	}

	// --- the city bed: traffic rumble a few blocks off, and a high synth hum --------------
	if (!S.Bed.IsValid())
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(Rate);
		W->NumChannels = 1;
		W->Duration = INDEFINITELY_LOOPING_DURATION;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		TSharedPtr<FBed, ESPMode::ThreadSafe> Gen = MakeShared<FBed, ESPMode::ThreadSafe>();
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([Gen](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf;
			Buf.SetNumUninitialized(Samples);
			for (int32 i = 0; i < Samples; ++i)
			{
				Gen->T += 1.0 / Rate;
				const float T = float(Gen->T);
				Gen->A += (Gen->R.FRandRange(-1.0f, 1.0f) - Gen->A) * 0.02f;   // rumble
				Gen->B += (Gen->A - Gen->B) * 0.2f;
				const float Swell = 0.7f + 0.3f * FMath::Sin(T * 0.21f);
				const float Hum = 0.05f * FMath::Sin(2.0f * PI * 110.0f * T) + 0.03f * FMath::Sin(2.0f * PI * 165.0f * T + 0.4f);
				Buf[i] = int16(FMath::Clamp(Gen->B * 7.0f * Swell + Hum, -1.0f, 1.0f) * 28000.0f);
			}
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Samples * sizeof(int16));
		});
		S.Bed = UGameplayStatics::SpawnSound2D(World, W, 0.28f, 1.0f, 0.0f, nullptr, false, false);
	}

	// --- traffic whooshing past ----------------------------------------------------------
	for (TActorIterator<ADCCRover> It(World); It; ++It)
	{
		ADCCRover* Car = *It;
		if (Car == Me || (Car->RallyDuty != 2 && Car->RallyDuty != 3))
		{
			continue;
		}
		const float D = FVector::Dist(Car->GetActorLocation(), Ear);
		const double* Last = S.PassedAt.Find(Car);
		if (D < 1400.0f && (!Last || Now - *Last > 5.0))
		{
			S.PassedAt.Add(Car, Now);
			const float Speed = Car->GetVelocity().Size();
			UGameplayStatics::SpawnSoundAttached(Wave(World, S.PassPcm), Car->GetRootComponent(), NAME_None, FVector::ZeroVector,
				EAttachLocation::KeepRelativeOffset, true, 0.8f, FMath::Clamp(0.8f + Speed / 6000.0f, 0.8f, 1.4f), 0.0f, S.Near.Get());
		}
	}

	// --- horns and sirens, somewhere in the city -----------------------------------------
	TArray<ADCCRover*> Traffic;
	for (TActorIterator<ADCCRover> It(World); It; ++It)
	{
		if (It->RallyDuty == 2) { Traffic.Add(*It); }
	}
	if (Now >= S.NextHorn && Traffic.Num() > 0)
	{
		S.NextHorn = Now + FMath::FRandRange(6.0f, 16.0f);
		ADCCRover* Who = Traffic[FMath::RandRange(0, Traffic.Num() - 1)];
		UGameplayStatics::SpawnSoundAtLocation(World, Wave(World, S.HornPcm[FMath::RandRange(0, S.HornPcm.Num() - 1)]),
			Who->GetActorLocation(), FRotator::ZeroRotator, 0.9f, FMath::FRandRange(0.9f, 1.1f), 0.0f, S.Far.Get());
	}
	if (Now >= S.NextSiren)
	{
		S.NextSiren = Now + FMath::FRandRange(40.0f, 75.0f);
		const float A = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector At = Ear + FVector(FMath::Cos(A), FMath::Sin(A), 0.0f) * FMath::FRandRange(9000.0f, 16000.0f);
		UGameplayStatics::SpawnSoundAtLocation(World, Wave(World, S.SirenPcm), At, FRotator::ZeroRotator, 0.8f, FMath::FRandRange(0.9f, 1.1f), 0.0f, S.Far.Get());
	}

}
