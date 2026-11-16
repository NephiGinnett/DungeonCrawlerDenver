#include "Arcade/DCCClimbAmbience.h"

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCNeonTraffic.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Engine/PointLight.h"
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
	const int32 Rate = 22050;

	// --- synthesis ---------------------------------------------------------------

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

	/** A bird: a few quick rising chirps, sometimes a trill. */
	void Bird(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		M.SetNumZeroed(int32(1.4f * Rate));
		const int32 Chirps = R.RandRange(2, 6);
		const float Base = R.FRandRange(2600.0f, 4200.0f);
		float At = 0.02f;
		for (int32 c = 0; c < Chirps; ++c)
		{
			const float Len = R.FRandRange(0.05f, 0.12f);
			const float Sweep = R.FRandRange(-0.35f, 0.6f);
			float Phase = 0.0f;
			const int32 From = int32(At * Rate);
			for (int32 i = 0; i < int32(Len * Rate) && From + i < M.Num(); ++i)
			{
				const float T = float(i) / (Len * Rate);
				const float Hz = Base * (1.0f + Sweep * T) * (1.0f + 0.04f * FMath::Sin(T * 60.0f));
				Phase += 2.0f * PI * Hz / Rate;
				M[From + i] += FMath::Sin(Phase) * FMath::Sin(PI * T);
			}
			At += Len + R.FRandRange(0.04f, 0.16f);
		}
		ToPcm(M, Out, 0.5f);
	}

	/** Rock tearing loose: a low groan with grit in it. */
	void Crack(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		M.SetNumZeroed(int32(1.2f * Rate));
		float Low = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / Rate;
			Low += (R.FRandRange(-1.0f, 1.0f) - Low) * 0.03f;
			const float Env = FMath::Min(1.0f, T / 0.05f) * FMath::Exp(-T / 0.5f);
			M[i] = (Low * 6.0f + FMath::Sin(2.0f * PI * 70.0f * T) * 0.4f) * Env;
			if (R.FRand() < 0.004f * Env) { M[i] += R.FRandRange(-1.0f, 1.0f); } // grit
		}
		ToPcm(M, Out, 0.7f);
	}

	/** A rock hitting a ledge: a deep thump, a bang, and debris. */
	void Impact(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		TArray<float> M;
		M.SetNumZeroed(int32(1.6f * Rate));
		float Low = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / Rate;
			Low += (R.FRandRange(-1.0f, 1.0f) - Low) * 0.05f;
			M[i] = FMath::Sin(2.0f * PI * 42.0f * T) * FMath::Exp(-T / 0.25f) * 1.2f
				+ R.FRandRange(-1.0f, 1.0f) * FMath::Exp(-T / 0.05f) * 0.7f
				+ Low * 5.0f * FMath::Exp(-T / 0.6f);
		}
		for (int32 c = 0; c < 40; ++c) // pebbles bouncing away
		{
			const int32 From = int32((0.1f + FMath::Pow(R.FRand(), 1.5f) * 1.3f) * Rate);
			const float Loud = R.FRandRange(0.1f, 0.35f);
			for (int32 i = 0; i < 120 && From + i < M.Num(); ++i)
			{
				M[From + i] += R.FRandRange(-1.0f, 1.0f) * Loud * FMath::Exp(-i / 25.0f);
			}
		}
		ToPcm(M, Out, 0.9f);
	}

	/** A gust: a band of noise that swells past and away. */
	void Whoosh(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		const float Len = 3.0f;
		TArray<float> M;
		M.SetNumZeroed(int32(Len * Rate));
		float A = 0.0f, B = 0.0f;
		for (int32 i = 0; i < M.Num(); ++i)
		{
			const float T = float(i) / Rate;
			const float Cut = 0.02f + 0.1f * FMath::Sin(PI * T / Len);
			A += (R.FRandRange(-1.0f, 1.0f) - A) * Cut;
			B += (A - B) * Cut;
			M[i] = (A - B) * FMath::Pow(FMath::Sin(PI * T / Len), 1.5f);
		}
		ToPcm(M, Out, 0.8f);
	}

	/** Endless wind, made on the audio thread as the sound asks for more. */
	struct FWind
	{
		FRandomStream R{ 14115 };
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

	USoundAttenuation* MakeFalloff(float Inner, float Falloff)
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0, 0);
		Att->Attenuation.FalloffDistance = Falloff;
		return Att;
	}

	// --- per-arena state (this machine only) ----------------------------------------

	struct FState
	{
		TWeakObjectPtr<UAudioComponent> Wind, Song, Fire, SummitFire;
		TStrongObjectPtr<USoundAttenuation> Near, Far, Birds;
		TSet<int32> Cracked, Falling, Landed;
		bool bGust = false;
		double Started = -1.0, NextBird = 0.0, NextSong = 0.0, NextFireCheck = 0.0;
		int32 Seed = 1;
		TArray<int16> CrackPcm, ImpactPcm, WhooshPcm;
		TArray<TArray<int16>> BirdPcm;
	};
	TMap<TWeakObjectPtr<ADCCArcadeClimb>, FState> GStates;
}

void FDCCClimbAmbience::Tick(ADCCArcadeClimb* Arena, float DeltaSeconds)
{
	UWorld* World = Arena ? Arena->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// --- falling: drop, don't glide (every machine, so prediction agrees) ---------------
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		if (UCharacterMovementComponent* Move = It->GetCharacterMovement())
		{
			Move->GravityScale = 1.75f;            // a 6 m drop takes ~0.8 s, not ~1.1 s
			Move->AirControl = 0.12f;              // was steering mid-air like a glider
			Move->FallingLateralFriction = 1.4f;   // sideways momentum bleeds off while falling
			Move->BrakingDecelerationFalling = 450.0f;
		}
	}

	// --- gusts push climbers along the face (2026-11-14: "the wind doesn't blow you anywhere") ---------
	// Applied identically on the server and on the climber's own machine, from the replicated gust state
	// and the shared server clock, so client prediction agrees instead of snapping the climber back.
	// 1.7 m/s, faster than anyone can traverse against it; a Wind Cloak halves it.
	if (Arena->bGustActive && Arena->GustFace >= 0)
	{
		const FVector N = ADCCArcadeClimb::FaceNormal(Arena->GustFace);
		const FVector Along = FVector::CrossProduct(FVector::UpVector, N).GetSafeNormal()
			* (FMath::Sin(float(Arena->ServerTime()) * 0.3f) >= 0.0f ? 1.0f : -1.0f);
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			ADCCCharacterBase* Climber = *It;
			if (!Climber->IsClimbing() || !(Arena->HasAuthority() || Climber->IsLocallyControlled()))
			{
				continue;
			}
			const FVector Out = (Climber->GetActorLocation() - Arena->GetActorLocation()).GetSafeNormal2D();
			if (FVector::DotProduct(Out, N) < 0.6f)
			{
				continue; // not on the gust's face
			}
			const bool bCloak = Climber->Wallet && Climber->Wallet->ClimbBoosts.Contains(TEXT("Wind Cloak"));
			Climber->AddActorWorldOffset(Along * (bCloak ? 85.0f : 170.0f) * DeltaSeconds, true);
		}
	}

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (auto It = GStates.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
	FState& S = GStates.FindOrAdd(Arena);
	const double Now = World->GetTimeSeconds();
	const FVector O = Arena->GetActorLocation();
	if (S.Started < 0.0)
	{
		S.Started = Now;
		S.NextBird = Now + 1.0;
		S.NextSong = Now + 6.0;
		S.Near.Reset(MakeFalloff(300.0f, 3500.0f));
		S.Far.Reset(MakeFalloff(250.0f, 5000.0f)); // was 800/12000: far rocks were as loud as near ones (Zoe)
		S.Birds.Reset(MakeFalloff(400.0f, 4500.0f));
		Crack(S.CrackPcm, 7);
		Impact(S.ImpactPcm, 11);
		Whoosh(S.WhooshPcm, 13);
		for (int32 b = 0; b < 6; ++b)
		{
			Bird(S.BirdPcm.AddDefaulted_GetRef(), 100 + b);
		}
	}

	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FVector Ear = Me ? Me->GetActorLocation() : O;
	const float Height = FMath::Clamp((Ear.Z - O.Z) / ADCCArcadeClimb::TopZ, 0.0f, 1.0f);

	// --- wind: always there, louder the higher you are and when a gust is on --------------
	if (!S.Wind.IsValid())
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(Rate);
		W->NumChannels = 1;
		W->Duration = INDEFINITELY_LOOPING_DURATION;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		TSharedPtr<FWind, ESPMode::ThreadSafe> Gen = MakeShared<FWind, ESPMode::ThreadSafe>();
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([Gen](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf;
			Buf.SetNumUninitialized(Samples);
			for (int32 i = 0; i < Samples; ++i)
			{
				Gen->T += 1.0 / Rate;
				const float T = float(Gen->T);
				const float Swell = 0.55f + 0.3f * FMath::Sin(T * 0.43f) + 0.15f * FMath::Sin(T * 1.27f + 2.0f);
				const float Cut = 0.015f + 0.02f * Swell;
				Gen->A += (Gen->R.FRandRange(-1.0f, 1.0f) - Gen->A) * Cut;
				Gen->B += (Gen->A - Gen->B) * 0.3f;
				Buf[i] = int16(FMath::Clamp(Gen->B * 9.0f * Swell, -1.0f, 1.0f) * 30000.0f);
			}
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Samples * sizeof(int16));
		});
		S.Wind = UGameplayStatics::SpawnSound2D(World, W, 0.0f, 1.0f, 0.0f, nullptr, false, false);
	}
	if (UAudioComponent* Wind = S.Wind.Get())
	{
		// Louder (2026-11-13): it was barely there. A gust on your own face roars.
		const bool bMyFace = Arena->bGustActive && Arena->GustFace >= 0
			&& FVector::DotProduct((Ear - O).GetSafeNormal2D(), ADCCArcadeClimb::FaceNormal(Arena->GustFace)) > 0.6f;
		const float Gust = Arena->bGustActive ? (bMyFace ? 1.2f : 0.4f) : 0.0f;
		Wind->SetVolumeMultiplier(0.35f + 0.9f * Height + Gust);
		if (bMyFace)
		{
			if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Me); Crawler && Crawler->IsClimbing())
			{
				Crawler->AddScreenShake(0.35f, 0.2f); // buffeted
			}
		}
	}

	// --- birds: in the woods and the fort, thinning out as you climb ----------------------
	if (Now >= S.NextBird)
	{
		S.NextBird = Now + FMath::FRandRange(1.2f, 4.5f) + Height * 8.0f;
		if (Height < 0.45f)
		{
			const float A = FMath::FRandRange(0.0f, 2.0f * PI);
			const float Dist = FMath::FRandRange(900.0f, 2600.0f);
			const FVector At(Ear.X + FMath::Cos(A) * Dist, Ear.Y + FMath::Sin(A) * Dist, O.Z + FMath::FRandRange(300.0f, 900.0f));
			const TArray<int16>& Pcm = S.BirdPcm[FMath::RandRange(0, S.BirdPcm.Num() - 1)];
			UGameplayStatics::SpawnSoundAtLocation(World, Wave(World, Pcm), At, FRotator::ZeroRotator,
				0.55f * (1.0f - Height * 1.6f), FMath::FRandRange(0.85f, 1.2f), 0.0f, S.Birds.Get());
		}
	}

	// --- fire: the fort's campfire and the summit brazier --------------------------------
	if (Now >= S.NextFireCheck)
	{
		S.NextFireCheck = Now + 2.0;
		static const TCHAR* FirePath = TEXT("/Game/Beach_house/Demo/StarterContent/Audio/Fire01_Cue.Fire01_Cue");
		USoundBase* FireSound = LoadObject<USoundBase>(nullptr, FirePath);
		if (FireSound)
		{
			if (!S.Fire.IsValid() || !S.Fire->IsPlaying())
			{
				if (const APointLight* Camp = Arena->Campfire.Get())
				{
					S.Fire = UGameplayStatics::SpawnSoundAtLocation(World, FireSound, Camp->GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, S.Near.Get());
				}
			}
			if (!S.SummitFire.IsValid() || !S.SummitFire->IsPlaying())
			{
				S.SummitFire = UGameplayStatics::SpawnSoundAtLocation(World, FireSound, O + FVector(-260, -260, ADCCArcadeClimb::TopZ + 60),
					FRotator::ZeroRotator, 0.9f, 1.0f, 0.0f, S.Near.Get());
			}
		}
	}

	// --- the song, now and then --------------------------------------------------------
	if (Now >= S.NextSong && (!S.Song.IsValid() || !S.Song->IsPlaying()))
	{
		if (USoundBase* Song = LoadObject<USoundBase>(nullptr, TEXT("/Game/Songs/Swept_Away.Swept_Away")))
		{
			S.Song = UGameplayStatics::SpawnSound2D(World, Song, 0.3f, 1.0f, 0.0f, nullptr, false, true);
			if (UAudioComponent* Playing = S.Song.Get())
			{
				Playing->FadeIn(4.0f, 0.3f);
			}
			S.NextSong = Now + FMath::Max(60.0f, Song->GetDuration()) + FMath::FRandRange(150.0f, 260.0f);
		}
		else
		{
			S.NextSong = Now + 600.0;
		}
	}

	// --- rockfall: heard where it is, felt when it lands near you -------------------------
	const double Clock = ADCCNeonTrafficCar::ClockOf(World);
	TSet<int32> Live;
	for (const FDCCRockfall& Rock : Arena->Rocks)
	{
		Live.Add(Rock.Id);
		const float Age = float(Clock - Rock.StartTime);
		if (!S.Cracked.Contains(Rock.Id))
		{
			S.Cracked.Add(Rock.Id);
			UGameplayStatics::SpawnSoundAtLocation(World, Wave(World, S.CrackPcm), Rock.Start, FRotator::ZeroRotator, 0.7f,
				FMath::FRandRange(0.8f, 1.1f), 0.0f, S.Far.Get());
		}
		if (Age < 0.0f)
		{
			continue;
		}
		const float Z = FMath::Max(Rock.StopZ, Rock.Start.Z - 0.5f * ADCCArcadeClimb::Gravity * Age * Age);
		const FVector Now3(Rock.Start.X, Rock.Start.Y, Z);
		if (Me)
		{
			// Passing close on the way down: a little shudder.
			const float Pass = FVector::Dist(Now3, Ear);
			if (Pass < 700.0f && Z > Rock.StopZ)
			{
				if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Me))
				{
					Crawler->AddScreenShake(0.5f * (1.0f - Pass / 700.0f), 0.25f);
				}
			}
		}
		if (Z <= Rock.StopZ && !S.Landed.Contains(Rock.Id))
		{
			S.Landed.Add(Rock.Id);
			UGameplayStatics::SpawnSoundAtLocation(World, Wave(World, S.ImpactPcm), Now3, FRotator::ZeroRotator, 0.8f,
				FMath::FRandRange(0.85f, 1.1f), 0.0f, S.Far.Get());
			const float Dist = FVector::Dist(Now3, Ear);
			if (Dist < 2000.0f)
			{
				if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Me))
				{
					Crawler->AddScreenShake(2.2f * (1.0f - Dist / 2000.0f), 0.7f);
				}
			}
		}
	}
	S.Cracked = S.Cracked.Intersect(Live);
	S.Landed = S.Landed.Intersect(Live);

	// --- gusts: a whoosh, loud if it is your face ----------------------------------------
	if (Arena->bGustActive && !S.bGust && Arena->GustFace >= 0)
	{
		const FVector N = ADCCArcadeClimb::FaceNormal(Arena->GustFace);
		const bool bMine = FVector::DotProduct((Ear - O).GetSafeNormal2D(), N) > 0.6f;
		UGameplayStatics::SpawnSound2D(World, Wave(World, S.WhooshPcm), bMine ? 0.9f : 0.3f, FMath::FRandRange(0.9f, 1.1f));
	}
	S.bGust = Arena->bGustActive;
}
