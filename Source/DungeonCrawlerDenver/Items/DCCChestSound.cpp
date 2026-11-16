#include "Items/DCCChestSound.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr int32 Rate = 22050;

	/** A ringing click: a short noise tick that sets a couple of metal partials ringing. */
	void AddClick(TArray<float>& Out, float AtSeconds, float Gain, FRandomStream& R)
	{
		const int32 Start = int32(AtSeconds * Rate);
		const int32 Len = int32(0.09f * Rate);
		for (int32 i = 0; i < Len && Start + i < Out.Num(); ++i)
		{
			const float T = float(i) / Rate;
			const float Tick = R.FRandRange(-1.0f, 1.0f) * FMath::Exp(-T * 900.0f);
			const float Ring = FMath::Sin(2.0f * PI * 3150.0f * T) * 0.5f + FMath::Sin(2.0f * PI * 4870.0f * T) * 0.3f;
			Out[Start + i] += Gain * (Tick + Ring * FMath::Exp(-T * 60.0f));
		}
	}

	/**
	 * A hinge creak is stick-slip: the wood catches and lets go many times a second, and each slip rings the lid's
	 * body. So: a train of pulses whose rate wanders (the lid speeding up and slowing down), each one exciting two
	 * woody resonances.
	 */
	void AddCreak(TArray<float>& Out, float AtSeconds, float Seconds, float Gain, FRandomStream& R)
	{
		const int32 Start = int32(AtSeconds * Rate);
		const int32 Len = int32(Seconds * Rate);
		float Next = 0.0f;
		for (int32 i = 0; i < Len; ++i)
		{
			const float T = float(i) / Rate;
			if (T < Next)
			{
				continue;
			}
			// Pulse rate: slow catch, faster as the lid swings, slows as it settles.
			const float U = T / Seconds;
			const float Hz = 38.0f + 70.0f * FMath::Sin(PI * U) + R.FRandRange(-6.0f, 6.0f);
			Next = T + 1.0f / Hz;
			const float Env = FMath::Min(1.0f, U / 0.08f) * FMath::Min(1.0f, (1.0f - U) / 0.25f);
			const float Body = 480.0f + 260.0f * U; // the pitch rises a little as the hinge opens
			const int32 PulseLen = int32(0.035f * Rate);
			for (int32 k = 0; k < PulseLen && Start + i + k < Out.Num(); ++k)
			{
				const float Tk = float(k) / Rate;
				const float Decay = FMath::Exp(-Tk * 130.0f);
				const float V = FMath::Sin(2.0f * PI * Body * Tk) * 0.7f + FMath::Sin(2.0f * PI * Body * 2.37f * Tk) * 0.3f
					+ R.FRandRange(-0.15f, 0.15f);
				Out[Start + i + k] += Gain * Env * V * Decay;
			}
		}
	}

	const TArray<int16>& OpenPcm()
	{
		static TArray<int16> Pcm;
		if (Pcm.IsEmpty())
		{
			FRandomStream R(907);
			TArray<float> Mix;
			Mix.Init(0.0f, int32(Rate * 0.95f));
			AddClick(Mix, 0.00f, 0.55f, R);  // the latch
			AddClick(Mix, 0.055f, 0.3f, R);  // ...and it drops back
			AddCreak(Mix, 0.11f, 0.75f, 0.45f, R);
			float Peak = 0.01f;
			for (float V : Mix) { Peak = FMath::Max(Peak, FMath::Abs(V)); }
			Pcm.SetNumUninitialized(Mix.Num());
			for (int32 i = 0; i < Mix.Num(); ++i)
			{
				Pcm[i] = int16(FMath::Clamp(Mix[i] / Peak, -1.0f, 1.0f) * 24000.0f);
			}
		}
		return Pcm;
	}
}

namespace DCCChestSound
{
	void PlayOpen(const UObject* WorldContext, const FVector& At)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		static TStrongObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			Att.Reset(NewObject<USoundAttenuation>(GetTransientPackage()));
			Att->Attenuation.bAttenuate = true;
			Att->Attenuation.bSpatialize = true;
			Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Att->Attenuation.AttenuationShapeExtents = FVector(400.0f, 0, 0);
			Att->Attenuation.FalloffDistance = 2600.0f;
		}
		const TArray<int16>& Pcm = OpenPcm();
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(Rate);
		Wave->NumChannels = 1;
		Wave->Duration = float(Pcm.Num()) / Rate;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		UGameplayStatics::PlaySoundAtLocation(World, Wave, At, 0.9f, FMath::FRandRange(0.94f, 1.06f), 0.0f, Att.Get());
	}
}
