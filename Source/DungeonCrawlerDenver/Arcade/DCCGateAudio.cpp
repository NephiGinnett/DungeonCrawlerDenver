// Gate Run's sound (2026-11-22, Zoe after the first night with Vaughn: "It was so disappointing to go into the
// dungeons and not hear any noise"). ADCCArcadeGate::TickGateAudio, from ClientTick on every machine with a player.
// Made in code like Dead Air's mine (DCCHauntAudio), with the packs' ambient loops layered on top where a theme has a
// fitting one.
//
//   the bed       a room tone per theme: a hum, a band of air, a pulse. Always on, quiet.
//   the loop      a recorded ambience from the packs (cave wind, drips, steam, a hallway), quieter still
//   the details   one-shots near you, per theme: server chirps, drips, clanks, bells, bass thumps, birds, chitter
//   the fight     a drum pulse that comes up while anything hostile is near, and a big hit when a boss is out
//   footsteps     moved to DCCFootsteps (every game has them now)
//   hits          a thump where a blow lands, a whoosh where it misses (OnAttackSeen, every machine)
//   hazards       a sizzle while a patch near you is live

#include "Arcade/DCCArcadeGate.h"

#include "Arcade/DCCGateThemes.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCSynthAudio.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr int32 GateRate = 22050;

	void GateToPcm(const TArray<float>& In, TArray<int16>& Out, float Peak)
	{
		float Max = 0.0001f;
		for (float V : In) { Max = FMath::Max(Max, FMath::Abs(V)); }
		Out.SetNumUninitialized(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			Out[i] = int16(FMath::Clamp(In[i] / Max * Peak, -1.0f, 1.0f) * 32000.0f);
		}
	}

	USoundWaveProcedural* GateWave(UWorld* World, const TArray<int16>& Samples)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(GateRate);
		W->NumChannels = 1;
		W->Duration = float(Samples.Num()) / GateRate;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		W->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		return W;
	}

	USoundAttenuation* GateFalloff(float Inner, float Falloff)
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0, 0);
		Att->Attenuation.FalloffDistance = Falloff;
		return Att;
	}

	void GatePlayAt(UWorld* World, const TArray<int16>& Pcm, const FVector& At, float Volume, float Pitch, USoundAttenuation* Att)
	{
		DCCSynth::PlayPcm(World, Pcm, GateRate, &At, Volume, Pitch, Att);
	}

	void GatePlay2D(UWorld* World, const TArray<int16>& Pcm, float Volume, float Pitch)
	{
		DCCSynth::PlayPcm(World, Pcm, GateRate, nullptr, Volume, Pitch);
	}

	/** A soft step: a short low thud of filtered noise. */
	void MakeStep(TArray<int16>& Out, int32 Seed, float Bright)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		M.SetNumZeroed(int32(0.12f * GateRate));
		float Low = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / GateRate;
			Low += (R.FRandRange(-1.0f, 1.0f) - Low) * (0.08f + 0.3f * Bright);
			M[i] = (Low * 2.0f + 0.5f * FMath::Sin(2.0f * PI * 85.0f * T)) * FMath::Exp(-T / 0.025f);
		}
		GateToPcm(M, Out, 0.5f);
	}

	/** A blow landing: a deep thump with a crack on top. */
	void MakeHit(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		M.SetNumZeroed(int32(0.3f * GateRate));
		float Phase = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / GateRate;
			Phase += 2.0f * PI * (60.0f + 140.0f * FMath::Exp(-T / 0.03f)) / GateRate;
			M[i] = FMath::Sin(Phase) * FMath::Exp(-T / 0.09f) + R.FRandRange(-1.0f, 1.0f) * FMath::Exp(-T / 0.012f) * 0.8f;
		}
		GateToPcm(M, Out, 0.8f);
	}

	/** A swing that catches air. */
	void MakeWhoosh(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		const float Len = 0.28f;
		M.SetNumZeroed(int32(Len * GateRate));
		float A = 0.0f, B = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / GateRate;
			const float Sweep = 0.05f + 0.35f * FMath::Sin(PI * T / Len);
			A += (R.FRandRange(-1.0f, 1.0f) - A) * Sweep;
			B += (A - B) * Sweep;
			M[i] = (A - B) * FMath::Sin(PI * T / Len);
		}
		GateToPcm(M, Out, 0.45f);
	}

	/** One drum hit for the fight pulse: a kick, or (bSnare) a crack. */
	void MakeDrum(TArray<int16>& Out, bool bSnare)
	{
		FRandomStream R(bSnare ? 71 : 72);
		TArray<float> M;
		M.SetNumZeroed(int32(0.35f * GateRate));
		float Phase = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / GateRate;
			if (bSnare)
			{
				Phase += 2.0f * PI * 190.0f / GateRate;
				M[i] = (0.5f * FMath::Sin(Phase) * FMath::Exp(-T / 0.04f)) + R.FRandRange(-1.0f, 1.0f) * FMath::Exp(-T / 0.07f) * 0.7f;
			}
			else
			{
				Phase += 2.0f * PI * (48.0f + 110.0f * FMath::Exp(-T / 0.02f)) / GateRate;
				M[i] = FMath::Sin(Phase) * FMath::Exp(-T / 0.16f);
			}
		}
		GateToPcm(M, Out, bSnare ? 0.5f : 0.9f);
	}

	/** The theme's detail sound. Each theme gets its own little vocabulary of noises. */
	void MakeDetail(TArray<int16>& Out, int32 Theme, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		switch (Theme)
		{
		case 0: // data center: a run of server chirps
		{
			M.SetNumZeroed(int32(0.6f * GateRate));
			const int32 Beeps = R.RandRange(2, 5);
			for (int32 b = 0; b < Beeps; ++b)
			{
				const int32 From = int32(b * 0.11f * GateRate);
				const float Hz = R.FRandRange(1400.0f, 3200.0f);
				for (int32 i = 0; i < int32(0.07f * GateRate) && From + i < M.Num(); ++i)
				{
					const float T = float(i) / GateRate;
					M[From + i] += FMath::Sign(FMath::Sin(2.0f * PI * Hz * T)) * 0.4f * FMath::Min(1.0f, (0.07f - T) / 0.01f);
				}
			}
			break;
		}
		case 1: // storm drain: a drip into deep water
		case 3: // mausoleum: the same, slower and lower
		{
			M.SetNumZeroed(int32(0.6f * GateRate));
			const float Hz = R.FRandRange(600.0f, 1500.0f) * (Theme == 3 ? 0.7f : 1.0f);
			float Phase = 0.0f;
			for (int32 i = 0; i < M.Num(); ++i)
			{
				const float T = float(i) / GateRate;
				Phase += 2.0f * PI * Hz * (1.0f + 0.7f * FMath::Exp(-T / 0.012f)) / GateRate;
				M[i] = FMath::Sin(Phase) * FMath::Exp(-T / 0.09f);
			}
			break;
		}
		case 2: // foundry: a hammer on an anvil, far off
		{
			M.SetNumZeroed(int32(1.2f * GateRate));
			for (int32 i = 0; i < M.Num(); ++i)
			{
				const float T = float(i) / GateRate;
				M[i] = (FMath::Sin(2.0f * PI * 880.0f * T) * 0.5f + FMath::Sin(2.0f * PI * 1320.0f * T) * 0.35f + FMath::Sin(2.0f * PI * 2310.0f * T) * 0.2f
					+ R.FRandRange(-1.0f, 1.0f) * FMath::Exp(-T / 0.004f)) * FMath::Exp(-T / 0.35f);
			}
			break;
		}
		case 4: // after hours: a bass thump through a wall, four on the floor
		{
			M.SetNumZeroed(int32(2.0f * GateRate));
			for (int32 k = 0; k < 4; ++k)
			{
				const int32 From = int32(k * 0.48f * GateRate);
				float Phase = 0.0f;
				for (int32 i = 0; i < int32(0.3f * GateRate) && From + i < M.Num(); ++i)
				{
					const float T = float(i) / GateRate;
					Phase += 2.0f * PI * (50.0f + 60.0f * FMath::Exp(-T / 0.02f)) / GateRate;
					M[From + i] += FMath::Sin(Phase) * FMath::Exp(-T / 0.12f);
				}
			}
			break;
		}
		case 5: // overgrowth: a bird that is not quite a bird
		{
			M.SetNumZeroed(int32(0.7f * GateRate));
			const float Base = R.FRandRange(1800.0f, 3000.0f);
			float Phase = 0.0f;
			for (int32 i = 0; i < M.Num(); ++i)
			{
				const float T = float(i) / GateRate;
				const float Chirp = FMath::Fmod(T, 0.17f);
				Phase += 2.0f * PI * Base * (1.0f + 0.4f * (0.17f - Chirp) / 0.17f) / GateRate;
				M[i] = FMath::Sin(Phase) * FMath::Exp(-Chirp / 0.04f) * (T < 0.6f ? 1.0f : 0.0f);
			}
			break;
		}
		default: // the hive: chitter
		{
			M.SetNumZeroed(int32(0.8f * GateRate));
			for (int32 i = 0; i < M.Num(); ++i)
			{
				const float T = float(i) / GateRate;
				const float Gate = FMath::Sin(2.0f * PI * 38.0f * T) > 0.2f ? 1.0f : 0.0f;
				M[i] = R.FRandRange(-1.0f, 1.0f) * Gate * FMath::Sin(PI * T / 0.8f);
			}
			break;
		}
		}
		GateToPcm(M, Out, 0.5f);
	}

	/** The room tone, made on the audio thread as the sound asks for more. */
	struct FGateBed
	{
		FRandomStream R{ 2026 };
		float A = 0.0f, B = 0.0f, C = 0.0f;
		double T = 0.0;
		float HumHz = 55.0f;
		float Air = 0.04f;     // how bright the band of air is
		float PulseHz = 0.0f;  // a slow throb (the hive breathes, the foundry roars)
	};

	struct FGateAudio
	{
		TWeakObjectPtr<UAudioComponent> Bed, Loop, Sizzle;
		TSharedPtr<FGateBed, ESPMode::ThreadSafe> BedGen;
		TStrongObjectPtr<USoundAttenuation> Near, Far;
		TArray<TArray<int16>> Steps, Hits, Whooshes, Details, Kick, Snare;
		TArray<int16> SizzlePcm;
		TMap<TWeakObjectPtr<ADCCCharacterBase>, FVector> LastFeet;
		TMap<TWeakObjectPtr<ADCCCharacterBase>, float> Stride;
		double NextDetail = 0.0, NextBeat = 0.0, NextLoopCheck = 0.0, NextSizzle = 0.0;
		int32 Beat = 0;
		float Fight = 0.0f;
		int32 Theme = -1;
		bool bReady = false;
	};
	TMap<TWeakObjectPtr<ADCCArcadeGate>, FGateAudio> GGateAudio;

	/** A pack's ambient loop for each theme, or null. Played quietly under the bed. */
	const TCHAR* LoopFor(int32 Theme)
	{
		switch (Theme)
		{
		case 0: return TEXT("/Game/SoulCity/Sound/Wav/AmbientLooping/Room_Hallway04Loop.Room_Hallway04Loop");
		case 1: return TEXT("/Game/SoulCave/Sound/Wav/AmbientLooping/WaterDrips04.WaterDrips04");
		case 2: return TEXT("/Game/SoulCity/Sound/Wav/AmbientLooping/SteamVent03.SteamVent03");
		case 3: return TEXT("/Game/SoulCave/Sound/Wav/AmbientLooping/CaveWind13.CaveWind13");
		case 4: return TEXT("/Game/SoulCity/Sound/Wav/AmbientLooping/Slum_Interior05Loop.Slum_Interior05Loop");
		case 5: return TEXT("/Game/SoulCave/Sound/Wav/AmbientLooping/WaterFountain03.WaterFountain03");
		default: return TEXT("/Game/SoulCity/Sound/Wav/AmbientLooping/SparkLoop02.SparkLoop02");
		}
	}
}

void ADCCArcadeGate::TickGateAudio(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || Seed == 0 || !bDungeonBuilt)
	{
		return;
	}
	for (auto It = GGateAudio.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
	FGateAudio& S = GGateAudio.FindOrAdd(this);
	const double Now = World->GetTimeSeconds();
	const int32 Theme = DCCGateThemes::IndexFor(Seed);
	if (!S.bReady)
	{
		S.bReady = true;
		S.Theme = Theme;
		S.Near.Reset(GateFalloff(250.0f, 2200.0f));
		S.Far.Reset(GateFalloff(500.0f, 5000.0f));
		const float Bright = Theme == 0 || Theme == 2 || Theme == 6 ? 0.6f : 0.15f; // metal and grates vs dirt and stone
		for (int32 i = 0; i < 4; ++i) { MakeStep(S.Steps.AddDefaulted_GetRef(), 100 + i, Bright); }
		for (int32 i = 0; i < 3; ++i) { MakeHit(S.Hits.AddDefaulted_GetRef(), 200 + i); }
		for (int32 i = 0; i < 3; ++i) { MakeWhoosh(S.Whooshes.AddDefaulted_GetRef(), 300 + i); }
		for (int32 i = 0; i < 4; ++i) { MakeDetail(S.Details.AddDefaulted_GetRef(), Theme, 400 + i); }
		MakeDrum(S.Kick.AddDefaulted_GetRef(), false);
		MakeDrum(S.Snare.AddDefaulted_GetRef(), true);
		{
			FRandomStream R(9);
			TArray<float> M;
			M.SetNumZeroed(int32(1.0f * GateRate));
			float A = 0.0f;
			for (int32 i = 0; i < M.Num(); ++i)
			{
				A += (R.FRandRange(-1.0f, 1.0f) - A) * 0.6f;
				M[i] = (R.FRandRange(-1.0f, 1.0f) - A) * (0.6f + 0.4f * FMath::Sin(float(i) / GateRate * 2.0f * PI * 9.0f));
			}
			GateToPcm(M, S.SizzlePcm, 0.35f);
		}
		S.NextDetail = Now + 3.0;
		S.NextBeat = Now;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FVector Ear = Me ? Me->GetActorLocation() : GetActorLocation();

	// The bed.
	if (!S.Bed.IsValid())
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(GateRate);
		W->NumChannels = 1;
		W->Duration = INDEFINITELY_LOOPING_DURATION;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		S.BedGen = MakeShared<FGateBed, ESPMode::ThreadSafe>();
		static const float Hums[7] = { 60.0f, 41.0f, 47.0f, 36.0f, 52.0f, 44.0f, 33.0f };
		static const float Airs[7] = { 0.10f, 0.03f, 0.06f, 0.02f, 0.05f, 0.04f, 0.03f };
		static const float Pulses[7] = { 0.0f, 0.12f, 0.6f, 0.08f, 2.0f, 0.15f, 0.45f };
		S.BedGen->HumHz = Hums[FMath::Clamp(Theme, 0, 6)];
		S.BedGen->Air = Airs[FMath::Clamp(Theme, 0, 6)];
		S.BedGen->PulseHz = Pulses[FMath::Clamp(Theme, 0, 6)];
		TSharedPtr<FGateBed, ESPMode::ThreadSafe> Gen = S.BedGen;
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([Gen](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf;
			Buf.SetNumUninitialized(Samples);
			for (int32 i = 0; i < Samples; ++i)
			{
				Gen->T += 1.0 / GateRate;
				const float T = float(Gen->T);
				const float Throb = Gen->PulseHz > 0.0f ? 0.65f + 0.35f * FMath::Sin(2.0f * PI * Gen->PulseHz * T) : 1.0f;
				Gen->A += (Gen->R.FRandRange(-1.0f, 1.0f) - Gen->A) * Gen->Air;
				Gen->B += (Gen->A - Gen->B) * 0.15f;
				const float AirBand = (Gen->A - Gen->B) * 2.5f;
				Gen->C += (Gen->R.FRandRange(-1.0f, 1.0f) - Gen->C) * 0.005f;
				const float Rumble = Gen->C * 10.0f;
				const float Hum = 0.08f * (FMath::Sin(2.0f * PI * Gen->HumHz * T) + 0.4f * FMath::Sin(2.0f * PI * Gen->HumHz * 2.0f * T));
				Buf[i] = int16(FMath::Clamp((AirBand + Rumble + Hum) * Throb, -1.0f, 1.0f) * 24000.0f);
			}
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Samples * sizeof(int16));
		});
		S.Bed = UGameplayStatics::SpawnSound2D(World, W, 0.3f, 1.0f, 0.0f, nullptr, false, false);
	}

	// The pack loop, restarted if it was not authored to loop.
	if (Now >= S.NextLoopCheck)
	{
		S.NextLoopCheck = Now + 1.0;
		if (!S.Loop.IsValid() || !S.Loop->IsPlaying())
		{
			if (USoundBase* Loop = LoadObject<USoundBase>(nullptr, LoopFor(Theme)))
			{
				S.Loop = UGameplayStatics::SpawnSound2D(World, Loop, 0.22f, 1.0f, 0.0f, nullptr, false, true);
			}
		}
	}

	// Who is near, and is anything hostile among them?
	const ADCCCharacterBase* MyCrawler = Cast<ADCCCharacterBase>(Me);
	bool bHostileNear = false, bBossNear = false;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* C = *It;
		if (!C || !C->IsAlive())
		{
			continue;
		}
		const float Dist = FVector::Dist(C->GetActorLocation(), Ear);
		if (MyCrawler && C != MyCrawler && MyCrawler->IsHostileTo(C) && Dist < 2600.0f)
		{
			bHostileNear = true;
			// The Gatekeeper's name is the only boss mark that reaches every machine (DisplayName is replicated).
			bBossNear |= C->DisplayName.ToString().Contains(TEXT("Gatekeeper"));
		}

	}

	// The fight: a drum pulse that fades in with a hostile near and out after.
	S.Fight = FMath::FInterpTo(S.Fight, bHostileNear ? 1.0f : 0.0f, DeltaSeconds, bHostileNear ? 1.5f : 0.4f);
	if (S.Fight > 0.05f && Now >= S.NextBeat)
	{
		const double BeatLength = bBossNear ? 60.0 / 132.0 : 60.0 / 112.0;
		S.NextBeat = Now + BeatLength;
		const int32 Step = S.Beat++ % 8;
		const float V = 0.55f * S.Fight;
		if (Step == 0 || Step == 3 || Step == 4 || (bBossNear && Step == 6))
		{
			GatePlay2D(World, S.Kick[0], V, bBossNear ? 0.9f : 1.0f);
		}
		if (Step == 2 || Step == 6)
		{
			GatePlay2D(World, S.Snare[0], V * 0.6f, FMath::FRandRange(0.97f, 1.03f));
		}
	}
	else if (S.Fight <= 0.05f)
	{
		S.NextBeat = Now;
		S.Beat = 0;
	}

	// A detail somewhere nearby.
	if (Now >= S.NextDetail)
	{
		const FVector At = Ear + FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector() * FMath::FRandRange(500.0f, 1800.0f) + FVector(0, 0, 150.0f);
		GatePlayAt(World, S.Details[FMath::RandRange(0, S.Details.Num() - 1)], At,
			FMath::FRandRange(0.35f, 0.7f), FMath::FRandRange(0.85f, 1.15f), S.Far.Get());
		S.NextDetail = Now + FMath::FRandRange(2.5f, 7.0f) * (S.Fight > 0.5f ? 2.0f : 1.0f);
	}

	// A hazard patch fizzing near you while it is live.
	const double ServerNow = ServerTime();
	float Closest = 1e9f;
	FVector HazardAt = FVector::ZeroVector;
	for (const FHazard& H : Hazards)
	{
		if (HazardState(H, ServerNow) == 0)
		{
			continue;
		}
		const float D = FVector::Dist2D(H.Center, Ear);
		if (D < Closest)
		{
			Closest = D;
			HazardAt = H.Center;
		}
	}
	if (Closest < 1400.0f)
	{
		if (Now >= S.NextSizzle)
		{
			S.NextSizzle = Now + 0.95;
			GatePlayAt(World, S.SizzlePcm, HazardAt, 0.4f, FMath::FRandRange(0.8f, 1.2f), S.Near.Get());
		}
	}
}

void ADCCArcadeGate::OnAttackSeen(ADCCCharacterBase* Attacker, ADCCCharacterBase* Target, bool bHit)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !Attacker)
	{
		return;
	}
	FGateAudio* S = GGateAudio.Find(this);
	if (!S || S->Hits.IsEmpty() || S->Whooshes.IsEmpty())
	{
		return;
	}
	if (bHit && Target)
	{
		GatePlayAt(World, S->Hits[FMath::RandRange(0, S->Hits.Num() - 1)], Target->GetActorLocation(),
			0.8f, FMath::FRandRange(0.85f, 1.15f), S->Near.Get());
	}
	else
	{
		GatePlayAt(World, S->Whooshes[FMath::RandRange(0, S->Whooshes.Num() - 1)],
			Attacker->GetActorLocation() + Attacker->GetActorForwardVector() * 80.0f, 0.6f, FMath::FRandRange(0.9f, 1.2f), S->Near.Get());
	}
}
