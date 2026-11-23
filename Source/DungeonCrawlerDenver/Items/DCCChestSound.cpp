#include "Items/DCCChestSound.h"

#include "Characters/DCCSynthAudio.h"

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

	/** A heavy wooden lid landing open against its stop: a dull knock with a low body. */
	void AddThud(TArray<float>& Out, float AtSeconds, float Gain, FRandomStream& R)
	{
		const int32 Start = int32(AtSeconds * Rate);
		float Low = 0.0f;
		for (int32 i = 0; i < int32(0.25f * Rate) && Start + i < Out.Num(); ++i)
		{
			const float T = float(i) / Rate;
			Low += (R.FRandRange(-1.0f, 1.0f) - Low) * 0.15f;
			Out[Start + i] += Gain * ((FMath::Sin(2.0f * PI * 105.0f * T) + 0.5f * FMath::Sin(2.0f * PI * 232.0f * T)) * FMath::Exp(-T * 22.0f)
				+ Low * 2.0f * FMath::Exp(-T * 60.0f));
		}
	}

	/** Coins shifting inside: little bright bells, scattered. */
	void AddCoins(TArray<float>& Out, float AtSeconds, float Seconds, float Gain, FRandomStream& R)
	{
		for (int32 c = 0; c < 9; ++c)
		{
			const int32 Start = int32((AtSeconds + R.FRandRange(0.0f, Seconds)) * Rate);
			const float Hz = R.FRandRange(3600.0f, 6200.0f);
			for (int32 i = 0; i < int32(0.18f * Rate) && Start + i < Out.Num(); ++i)
			{
				const float T = float(i) / Rate;
				Out[Start + i] += Gain * (FMath::Sin(2.0f * PI * Hz * T) + 0.4f * FMath::Sin(2.0f * PI * Hz * 1.51f * T)) * FMath::Exp(-T * 28.0f);
			}
		}
	}

	/** The reward: a quick rising sparkle, the sound of "something good is in here". */
	void AddChime(TArray<float>& Out, float AtSeconds, float Gain)
	{
		const float Notes[] = { 1046.5f, 1318.5f, 1568.0f, 2093.0f };
		for (int32 n = 0; n < 4; ++n)
		{
			const int32 Start = int32((AtSeconds + n * 0.07f) * Rate);
			for (int32 i = 0; i < int32(0.7f * Rate) && Start + i < Out.Num(); ++i)
			{
				const float T = float(i) / Rate;
				const float Env = FMath::Min(1.0f, T / 0.005f) * FMath::Exp(-T * 5.5f);
				Out[Start + i] += Gain * Env * (FMath::Sin(2.0f * PI * Notes[n] * T) + 0.3f * FMath::Sin(2.0f * PI * Notes[n] * 2.0f * T));
			}
		}
	}

	/** A chunky latch: a low clack under the metal ring. */
	void AddLatch(TArray<float>& Out, float AtSeconds, float Gain, FRandomStream& R)
	{
		AddClick(Out, AtSeconds, Gain * 0.6f, R);
		const int32 Start = int32(AtSeconds * Rate);
		for (int32 i = 0; i < int32(0.08f * Rate) && Start + i < Out.Num(); ++i)
		{
			const float T = float(i) / Rate;
			Out[Start + i] += Gain * FMath::Sin(2.0f * PI * 150.0f * T) * FMath::Exp(-T * 45.0f);
		}
	}

	const TArray<int16>& OpenPcm()
	{
		// 2026-11-23 (Zoe: "We need better chest sounds"): a chunky latch, the hinge, the lid hitting its stop, coins
		// shifting inside, and a rising sparkle. About 1.6 s.
		static TArray<int16> Pcm;
		if (Pcm.IsEmpty())
		{
			FRandomStream R(907);
			TArray<float> Mix;
			Mix.Init(0.0f, int32(Rate * 1.6f));
			AddLatch(Mix, 0.00f, 0.6f, R);
			AddLatch(Mix, 0.06f, 0.3f, R);
			AddCreak(Mix, 0.12f, 0.55f, 0.38f, R);
			AddThud(Mix, 0.66f, 0.55f, R);
			AddCoins(Mix, 0.70f, 0.35f, 0.16f, R);
			AddChime(Mix, 0.72f, 0.2f);
			float Peak = 0.01f;
			for (float V : Mix) { Peak = FMath::Max(Peak, FMath::Abs(V)); }
			Pcm.SetNumUninitialized(Mix.Num());
			for (int32 i = 0; i < Mix.Num(); ++i)
			{
				Pcm[i] = int16(FMath::Clamp(Mix[i] / Peak, -1.0f, 1.0f) * 26000.0f);
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
		const FVector Where = At;
		DCCSynth::PlayPcm(World, OpenPcm(), Rate, &Where, 1.0f, FMath::FRandRange(0.96f, 1.04f), Att.Get());
	}
}
