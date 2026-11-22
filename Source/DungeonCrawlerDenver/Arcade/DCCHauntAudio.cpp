// Dead Air's sound (2026-11-16, Zoe: "the mine also needs a lot of background audio!" and Platinum Dust Mine "very low"
// under it). ADCCArcadeHaunt::TickMineAudio, from ClientTick on every machine with a player. All of it is made here in
// code, like Fourteener's and Repo Rally's ambience: no assets but the song.
//
//   the bed     a low, hollow room tone that never stops, deeper and louder the further down you are
//   drips       water plinking near you, every second or two
//   creaks      timbers taking the weight, here and there
//   rumbles     the mountain settling, far off
//   taps        a pick, tick tick tick, somewhere in a drift nobody is in
//   the song    Platinum Dust Mine, very low, looping

#include "Arcade/DCCArcadeHaunt.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rooms/DCCDungeonGrid.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr int32 MineRate = 22050;

	void MineToPcm(const TArray<float>& In, TArray<int16>& Out, float Peak)
	{
		float Max = 0.0001f;
		for (float V : In) { Max = FMath::Max(Max, FMath::Abs(V)); }
		Out.SetNumUninitialized(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			Out[i] = int16(FMath::Clamp(In[i] / Max * Peak, -1.0f, 1.0f) * 32000.0f);
		}
	}

	/** A drop into a puddle: a short pitched plink with a little ring. */
	void MineDrip(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		M.SetNumZeroed(int32(0.5f * MineRate));
		const float Hz = R.FRandRange(900.0f, 1900.0f);
		float Phase = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / MineRate;
			Phase += 2.0f * PI * Hz * (1.0f + 0.6f * FMath::Exp(-T / 0.01f)) / MineRate; // the pitch drops as it lands
			M[i] = FMath::Sin(Phase) * FMath::Exp(-T / 0.06f) + 0.25f * FMath::Sin(Phase * 2.01f) * FMath::Exp(-T / 0.03f);
		}
		MineToPcm(M, Out, 0.45f);
	}

	/** A timber under load: a slow, scraping groan. */
	void MineCreak(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		const float Len = R.FRandRange(0.8f, 1.6f);
		TArray<float> M;
		M.SetNumZeroed(int32(Len * MineRate));
		const float Base = R.FRandRange(70.0f, 140.0f);
		float Phase = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / MineRate;
			const float Env = FMath::Sin(PI * T / Len);
			// Stick-slip: the pitch stutters, so it reads as wood, not a tone.
			const float Hz = Base * (1.0f + 0.25f * FMath::Sin(T * 3.0f)) * (R.FRand() < 0.02f ? 1.5f : 1.0f);
			Phase += 2.0f * PI * Hz / MineRate;
			const float Saw = FMath::Fmod(Phase / (2.0f * PI), 1.0f) * 2.0f - 1.0f;
			M[i] = Saw * Env * (0.6f + 0.4f * FMath::Sin(T * 37.0f));
		}
		MineToPcm(M, Out, 0.4f);
	}

	/** The mountain settling: a long, deep roll with grit. */
	void MineRumble(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		const float Len = 3.5f;
		TArray<float> M;
		M.SetNumZeroed(int32(Len * MineRate));
		float Low = 0.0f, Lower = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / MineRate;
			Low += (R.FRandRange(-1.0f, 1.0f) - Low) * 0.02f;
			Lower += (Low - Lower) * 0.05f;
			const float Env = FMath::Min(1.0f, T / 0.6f) * FMath::Exp(-FMath::Max(0.0f, T - 0.6f) / 1.2f);
			M[i] = (Lower * 10.0f + FMath::Sin(2.0f * PI * 38.0f * T) * 0.3f) * Env;
			if (R.FRand() < 0.002f * Env) { M[i] += R.FRandRange(-0.5f, 0.5f); }
		}
		MineToPcm(M, Out, 0.6f);
	}

	/** A pick on rock, three or four strokes, far away. */
	void MineTaps(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		M.SetNumZeroed(int32(2.6f * MineRate));
		const int32 Strokes = R.RandRange(3, 4);
		for (int32 s = 0; s < Strokes; ++s)
		{
			const int32 From = int32((0.05f + s * R.FRandRange(0.55f, 0.7f)) * MineRate);
			for (int32 i = 0; i < int32(0.25f * MineRate) && From + i < M.Num(); ++i)
			{
				const float T = float(i) / MineRate;
				M[From + i] += (FMath::Sin(2.0f * PI * 2400.0f * T) * 0.5f + FMath::Sin(2.0f * PI * 3700.0f * T) * 0.3f + R.FRandRange(-0.6f, 0.6f)) * FMath::Exp(-T / 0.02f);
			}
		}
		MineToPcm(M, Out, 0.5f);
	}

	USoundWaveProcedural* MineWave(UWorld* World, const TArray<int16>& Samples)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(MineRate);
		W->NumChannels = 1;
		W->Duration = float(Samples.Num()) / MineRate;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		W->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		return W;
	}

	USoundAttenuation* MineFalloff(float Inner, float Falloff)
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0, 0);
		Att->Attenuation.FalloffDistance = Falloff;
		return Att;
	}

	/** The bed, made on the audio thread as the sound asks for more: two bands of filtered noise and a slow hum. */
	struct FMineBed
	{
		FRandomStream R{ 1893 };
		float A = 0.0f, B = 0.0f, C = 0.0f;
		double T = 0.0;
		TAtomic<float> Depth{ 0.0f };
	};

	struct FMineAudio
	{
		TWeakObjectPtr<UAudioComponent> Bed, Song;
		TSharedPtr<FMineBed, ESPMode::ThreadSafe> BedGen;
		TStrongObjectPtr<USoundAttenuation> Near, Far;
		TArray<TArray<int16>> Drips, Creaks, Rumbles, Taps;
		double NextDrip = 0.0, NextCreak = 0.0, NextRumble = 0.0, NextTaps = 0.0, NextSong = 0.0;
		bool bReady = false;
	};
	TMap<TWeakObjectPtr<ADCCArcadeHaunt>, FMineAudio> GMineAudio;
}

void ADCCArcadeHaunt::TickMineAudio(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !Grid)
	{
		return;
	}
	for (auto It = GMineAudio.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
	FMineAudio& S = GMineAudio.FindOrAdd(this);
	const double Now = World->GetTimeSeconds();
	if (!S.bReady)
	{
		S.bReady = true;
		S.Near.Reset(MineFalloff(200.0f, 1800.0f));
		S.Far.Reset(MineFalloff(400.0f, 4500.0f));
		for (int32 i = 0; i < 6; ++i) { MineDrip(S.Drips.AddDefaulted_GetRef(), 200 + i); }
		for (int32 i = 0; i < 4; ++i) { MineCreak(S.Creaks.AddDefaulted_GetRef(), 300 + i); }
		for (int32 i = 0; i < 2; ++i) { MineRumble(S.Rumbles.AddDefaulted_GetRef(), 400 + i); }
		for (int32 i = 0; i < 3; ++i) { MineTaps(S.Taps.AddDefaulted_GetRef(), 500 + i); }
		S.NextDrip = Now + 1.0;
		S.NextCreak = Now + 5.0;
		S.NextRumble = Now + 18.0;
		S.NextTaps = Now + 30.0;
		S.NextSong = Now + 3.0;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	// While the feed plays a post, the mine is heard from that post's camera (2026-11-22): the found footage has its
	// own air, drips and creaks under the sounds the round logged (PlayReelSound). The song stays off for it.
	const bool bReelAir = ReelPhase == 2 && bReelEarValid;
	const FVector Ear = bReelAir ? ReelEar.GetLocation() : (Me ? Me->GetActorLocation() : GetActorLocation());
	const float Deep = DepthAt(Ear);
	const bool bOver = ReelPhase != 0 && !bReelAir; // extraction and the vote: quiet

	// The bed.
	if (!S.Bed.IsValid())
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(MineRate);
		W->NumChannels = 1;
		W->Duration = INDEFINITELY_LOOPING_DURATION;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		S.BedGen = MakeShared<FMineBed, ESPMode::ThreadSafe>();
		TSharedPtr<FMineBed, ESPMode::ThreadSafe> Gen = S.BedGen;
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([Gen](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf;
			Buf.SetNumUninitialized(Samples);
			const float D = Gen->Depth.Load();
			for (int32 i = 0; i < Samples; ++i)
			{
				Gen->T += 1.0 / MineRate;
				const float T = float(Gen->T);
				// Air moving through the drifts: a hollow band of noise that slowly breathes.
				const float Breath = 0.6f + 0.25f * FMath::Sin(T * 0.21f) + 0.15f * FMath::Sin(T * 0.53f + 1.0f);
				Gen->A += (Gen->R.FRandRange(-1.0f, 1.0f) - Gen->A) * 0.04f;
				Gen->B += (Gen->A - Gen->B) * 0.12f;
				const float Hollow = (Gen->A - Gen->B) * 3.0f * Breath;
				// The rock's own rumble, lower the deeper you go.
				Gen->C += (Gen->R.FRandRange(-1.0f, 1.0f) - Gen->C) * 0.006f;
				const float Rock = Gen->C * 14.0f * (0.5f + D);
				// A faint hum, like a pump left running somewhere.
				const float Hum = 0.05f * FMath::Sin(2.0f * PI * 55.0f * T) * (0.4f + 0.6f * D);
				Buf[i] = int16(FMath::Clamp(Hollow + Rock + Hum, -1.0f, 1.0f) * 26000.0f);
			}
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Samples * sizeof(int16));
		});
		S.Bed = UGameplayStatics::SpawnSound2D(World, W, 0.0f, 1.0f, 0.0f, nullptr, false, false);
	}
	if (S.BedGen.IsValid())
	{
		S.BedGen->Depth.Store(Deep);
	}
	if (UAudioComponent* Bed = S.Bed.Get())
	{
		Bed->SetVolumeMultiplier(bOver ? 0.0f : 0.25f + 0.35f * Deep);
	}

	// The song, very low, looping with a breath between plays.
	if (ReelPhase == 0 && Now >= S.NextSong && (!S.Song.IsValid() || !S.Song->IsPlaying()))
	{
		if (USoundBase* Song = LoadObject<USoundBase>(nullptr, TEXT("/Game/Songs/Platinum_Dust_Mine.Platinum_Dust_Mine")))
		{
			S.Song = UGameplayStatics::SpawnSound2D(World, Song, 0.12f, 1.0f, 0.0f, nullptr, false, true);
			S.NextSong = Now + FMath::Max(30.0f, Song->GetDuration()) + 12.0;
		}
		else
		{
			S.NextSong = Now + 600.0; // not imported: the mine still has its own sounds
		}
	}
	if (ReelPhase != 0 && S.Song.IsValid())
	{
		S.Song->FadeOut(2.0f, 0.0f);
		S.Song.Reset();
	}
	if (bOver)
	{
		return;
	}

	// A point in open mine near the ear (or far from it), for the one-shots.
	auto Somewhere = [&](float MinD, float MaxD) -> FVector
	{
		for (int32 Try = 0; Try < 10; ++Try)
		{
			const FVector P = Ear + FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector() * FMath::FRandRange(MinD, MaxD);
			const FIntPoint Cell = Grid->CellOf(P);
			if (Grid->IsOpen(Cell.X, Cell.Y))
			{
				return FVector(P.X, P.Y, GetActorLocation().Z + FMath::FRandRange(150.0f, WallHeight - 40.0f));
			}
		}
		return Ear + FVector(0, 0, 200.0f);
	};
	auto Play = [&](const TArray<int16>& Pcm, const FVector& At, USoundAttenuation* Att, float Volume, float Pitch)
	{
		// In a clip, the point near the camera is carried over to the viewer's ears.
		UGameplayStatics::PlaySoundAtLocation(World, MineWave(World, Pcm), bReelAir ? ReelToListener(At) : At, Volume, Pitch, 0.0f, Att);
	};

	if (Now >= S.NextDrip)
	{
		Play(S.Drips[FMath::RandRange(0, S.Drips.Num() - 1)], Somewhere(150.0f, 900.0f), S.Near.Get(), FMath::FRandRange(0.4f, 0.8f), FMath::FRandRange(0.85f, 1.2f));
		S.NextDrip = Now + FMath::FRandRange(0.5f, 2.2f);
	}
	if (Now >= S.NextCreak)
	{
		Play(S.Creaks[FMath::RandRange(0, S.Creaks.Num() - 1)], Somewhere(300.0f, 1600.0f), S.Near.Get(), FMath::FRandRange(0.4f, 0.75f), FMath::FRandRange(0.8f, 1.15f));
		S.NextCreak = Now + FMath::FRandRange(5.0f, 13.0f);
	}
	if (Now >= S.NextRumble)
	{
		UGameplayStatics::PlaySound2D(World, MineWave(World, S.Rumbles[FMath::RandRange(0, S.Rumbles.Num() - 1)]), 0.35f + 0.4f * Deep, FMath::FRandRange(0.8f, 1.0f));
		S.NextRumble = Now + FMath::FRandRange(18.0f, 40.0f);
	}
	if (Now >= S.NextTaps)
	{
		Play(S.Taps[FMath::RandRange(0, S.Taps.Num() - 1)], Somewhere(1800.0f, 3500.0f), S.Far.Get(), 0.6f, FMath::FRandRange(0.9f, 1.1f));
		S.NextTaps = Now + FMath::FRandRange(25.0f, 60.0f);
	}
}
