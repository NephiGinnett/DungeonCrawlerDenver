#include "Arcade/DCCCasino.h"

#include "Arcade/DCCArcade.h"
#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCWalletComponent.h"
#include "Progression/DCCAchievementComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Session/DCCProfile.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StrongObjectPtr.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	const FLinearColor CarpetGold(0.72f, 0.55f, 0.12f);
	const FLinearColor Felt(0.04f, 0.22f, 0.10f);
	const FLinearColor Wood(0.30f, 0.15f, 0.06f);
	const FLinearColor Ink(0.06f, 0.04f, 0.08f);

	FLinearColor CarpetColour(int32 X, int32 Y, bool bCasino)
	{
		const int32 H = FMath::Abs(X * 3 + Y * 5);
		const FLinearColor Burgundy(0.40f, 0.05f, 0.09f);
		const FLinearColor Navy(0.06f, 0.08f, 0.22f);
		const FLinearColor Wine(0.26f, 0.04f, 0.12f);
		const FLinearColor Teal(0.04f, 0.26f, 0.28f);
		const FLinearColor Plum(0.22f, 0.06f, 0.18f);
		if (bCasino)
		{
			switch (H % 6)
			{
			case 0: return CarpetGold;
			case 1: return Burgundy;
			case 2: return Navy;
			case 3: return Wine;
			case 4: return Plum;
			default: return Teal;
			}
		}
		// Brighter repeat, the arcade-and-movie-palace mix.
		switch (H % 8)
		{
		case 0: return FLinearColor(0.55f, 0.08f, 0.10f);
		case 1: return Navy;
		case 2: return FLinearColor(0.75f, 0.55f, 0.08f);
		case 3: return Burgundy;
		case 4: return FLinearColor(0.10f, 0.18f, 0.48f);
		case 5: return Wine;
		case 6: return FLinearColor(0.08f, 0.34f, 0.16f);
		default: return Teal;
		}
	}

	const TCHAR* GameTitle(int32 Table)
	{
		switch (Table)
		{
		case 0: return TEXT("Skee-Ball");
		case 1: return TEXT("Air Hockey");
		case 2: return TEXT("Roulette");
		case 3: return TEXT("Blackjack");
		case 4: return TEXT("The Wheel");
		default: return TEXT("Casino");
		}
	}

	const int32 WheelPay[] = { 1, 2, 5, 10, 20, 0 };
	const TCHAR* WheelWord[] = { TEXT("1"), TEXT("2"), TEXT("5"), TEXT("10"), TEXT("20"), TEXT("Bust") };
	const FLinearColor PitColours[] = {
		FLinearColor(1.0f, 0.15f, 0.2f), FLinearColor(1.0f, 0.75f, 0.1f), FLinearColor(0.2f, 0.45f, 1.0f),
		FLinearColor(0.15f, 0.8f, 0.35f), FLinearColor(0.7f, 0.2f, 0.9f), FLinearColor(0.10f, 0.10f, 0.12f),
	};
	const FLinearColor RingColours[] = {
		CarpetGold, FLinearColor(0.75f, 0.12f, 0.10f), FLinearColor(0.15f, 0.35f, 0.90f),
		FLinearColor(0.10f, 0.62f, 0.28f), FLinearColor(0.92f, 0.92f, 0.88f),
	};

	void Tint(AActor* Actor, const FLinearColor& Colour)
	{
		UStaticMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		UMaterialInstanceDynamic* Mid = Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr;
		if (Mid)
		{
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
			Mid->SetVectorParameterValue(TEXT("Emissive Color"), Colour);
		}
	}

	void Say(ATextRenderActor* Sign, const FString& Text, const FColor& Colour)
	{
		if (UTextRenderComponent* Render = Sign ? Sign->GetTextRender() : nullptr)
		{
			Render->SetText(FText::FromString(Text));
			Render->SetTextRenderColor(Colour);
		}
	}

	/** A slice lying in the YZ plane, face toward the room (-X). Degrees, 0 is up. */
	FRotator WallSlice(float Degrees)
	{
		return FRotator(90.0f - Degrees, 90.0f, 90.0f);
	}

	FVector WallRadial(float Degrees, float Radius)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FVector(0.0f, FMath::Sin(A) * Radius, FMath::Cos(A) * Radius);
	}

	/** Flat on a table, long axis pointing out from the hub. Degrees, 0 is +X. */
	FRotator TableSlice(float Degrees)
	{
		return FRotator(0.0f, Degrees, 0.0f);
	}

	int32 SkeePoints(double Mark)
	{
		const double FromCentre = FMath::Abs(Mark - 0.5);
		if (FromCentre < 0.05) return 50;
		if (FromCentre < 0.12) return 40;
		if (FromCentre < 0.20) return 30;
		if (FromCentre < 0.32) return 20;
		return 10;
	}

	// ---- The three show games, rebuilt 2026-11-21 (Zoe: "The wheel and roulette both need work, and ski ball could use
	// a bit more, like seeing the ball go up the table instead of just thrown. It should all be a lot more fun").

	constexpr float WheelSpinTime = 5.6f;
	constexpr float RouletteSpinTime = 6.4f;
	constexpr float WheelHubZ = 450.0f;
	constexpr float RouletteTop = 86.0f;
	constexpr int32 WheelSliceCount = 12;

	/** Twelve slices, each pay twice: slice j pays WheelPay[j % 6]. */
	FLinearColor SliceColour(int32 Slice)
	{
		return (Slice % 6) == 5 ? FLinearColor(0.20f, 0.20f, 0.24f) : PitColours[Slice % 6];
	}

	FLinearColor WheelBulbColour(int32 Bulb)
	{
		return (Bulb % 2) ? FLinearColor(1.0f, 0.82f, 0.45f) : FLinearColor(1.0f, 0.25f, 0.35f);
	}

	/** A single-zero wheel in the usual order, so red and black alternate round the rim. */
	const int32 RouletteOrder[37] = { 0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10, 5, 24, 16,
		33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26 };

	int32 RoulettePocketOf(int32 Number)
	{
		for (int32 p = 0; p < 37; ++p)
		{
			if (RouletteOrder[p] == Number)
			{
				return p;
			}
		}
		return 0;
	}

	// ---- Reels and scratchcards (2026-11-21, Zoe: "It'd be nice if you actually got to pull a lever to spin the wheel
	// or something. Or if we could have scratchcards and reels or something").
	constexpr int32 SlotSymbols = 6;
	const TCHAR* SlotSymbolName[SlotSymbols] = { TEXT("cherries"), TEXT("lemons"), TEXT("bells"), TEXT("bars"), TEXT("diamonds"), TEXT("sevens") };
	const int32 SlotWeight[SlotSymbols] = { 30, 26, 18, 12, 9, 5 };
	const int32 SlotThree[SlotSymbols] = { 3, 4, 6, 10, 20, 50 };
	/** The pull takes this long before anything turns, then the reels stop one by one. */
	constexpr float SlotLeverLead = 0.3f;
	constexpr float SlotReelStop0 = 1.35f;
	constexpr float SlotReelGap = 0.45f;
	constexpr float SlotSpinEnd = SlotLeverLead + SlotReelStop0 + SlotReelGap * 2.0f;
	const FLinearColor SlotTopColours[] = {
		FLinearColor(1.0f, 0.25f, 0.6f), FLinearColor(0.2f, 0.8f, 1.0f), FLinearColor(1.0f, 0.75f, 0.15f), FLinearColor(0.45f, 1.0f, 0.3f),
	};
	const int32 CardAmounts[6] = { 1, 2, 5, 10, 25, 100 };
	const int32 CardWeights[6] = { 40, 28, 18, 9, 4, 1 };

	int32 Weighted(const int32* Weights, int32 Count)
	{
		int32 Total = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			Total += Weights[i];
		}
		int32 Roll = FMath::RandRange(0, Total - 1);
		for (int32 i = 0; i < Count; ++i)
		{
			if (Roll < Weights[i])
			{
				return i;
			}
			Roll -= Weights[i];
		}
		return 0;
	}

	int32 SlotMultiplier(uint8 A, uint8 B, uint8 C, FString& Why)
	{
		if (A == B && B == C)
		{
			Why = A == 5 ? FString(TEXT("JACKPOT, three sevens")) : FString::Printf(TEXT("three %s"), SlotSymbolName[A]);
			return SlotThree[A];
		}
		if (A == B || B == C || A == C)
		{
			const uint8 Pair = (A == B || A == C) ? A : B;
			Why = FString::Printf(TEXT("two %s"), SlotSymbolName[Pair]);
			return Pair == 0 ? 2 : 1;
		}
		Why = TEXT("Nothing lined up.");
		return 0;
	}

	/** Out from a reel's axle at this angle. 0 faces the player (-Y); the reels roll down, toward negative angles. */
	FVector ReelRadial(float Degrees)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FVector(0.0f, -FMath::Cos(A), FMath::Sin(A));
	}

	FVector ReelUp(float Degrees)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FVector(0.0f, FMath::Sin(A), FMath::Cos(A));
	}

	/** A lever's angle, T seconds after the pull: down fast, a beat, then a springy return. */
	float LeverAngle(float T)
	{
		if (T < 0.0f || T > 1.1f)
		{
			return 0.0f;
		}
		if (T < 0.22f)
		{
			return 105.0f * FMath::Sin(T / 0.22f * HALF_PI);
		}
		if (T < 0.42f)
		{
			return 105.0f;
		}
		const float U = (T - 0.42f) / 0.68f;
		return 105.0f * (1.0f - U) * (1.0f - U) + FMath::Sin(U * PI * 3.0f) * (1.0f - U) * 9.0f;
	}

	void PoseLever(AActor* Arm, AActor* Knob, const FVector& Pivot, const FVector& Toward, float Degrees, float Length)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		const FVector D = (FVector(0.0f, 0.0f, 1.0f) * FMath::Cos(A) + Toward * FMath::Sin(A)).GetSafeNormal();
		const FVector Axle = FVector::CrossProduct(Toward, FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal();
		if (Arm)
		{
			Arm->SetActorLocationAndRotation(Pivot + D * (Length * 0.5f), FRotationMatrix::MakeFromZX(D, Axle).Rotator());
		}
		if (Knob)
		{
			Knob->SetActorLocation(Pivot + D * Length);
		}
	}

	/**
	 * One skee-ball cabinet, from where its thrower stands. The ball rolls up the lane, hops the lip, and lands in a
	 * hole on a board tilted back at 38 degrees: five rings round the 50, each with its hole at the bottom of the ring.
	 */
	struct FSkeeLane
	{
		FVector LaneFrom;
		FVector LaneTo;
		FVector RampTop;
		FVector LaneDir;
		FVector LaneUp;
		FVector RampDir;
		FVector RampUp;
		FVector BoardFoot;
		FVector BoardUp;
		FVector BoardOut;
		FVector Centre;
		float Radius[5] = { 22.0f, 46.0f, 70.0f, 94.0f, 118.0f };

		/** How far each ring stands off the board. The 50 is on top. */
		float Lift(int32 Ring) const { return 2.0f + (4 - Ring) * 3.0f; }

		/** The mouth of the hole for this ring (0 is the 50). */
		FVector Hole(int32 Ring) const
		{
			const float Down = Ring == 0 ? 0.0f : (Radius[Ring - 1] + Radius[Ring]) * 0.5f;
			return Centre - BoardUp * Down + BoardOut * (Lift(Ring) + 2.5f);
		}
	};

	FSkeeLane SkeeLane(const FVector& Throw)
	{
		const FVector X(1.0f, 0.0f, 0.0f);
		// The thrower stands just right of their lane, so the camera over their shoulder sees the ball go.
		const float LaneX = Throw.X - 95.0f;
		FSkeeLane L;
		L.LaneFrom = FVector(LaneX, Throw.Y + 70.0f, 34.0f);
		L.LaneTo = FVector(LaneX, Throw.Y + 500.0f, 84.0f);
		L.LaneDir = (L.LaneTo - L.LaneFrom).GetSafeNormal();
		L.LaneUp = FVector::CrossProduct(X, L.LaneDir);
		L.RampTop = L.LaneTo + FVector(0.0f, 40.0f, 26.0f);
		L.RampDir = (L.RampTop - L.LaneTo).GetSafeNormal();
		L.RampUp = FVector::CrossProduct(X, L.RampDir);
		const float Tilt = FMath::DegreesToRadians(38.0f);
		L.BoardFoot = FVector(LaneX, Throw.Y + 570.0f, 70.0f);
		L.BoardUp = FVector(0.0f, FMath::Cos(Tilt), FMath::Sin(Tilt));
		L.BoardOut = FVector::CrossProduct(X, L.BoardUp);
		L.Centre = L.BoardFoot + L.BoardUp * 215.0f;
		return L;
	}

	/** Swap a basic shape onto the glowing light material, so Tint can light it up past white. */
	void MakeGlow(AActor* Actor, const FLinearColor& Colour)
	{
		static TStrongObjectPtr<UMaterialInterface> Glow;
		if (!Glow.IsValid())
		{
			Glow.Reset(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_light.MI_light")));
		}
		UStaticMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		if (!Mesh || !Glow.IsValid())
		{
			return;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Glow.Get(), Mesh);
		Mid->SetVectorParameterValue(TEXT("Color"), Colour);
		Mid->SetVectorParameterValue(TEXT("Emissive Color"), Colour);
		Mesh->SetMaterial(0, Mid);
	}

	// Small synthesised sounds, made once (the same procedural-wave trick Dead Air uses for its knocks).
	constexpr int32 CasinoRate = 22050;
	enum class ECasinoSound : uint8 { Tick, Clack, Roll, Whirr, Drop, Chime, Count };

	const TArray<int16>& CasinoPcm(ECasinoSound Kind)
	{
		static TArray<int16> Cache[int32(ECasinoSound::Count)];
		TArray<int16>& Pcm = Cache[int32(Kind)];
		if (!Pcm.IsEmpty())
		{
			return Pcm;
		}
		const float Lengths[] = { 0.04f, 0.09f, 0.95f, 3.6f, 0.4f, 1.1f };
		const float Length = Lengths[int32(Kind)];
		const int32 N = int32(CasinoRate * Length);
		FRandomStream Noise(int32(Kind) * 7919 + 3);
		float Low = 0.0f;
		Pcm.SetNumUninitialized(N);
		for (int32 i = 0; i < N; ++i)
		{
			const float T = float(i) / CasinoRate;
			const float White = Noise.FRandRange(-1.0f, 1.0f);
			float V = 0.0f;
			switch (Kind)
			{
			case ECasinoSound::Tick:
				V = (White * 0.5f + FMath::Sin(2.0f * PI * 2300.0f * T) * 0.6f) * FMath::Exp(-T * 170.0f);
				break;
			case ECasinoSound::Clack:
				V = (White * 0.35f + FMath::Sin(2.0f * PI * 3100.0f * T) * 0.55f + FMath::Sin(2.0f * PI * 1650.0f * T) * 0.35f)
					* FMath::Exp(-T * 85.0f);
				break;
			case ECasinoSound::Roll:
			case ECasinoSound::Whirr:
			{
				// Low-passed noise with a wobble: a hard ball on wood.
				Low += (Kind == ECasinoSound::Roll ? 0.07f : 0.12f) * (White - Low);
				const float Wobble = 0.65f + 0.35f * FMath::Sin(2.0f * PI * (Kind == ECasinoSound::Roll ? 23.0f : 38.0f) * T);
				const float Fade = FMath::Min(1.0f, T * 20.0f) * FMath::Clamp((Length - T) / (Length * 0.6f), 0.0f, 1.0f);
				V = Low * 3.2f * Wobble * Fade;
				break;
			}
			case ECasinoSound::Drop:
				V = FMath::Sin(2.0f * PI * (130.0f + 90.0f * FMath::Exp(-T * 30.0f)) * T) * FMath::Exp(-T * 13.0f)
					+ White * FMath::Exp(-T * 220.0f) * 0.4f;
				break;
			case ECasinoSound::Chime:
			{
				const float Notes[] = { 1046.5f, 1318.5f, 1568.0f, 2093.0f };
				for (int32 n = 0; n < 4; ++n)
				{
					const float Start = n * 0.085f;
					if (T >= Start)
					{
						const float U = T - Start;
						V += FMath::Sin(2.0f * PI * Notes[n] * U) * FMath::Exp(-U * 4.5f) * 0.32f;
					}
				}
				break;
			}
			default:
				break;
			}
			Pcm[i] = int16(FMath::Clamp(V * 0.8f, -1.0f, 1.0f) * 24000.0f);
		}
		return Pcm;
	}

	void CasinoSound(UWorld* World, ECasinoSound Kind, const FVector& At, float Volume = 1.0f, float Pitch = 1.0f)
	{
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		// At most one of each sound per 45 ms, so a fast wheel still clicks without a wall of voices.
		static double LastPlayed[int32(ECasinoSound::Count)] = {};
		const double Now = World->GetRealTimeSeconds();
		if (Now - LastPlayed[int32(Kind)] < 0.045 && Now >= LastPlayed[int32(Kind)])
		{
			return;
		}
		LastPlayed[int32(Kind)] = Now;
		static TStrongObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			Att.Reset(NewObject<USoundAttenuation>(GetTransientPackage()));
			Att->Attenuation.bAttenuate = true;
			Att->Attenuation.bSpatialize = true;
			Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Att->Attenuation.AttenuationShapeExtents = FVector(600.0f, 0, 0);
			Att->Attenuation.FalloffDistance = 2600.0f;
		}
		const TArray<int16>& Pcm = CasinoPcm(Kind);
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(CasinoRate);
		Wave->NumChannels = 1;
		Wave->Duration = float(Pcm.Num()) / CasinoRate;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		// A procedural wave never ends by itself; left running, fast ticks pile up and steal the room's song.
		if (UAudioComponent* Voice = UGameplayStatics::SpawnSoundAtLocation(World, Wave, At, FRotator::ZeroRotator, Volume, Pitch,
			0.0f, Att.Get(), nullptr, true))
		{
			Voice->StopDelayed(Wave->Duration / FMath::Max(0.1f, Pitch) + 0.05f);
		}
	}

	FString CrawlerName(const ADCCCharacterBase* Who)
	{
		const APlayerState* State = Who ? Who->GetPlayerState() : nullptr;
		return State ? State->GetPlayerName() : FString();
	}

	void Tell(ADCCCharacterBase* Who, const FString& Line)
	{
		if (Who && Who->Wallet)
		{
			Who->Wallet->Tell(Line);
		}
	}
}

int32 DCCCasinoHandValue(uint8 A, uint8 B, uint8 C, uint8 D, uint8 E)
{
	const uint8 Cards[] = { A, B, C, D, E };
	int32 Total = 0;
	int32 Aces = 0;
	for (uint8 Card : Cards)
	{
		if (Card == 0)
		{
			continue;
		}
		if (Card == 1)
		{
			++Aces;
			Total += 11;
		}
		else
		{
			Total += FMath::Min<int32>(Card, 10);
		}
	}
	while (Total > 21 && Aces > 0)
	{
		Total -= 10;
		--Aces;
	}
	return Total;
}

FString DCCCasinoCardName(uint8 Rank)
{
	switch (Rank)
	{
	case 1: return TEXT("A");
	case 11: return TEXT("J");
	case 12: return TEXT("Q");
	case 13: return TEXT("K");
	default: return Rank ? FString::FromInt(Rank) : FString();
	}
}

FString DCCCasinoHandText(uint8 A, uint8 B, uint8 C, uint8 D, uint8 E, bool bHideHole)
{
	const uint8 Cards[] = { A, B, C, D, E };
	FString Out;
	for (int32 i = 0; i < 5; ++i)
	{
		if (Cards[i] == 0)
		{
			break;
		}
		if (!Out.IsEmpty())
		{
			Out += TEXT(" ");
		}
		Out += (bHideHole && i == 1) ? FString(TEXT("?")) : DCCCasinoCardName(Cards[i]);
	}
	return Out;
}

bool DCCCasinoRedPocket(int32 Number)
{
	static const int32 Reds[] = { 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 };
	for (int32 Red : Reds)
	{
		if (Red == Number)
		{
			return true;
		}
	}
	return false;
}

// ---- Layout ---------------------------------------------------------------------------------------------------------

void ADCCCasinoCascade::DenBounds(float& L, float& W)
{
	const int32 Pads = DCCArcade::Games().Num();
	L = FMath::Max(2600.0f, (Pads - 1) * 500.0f + 1100.0f);
	W = 1700.0f;
}

ADCCCasinoCascade::FRoom ADCCCasinoCascade::RoomFor(const FVector& O)
{
	float L = 0.0f;
	float W = 0.0f;
	DenBounds(L, W);
	FRoom R;
	R.HalfX = 2400.0f;
	R.HalfY = W;
	// Overlaps the Safe Room floor by a metre so the doorway has no gap.
	R.Center = O + FVector(L - 100.0f + R.HalfX, 0.0f, 0.0f);
	R.Door = O + FVector(L - 180.0f, 0.0f, 120.0f);

	auto World = [&R](float X, float Y)
	{
		return R.Center + FVector(X, Y, 0.0f);
	};
	auto FaceToward = [](const FVector& From, const FVector& To)
	{
		FRotator Face = (To - From).Rotation();
		Face.Pitch = 0.0f;
		Face.Roll = 0.0f;
		return Face;
	};

	// Games sit on the walls. The middle, from the west door toward the wheel, stays open.
	// Skee-Ball: six lanes along the north wall. The rings are Tables[0]. Throwers face the wall.
	{
		const float LaneX0 = -1680.0f;
		const float LaneStep = 270.0f;
		const float RingY = R.HalfY - 190.0f;
		const float ThrowY = RingY - 760.0f;
		R.Tables[0] = World(LaneX0 + LaneStep * 2.5f, RingY);
		for (int32 i = 0; i < SeatCount; ++i)
		{
			FSpot& Spot = R.Seats[0][i];
			Spot.At = World(LaneX0 + i * LaneStep, ThrowY) + FVector(0.0f, 0.0f, 100.0f);
			Spot.Face = FRotator(0.0f, 90.0f, 0.0f);
		}
		R.Staff[0].At = World(LaneX0 + LaneStep * 5.0f + 240.0f, ThrowY + 60.0f) + FVector(0.0f, 0.0f, 92.0f);
		R.Staff[0].Face = FRotator(0.0f, 180.0f, 0.0f);
	}

	// Air hockey, southwest, clear of the door. 0-2 gold (south), 3-5 blue (north).
	{
		const FVector At = World(-1860.0f, -980.0f);
		R.Tables[1] = At;
		for (int32 i = 0; i < 3; ++i)
		{
			FSpot& South = R.Seats[1][i];
			South.At = At + FVector((i - 1) * 180.0f, -310.0f, 100.0f);
			South.Face = FRotator(0.0f, 90.0f, 0.0f);
			FSpot& North = R.Seats[1][i + 3];
			North.At = At + FVector((i - 1) * 180.0f, 310.0f, 100.0f);
			North.Face = FRotator(0.0f, -90.0f, 0.0f);
		}
		R.Staff[1].At = At + FVector(520.0f, 0.0f, 92.0f);
		R.Staff[1].Face = FRotator(0.0f, 180.0f, 0.0f);
	}

	auto Ring = [&](int32 Table, const FVector& At, float Radius, float CentreYaw, bool bArc)
	{
		R.Tables[Table] = At;
		for (int32 i = 0; i < SeatCount; ++i)
		{
			const float Along = bArc ? (-60.0f + (i + 0.5f) * (120.0f / SeatCount)) : (360.0f * i / SeatCount);
			const FVector Dir = FRotator(0.0f, CentreYaw + Along, 0.0f).Vector();
			FSpot& Spot = R.Seats[Table][i];
			Spot.At = At + Dir * Radius + FVector(0.0f, 0.0f, 100.0f);
			Spot.Face = FaceToward(Spot.At, At);
		}
	};

	// Roulette against the north wall, east of the lanes. The arc opens south, into the room.
	{
		const FVector At = World(980.0f, R.HalfY - 520.0f);
		Ring(2, At, 360.0f, -90.0f, true);
		R.Staff[2].At = At + FVector(0.0f, 280.0f, 92.0f);
		R.Staff[2].Face = FRotator(0.0f, -90.0f, 0.0f);
	}

	// Blackjack against the south wall. The arc opens north. June stands on the wall side.
	{
		const FVector At = World(-520.0f, -R.HalfY + 560.0f);
		Ring(3, At, 380.0f, 90.0f, true);
		R.Staff[3].At = At + FVector(0.0f, -300.0f, 92.0f);
		R.Staff[3].Face = FRotator(0.0f, 90.0f, 0.0f);
	}

	// The wheel on the east wall, north of the title, so the door does not look straight into the back of someone's head.
	{
		const FVector At = World(R.HalfX - 420.0f, 420.0f);
		Ring(4, At, 820.0f, 180.0f, true);
		R.Staff[4].At = At + FVector(180.0f, -640.0f, 92.0f);
		R.Staff[4].Face = FRotator(0.0f, 100.0f, 0.0f);
	}

	// Bar along the south wall, east half. Bud stands at the open west end, in the room.
	R.Staff[5].At = World(400.0f, -R.HalfY + 420.0f) + FVector(0.0f, 0.0f, 92.0f);
	R.Staff[5].Face = FRotator(0.0f, 0.0f, 0.0f);
	return R;
}

// ---- The room -------------------------------------------------------------------------------------------------------

void ADCCCasinoCascade::Dress(ADCCArcadeLobby* Lobby)
{
	if (!Lobby)
	{
		return;
	}
	float L = 0.0f;
	float W = 0.0f;
	DenBounds(L, W);
	const float H = 900.0f;
	const FVector O = Lobby->GetActorLocation();
	const FRoom R = RoomFor(O);
	const FLinearColor Wall(0.08f, 0.06f, 0.12f);
	const TCHAR* WallMat = TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall2");

	auto Carpet = [&](const FVector& Center, float HalfX, float HalfY, bool bCasino)
	{
		constexpr float Tile = 400.0f;
		for (float X = -HalfX + Tile * 0.5f; X < HalfX - 30.0f; X += Tile)
		{
			for (float Y = -HalfY + Tile * 0.5f; Y < HalfY - 30.0f; Y += Tile)
			{
				const int32 IX = FMath::RoundToInt(X / Tile);
				const int32 IY = FMath::RoundToInt(Y / Tile);
				Lobby->Block(Center + FVector(X, Y, 2.0f), FVector(Tile - 10.0f, Tile - 10.0f, 4.0f),
					CarpetColour(IX, IY, bCasino), 0.0f, false);
				if (((IX + IY) & 1) == 0)
				{
					Lobby->Shape(TEXT("Cube"), Center + FVector(X, Y, 5.5f), FVector(72.0f, 72.0f, 2.5f),
						CarpetGold, FRotator(0.0f, 45.0f, 0.0f), false);
				}
			}
		}
	};

	Carpet(O, L, W, false);
	// The casino carpet (2026-11-21): one deep wine field with a small repeating motif, the way a casino floor reads,
	// instead of a tile of every colour.
	{
		Lobby->Block(R.Center + FVector(0.0f, 0.0f, 2.0f), FVector(R.HalfX * 2.0f - 20.0f, R.HalfY * 2.0f - 20.0f, 4.0f),
			FLinearColor(0.075f, 0.012f, 0.022f), 0.0f, false);
		constexpr float Tile = 400.0f;
		for (float X = -R.HalfX + Tile * 0.5f; X < R.HalfX - 60.0f; X += Tile)
		{
			for (float Y = -R.HalfY + Tile * 0.5f; Y < R.HalfY - 60.0f; Y += Tile)
			{
				Lobby->Shape(TEXT("Cube"), R.Center + FVector(X, Y, 4.6f), FVector(150.0f, 150.0f, 1.5f),
					FLinearColor(0.07f, 0.03f, 0.12f), FRotator(0.0f, 45.0f, 0.0f), false);
				Lobby->Shape(TEXT("Cube"), R.Center + FVector(X, Y, 5.6f), FVector(44.0f, 44.0f, 1.5f),
					CarpetGold * 0.7f, FRotator(0.0f, 45.0f, 0.0f), false);
			}
		}
	}

	// East wall of the Safe Room, with a door into the casino. The lintel keeps the ceiling sealed.
	const float DoorHalf = 340.0f;
	const float SideLen = W - DoorHalf;
	const float SideCenter = (W + DoorHalf) * 0.5f;
	if (AActor* South = Lobby->Block(O + FVector(L + 20.0f, -SideCenter, H * 0.5f), FVector(40.0f, SideLen, H), Wall))
	{
		Lobby->Paint(South, WallMat);
	}
	if (AActor* North = Lobby->Block(O + FVector(L + 20.0f, SideCenter, H * 0.5f), FVector(40.0f, SideLen, H), Wall))
	{
		Lobby->Paint(North, WallMat);
	}
	if (AActor* Lintel = Lobby->Block(O + FVector(L + 20.0f, 0.0f, 690.0f), FVector(40.0f, DoorHalf * 2.0f, 420.0f), Wall))
	{
		Lobby->Paint(Lintel, WallMat);
	}
	Lobby->Sign(O + FVector(L - 50.0f, 0.0f, 560.0f), 180.0f, TEXT("CASINO CASCADE"), FColor(255, 196, 80), 64.0f);

	// Ceiling over both rooms, so the sky stays outside.
	Lobby->Block(O + FVector(0.0f, 0.0f, 930.0f), FVector(L * 2.0f + 200.0f, W * 2.0f + 200.0f, 50.0f), FLinearColor(0.05f, 0.04f, 0.07f));
	Lobby->Block(R.Center + FVector(0.0f, 0.0f, 930.0f), FVector(R.HalfX * 2.0f + 120.0f, R.HalfY * 2.0f + 120.0f, 50.0f),
		FLinearColor(0.10f, 0.04f, 0.06f));

	const FLinearColor BulbColours[] = {
		FLinearColor(1.0f, 0.15f, 0.2f), FLinearColor(1.0f, 0.75f, 0.1f), FLinearColor(0.2f, 0.45f, 1.0f),
		FLinearColor(0.15f, 0.8f, 0.35f), FLinearColor(0.7f, 0.2f, 0.9f),
	};
	int32 Bulb = 0;
	for (float X = -L + 300.0f; X < L - 200.0f; X += 800.0f)
	{
		for (float Y = -W + 300.0f; Y < W - 200.0f; Y += 800.0f)
		{
			const FLinearColor Colour = BulbColours[Bulb % UE_ARRAY_COUNT(BulbColours)];
			++Bulb;
			if (AActor* Panel = Lobby->Block(O + FVector(X, Y, 890.0f), FVector(340.0f, 340.0f, 8.0f), Colour * 0.55f, 0.0f, false))
			{
				Lobby->Paint(Panel, TEXT("/Game/Synthwave_city/Material/MI_light"));
			}
			Lobby->Light(O + FVector(X, Y, 840.0f), Colour, 3500.0f, 900.0f);
		}
	}
	for (float X = -L + 200.0f; X <= L - 200.0f; X += 280.0f)
	{
		if (AActor* BulbMesh = Lobby->Shape(TEXT("Sphere"), O + FVector(X, 0.0f, 820.0f), FVector(16.0f, 16.0f, 16.0f),
			BulbColours[(FMath::RoundToInt(FMath::Abs(X)) / 280) % UE_ARRAY_COUNT(BulbColours)], FRotator::ZeroRotator, false))
		{
			Lobby->Paint(BulbMesh, TEXT("/Game/Synthwave_city/Material/MI_light"));
		}
	}
	// A few stars over the open middle, above head height, off the vote pads.
	for (int32 i = -2; i <= 2; ++i)
	{
		const FVector At = O + FVector(i * 700.0f, -200.0f, 760.0f);
		Lobby->Shape(TEXT("Cone"), At, FVector(36.0f, 36.0f, 28.0f), BulbColours[(i + 5) % UE_ARRAY_COUNT(BulbColours)],
			FRotator::ZeroRotator, false);
	}

	// Casino shell. No west wall: that side is the door.
	const FVector C = R.Center;
	Lobby->Block(C + FVector(0.0f, 0.0f, -20.0f), FVector(R.HalfX * 2.0f, R.HalfY * 2.0f, 40.0f), Ink);
	auto CasinoWall = [&](const FVector& At, const FVector& Size)
	{
		if (AActor* Face = Lobby->Block(At, Size, FLinearColor(0.12f, 0.04f, 0.06f)))
		{
			Lobby->Paint(Face, TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall1"));
		}
	};
	CasinoWall(C + FVector(0.0f, R.HalfY + 20.0f, H * 0.5f), FVector(R.HalfX * 2.0f + 80.0f, 40.0f, H));
	CasinoWall(C + FVector(0.0f, -(R.HalfY + 20.0f), H * 0.5f), FVector(R.HalfX * 2.0f + 80.0f, 40.0f, H));
	CasinoWall(C + FVector(R.HalfX + 20.0f, 0.0f, H * 0.5f), FVector(40.0f, R.HalfY * 2.0f, H));
	Lobby->Sign(C + FVector(R.HalfX - 50.0f, 0.0f, 760.0f), 180.0f, TEXT("CASINO CASCADE"), FColor(255, 210, 120), 80.0f);
	// The walk from the door to the wheel. Games stay off it.
	Lobby->Block(C + FVector(-500.0f, 0.0f, 6.0f), FVector(2600.0f, 280.0f, 2.0f), FLinearColor(0.04f, 0.015f, 0.03f), 0.0f, false);

	for (int32 i = 0; i < 4; ++i)
	{
		Lobby->Light(C + FVector((i - 1.5f) * 900.0f, 0.0f, 760.0f), FLinearColor(1.0f, 0.72f, 0.35f), 6000.0f, 1400.0f);
	}

	// The games themselves are built on the casino actor, so the wheel, the ball and the puck can move.
}

// ---- The floor ------------------------------------------------------------------------------------------------------

ADCCArcadeLobby* ADCCCasinoCascade::FindLobby() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADCCArcadeLobby> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADCCCasinoCascade::BuildFloor()
{
	if (bFloorBuilt)
	{
		return;
	}
	ADCCArcadeLobby* Lobby = FindLobby();
	UWorld* World = GetWorld();
	if (!Lobby || !World)
	{
		return;
	}
	bFloorBuilt = true;
	const FRoom R = Room();

	const TCHAR* BarMesh = TEXT("/Game/Diner/Meshes/Blocking/SM_bar");
	const TCHAR* BarTop = TEXT("/Game/Diner/Meshes/Blocking/SM_bar_top");
	const TCHAR* Stool = TEXT("/Game/Diner/Meshes/Blocking/SM_stool");
	const TCHAR* Table = TEXT("/Game/Diner/Meshes/Blocking/SM_table");
	const TCHAR* Glass = TEXT("/Game/Diner/Meshes/Blocking/SM_glass");
	const TCHAR* Bottle = TEXT("/Game/Cave_Camp/Meshes/SM_bottle");
	const TCHAR* WoodGlass = TEXT("/Game/Cave_Camp/Meshes/SM_wooden_glass");
	const TCHAR* Mug = TEXT("/Game/Legendary_Forge/Meshes/Mug/Sm_mug");
	const TCHAR* Plate = TEXT("/Game/Diner/Meshes/SM_plate");
	const TCHAR* Neon = TEXT("/Game/Cyberpunk_City/Meshes/Neon/SM_neon_01");
	const TCHAR* NeonB = TEXT("/Game/Beach_house/Meshes/Neon/SM_neon");
	const TCHAR* Bench = TEXT("/Game/Cyberpunk_City/Meshes/Bench/SM_bench");
	const TCHAR* Couch = TEXT("/Game/Beach_house/Meshes/Furniture/Couch/SM_small_couch");
	const TCHAR* Plant = TEXT("/Game/Beach_house/Meshes/Plants/SM_plants_01");
	const TCHAR* Ceil = TEXT("/Game/Beach_house/Meshes/Lamp/SM_lamp_ceiling");
	const TCHAR* Rug = TEXT("/Game/Legendary_Forge/Meshes/Caroet/SM_carpet_01");
	const TCHAR* Wide = TEXT("June shows 21. The table pays everyone who stood.     ");

	auto Glow = [&](const FVector& At) -> APointLight*
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APointLight* Lit = World->SpawnActor<APointLight>(At, FRotator::ZeroRotator, Params);
		if (!Lit)
		{
			return nullptr;
		}
		Lit->SetReplicates(false);
		if (UPointLightComponent* Comp = Cast<UPointLightComponent>(Lit->GetLightComponent()))
		{
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetIntensity(1600.0f);
			Comp->SetAttenuationRadius(700.0f);
			Comp->SetLightColor(FLinearColor(1.0f, 0.82f, 0.42f));
		}
		return Lit;
	};
	auto Hide = [](AActor* Actor)
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
		}
		return Actor;
	};

	for (int32 t = 0; t < TableCount; ++t)
	{
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const FLinearColor MarkColour = (t == 1 && s >= 3) ? FLinearColor(0.15f, 0.35f, 0.9f) : CarpetGold * 0.85f;
			Lobby->Shape(TEXT("Cylinder"), FVector(R.Seats[t][s].At.X, R.Seats[t][s].At.Y, 6.0f),
				FVector(58.0f, 58.0f, 3.0f), MarkColour, FRotator::ZeroRotator, false);
		}
		PitLights[t] = Glow(R.Tables[t] + FVector(0.0f, 0.0f, 420.0f));
	}

	// Skee-Ball (rebuilt 2026-11-21). Each lane is a cabinet: a lane you roll up, a lip that hops the ball, and a
	// target tilted back toward you with five rings round the 50, each with its hole. The ball is watched all the way.
	for (int32 i = 0; i < SeatCount; ++i)
	{
		const FSkeeLane S = SkeeLane(R.Seats[0][i].At);
		const FVector X(1.0f, 0.0f, 0.0f);
		const FLinearColor LaneTop(0.30f, 0.16f, 0.06f);
		const float LaneLen = FVector::Dist(S.LaneFrom, S.LaneTo);
		const FVector LaneMid = (S.LaneFrom + S.LaneTo) * 0.5f;
		const FRotator LaneRot = FRotationMatrix::MakeFromYZ(S.LaneDir, S.LaneUp).Rotator();
		// The cabinet body, its top flush with the lane, sunk into the floor at the near end.
		Lobby->Shape(TEXT("Cube"), LaneMid - S.LaneUp * 50.0f, FVector(176.0f, LaneLen, 100.0f), Wood, LaneRot, true);
		Lobby->Shape(TEXT("Cube"), LaneMid + S.LaneUp * 1.0f, FVector(116.0f, LaneLen, 2.0f), LaneTop, LaneRot, false);
		for (const float Side : { -1.0f, 1.0f })
		{
			Lobby->Shape(TEXT("Cube"), LaneMid + X * (Side * 72.0f) + S.LaneUp * 14.0f, FVector(12.0f, LaneLen, 30.0f),
				CarpetGold * 0.8f, LaneRot, false);
		}
		// The lip the ball hops off.
		const float RampLen = FVector::Dist(S.LaneTo, S.RampTop);
		const FRotator RampRot = FRotationMatrix::MakeFromYZ(S.RampDir, S.RampUp).Rotator();
		Lobby->Shape(TEXT("Cube"), (S.LaneTo + S.RampTop) * 0.5f - S.RampUp * 20.0f, FVector(116.0f, RampLen + 6.0f, 40.0f),
			LaneTop * 0.75f, RampRot, false);
		// The target board, the wood under it, the cheeks either side, and a hood with a light strip.
		const FRotator BoardRot = FRotationMatrix::MakeFromYZ(S.BoardUp, S.BoardOut).Rotator();
		constexpr float BoardLen = 340.0f;
		const FVector BoardMid = S.BoardFoot + S.BoardUp * (BoardLen * 0.5f);
		const FVector BoardTop = S.BoardFoot + S.BoardUp * BoardLen;
		Lobby->Shape(TEXT("Cube"), BoardMid - S.BoardOut * 6.0f, FVector(250.0f, BoardLen, 12.0f), Ink, BoardRot, false);
		Lobby->Shape(TEXT("Cube"), BoardMid - S.BoardOut * 72.0f, FVector(250.0f, BoardLen, 120.0f), Wood, BoardRot, true);
		for (const float Side : { -1.0f, 1.0f })
		{
			Lobby->Shape(TEXT("Cube"), BoardMid + X * (Side * 131.0f) + S.BoardOut * 18.0f, FVector(12.0f, BoardLen, 60.0f),
				Wood * 1.4f, BoardRot, false);
		}
		Lobby->Block(FVector(S.BoardFoot.X, BoardTop.Y + 20.0f, (BoardTop.Z + 70.0f) * 0.5f),
			FVector(262.0f, 120.0f, BoardTop.Z + 70.0f), Wood, 0.0f, true);
		if (AActor* Strip = Lobby->Shape(TEXT("Cube"), FVector(S.BoardFoot.X, BoardTop.Y - 42.0f, BoardTop.Z + 46.0f),
			FVector(236.0f, 4.0f, 12.0f), RingColours[i % 4] * 2.0f, FRotator::ZeroRotator, false))
		{
			MakeGlow(Strip, RingColours[i % 4] * 2.0f);
		}
		// Rings, outermost first, each a little prouder than the one round it, and a hole at the bottom of each.
		const FRotator Flat = FRotationMatrix::MakeFromZX(S.BoardOut, X).Rotator();
		for (int32 Ring = 4; Ring >= 0; --Ring)
		{
			const float D = S.Radius[Ring] * 2.0f;
			SkeeRings[i][Ring] = Lobby->Shape(TEXT("Cylinder"), S.Centre + S.BoardOut * S.Lift(Ring), FVector(D, D, 4.0f),
				RingColours[Ring], Flat, false);
		}
		for (int32 Ring = 0; Ring < 5; ++Ring)
		{
			const float D = Ring == 0 ? 26.0f : 22.0f;
			Lobby->Shape(TEXT("Cylinder"), S.Hole(Ring) - S.BoardOut * 0.4f, FVector(D, D, 1.0f), FLinearColor(0.01f, 0.01f, 0.01f),
				Flat, false);
		}
		// Three balls waiting at the foot of the lane, and the one in play.
		for (int32 b = 0; b < 3; ++b)
		{
			Lobby->Shape(TEXT("Sphere"), S.LaneFrom + FVector(-24.0f + b * 24.0f, -40.0f, 6.0f), FVector(22.0f, 22.0f, 22.0f),
				FLinearColor(0.55f, 0.05f, 0.04f), FRotator::ZeroRotator, false);
		}
		SkeeBalls[i] = Hide(Lobby->Shape(TEXT("Sphere"), FVector(0.0f, 0.0f, -400.0f),
			FVector(24.0f, 24.0f, 24.0f), FLinearColor(0.55f, 0.05f, 0.04f), FRotator::ZeroRotator, false));
	}
	Marquees[0] = Lobby->Sign(FVector(R.Tables[0].X, R.Center.Y + R.HalfY - 36.0f, 500.0f), -90.0f, Wide,
		FColor(255, 220, 160), 22.0f);
	Lobby->Prop(Neon, FVector(R.Tables[0].X, R.Center.Y + R.HalfY - 28.0f, 580.0f), 0.0f, FVector(1700.0f, 16.0f, 20.0f), false, true);
	Lobby->Prop(NeonB, FVector(R.Tables[2].X, R.Center.Y + R.HalfY - 28.0f, 580.0f), 0.0f, FVector(760.0f, 16.0f, 20.0f), false, true);

	// Air hockey. A diner table is the cabinet. The ice, the rails and the goals sit on it.
	{
		const FVector At = R.Tables[1];
		const FLinearColor Ice(0.62f, 0.86f, 0.95f);
		const FLinearColor Blue(0.12f, 0.32f, 0.92f);
		Lobby->Prop(Rug, At, 0.0f, FVector(340.0f, 620.0f, 0.0f), false);
		Lobby->Block(At + FVector(0.0f, 0.0f, 40.0f), FVector(260.0f, 540.0f, 80.0f), Wood);
		Lobby->Block(At + FVector(0.0f, 0.0f, 86.0f), FVector(210.0f, 460.0f, 6.0f), Ice, 0.0f, false);
		Lobby->Block(At + FVector(0.0f, 0.0f, 90.0f), FVector(210.0f, 4.0f, 2.0f), Blue, 0.0f, false);
		Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 0.0f, 90.0f), FVector(70.0f, 70.0f, 2.0f), Blue, FRotator::ZeroRotator, false);
		Lobby->Block(At + FVector(-112.0f, 0.0f, 100.0f), FVector(8.0f, 470.0f, 16.0f), CarpetGold);
		Lobby->Block(At + FVector(112.0f, 0.0f, 100.0f), FVector(8.0f, 470.0f, 16.0f), Blue);
		Lobby->Block(At + FVector(-48.0f, 228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), CarpetGold);
		Lobby->Block(At + FVector(48.0f, 228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), CarpetGold);
		Lobby->Block(At + FVector(0.0f, 228.0f, 140.0f), FVector(104.0f, 8.0f, 8.0f), CarpetGold);
		Lobby->Block(At + FVector(-48.0f, -228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), Blue);
		Lobby->Block(At + FVector(48.0f, -228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), Blue);
		Lobby->Block(At + FVector(0.0f, -228.0f, 140.0f), FVector(104.0f, 8.0f, 8.0f), Blue);
		Puck = Lobby->Shape(TEXT("Sphere"), At + FVector(0.0f, 0.0f, 100.0f), FVector(18.0f, 18.0f, 10.0f), Ink, FRotator::ZeroRotator, false);
		Mallets[0] = Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, -150.0f, 100.0f), FVector(32.0f, 32.0f, 10.0f), CarpetGold, FRotator::ZeroRotator, false);
		Mallets[1] = Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 150.0f, 100.0f), FVector(32.0f, 32.0f, 10.0f), Blue, FRotator::ZeroRotator, false);
		Marquees[1] = Lobby->Sign(At + FVector(0.0f, 300.0f, 230.0f), -90.0f, Wide, FColor(210, 230, 255), 20.0f);
	}

	// Three stools on the open side. The diner chair is a booth, and six of them became a wall in front of the felt.
	auto Stools = [&](int32 Index)
	{
		const int32 Use[] = { 1, 2, 4 };
		for (int32 s : Use)
		{
			const FSpot& Spot = R.Seats[Index][s];
			const FVector At = FVector(Spot.At.X, Spot.At.Y, 0.0f) - Spot.Face.Vector() * 30.0f;
			if (!Lobby->Prop(Stool, At, Spot.Face.Yaw, FVector(0.0f, 0.0f, 72.0f), false))
			{
				Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 0.0f, 36.0f), FVector(28.0f, 28.0f, 64.0f), Ink, FRotator::ZeroRotator, false);
			}
		}
	};

	// Roulette (rebuilt 2026-11-21). A 37-pocket wheel in a wooden bowl. The rotor turns one way and the ball the other;
	// the ball runs the track, rattles over the diamonds and drops into the pocket the server already rolled.
	{
		const FVector At = R.Tables[2];
		const FVector Bowl = At + FVector(0.0f, 40.0f, 0.0f);
		const float T = RouletteTop;
		const FLinearColor PocketRed(0.82f, 0.08f, 0.09f);
		const FLinearColor PocketBlack(0.07f, 0.06f, 0.07f);
		const FLinearColor PocketGreen(0.05f, 0.62f, 0.22f);
		Lobby->Prop(Rug, At, 30.0f, FVector(540.0f, 440.0f, 0.0f), false);
		if (!Lobby->Prop(Table, At, 0.0f, FVector(440.0f, 340.0f, 74.0f), true))
		{
			Lobby->Block(At + FVector(0.0f, 0.0f, 36.0f), FVector(440.0f, 340.0f, 72.0f), Wood);
		}
		Lobby->Block(At + FVector(0.0f, 0.0f, 78.0f), FVector(400.0f, 300.0f, 8.0f), Felt, 0.0f, false);
		// The bowl: a wooden base, a wall of staves, the track, and eight brass diamonds to knock the ball about.
		Lobby->Shape(TEXT("Cylinder"), Bowl + FVector(0.0f, 0.0f, T + 9.0f), FVector(290.0f, 290.0f, 18.0f), Wood, FRotator::ZeroRotator, false);
		for (int32 k = 0; k < 36; ++k)
		{
			const float A = k * 10.0f;
			Lobby->Shape(TEXT("Cube"), Bowl + FRotator(0.0f, A, 0.0f).Vector() * 139.0f + FVector(0.0f, 0.0f, T + 26.0f),
				FVector(10.0f, 26.0f, 18.0f), (k % 2) ? Wood * 1.25f : Wood * 1.05f, FRotator(0.0f, A, 0.0f), false);
		}
		Lobby->Shape(TEXT("Cylinder"), Bowl + FVector(0.0f, 0.0f, T + 19.0f), FVector(268.0f, 268.0f, 2.0f),
			FLinearColor(0.16f, 0.08f, 0.035f), FRotator::ZeroRotator, false);
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = k * 45.0f + 22.5f;
			Lobby->Shape(TEXT("Cube"), Bowl + FRotator(0.0f, A, 0.0f).Vector() * 110.0f + FVector(0.0f, 0.0f, T + 22.0f),
				FVector(12.0f, 7.0f, 5.0f), CarpetGold, FRotator(0.0f, A, 0.0f), false);
		}
		RouletteDisc = Lobby->Shape(TEXT("Cylinder"), Bowl + FVector(0.0f, 0.0f, T + 22.0f), FVector(190.0f, 190.0f, 8.0f),
			FLinearColor(0.34f, 0.16f, 0.06f), FRotator::ZeroRotator, false);
		for (int32 p = 0; p < 37; ++p)
		{
			const int32 Number = RouletteOrder[p];
			const FLinearColor Colour = Number == 0 ? PocketGreen : DCCCasinoRedPocket(Number) ? PocketRed : PocketBlack;
			RoulettePockets[p] = Lobby->Shape(TEXT("Cube"), Bowl + FVector(0.0f, 0.0f, T + 28.0f),
				FVector(24.0f, 12.5f, 6.0f), Colour, FRotator::ZeroRotator, false);
			RouletteFrets[p] = Lobby->Shape(TEXT("Cube"), Bowl + FVector(0.0f, 0.0f, T + 30.0f),
				FVector(26.0f, 2.0f, 10.0f), CarpetGold, FRotator::ZeroRotator, false);
		}
		// The turret in the middle: a brass cone and a cross that turns with the rotor.
		Lobby->Shape(TEXT("Cone"), Bowl + FVector(0.0f, 0.0f, T + 40.0f), FVector(56.0f, 56.0f, 28.0f), CarpetGold, FRotator::ZeroRotator, false);
		for (int32 k = 0; k < 2; ++k)
		{
			RouletteCross[k] = Lobby->Shape(TEXT("Cube"), Bowl + FVector(0.0f, 0.0f, T + 50.0f), FVector(76.0f, 6.0f, 6.0f),
				CarpetGold, FRotator::ZeroRotator, false);
		}
		Lobby->Shape(TEXT("Sphere"), Bowl + FVector(0.0f, 0.0f, T + 56.0f), FVector(14.0f, 14.0f, 14.0f), CarpetGold, FRotator::ZeroRotator, false);
		RouletteBallActor = Lobby->Shape(TEXT("Sphere"), Bowl + FVector(80.0f, 0.0f, T + 30.0f), FVector(12.0f, 12.0f, 12.0f),
			FLinearColor(0.95f, 0.95f, 0.92f), FRotator::ZeroRotator, false);
		const TCHAR* Bets[] = { TEXT("RED"), TEXT("BLACK"), TEXT("0") };
		const FLinearColor BetColours[] = { PocketRed, PocketBlack, PocketGreen };
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Spot = At + FVector(-110.0f + i * 110.0f, -122.0f, 88.0f);
			Lobby->Block(Spot, FVector(96.0f, 40.0f, 6.0f), BetColours[i], 0.0f, false);
			Lobby->Sign(Spot + FVector(0.0f, -30.0f, 22.0f), -90.0f, Bets[i], FColor::White, 26.0f, false);
		}
		Stools(2);
		Marquees[2] = Lobby->Sign(FVector(At.X, R.Center.Y + R.HalfY - 36.0f, 440.0f), -90.0f, Wide, FColor(255, 210, 160), 22.0f);
	}

	// Blackjack. Same real table, cards that come out when June deals.
	{
		const FVector At = R.Tables[3];
		Lobby->Prop(Rug, At, 10.0f, FVector(500.0f, 380.0f, 0.0f), false);
		if (!Lobby->Prop(Table, At, 90.0f, FVector(360.0f, 220.0f, 74.0f), true))
		{
			Lobby->Block(At + FVector(0.0f, 0.0f, 36.0f), FVector(320.0f, 200.0f, 72.0f), Wood);
		}
		Lobby->Block(At + FVector(0.0f, 0.0f, 80.0f), FVector(280.0f, 160.0f, 8.0f), Felt, 0.0f, false);
		for (int32 Row = 0; Row < 7; ++Row)
		{
			for (int32 Card = 0; Card < 5; ++Card)
			{
				Cards[Row][Card] = Hide(Lobby->Shape(TEXT("Cube"), FVector(0.0f, 0.0f, -400.0f),
					FVector(22.0f, 32.0f, 2.0f), FLinearColor(0.93f, 0.9f, 0.84f), FRotator::ZeroRotator, false));
			}
		}
		Stools(3);
		Marquees[3] = Lobby->Sign(FVector(At.X, R.Center.Y - R.HalfY + 36.0f, 420.0f), 90.0f, Wide, FColor(255, 220, 170), 22.0f);
	}

	// The wheel (rebuilt 2026-11-21). Twelve slices (each pay twice), brass pegs round the rim, a flapper at the top that
	// clicks over them, and a ring of bulbs that chases while it turns and flashes for the winner.
	{
		const FVector Hub = R.Tables[4] + FVector(0.0f, 0.0f, WheelHubZ);
		Lobby->Shape(TEXT("Cylinder"), FVector(R.Tables[4].X + 40.0f, R.Tables[4].Y, WheelHubZ * 0.5f), FVector(60.0f, 60.0f, WheelHubZ),
			Wood, FRotator::ZeroRotator, true);
		Lobby->Shape(TEXT("Cylinder"), Hub + FVector(24.0f, 0.0f, 0.0f), FVector(770.0f, 770.0f, 20.0f), CarpetGold * 0.75f, FRotator(90.0f, 0.0f, 0.0f), false);
		PrizeDisc = Lobby->Shape(TEXT("Cylinder"), Hub, FVector(664.0f, 664.0f, 18.0f), Ink, FRotator(90.0f, 0.0f, 0.0f), false);
		const float BandC[] = { 92.0f, 158.0f, 224.0f, 290.0f };
		for (int32 j = 0; j < WheelSliceCount; ++j)
		{
			const float A = j * 30.0f;
			for (int32 Band = 0; Band < 4; ++Band)
			{
				PrizeParts[j][Band] = Lobby->Shape(TEXT("Cube"), Hub + FVector(-16.0f, 0.0f, 0.0f) + WallRadial(A, BandC[Band]),
					FVector(64.0f, BandC[Band] * 0.515f, 8.0f), SliceColour(j), WallSlice(A), false);
			}
			const FColor InkOn = (j % 6 == 1 || j % 6 == 3) ? FColor(24, 18, 16) : FColor::White;
			WheelLabels[j] = Lobby->Sign(Hub + FVector(-34.0f, 0.0f, 0.0f) + WallRadial(A, 236.0f), 180.0f, WheelWord[j % 6], InkOn, 40.0f, false);
			WheelPegs[j] = Lobby->Shape(TEXT("Cylinder"), Hub + FVector(-26.0f, 0.0f, 0.0f) + WallRadial(A + 15.0f, 334.0f),
				FVector(14.0f, 14.0f, 34.0f), CarpetGold, FRotator(90.0f, 0.0f, 0.0f), false);
		}
		Lobby->Shape(TEXT("Cylinder"), Hub + FVector(-26.0f, 0.0f, 0.0f), FVector(96.0f, 96.0f, 14.0f), CarpetGold, FRotator(90.0f, 0.0f, 0.0f), false);
		for (int32 k = 0; k < 24; ++k)
		{
			WheelBulbs[k] = Lobby->Shape(TEXT("Sphere"), Hub + FVector(-6.0f, 0.0f, 0.0f) + WallRadial(k * 15.0f + 7.5f, 360.0f),
				FVector(30.0f, 30.0f, 30.0f), WheelBulbColour(k), FRotator::ZeroRotator, false);
			MakeGlow(WheelBulbs[k], WheelBulbColour(k));
		}
		// The flapper hangs from a brass block above the rim.
		Lobby->Shape(TEXT("Cube"), Hub + FVector(-30.0f, 0.0f, 392.0f), FVector(36.0f, 56.0f, 34.0f), CarpetGold, FRotator::ZeroRotator, false);
		WheelFlapper = Lobby->Shape(TEXT("Cube"), Hub + FVector(-30.0f, 0.0f, 352.0f), FVector(16.0f, 14.0f, 48.0f),
			FLinearColor(0.85f, 0.1f, 0.12f), FRotator::ZeroRotator, false);
		for (int32 i = 0; i < 6; ++i)
		{
			const FColor InkOn = (i == 1 || i == 3) ? FColor(24, 18, 16) : FColor::White;
			const FVector Key = Hub + FVector(-30.0f, -490.0f, 150.0f - i * 64.0f);
			Lobby->Block(Key, FVector(12.0f, 168.0f, 54.0f), SliceColour(i), 0.0f, false);
			Lobby->Sign(Key + FVector(-14.0f, 0.0f, 0.0f), 180.0f, i == 5 ? FString(WheelWord[i]) : FString::Printf(TEXT("x%s"), WheelWord[i]),
				InkOn, 32.0f, false);
		}
		Marquees[4] = Lobby->Sign(FVector(R.Center.X + R.HalfX - 36.0f, R.Tables[4].Y, 872.0f), 180.0f, Wide, FColor(255, 214, 120), 24.0f);
		Lobby->Prop(Neon, FVector(R.Center.X + R.HalfX - 24.0f, R.Tables[4].Y, 896.0f), 90.0f, FVector(520.0f, 16.0f, 8.0f), false, true);
	}

	// The wheel's lever, on a brass post in front of it. Pulling it is what starts the spin.
	{
		const FVector Post = R.Tables[4] + FVector(-300.0f, -330.0f, 0.0f);
		Lobby->Block(Post + FVector(0.0f, 0.0f, 60.0f), FVector(64.0f, 64.0f, 120.0f), Wood);
		Lobby->Block(Post + FVector(0.0f, 0.0f, 126.0f), FVector(44.0f, 52.0f, 20.0f), CarpetGold);
		WheelLever[0] = Lobby->Shape(TEXT("Cylinder"), Post + FVector(0.0f, 0.0f, 200.0f), FVector(14.0f, 14.0f, 150.0f),
			FLinearColor(0.75f, 0.75f, 0.8f), FRotator::ZeroRotator, false);
		WheelLever[1] = Lobby->Shape(TEXT("Sphere"), Post + FVector(0.0f, 0.0f, 280.0f), FVector(42.0f, 42.0f, 42.0f),
			FLinearColor(1.0f, 0.1f, 0.12f), FRotator::ZeroRotator, false);
		MakeGlow(WheelLever[1], FLinearColor(1.0f, 0.1f, 0.12f));
		Lobby->Sign(Post + FVector(-34.0f, 0.0f, 90.0f), 180.0f, TEXT("PULL"), FColor(255, 214, 120), 20.0f, false);
	}

	// Reels and the card machine (2026-11-21), along the north wall between the skee lanes and roulette.
	for (int32 m = 0; m <= SlotCount; ++m)
	{
		const FVector Cm = MachineAt(R, m);
		const bool bCards = m == SlotCount;
		const FLinearColor Body = bCards ? FLinearColor(0.03f, 0.20f, 0.18f) : FLinearColor(0.24f, 0.03f, 0.10f);
		const FLinearColor Lit = bCards ? FLinearColor(0.2f, 1.0f, 0.8f) : SlotTopColours[m % 4];
		Lobby->Block(Cm + FVector(0.0f, 0.0f, 55.0f), FVector(160.0f, 120.0f, 110.0f), Body);
		Lobby->Block(Cm + FVector(0.0f, 25.0f, 190.0f), FVector(160.0f, 70.0f, 170.0f), Body * 0.8f);
		// The bezel round the window, and the cheeks that close it in.
		Lobby->Block(Cm + FVector(0.0f, -80.0f, 248.0f), FVector(164.0f, 10.0f, 54.0f), CarpetGold, 0.0f, false);
		Lobby->Block(Cm + FVector(0.0f, -80.0f, 124.0f), FVector(164.0f, 10.0f, 48.0f), CarpetGold, 0.0f, false);
		Lobby->Block(Cm + FVector(-71.0f, -80.0f, 185.0f), FVector(22.0f, 10.0f, 76.0f), CarpetGold, 0.0f, false);
		Lobby->Block(Cm + FVector(71.0f, -80.0f, 185.0f), FVector(22.0f, 10.0f, 76.0f), CarpetGold, 0.0f, false);
		Lobby->Block(Cm + FVector(-82.0f, -40.0f, 188.0f), FVector(4.0f, 84.0f, 176.0f), Body, 0.0f, false);
		Lobby->Block(Cm + FVector(82.0f, -40.0f, 188.0f), FVector(4.0f, 84.0f, 176.0f), Body, 0.0f, false);
		if (AActor* Top = Lobby->Shape(TEXT("Cube"), Cm + FVector(0.0f, -20.0f, 302.0f), FVector(150.0f, 60.0f, 46.0f), Lit, FRotator::ZeroRotator, false))
		{
			MakeGlow(Top, Lit * 1.5f);
			if (!bCards)
			{
				SlotTops[m] = Top;
			}
		}
		if (bCards)
		{
			// A lit window with the card art, and a card waiting in the slot.
			if (AActor* Face = Lobby->Shape(TEXT("Cube"), Cm + FVector(0.0f, -78.0f, 185.0f), FVector(120.0f, 4.0f, 72.0f), Lit * 0.6f, FRotator::ZeroRotator, false))
			{
				MakeGlow(Face, Lit * 0.6f);
			}
			Lobby->Sign(Cm + FVector(0.0f, -84.0f, 198.0f), -90.0f, TEXT("SCRATCH"), FColor(10, 40, 36), 22.0f, false);
			Lobby->Sign(Cm + FVector(0.0f, -84.0f, 172.0f), -90.0f, TEXT("cards"), FColor(10, 40, 36), 16.0f, false);
			Lobby->Sign(Cm + FVector(0.0f, -87.0f, 248.0f), -90.0f, TEXT("MOTHER LODE"), FColor(60, 24, 8), 15.0f, false);
			Lobby->Block(Cm + FVector(0.0f, -64.0f, 112.0f), FVector(70.0f, 8.0f, 44.0f), FLinearColor(0.92f, 0.85f, 0.6f), 0.0f, false);
			continue;
		}
		SlotSigns[m] = Lobby->Sign(Cm + FVector(0.0f, -87.0f, 248.0f), -90.0f, TEXT("2 A PULL"), FColor(60, 24, 8), 16.0f, false);
		for (int32 r = 0; r < 3; ++r)
		{
			const FVector Axle = Cm + FVector(-42.0f + r * 42.0f, -40.0f, 185.0f);
			Lobby->Shape(TEXT("Cylinder"), Axle, FVector(64.0f, 64.0f, 38.0f), FLinearColor(0.95f, 0.92f, 0.84f), FRotator(90.0f, 0.0f, 0.0f), false);
			for (int32 k = 0; k < SlotSymbols; ++k)
			{
				AActor* Symbol = nullptr;
				switch (k)
				{
				case 0: Symbol = Lobby->Shape(TEXT("Sphere"), Axle, FVector(20.0f, 20.0f, 20.0f), FLinearColor(0.85f, 0.04f, 0.08f), FRotator::ZeroRotator, false); break;
				case 1: Symbol = Lobby->Shape(TEXT("Sphere"), Axle, FVector(26.0f, 18.0f, 18.0f), FLinearColor(1.0f, 0.85f, 0.1f), FRotator::ZeroRotator, false); break;
				case 2: Symbol = Lobby->Shape(TEXT("Cone"), Axle, FVector(22.0f, 22.0f, 22.0f), CarpetGold * 1.2f, FRotator::ZeroRotator, false); break;
				case 3: Symbol = Lobby->Sign(Axle, 0.0f, TEXT("BAR"), FColor(20, 16, 16), 13.0f, false); break;
				case 4: Symbol = Lobby->Shape(TEXT("Cube"), Axle, FVector(16.0f, 16.0f, 16.0f), FLinearColor(0.2f, 0.85f, 1.0f), FRotator::ZeroRotator, false); break;
				default: Symbol = Lobby->Sign(Axle, 0.0f, TEXT("7"), FColor(220, 20, 30), 30.0f, false); break;
				}
				SlotSymbolActors[m][r][k] = Symbol;
			}
		}
		// The lever, on the player's right hand.
		const FVector Pivot = Cm + FVector(-96.0f, -30.0f, 200.0f);
		Lobby->Block(Pivot, FVector(20.0f, 30.0f, 34.0f), CarpetGold, 0.0f, false);
		SlotLevers[m][0] = Lobby->Shape(TEXT("Cylinder"), Pivot, FVector(9.0f, 9.0f, 80.0f), FLinearColor(0.75f, 0.75f, 0.8f), FRotator::ZeroRotator, false);
		SlotLevers[m][1] = Lobby->Shape(TEXT("Sphere"), Pivot, FVector(24.0f, 24.0f, 24.0f), FLinearColor(1.0f, 0.1f, 0.12f), FRotator::ZeroRotator, false);
		MakeGlow(SlotLevers[m][1], FLinearColor(1.0f, 0.1f, 0.12f));
	}

	// Bar. The diner counter, stools, and the bottles that are already in the packs.
	{
		const FVector Bar = R.Center + FVector(1280.0f, -R.HalfY + 250.0f, 0.0f);
		if (!Lobby->Prop(BarMesh, Bar, 0.0f, FVector(1480.0f, 88.0f, 108.0f), true, true))
		{
			Lobby->Block(Bar + FVector(0.0f, 0.0f, 54.0f), FVector(1480.0f, 88.0f, 108.0f), Wood);
		}
		Lobby->Prop(BarTop, Bar + FVector(0.0f, 16.0f, 112.0f), 0.0f, FVector(1540.0f, 108.0f, 12.0f), false, true);
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector At = Bar + FVector(-600.0f + i * 230.0f, 130.0f, 0.0f);
			if (!Lobby->Prop(Stool, At, -90.0f, FVector(0.0f, 0.0f, 78.0f), true))
			{
				Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 0.0f, 40.0f), FVector(32.0f, 32.0f, 74.0f), Ink, FRotator::ZeroRotator, false);
			}
		}
		const TCHAR* Bits[] = { Bottle, Glass, Mug, Plate, WoodGlass, Bottle, Glass, Mug };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Bits); ++i)
		{
			Lobby->Prop(Bits[i], Bar + FVector(-460.0f + i * 110.0f, -8.0f, 124.0f), i * 28.0f, FVector(0.0f, 0.0f, (i % 2) ? 24.0f : 18.0f), false);
		}
		Lobby->Sign(FVector(Bar.X, R.Center.Y - R.HalfY + 36.0f, 300.0f), 90.0f, TEXT("BAR AND BUD"), FColor(120, 220, 140), 36.0f);
		Lobby->Sign(FVector(Bar.X, R.Center.Y - R.HalfY + 36.0f, 250.0f), 90.0f, TEXT("Cola, a red, and something legal"), FColor(210, 220, 200), 16.0f);
		Lobby->Light(Bar + FVector(0.0f, 40.0f, 220.0f), FLinearColor(0.4f, 1.0f, 0.5f), 2200.0f, 640.0f);
		Lobby->Prop(NeonB, FVector(Bar.X, R.Center.Y - R.HalfY + 30.0f, 360.0f), 0.0f, FVector(1300.0f, 16.0f, 18.0f), false, true);
	}

	// Watching spots, off the aisle. A bench by the door, a couch aimed at the wheel, plants in the corners.
	Lobby->Prop(Bench, R.Center + FVector(-R.HalfX + 260.0f, 680.0f, 0.0f), 90.0f, FVector(170.0f, 52.0f, 78.0f), true);
	Lobby->Prop(Couch, R.Center + FVector(-180.0f, -520.0f, 0.0f), 0.0f, FVector(190.0f, 78.0f, 70.0f), false);
	Lobby->Prop(Plant, R.Center + FVector(-R.HalfX + 160.0f, R.HalfY - 150.0f, 0.0f), 20.0f, FVector(0.0f, 0.0f, 130.0f), true);
	Lobby->Prop(Plant, R.Center + FVector(R.HalfX - 180.0f, -R.HalfY + 180.0f, 0.0f), -30.0f, FVector(0.0f, 0.0f, 130.0f), true);
	for (int32 i = 0; i < 3; ++i)
	{
		Lobby->Prop(Ceil, R.Center + FVector(-700.0f + i * 700.0f, 0.0f, 860.0f), 0.0f, FVector(0.0f, 0.0f, 46.0f), false);
	}
	for (int32 i = 0; i < 10; ++i)
	{
		Chase[i] = Lobby->Shape(TEXT("Sphere"), R.Center + FVector(-1500.0f + i * 300.0f, 0.0f, 800.0f),
			FVector(14.0f, 14.0f, 14.0f), PitColours[i % 5], FRotator::ZeroRotator, false);
	}
}

void ADCCCasinoCascade::AnimateFloor(float DeltaSeconds)
{
	if (!bFloorBuilt)
	{
		return;
	}
	const FRoom R = Room();
	const bool bEdges = bVisualCaught;
	const double Now = EstimatedNow();
	auto Advance = [&](FSpin& Spin, float& Angle, float Idle)
	{
		if (Spin.bLive)
		{
			Spin.Age += DeltaSeconds;
			const float U = FMath::Clamp(Spin.Age / Spin.Duration, 0.0f, 1.0f);
			// Fast off the hand, then a long crawl, so the last few pegs are the suspense.
			const float Eased = 1.0f - FMath::Pow(1.0f - U, 3.2f);
			Angle = FMath::Lerp(Spin.From, Spin.To, Eased);
			if (U >= 1.0f)
			{
				Spin.bLive = false;
				Angle = FMath::Fmod(Spin.To, 360.0f);
				if (Angle < 0.0f)
				{
					Angle += 360.0f;
				}
			}
		}
		else
		{
			Angle += DeltaSeconds * Idle;
		}
	};
	auto Arm = [&](FSpin& Spin, float& Angle, float Target, float MinTravel, float Duration)
	{
		Spin.bLive = true;
		Spin.Age = 0.0f;
		Spin.Duration = Duration;
		Spin.From = Angle;
		Spin.To = Target;
		while (Spin.To < Spin.From + MinTravel)
		{
			Spin.To += 360.0f;
		}
	};

	const bool bWheelResult = Tables.IsValidIndex(4) && Tables[4].Result >= 0;
	const bool bRouletteResult = Tables.IsValidIndex(2) && Tables[2].Result >= 0;
	if (bEdges && bWheelResult && Tables[4].Phase == 3 && SeenPhase[4] != 3)
	{
		WheelSlice = Tables[4].Result + ((++WheelSpins & 1) ? 6 : 0);
		Arm(WheelSpin, WheelAngle, -30.0f * WheelSlice, 1080.0f, WheelSpinTime);
		// The lever goes down first; the wheel takes off as it bottoms out.
		WheelSpin.Age = -SlotLeverLead;
		WheelLeverAge = 0.0f;
		CasinoSound(GetWorld(), ECasinoSound::Clack, R.Tables[4] + FVector(-300.0f, -330.0f, 200.0f), 0.9f, 0.55f);
	}
	if (bEdges && bRouletteResult && Tables[2].Phase == 3 && SeenPhase[2] != 3)
	{
		Arm(RouletteSpinAnim, RouletteAngle, RouletteAngle, 620.0f, RouletteSpinTime);
		LastBounce = -1;
		CasinoSound(GetWorld(), ECasinoSound::Whirr, R.Tables[2] + FVector(0.0f, 40.0f, RouletteTop + 30.0f), 0.8f);
	}
	if (!bVisualCaught && bWheelResult)
	{
		WheelSlice = Tables[4].Result;
		WheelAngle = -30.0f * WheelSlice;
	}
	const bool bWheelWasLive = WheelSpin.bLive;
	const bool bRouletteWasLive = RouletteSpinAnim.bLive;
	const bool bWheelHold = bWheelResult && Tables[4].Phase == 3;
	Advance(WheelSpin, WheelAngle, bWheelHold ? 0.0f : 6.0f);
	Advance(RouletteSpinAnim, RouletteAngle, 8.0f);
	const bool bBlink = FMath::Fmod(Now * 4.0, 1.0) < 0.5;

	// ---- The wheel.
	const FVector Hub = R.Tables[4] + FVector(0.0f, 0.0f, WheelHubZ);
	if (bWheelWasLive && !WheelSpin.bLive)
	{
		const bool bBust = (WheelSlice % 6) == 5;
		CasinoSound(GetWorld(), bBust ? ECasinoSound::Drop : ECasinoSound::Chime, Hub, bBust ? 1.0f : 0.8f);
	}
	if (PrizeDisc)
	{
		PrizeDisc->SetActorRotation(WallSlice(WheelAngle));
	}
	const bool bWheelShow = bWheelHold && !WheelSpin.bLive;
	const float BandC[] = { 92.0f, 158.0f, 224.0f, 290.0f };
	for (int32 j = 0; j < WheelSliceCount; ++j)
	{
		const float A = WheelAngle + j * 30.0f;
		const bool bWinner = bWheelShow && j == WheelSlice;
		for (int32 Band = 0; Band < 4; ++Band)
		{
			if (AActor* Part = PrizeParts[j][Band])
			{
				Part->SetActorLocationAndRotation(Hub + FVector(-16.0f - (j & 1) * 0.6f, 0.0f, 0.0f) + WallRadial(A, BandC[Band]), WallSlice(A));
				if (bWinner)
				{
					Tint(Part, bBlink ? FLinearColor(1.0f, 0.97f, 0.85f) : SliceColour(j));
				}
				else if (bWheelShowWas && !bWheelShow)
				{
					Tint(Part, SliceColour(j));
				}
			}
		}
		if (WheelLabels[j])
		{
			// Upright, so 10 and Bust can be read while the slice turns.
			WheelLabels[j]->SetActorLocation(Hub + FVector(-34.0f, 0.0f, 0.0f) + WallRadial(A, 236.0f));
			WheelLabels[j]->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
		}
		if (WheelPegs[j])
		{
			WheelPegs[j]->SetActorLocation(Hub + FVector(-26.0f, 0.0f, 0.0f) + WallRadial(A + 15.0f, 334.0f));
		}
	}
	bWheelShowWas = bWheelShow;
	{
		// A peg passes the top every 30 degrees: the flapper kicks and clicks.
		const int32 Peg = FMath::FloorToInt((WheelAngle + 15.0f) / 30.0f);
		if (Peg != LastPeg)
		{
			if (WheelSpin.bLive && LastPeg != MIN_int32)
			{
				FlapperKick = 1.0f;
				CasinoSound(GetWorld(), ECasinoSound::Tick, Hub + FVector(-30.0f, 0.0f, 340.0f), 0.6f, FMath::FRandRange(0.95f, 1.08f));
			}
			LastPeg = Peg;
		}
		FlapperKick = FMath::Max(0.0f, FlapperKick - DeltaSeconds * 6.0f);
		if (WheelFlapper)
		{
			const FVector Pivot = Hub + FVector(-30.0f, 0.0f, 376.0f);
			const FRotator Swing(0.0f, 0.0f, -FlapperKick * 32.0f);
			WheelFlapper->SetActorLocationAndRotation(Pivot + Swing.RotateVector(FVector(0.0f, 0.0f, -24.0f)), Swing);
		}
	}
	for (int32 k = 0; k < 24; ++k)
	{
		float Level = 0.45f + 0.3f * FMath::Sin(float(Now) * 2.0f + k * 0.5f);
		if (WheelSpin.bLive)
		{
			Level = ((k + int32(Now * 14.0)) % 4 == 0) ? 1.6f : 0.15f;
		}
		else if (bWheelShow)
		{
			Level = bBlink ? 1.8f : 0.1f;
		}
		Tint(WheelBulbs[k], WheelBulbColour(k) * Level * 2.0f);
	}

	// ---- Roulette.
	const FVector Bowl = R.Tables[2] + FVector(0.0f, 40.0f, 0.0f);
	const float Step = 360.0f / 37.0f;
	const bool bRouletteShow = bRouletteResult && Tables[2].Phase == 3 && !RouletteSpinAnim.bLive;
	const int32 WinPocket = bRouletteResult ? RoulettePocketOf(Tables[2].Result) : 0;
	for (int32 p = 0; p < 37; ++p)
	{
		const float Deg = RouletteAngle + p * Step;
		if (RoulettePockets[p])
		{
			RoulettePockets[p]->SetActorLocationAndRotation(Bowl + FRotator(0.0f, Deg, 0.0f).Vector() * 80.0f
				+ FVector(0.0f, 0.0f, RouletteTop + 28.0f), FRotator(0.0f, Deg, 0.0f));
		}
		if (RouletteFrets[p])
		{
			const float Fret = Deg + Step * 0.5f;
			RouletteFrets[p]->SetActorLocationAndRotation(Bowl + FRotator(0.0f, Fret, 0.0f).Vector() * 80.0f
				+ FVector(0.0f, 0.0f, RouletteTop + 30.0f), FRotator(0.0f, Fret, 0.0f));
		}
	}
	for (int32 k = 0; k < 2; ++k)
	{
		if (RouletteCross[k])
		{
			RouletteCross[k]->SetActorLocationAndRotation(Bowl + FVector(0.0f, 0.0f, RouletteTop + 50.0f),
				FRotator(0.0f, RouletteAngle + k * 90.0f, 0.0f));
		}
	}
	if (bRouletteShow || (RouletteShownPocket >= 0 && RoulettePockets[RouletteShownPocket]))
	{
		const int32 Number = RouletteOrder[bRouletteShow ? WinPocket : RouletteShownPocket];
		const FLinearColor Own = Number == 0 ? FLinearColor(0.05f, 0.62f, 0.22f)
			: DCCCasinoRedPocket(Number) ? FLinearColor(0.82f, 0.08f, 0.09f) : FLinearColor(0.07f, 0.06f, 0.07f);
		if (bRouletteShow)
		{
			Tint(RoulettePockets[WinPocket], bBlink ? FLinearColor(1.0f, 0.9f, 0.5f) : Own);
			RouletteShownPocket = WinPocket;
		}
		else
		{
			Tint(RoulettePockets[RouletteShownPocket], Own);
			RouletteShownPocket = -1;
		}
	}
	if (RouletteBallActor)
	{
		const float Track = 127.0f;
		const float Pocket = 80.0f;
		float Rel = WinPocket * Step;
		float Radius = Pocket;
		float Z = RouletteTop + 30.0f;
		if (RouletteSpinAnim.bLive)
		{
			const float U = FMath::Clamp(RouletteSpinAnim.Age / RouletteSpinAnim.Duration, 0.0f, 1.0f);
			constexpr float Settle = 0.9f;
			const float E = 1.0f - FMath::Pow(1.0f - FMath::Min(U / Settle, 1.0f), 2.0f);
			// Seven laps the other way round, relative to the rotor, ending in the rolled pocket.
			Rel = WinPocket * Step + 2520.0f * (1.0f - E);
			if (U < 0.5f)
			{
				Radius = Track;
				Z = RouletteTop + 26.0f;
			}
			else if (U < Settle)
			{
				// Off the track, over the diamonds, bouncing pocket to pocket.
				const float V = (U - 0.5f) / (Settle - 0.5f);
				const float Bounce = FMath::Sin(V * PI * 5.0f);
				Radius = FMath::Lerp(Track, Pocket, FMath::SmoothStep(0.0f, 1.0f, V)) + Bounce * (1.0f - V) * 10.0f;
				Z = FMath::Lerp(RouletteTop + 26.0f, RouletteTop + 30.0f, V) + FMath::Abs(Bounce) * (1.0f - V) * 18.0f;
				const int32 Hit = FMath::FloorToInt(V * 5.0f);
				if (Hit != LastBounce)
				{
					LastBounce = Hit;
					CasinoSound(GetWorld(), ECasinoSound::Clack, Bowl + FVector(0.0f, 0.0f, Z), 0.35f + 0.5f * (1.0f - V), FMath::FRandRange(0.9f, 1.15f));
				}
			}
			else if (LastBounce != 99)
			{
				LastBounce = 99;
				CasinoSound(GetWorld(), ECasinoSound::Clack, Bowl + FVector(0.0f, 0.0f, Z), 0.5f, 0.8f);
			}
		}
		const float Abs = RouletteAngle + Rel;
		RouletteBallActor->SetActorLocation(Bowl + FRotator(0.0f, Abs, 0.0f).Vector() * Radius + FVector(0.0f, 0.0f, Z));
	}
	if (bRouletteWasLive && !RouletteSpinAnim.bLive)
	{
		CasinoSound(GetWorld(), ECasinoSound::Chime, Bowl + FVector(0.0f, 0.0f, 120.0f), 0.7f);
	}

	// ---- The wheel's lever.
	WheelLeverAge += DeltaSeconds;
	PoseLever(WheelLever[0], WheelLever[1], R.Tables[4] + FVector(-300.0f, -330.0f, 136.0f), FVector(-1.0f, 0.0f, 0.0f),
		LeverAngle(WheelLeverAge), 150.0f);

	// ---- Reels. A new Serial pulls the lever and spins; the reels stop left to right on the server's symbols.
	for (int32 m = 0; m < SlotCount; ++m)
	{
		const FVector Cm = MachineAt(R, m);
		const FDCCCasinoSlotNet* Net = Slots.IsValidIndex(m) ? &Slots[m] : nullptr;
		const bool bPlayed = Net && Net->Serial > 0; // an unplayed machine shows a mixed row, not three cherries
		const uint8 Result[3] = { bPlayed ? Net->Reel0 : uint8((m + 1) % 6), bPlayed ? Net->Reel1 : uint8((m + 3) % 6), bPlayed ? Net->Reel2 : uint8((m + 5) % 6) };
		if (Net && Net->Serial != SlotSeen[m])
		{
			SlotSeen[m] = Net->Serial;
			if (bEdges)
			{
				SlotLive[m] = true;
				SlotAge[m] = 0.0f;
				for (int32 r = 0; r < 3; ++r)
				{
					ReelFrom[m][r] = ReelAngle[m][r];
					float To = -60.0f * (Result[r] % SlotSymbols);
					while (To > ReelFrom[m][r] - (1080.0f + r * 360.0f))
					{
						To -= 360.0f;
					}
					ReelTo[m][r] = To;
				}
				CasinoSound(GetWorld(), ECasinoSound::Clack, Cm + FVector(-96.0f, -40.0f, 200.0f), 0.8f, 0.6f);
				CasinoSound(GetWorld(), ECasinoSound::Roll, Cm + FVector(0.0f, -60.0f, 185.0f), 0.6f, 0.65f);
			}
			else
			{
				for (int32 r = 0; r < 3; ++r)
				{
					ReelAngle[m][r] = -60.0f * (Result[r] % SlotSymbols);
				}
			}
		}
		if (SlotLive[m])
		{
			const float Was = SlotAge[m];
			SlotAge[m] += DeltaSeconds;
			const float T = SlotAge[m];
			for (int32 r = 0; r < 3; ++r)
			{
				const float Stop = SlotLeverLead + SlotReelStop0 + r * SlotReelGap;
				if (T <= SlotLeverLead)
				{
					ReelAngle[m][r] = ReelFrom[m][r];
				}
				else if (T < Stop)
				{
					const float U = (T - SlotLeverLead) / (Stop - SlotLeverLead);
					ReelAngle[m][r] = FMath::Lerp(ReelFrom[m][r], ReelTo[m][r], 1.0f - FMath::Pow(1.0f - U, 2.0f));
				}
				else
				{
					// A little kick back as it locks.
					const float After = T - Stop;
					ReelAngle[m][r] = ReelTo[m][r] + 6.0f * FMath::Sin(After * 30.0f) * FMath::Exp(-After * 10.0f);
				}
				if (Was < Stop && T >= Stop)
				{
					CasinoSound(GetWorld(), ECasinoSound::Clack, Cm + FVector(-42.0f + r * 42.0f, -60.0f, 185.0f), 0.7f, 0.85f + r * 0.1f);
				}
			}
			const bool bWon = Net && Net->Pay > 0;
			if (Was < SlotSpinEnd + 0.05f && T >= SlotSpinEnd + 0.05f)
			{
				if (bWon)
				{
					CasinoSound(GetWorld(), ECasinoSound::Chime, Cm + FVector(0.0f, -60.0f, 260.0f), Net->Pay >= 20 ? 1.0f : 0.7f, Net->Pay >= 20 ? 1.25f : 1.0f);
				}
				Say(SlotSigns[m], bWon ? FString::Printf(TEXT("WIN %d"), int32(Net->Pay)) : FString(TEXT("NO LUCK")),
					bWon ? FColor(140, 10, 20) : FColor(60, 24, 8));
			}
			const FLinearColor Glow = SlotTopColours[m % 4];
			if (T > SlotSpinEnd && bWon)
			{
				Tint(SlotTops[m], Glow * (FMath::Fmod(T * 6.0f, 1.0f) < 0.5f ? 4.0f : 0.3f));
			}
			if (T > SlotSpinEnd + 3.0f)
			{
				SlotLive[m] = false;
				Tint(SlotTops[m], Glow * 1.5f);
				Say(SlotSigns[m], TEXT("2 A PULL"), FColor(60, 24, 8));
			}
		}
		PoseLever(SlotLevers[m][0], SlotLevers[m][1], Cm + FVector(-96.0f, -30.0f, 210.0f), FVector(0.0f, -1.0f, 0.0f),
			SlotLive[m] ? LeverAngle(SlotAge[m]) : 0.0f, 80.0f);
		for (int32 r = 0; r < 3; ++r)
		{
			const FVector Axle = Cm + FVector(-42.0f + r * 42.0f, -40.0f, 185.0f);
			for (int32 k = 0; k < SlotSymbols; ++k)
			{
				AActor* Symbol = SlotSymbolActors[m][r][k];
				if (!Symbol)
				{
					continue;
				}
				const float A = ReelAngle[m][r] + k * 60.0f;
				const FVector Out = ReelRadial(A);
				const FVector Up = ReelUp(A);
				FRotator Rot = FRotator::ZeroRotator;
				if (k == 3 || k == 5)
				{
					Rot = FRotationMatrix::MakeFromXZ(Out, Up).Rotator(); // text faces out of the reel
				}
				else if (k == 2)
				{
					Rot = FRotationMatrix::MakeFromZX(Up, Out).Rotator(); // the bell stands upright on the reel
				}
				else if (k == 4)
				{
					Rot = FRotationMatrix::MakeFromZX(Out, (FVector(1.0f, 0.0f, 0.0f) + Up).GetSafeNormal()).Rotator(); // a diamond, on its point
				}
				Symbol->SetActorLocationAndRotation(Axle + Out * (k == 3 || k == 5 ? 33.5f : 34.0f), Rot);
			}
		}
	}

	// ---- Skee balls. A new BallSerial rolls one up its lane, over the lip, and into the hole it earned.
	for (int32 s = 0; s < SeatCount; ++s)
	{
		const FDCCCasinoSeatNet* Seat = Seats.IsValidIndex(s) ? &Seats[s] : nullptr;
		const FSkeeLane S = SkeeLane(R.Seats[0][s].At);
		if (bEdges && Seat && Seat->BallSerial != SeenSerial[s] && Seat->LastBall > 0 && SkeeBalls[s])
		{
			BallT[s] = 0.001f;
			BallPts[s] = Seat->LastBall;
			SkeeBalls[s]->SetActorHiddenInGame(false);
			CasinoSound(GetWorld(), ECasinoSound::Roll, S.LaneFrom + FVector(0.0f, 200.0f, 20.0f), 0.9f, FMath::FRandRange(0.95f, 1.05f));
		}
		if (Seat)
		{
			SeenSerial[s] = Seat->BallSerial;
		}
		if (!SkeeBalls[s] || BallT[s] <= 0.0f)
		{
			continue;
		}
		const float Was = BallT[s];
		BallT[s] += DeltaSeconds / 1.9f;
		const float T = BallT[s];
		int32 Ring = 4;
		if (BallPts[s] >= 50) Ring = 0;
		else if (BallPts[s] >= 40) Ring = 1;
		else if (BallPts[s] >= 30) Ring = 2;
		else if (BallPts[s] >= 20) Ring = 3;
		const FVector Target = S.Hole(Ring) + S.BoardOut * 12.0f;
		FVector At = Target;
		if (T < 0.42f)
		{
			// Up the lane, slowing as it climbs, with a little drift.
			const float U = T / 0.42f;
			const float Along = 1.0f - FMath::Pow(1.0f - U, 1.6f);
			At = FMath::Lerp(S.LaneFrom, S.LaneTo, Along) + S.LaneUp * 12.0f + FVector(FMath::Sin(U * 9.0f) * 6.0f * (1.0f - U), 0.0f, 0.0f);
		}
		else if (T < 0.52f)
		{
			const float U = (T - 0.42f) / 0.10f;
			At = FMath::Lerp(S.LaneTo, S.RampTop, U) + S.RampUp * 12.0f;
		}
		else if (T < 0.86f)
		{
			// Off the lip and into the air. The 50 needs the biggest hop.
			const float U = (T - 0.52f) / 0.34f;
			At = FMath::Lerp(S.RampTop + S.RampUp * 12.0f, Target, U);
			At.Z += 4.0f * U * (1.0f - U) * (40.0f + (4 - Ring) * 14.0f);
		}
		else if (T < 1.0f)
		{
			const float U = (T - 0.86f) / 0.14f;
			At = Target - S.BoardOut * (U * 34.0f);
		}
		if (Was < 0.86f && T >= 0.86f)
		{
			CasinoSound(GetWorld(), ECasinoSound::Drop, Target, 0.9f, 1.0f + (4 - Ring) * 0.08f);
			if (Ring <= 1)
			{
				CasinoSound(GetWorld(), ECasinoSound::Chime, Target, 0.6f, Ring == 0 ? 1.2f : 1.0f);
			}
		}
		SkeeBalls[s]->SetActorLocation(At);
		SkeeBalls[s]->SetActorHiddenInGame(T >= 1.0f);

		for (int32 k = 0; k < 5; ++k)
		{
			const bool bFlash = k == Ring && T > 0.86f && T < 1.5f && FMath::Fmod(T * 8.0f, 1.0f) < 0.6f;
			Tint(SkeeRings[s][k], bFlash ? FLinearColor(1.0f, 0.95f, 0.7f) : RingColours[k]);
		}
		if (T > 1.6f)
		{
			BallT[s] = 0.0f;
			SkeeBalls[s]->SetActorHiddenInGame(true);
		}
	}

	// Air hockey puck, and a mallet that leans in when that side has shot.
	if (Puck)
	{
		FVector At = R.Tables[1] + FVector(0.0f, 0.0f, 100.0f);
		if (bEdges && Tables.IsValidIndex(1))
		{
			if (Tables[1].ScoreA != SeenScoreA)
			{
				PuckHit = 0.42f;
				PuckSide = 1;
			}
			else if (Tables[1].ScoreB != SeenScoreB)
			{
				PuckHit = 0.42f;
				PuckSide = -1;
			}
		}
		if (PuckHit > 0.0f)
		{
			PuckHit = FMath::Max(0.0f, PuckHit - DeltaSeconds);
			const float U = 1.0f - FMath::Clamp(PuckHit / 0.42f, 0.0f, 1.0f);
			At.Y += PuckSide * FMath::Lerp(0.0f, 190.0f, U);
		}
		else if (Tables.IsValidIndex(1) && Tables[1].Phase == 2)
		{
			const double MarkAt = Mark(Now, 0);
			At.Y += FMath::Lerp(-150.0, 150.0, MarkAt);
			At.X += FMath::Sin(Now * 3.2) * 28.0;
		}
		Puck->SetActorLocation(At);
	}
	if (Tables.IsValidIndex(1))
	{
		int32 Done[2] = { 0, 0 };
		for (int32 s = 0; s < SeatCount; ++s)
		{
			if (Seats.IsValidIndex(SeatCount + s) && Seats[SeatCount + s].bDone)
			{
				++Done[s < 3 ? 0 : 1];
			}
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (Done[Side] < SeenMallet[Side])
			{
				SeenMallet[Side] = uint8(Done[Side]);
			}
			else if (bEdges && Done[Side] > SeenMallet[Side])
			{
				MalletT[Side] = 1.0f;
				SeenMallet[Side] = uint8(Done[Side]);
			}
			MalletT[Side] = FMath::Max(0.0f, MalletT[Side] - DeltaSeconds * 1.6f);
		}
		if (Mallets[0])
		{
			Mallets[0]->SetActorLocation(R.Tables[1] + FVector(0.0f, -150.0f + MalletT[0] * 55.0f, 100.0f));
		}
		if (Mallets[1])
		{
			Mallets[1]->SetActorLocation(R.Tables[1] + FVector(0.0f, 150.0f - MalletT[1] * 55.0f, 100.0f));
		}
	}

	// Cards on the felt. The dealer's hole card stays face down until she shows.
	if (Tables.IsValidIndex(3))
	{
		const FDCCCasinoTableNet& Pit = Tables[3];
		const FVector Table = R.Tables[3];
		auto Lay = [&](int32 Row, const FVector& Origin, const FVector& Right, uint8 Rank0, uint8 Rank1, uint8 Rank2, uint8 Rank3, uint8 Rank4, bool bHideHole)
		{
			const uint8 Ranks[] = { Rank0, Rank1, Rank2, Rank3, Rank4 };
			for (int32 c = 0; c < 5; ++c)
			{
				AActor* Card = Cards[Row][c];
				if (!Card)
				{
					continue;
				}
				if (Ranks[c] == 0 || Pit.Phase < 2)
				{
					Card->SetActorHiddenInGame(true);
					continue;
				}
				Card->SetActorHiddenInGame(false);
				Card->SetActorLocation(Origin + Right * (c * 26.0f) + FVector(0.0f, 0.0f, 88.0f));
				Card->SetActorRotation(FRotator(0.0f, Right.Rotation().Yaw + 90.0f, 0.0f));
				const bool bDown = bHideHole && c == 1;
				const FLinearColor Colour = bDown ? FLinearColor(0.08f, 0.04f, 0.16f)
					: ((Ranks[c] % 2) ? FLinearColor(0.72f, 0.08f, 0.10f) : FLinearColor(0.93f, 0.90f, 0.84f));
				Tint(Card, Colour);
			}
		};
		Lay(6, Table + FVector(-50.0f, -36.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f),
			Pit.Dealer0, Pit.Dealer1, Pit.Dealer2, Pit.Dealer3, Pit.Dealer4, Pit.bShowDealer == 0);
		for (int32 s = 0; s < SeatCount; ++s)
		{
			if (!Seats.IsValidIndex(3 * SeatCount + s))
			{
				continue;
			}
			const FDCCCasinoSeatNet& Seat = Seats[3 * SeatCount + s];
			const FVector To = (Table - FVector(R.Seats[3][s].At.X, R.Seats[3][s].At.Y, 0.0f)).GetSafeNormal2D();
			const FVector Right(-To.Y, To.X, 0.0f);
			const FVector Origin = FMath::Lerp(FVector(R.Seats[3][s].At.X, R.Seats[3][s].At.Y, 0.0f), Table, 0.40f);
			Lay(s, Origin, Right, Seat.Card0, Seat.Card1, Seat.Card2, Seat.Card3, Seat.Card4, false);
		}
	}

	bool bCelebration = WheelSpin.bLive || RouletteSpinAnim.bLive;
	for (int32 t = 0; t < TableCount; ++t)
	{
		if (!Tables.IsValidIndex(t))
		{
			continue;
		}
		const FDCCCasinoTableNet& Pit = Tables[t];
		if (Pit.Phase == 3)
		{
			bCelebration = true;
		}
		FString Line;
		FColor Colour(255, 220, 170);
		if (t == 4 && WheelSpin.bLive)
		{
			Line = TEXT("The wheel is turning.");
			Colour = FColor(255, 196, 80);
		}
		else if (t == 2 && RouletteSpinAnim.bLive)
		{
			Line = TEXT("The ball is in the wheel.");
			Colour = FColor(255, 196, 80);
		}
		else if (Pit.Phase >= 2 && !Pit.Line.IsEmpty())
		{
			Line = Pit.Line;
			Colour = Pit.Phase == 3 ? FColor(255, 230, 140) : FColor(230, 214, 190);
		}
		else if (t == 0)
		{
			Line = TEXT("Three balls. The rings pay 10 to 50.");
		}
		else if (t == 1)
		{
			Line = Pit.Phase == 0 ? FString(TEXT("First to 3. Gold south, blue north.")) : FString::Printf(TEXT("Gold %d, blue %d."), Pit.ScoreA, Pit.ScoreB);
		}
		else if (t == 2)
		{
			Line = TEXT("Red, black, or green. Five tickets.");
		}
		else if (t == 3)
		{
			Line = TEXT("June deals. Five tickets, or the house comps.");
		}
		else
		{
			Line = TEXT("Pick a wedge. Bust pays nothing.");
		}
		Say(Marquees[t], Line, Colour);
		if (UPointLightComponent* Lit = PitLights[t] ? Cast<UPointLightComponent>(PitLights[t]->GetLightComponent()) : nullptr)
		{
			const bool bHot = Pit.Phase == 3 || (t == 4 && WheelSpin.bLive) || (t == 2 && RouletteSpinAnim.bLive);
			const float Pulse = bHot ? (1.5f + 0.7f * FMath::Sin(float(Now) * 9.0f)) : 0.65f;
			Lit->SetIntensity(1500.0f * Pulse);
			Lit->SetLightColor(bHot ? FLinearColor(1.0f, 0.72f, 0.25f) : FLinearColor(1.0f, 0.84f, 0.5f));
		}
		SeenPhase[t] = Pit.Phase;
		SeenResult[t] = Pit.Result;
	}
	if (Tables.IsValidIndex(1))
	{
		SeenScoreA = Tables[1].ScoreA;
		SeenScoreB = Tables[1].ScoreB;
	}

	for (int32 i = 0; i < 10; ++i)
	{
		const float Wave = bCelebration ? (0.55f + 0.45f * FMath::Sin(float(Now) * 7.0f - i * 0.7f)) : 0.22f;
		Tint(Chase[i], PitColours[i % 5] * (0.35f + Wave));
	}

	if (!bVisualCaught)
	{
		for (int32 s = 0; s < SeatCount && Seats.IsValidIndex(s); ++s)
		{
			SeenSerial[s] = Seats[s].BallSerial;
		}
		bVisualCaught = true;
	}
}

// ---- Staff ----------------------------------------------------------------------------------------------------------

ADCCCasinoStaff::ADCCCasinoStaff()
{
	DialogueTable = nullptr;
	bLoopWhenExhausted = false;
	TalkRadius = 420.0f;
	SpeechColour = FLinearColor(1.0f, 0.86f, 0.55f);

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	IdleAnimation = Idle.Object;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (Mesh && Manny.Succeeded())
	{
		Mesh->SetSkeletalMeshAsset(Manny.Object);
	}
}

void ADCCCasinoStaff::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCCasinoStaff, Post);
}

void ADCCCasinoStaff::BuildScript()
{
	Script.Reset();
	NextLineIndex = 0;
}

void ADCCCasinoStaff::BeginPlay()
{
	SpeakerId = TEXT("CasinoStaff");
	bLoopWhenExhausted = false;
	Super::BeginPlay();
	ApplyLook();
}

void ADCCCasinoStaff::OnRep_Role()
{
	ApplyLook();
}

void ADCCCasinoStaff::ApplyLook()
{
	if (!Mesh || (bDressed && AppliedPost == Post))
	{
		return;
	}
	for (UStaticMeshComponent* Part : Worn)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Worn.Reset();
	static const TCHAR* Names[] = {
		TEXT("Lane Okada"), TEXT("Puck Ruiz"), TEXT("Vale Mercer"), TEXT("June Pell"), TEXT("Otto Voss"), TEXT("Bud Alvarez"),
	};
	static const TCHAR* Jobs[] = {
		TEXT("Skee-Ball"), TEXT("Air Hockey"), TEXT("Roulette"), TEXT("Blackjack"), TEXT("The Wheel"), TEXT("Bar"),
	};
	const int32 Index = FMath::Clamp<int32>(Post, 0, 5);
	DisplayName = FText::FromString(FString::Printf(TEXT("%s - %s"), Names[Index], Jobs[Index]));
	const bool bQuinn = Index == 2 || Index == 3 || Index == 5;
	if (bQuinn)
	{
		if (USkeletalMesh* Quinn = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")))
		{
			Mesh->SetSkeletalMeshAsset(Quinn);
		}
	}
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		AppliedPost = Post;
		bDressed = true;
		ShowIdleText();
		return;
	}

	const FLinearColor Vests[] = {
		FLinearColor(0.75f, 0.15f, 0.18f), FLinearColor(0.1f, 0.25f, 0.55f), FLinearColor(0.12f, 0.12f, 0.14f),
		FLinearColor(0.1f, 0.1f, 0.12f), FLinearColor(0.35f, 0.08f, 0.1f), FLinearColor(0.08f, 0.22f, 0.12f),
	};
	const FLinearColor Trims[] = {
		FLinearColor(1.0f, 0.8f, 0.2f), FLinearColor(0.7f, 0.85f, 1.0f), CarpetGold, CarpetGold, CarpetGold, FLinearColor(0.3f, 0.8f, 0.35f),
	};
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Wear = [&](UStaticMesh* MeshAsset, const FVector& Rel, const FVector& Size, const FLinearColor& Colour)
	{
		if (!MeshAsset || !Collision)
		{
			return;
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetStaticMesh(MeshAsset);
		Part->SetupAttachment(Collision.Get());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(Rel);
		Part->SetRelativeScale3D(Size / 100.0f);
		Part->SetCastShadow(false);
		if (Base)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Part);
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
			Part->SetMaterial(0, Mid);
		}
		Part->RegisterComponent();
		Worn.Add(Part);
	};
	Wear(Cube, FVector(8.0f, 0.0f, 8.0f), FVector(22.0f, 36.0f, 42.0f), Vests[Index]);
	Wear(Cube, FVector(16.0f, 0.0f, 22.0f), FVector(6.0f, 16.0f, 18.0f), Trims[Index]);
	Wear(Sphere, FVector(14.0f, 0.0f, 42.0f), FVector(8.0f, 14.0f, 8.0f), Trims[Index]);
	if (Index == 5)
	{
		Wear(Sphere, FVector(12.0f, 14.0f, 18.0f), FVector(8.0f, 8.0f, 8.0f), FLinearColor(0.15f, 0.55f, 0.2f));
	}
	else if (Index == 4)
	{
		Wear(Sphere, FVector(0.0f, 0.0f, 78.0f), FVector(22.0f, 22.0f, 10.0f), CarpetGold);
	}
	AppliedPost = Post;
	bDressed = true;
	ShowIdleText();
}

void ADCCCasinoStaff::SayRules()
{
	const TCHAR* Text = TEXT("Ask me again in a second.");
	switch (Post)
	{
	case 0:
		Text = TEXT("Three balls. Roll when the mark sits in the gold for a fifty. Tickets are the score, split by ten. Your friends do not have to be here.");
		break;
	case 1:
		Text = TEXT("Gold on the near side, blue on the far side. First to three. Shoot when the bar is on your colour. One player takes the house.");
		break;
	case 2:
		Text = TEXT("Red or black pays even. Green pays fourteen times. Five tickets, or the house comps you if you are broke. Spin when this table is ready.");
		break;
	case 3:
		Text = TEXT("You and whoever sits. Hit or stand. I stand on seventeen. Two cards that make twenty-one pay a little more. Empty chairs stay empty.");
		break;
	case 4:
		Text = TEXT("Pick a wedge. If the wheel lands on it, it pays that many times your five. Bust pays nothing. The Safe Room does not have to sit down.");
		break;
	default:
		Text = TEXT("Cola is 2 tickets. The red is 4. The legal bud is 6. I pour. The tables start themselves.");
		break;
	}
	FDCCDialogueLine Line;
	Line.Text = FText::FromString(Text);
	Line.DisplaySeconds = 8.0f;
	ShowLine(Line);
}

// ---- The pit --------------------------------------------------------------------------------------------------------

ADCCCasinoCascade::ADCCCasinoCascade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetReplicateMovement(false);
	SetNetUpdateFrequency(15.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADCCCasinoCascade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCCasinoCascade, Tables);
	DOREPLIFETIME(ADCCCasinoCascade, Seats);
	DOREPLIFETIME(ADCCCasinoCascade, ServerClock);
	DOREPLIFETIME(ADCCCasinoCascade, Slots);
	DOREPLIFETIME(ADCCCasinoCascade, Scratchcards);
}

void ADCCCasinoCascade::OnRep_Clock()
{
	if (const UWorld* World = GetWorld())
	{
		ClockOffset = ServerClock - World->GetTimeSeconds();
	}
}

double ADCCCasinoCascade::EstimatedNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	return HasAuthority() ? World->GetTimeSeconds() : World->GetTimeSeconds() + ClockOffset;
}

double ADCCCasinoCascade::Mark(double Now, int32 Salt)
{
	const double Wrapped = FMath::Fmod(Now * 0.55 + double(Salt) * 0.17, 1.0);
	return Wrapped < 0.0 ? Wrapped + 1.0 : Wrapped;
}

ADCCCasinoCascade* ADCCCasinoCascade::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADCCCasinoCascade> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADCCCasinoCascade::DrinkInfo(int32 Index, FString& Label, int32& Price, FString& Spoken)
{
	switch (Index)
	{
	case 0:
		Label = TEXT("Cascade Cola");
		Price = 2;
		Spoken = TEXT("Cold. The ice is a suggestion.");
		break;
	case 1:
		Label = TEXT("House Red");
		Price = 4;
		Spoken = TEXT("It is red. That is the review.");
		break;
	default:
		Label = TEXT("Legal Bud");
		Price = 6;
		Spoken = TEXT("On the menu. The label has been read.");
		break;
	}
}

void ADCCCasinoCascade::EnsureArrays()
{
	if (Slots.Num() != SlotCount)
	{
		Slots.SetNum(SlotCount);
	}
	if (Tables.Num() == TableCount && Seats.Num() == TableCount * SeatCount)
	{
		return;
	}
	Tables.SetNum(TableCount);
	Seats.SetNum(TableCount * SeatCount);
}

// ---- Reels and scratchcards -----------------------------------------------------------------------------------------

FVector ADCCCasinoCascade::MachineAt(const FRoom& R, int32 Index)
{
	// On the north wall, between the last skee lane and the roulette table. The card machine is last, a step apart.
	return R.Center + FVector(-100.0f + Index * 180.0f + (Index >= SlotCount ? 30.0f : 0.0f), R.HalfY - 110.0f, 0.0f);
}

int32 ADCCCasinoCascade::SlotNear(const FVector& At) const
{
	const FRoom R = Room();
	int32 Best = -1;
	float BestDist = 120.0f;
	for (int32 i = 0; i <= SlotCount; ++i)
	{
		const float Dist = FVector::Dist2D(At, MachineAt(R, i) + FVector(0.0f, -190.0f, 0.0f));
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

int32 ADCCCasinoCascade::CardAmount(int32 Index)
{
	return CardAmounts[FMath::Clamp(Index, 0, 5)];
}

const FDCCScratchNet* ADCCCasinoCascade::CardFor(const APlayerState* State) const
{
	const FString Name = State ? State->GetPlayerName() : FString();
	if (Name.IsEmpty())
	{
		return nullptr;
	}
	return Scratchcards.FindByPredicate([&Name](const FDCCScratchNet& Card) { return Card.Owner == Name; });
}

bool ADCCCasinoCascade::SpendFor(ADCCCharacterBase* Who, int32 Price)
{
	APlayerState* State = Who ? Who->GetPlayerState() : nullptr;
	const FString Key = DCCProfile::KeyFor(State);
	if (Key.IsEmpty() || !DCCProfile::SpendTickets(Key, Price))
	{
		// No comps here: a free pull that can pay would be a ticket tap.
		Tell(Who, FString::Printf(TEXT("That is %d tickets. Skee-ball pays tickets."), Price));
		return false;
	}
	if (Who->Wallet)
	{
		Who->Wallet->Tickets = DCCProfile::GetTickets(Key);
	}
	Tell(Who, FString::Printf(TEXT("-%d tickets."), Price));
	return true;
}

void ADCCCasinoCascade::PullSlot(ADCCCharacterBase* Who, int32 Machine)
{
	if (!HasAuthority() || !Who || Machine < 0 || Machine >= SlotCount || IsSeated(Who))
	{
		return;
	}
	EnsureArrays();
	if (FVector::Dist2D(Who->GetActorLocation(), MachineAt(Room(), Machine)) > 420.0f)
	{
		Tell(Who, TEXT("Step up to the machine."));
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < SlotBusy[Machine])
	{
		Tell(Who, TEXT("Let the reels stop first."));
		return;
	}
	if (!SpendFor(Who, SlotStake))
	{
		return;
	}
	FDCCCasinoSlotNet& Slot = Slots[Machine];
	Slot.Player = CrawlerName(Who);
	Slot.Reel0 = uint8(Weighted(SlotWeight, SlotSymbols));
	Slot.Reel1 = uint8(Weighted(SlotWeight, SlotSymbols));
	Slot.Reel2 = uint8(Weighted(SlotWeight, SlotSymbols));
	FString Why;
	const int32 Mult = SlotMultiplier(Slot.Reel0, Slot.Reel1, Slot.Reel2, Why);
	Slot.Pay = int16(Mult * SlotStake);
	++Slot.Serial;
	SlotBusy[Machine] = Now + SlotSpinEnd + 0.5;
	SlotRevealAt[Machine] = Now + SlotSpinEnd + 0.1;
	SlotOwed[Machine].Who = Who;
	SlotOwed[Machine].Pay = Slot.Pay;
	SlotOwed[Machine].Text = Why;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s pulled reel %d: %d %d %d, pays %d"), *Slot.Player, Machine, Slot.Reel0, Slot.Reel1, Slot.Reel2, Slot.Pay);
}

void ADCCCasinoCascade::BuyCard(ADCCCharacterBase* Who)
{
	if (!HasAuthority() || !Who || IsSeated(Who))
	{
		return;
	}
	if (FVector::Dist2D(Who->GetActorLocation(), MachineAt(Room(), SlotCount)) > 420.0f)
	{
		Tell(Who, TEXT("Step up to the machine."));
		return;
	}
	ClaimCard(Who); // an unfinished card pays out before the next one
	if (!SpendFor(Who, CardPrice))
	{
		return;
	}
	const FString Name = CrawlerName(Who);
	FDCCScratchNet* Card = Scratchcards.FindByPredicate([&Name](const FDCCScratchNet& Each) { return Each.Owner == Name; });
	if (!Card)
	{
		Card = &Scratchcards.AddDefaulted_GetRef();
		Card->Owner = Name;
	}
	// Decide the card first, then deal the cells: the winning amount three times, every other amount at most twice.
	const int32 Win = FMath::FRand() < 0.45f ? Weighted(CardWeights, 6) : -1;
	TArray<uint8> Cells;
	int32 Count[6] = {};
	if (Win >= 0)
	{
		Cells.Init(uint8(Win), 3);
		Count[Win] = 3;
	}
	while (Cells.Num() < 9)
	{
		const int32 Pick = FMath::RandRange(0, 5);
		if (Pick == Win || Count[Pick] >= 2)
		{
			continue;
		}
		Cells.Add(uint8(Pick));
		++Count[Pick];
	}
	for (int32 i = Cells.Num() - 1; i > 0; --i)
	{
		Cells.Swap(i, FMath::RandRange(0, i));
	}
	Card->Cells = Cells;
	Card->Prize = int16(Win >= 0 ? CardAmounts[Win] : 0);
	Card->bClaimed = 0;
	++Card->Serial;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s bought a card, prize %d"), *Name, int32(Card->Prize));
}

void ADCCCasinoCascade::ClaimCard(ADCCCharacterBase* Who)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	const FString Name = CrawlerName(Who);
	FDCCScratchNet* Card = Scratchcards.FindByPredicate([&Name](const FDCCScratchNet& Each) { return Each.Owner == Name; });
	if (!Card || Card->bClaimed)
	{
		return;
	}
	Card->bClaimed = 1;
	if (Card->Prize > 0)
	{
		Give(Who, Card->Prize, FString::Printf(TEXT("Mother Lode, three %ds"), int32(Card->Prize)));
	}
	else
	{
		Tell(Who, TEXT("No three alike."));
	}
	ForceNetUpdate();
}

void ADCCCasinoCascade::BeginPlay()
{
	Super::BeginPlay();
	BuildFloor();
	if (HasAuthority())
	{
		EnsureArrays();
		SpawnStaff();
	}
}

void ADCCCasinoCascade::SpawnStaff()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FRoom Layout = Room();
	for (int32 i = 0; i < 6; ++i)
	{
		const FTransform Xform(Layout.Staff[i].Face, Layout.Staff[i].At);
		ADCCCasinoStaff* Staff = World->SpawnActorDeferred<ADCCCasinoStaff>(ADCCCasinoStaff::StaticClass(), Xform, this);
		if (!Staff)
		{
			continue;
		}
		Staff->Post = uint8(i);
		Staff->FinishSpawning(Xform);
	}
}

int32 ADCCCasinoCascade::FindSeat(const APlayerState* State, int32& Table, int32& Seat) const
{
	Table = -1;
	Seat = -1;
	if (!State || State->GetPlayerName().IsEmpty())
	{
		return -1;
	}
	const FString& Name = State->GetPlayerName();
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (Seats[i].Name == Name)
		{
			Table = i / SeatCount;
			Seat = i % SeatCount;
			return i;
		}
	}
	return -1;
}

bool ADCCCasinoCascade::IsSeated(const ADCCCharacterBase* Who) const
{
	int32 Table = -1;
	int32 Seat = -1;
	return Who && FindSeat(Who->GetPlayerState(), Table, Seat) >= 0;
}

bool ADCCCasinoCascade::IsSeatedState(const APlayerState* State) const
{
	int32 Table = -1;
	int32 Seat = -1;
	return FindSeat(State, Table, Seat) >= 0;
}

bool ADCCCasinoCascade::Near(const FRoom& Layout, int32 Table, const FVector& At) const
{
	if (!Tables.IsValidIndex(Table))
	{
		return false;
	}
	if (FVector::Dist2D(At, Layout.Tables[Table]) < 1100.0f)
	{
		return true;
	}
	if (Layout.Staff[Table].At.Z != 0.0f && FVector::Dist2D(At, Layout.Staff[Table].At) < 550.0f)
	{
		return true;
	}
	return false;
}

bool ADCCCasinoCascade::Contains(const FVector& At) const
{
	const FRoom Layout = Room();
	const FVector D = At - Layout.Center;
	// The west side includes the doorway. Standing in the opening is already in the pit.
	return D.X >= -(Layout.HalfX + 280.0f) && D.X <= Layout.HalfX
		&& FMath::Abs(D.Y) <= Layout.HalfY
		&& D.Z > -200.0f && D.Z < 1200.0f;
}

int32 ADCCCasinoCascade::ClosestTable(const FVector& At) const
{
	// 420 reaches a stool or a throw line. The aisle (the door, straight east) stays outside it.
	const FRoom Layout = Room();
	int32 Best = -1;
	float BestDist = 420.0f;
	for (int32 t = 0; t < TableCount; ++t)
	{
		float Dist = FVector::Dist2D(At, Layout.Tables[t]);
		for (int32 s = 0; s < SeatCount; ++s)
		{
			Dist = FMath::Min(Dist, FVector::Dist2D(At, Layout.Seats[t][s].At));
		}
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = t;
		}
	}
	return Best;
}

int32 ADCCCasinoCascade::Occupied(int32 Table) const
{
	int32 Count = 0;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		const int32 Index = Table * SeatCount + s;
		if (Seats.IsValidIndex(Index) && !Seats[Index].Name.IsEmpty())
		{
			++Count;
		}
	}
	return Count;
}

void ADCCCasinoCascade::Release(ADCCCharacterBase* Who)
{
	if (!Who)
	{
		return;
	}
	Held.Remove(Who);
	if (UCharacterMovementComponent* Move = Who->GetCharacterMovement())
	{
		if (Move->MovementMode == MOVE_None)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
}

void ADCCCasinoCascade::Sit(ADCCCharacterBase* Who, const FRoom& Layout, int32 Table, int32 Seat)
{
	const FSpot& Spot = Layout.Seats[Table][Seat];
	Who->SetActorLocation(Spot.At, false, nullptr, ETeleportType::TeleportPhysics);
	Who->SetActorRotation(Spot.Face);
	// Turn the camera to the game too (2026-11-21): sitting at the wheel used to leave you looking back across the room.
	if (APlayerController* PC = Cast<APlayerController>(Who->GetController()))
	{
		FRotator View = Spot.Face;
		// Skee looks up the lane, roulette and the card table look down onto the felt, the wheel looks up at the wheel.
		const float Pitches[] = { -24.0f, -26.0f, -34.0f, -26.0f, 10.0f };
		View.Pitch = Pitches[FMath::Clamp(Table, 0, 4)];
		PC->ClientSetRotation(View, true);
	}
	if (UCharacterMovementComponent* Move = Who->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	Held.Add(Who);
}

int32 ADCCCasinoCascade::TakeStake(ADCCCharacterBase* Who, FDCCCasinoSeatNet& Seat)
{
	Seat.Stake = 0;
	APlayerState* State = Who ? Who->GetPlayerState() : nullptr;
	const FString Key = DCCProfile::KeyFor(State);
	if (Key.IsEmpty())
	{
		return 0;
	}
	if (!DCCProfile::SpendTickets(Key, Stake))
	{
		Tell(Who, TEXT("This one is on the house."));
		return 0;
	}
	if (Who->Wallet)
	{
		Who->Wallet->Tickets = DCCProfile::GetTickets(Key);
	}
	Seat.Stake = Stake;
	Tell(Who, TEXT("-5 tickets."));
	return Stake;
}

void ADCCCasinoCascade::Give(ADCCCharacterBase* Who, int32 Amount, const FString& Why)
{
	if (!Who)
	{
		return;
	}
	if (Amount <= 0)
	{
		Tell(Who, Why);
		return;
	}
	UDCCWalletComponent::AwardTickets(Who->GetPlayerState(), Amount, Why);
	// A push hands the stake back. That is not a win, and it must not fill the pit achievements.
	if (Why == TEXT("push"))
	{
		return;
	}
	UDCCAchievementComponent::RecordFor(Who, EDCCAchievementStat::CasinoTicketsWon, Amount);
	if (!Why.StartsWith(TEXT("skee")))
	{
		UDCCAchievementComponent::RecordFor(Who, EDCCAchievementStat::CasinoWins, 1);
	}
	if (Amount >= 50)
	{
		UDCCAchievementComponent::RecordFor(Who, EDCCAchievementStat::CasinoJackpot, 1);
	}
}

uint8 ADCCCasinoCascade::DrawRank() const
{
	return uint8(FMath::RandRange(1, 13));
}

void ADCCCasinoCascade::SetSeatCard(FDCCCasinoSeatNet& Seat, int32 Index, uint8 Rank)
{
	switch (Index)
	{
	case 0: Seat.Card0 = Rank; break;
	case 1: Seat.Card1 = Rank; break;
	case 2: Seat.Card2 = Rank; break;
	case 3: Seat.Card3 = Rank; break;
	default: Seat.Card4 = Rank; break;
	}
}

void ADCCCasinoCascade::SetDealerCard(FDCCCasinoTableNet& Table, int32 Index, uint8 Rank)
{
	switch (Index)
	{
	case 0: Table.Dealer0 = Rank; break;
	case 1: Table.Dealer1 = Rank; break;
	case 2: Table.Dealer2 = Rank; break;
	case 3: Table.Dealer3 = Rank; break;
	default: Table.Dealer4 = Rank; break;
	}
}

void ADCCCasinoCascade::Join(ADCCCharacterBase* Who, int32 Table)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	EnsureArrays();
	const FString Name = CrawlerName(Who);
	if (Name.IsEmpty())
	{
		return;
	}
	if (Table == 5)
	{
		Tell(Who, TEXT("I pour. The tables are the games."));
		return;
	}
	if (!Tables.IsValidIndex(Table))
	{
		return;
	}
	const FRoom Layout = Room();
	if (!Near(Layout, Table, Who->GetActorLocation()))
	{
		Tell(Who, TEXT("Step up to the table."));
		return;
	}
	int32 HaveTable = -1;
	int32 HaveSeat = -1;
	if (FindSeat(Who->GetPlayerState(), HaveTable, HaveSeat) >= 0)
	{
		Tell(Who, HaveTable == Table ? FString(TEXT("You are already in.")) : FString(TEXT("Esc or Tab leaves the other game.")));
		return;
	}
	int32 Open = -1;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		if (Seats[Table * SeatCount + s].Name.IsEmpty())
		{
			Open = s;
			break;
		}
	}
	if (Open < 0)
	{
		Tell(Who, TEXT("Every spot is taken."));
		return;
	}

	FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + Open];
	Seat = FDCCCasinoSeatNet();
	Seat.Name = Name;
	Sit(Who, Layout, Table, Open);

	FDCCCasinoTableNet& Pit = Tables[Table];
	const double Now = GetWorld()->GetTimeSeconds();
	if (Table == 0)
	{
		Pit.Phase = 2;
		Tell(Who, TEXT("You are on the lane. Roll when you like."));
	}
	else if (Pit.Phase == 0 || Pit.Phase == 3)
	{
		Pit.Phase = 1;
		// The wheel waits for someone to pull its lever (2026-11-21). The rest deal after a few seconds.
		Pit.EndsAt = Now + (Table == 4 ? 40.0 : 6.0);
		Pit.Line = Table == 4 ? TEXT("Pick a wedge, then pull the lever.") : TEXT("Sitting down. Deal when you are ready, or wait a few seconds.");
		Tell(Who, TEXT("You are in. This table does not wait on the room."));
	}
	else if (Pit.Phase == 1)
	{
		Pit.EndsAt = FMath::Max(Pit.EndsAt, Now + 2.0);
		Tell(Who, TEXT("You are in."));
	}
	else
	{
		Tell(Who, TEXT("Next hand is yours. You can watch this one."));
	}
	MenuIndex.Remove(Who);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s joined %s seat %d"), *Name, GameTitle(Table), Open);
}

void ADCCCasinoCascade::Leave(ADCCCharacterBase* Who)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	int32 Table = -1;
	int32 Seat = -1;
	const int32 Index = FindSeat(Who->GetPlayerState(), Table, Seat);
	if (Index < 0)
	{
		Release(Who);
		return;
	}
	const bool bWasTurn = Tables.IsValidIndex(Table) && Tables[Table].Turn == Seat;
	Seats[Index] = FDCCCasinoSeatNet();
	Release(Who);
	if (Occupied(Table) == 0 && Tables.IsValidIndex(Table))
	{
		Tables[Table] = FDCCCasinoTableNet();
	}
	else if (bWasTurn)
	{
		AdvanceBlackjack(Table);
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s left %s"), *CrawlerName(Who), GameTitle(Table));
}

void ADCCCasinoCascade::Buy(ADCCCharacterBase* Who, int32 Drink)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	const FRoom Layout = Room();
	if (FVector::Dist2D(Who->GetActorLocation(), Layout.Staff[5].At) > 700.0f)
	{
		bool bClose = false;
		for (int32 i = 0; i < TableCount && !bClose; ++i)
		{
			bClose = Near(Layout, i, Who->GetActorLocation());
		}
		if (!bClose)
		{
			return;
		}
	}
	FString Label;
	int32 Price = 0;
	FString Spoken;
	DrinkInfo(FMath::Clamp(Drink, 0, DrinkCount - 1), Label, Price, Spoken);
	APlayerState* State = Who->GetPlayerState();
	const FString Key = DCCProfile::KeyFor(State);
	if (Key.IsEmpty() || !DCCProfile::SpendTickets(Key, Price))
	{
		Tell(Who, FString::Printf(TEXT("%s is %d tickets."), *Label, Price));
		return;
	}
	if (Who->Wallet)
	{
		Who->Wallet->Tickets = DCCProfile::GetTickets(Key);
	}
	Tell(Who, FString::Printf(TEXT("-%d tickets. %s"), Price, *Spoken));
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s bought %s"), *CrawlerName(Who), *Label);
}

void ADCCCasinoCascade::StartHand(int32 Table)
{
	if (!Tables.IsValidIndex(Table) || Occupied(Table) < 1)
	{
		return;
	}
	FDCCCasinoTableNet& Pit = Tables[Table];
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		if (Seat.Name.IsEmpty())
		{
			continue;
		}
		Seat.bInHand = 1;
		Seat.bDone = 0;
		Seat.Rolls = 0;
		Seat.Score = 0;
		Seat.Card0 = Seat.Card1 = Seat.Card2 = Seat.Card3 = Seat.Card4 = 0;
	}
	Pit.bShowDealer = 0;
	Pit.Dealer0 = Pit.Dealer1 = Pit.Dealer2 = Pit.Dealer3 = Pit.Dealer4 = 0;
	Pit.Turn = 255;
	Pit.Result = -1;

	if (Table == 1)
	{
		Pit.Phase = 2;
		Pit.ScoreA = 0;
		Pit.ScoreB = 0;
		Pit.EndsAt = Now + 5.0;
		Pit.Line = TEXT("First to 3. Gold shoots the left mark, blue the right.");
		for (int32 s = 0; s < SeatCount; ++s)
		{
			Seats[Table * SeatCount + s].bDone = 0;
		}
		return;
	}

	if (Table == 2 || Table == 4)
	{
		const bool bWheel = Table == 4;
		Pit.Result = bWheel ? FMath::RandRange(0, 5) : FMath::RandRange(0, 36);
		FString Summary;
		for (int32 s = 0; s < SeatCount; ++s)
		{
			FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
			if (Seat.Name.IsEmpty())
			{
				continue;
			}
			ADCCCharacterBase* Who = nullptr;
			for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
			{
				if (CrawlerName(*It) == Seat.Name)
				{
					Who = *It;
					break;
				}
			}
			TakeStake(Who, Seat);
			bool bWin = false;
			int32 Pay = 0;
			FString Bet;
			if (bWheel)
			{
				int32 Pick = Seat.Choice >= 20 && Seat.Choice <= 25 ? int32(Seat.Choice) - 20 : 0;
				if (Seat.Choice == 255)
				{
					Pick = 0;
				}
				Bet = WheelWord[Pick];
				bWin = Pick == Pit.Result && WheelPay[Pick] > 0;
				if (bWin)
				{
					Pay = Seat.Stake > 0 ? Seat.Stake * WheelPay[Pick] : WheelPay[Pick];
				}
				else if (Pit.Result == Pick)
				{
					Bet = TEXT("Bust");
				}
			}
			else
			{
				int32 Pick = Seat.Choice >= 10 && Seat.Choice <= 12 ? int32(Seat.Choice) : 10;
				const bool bGreen = Pit.Result == 0;
				const bool bRed = DCCCasinoRedPocket(Pit.Result);
				Bet = Pick == 12 ? TEXT("green") : Pick == 11 ? TEXT("black") : TEXT("red");
				bWin = (Pick == 12 && bGreen) || (Pick == 10 && bRed) || (Pick == 11 && !bRed && !bGreen);
				if (bWin)
				{
					Pay = Seat.Stake > 0 ? (Pick == 12 ? Seat.Stake * 15 : Seat.Stake * 2) : (Pick == 12 ? 8 : 4);
				}
			}
			// Held until the wheel stops (ServerTickGame), so the ticket pop never spoils the spin.
			FPendingPay& Owed = PendingPays[Table].AddDefaulted_GetRef();
			Owed.Who = Who;
			Owed.Pay = bWin ? Pay : 0;
			Owed.Text = bWin ? FString::Printf(TEXT("%s hit"), *Bet) : FString::Printf(TEXT("%s did not come up."), *Bet);
		}
		if (bWheel)
		{
			PendingLine[Table] = FString::Printf(TEXT("The wheel stops on %s."), WheelWord[Pit.Result]);
		}
		else
		{
			const TCHAR* Colour = Pit.Result == 0 ? TEXT("green") : DCCCasinoRedPocket(Pit.Result) ? TEXT("red") : TEXT("black");
			PendingLine[Table] = FString::Printf(TEXT("Ball on %d, %s."), Pit.Result, Colour);
		}
		Pit.Line = bWheel ? TEXT("The wheel is turning.") : TEXT("No more bets.");
		Pit.Phase = 3;
		RevealAt[Table] = Now + (bWheel ? WheelSpinTime : RouletteSpinTime) + 0.3;
		Pit.EndsAt = RevealAt[Table] + 4.5;
		UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s resolved: %s"), GameTitle(Table), *PendingLine[Table]);
		return;
	}

	if (Table == 3)
	{
		for (int32 s = 0; s < SeatCount; ++s)
		{
			FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
			if (Seat.Name.IsEmpty())
			{
				continue;
			}
			ADCCCharacterBase* Who = nullptr;
			for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
			{
				if (CrawlerName(*It) == Seat.Name)
				{
					Who = *It;
					break;
				}
			}
			TakeStake(Who, Seat);
			SetSeatCard(Seat, 0, DrawRank());
			SetSeatCard(Seat, 1, DrawRank());
			if (DCCCasinoHandValue(Seat.Card0, Seat.Card1, 0, 0, 0) == 21)
			{
				Seat.bDone = 1;
			}
		}
		Pit.Dealer0 = DrawRank();
		Pit.Dealer1 = DrawRank();
		Pit.Phase = 2;
		Pit.Line = TEXT("June deals.");
		Pit.EndsAt = Now + 25.0;
		AdvanceBlackjack(Table);
		return;
	}
}

void ADCCCasinoCascade::AdvanceBlackjack(int32 Table)
{
	if (!Tables.IsValidIndex(Table))
	{
		return;
	}
	FDCCCasinoTableNet& Pit = Tables[Table];
	Pit.Turn = 255;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		const FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		if (!Seat.Name.IsEmpty() && Seat.bInHand && !Seat.bDone)
		{
			Pit.Turn = uint8(s);
			Pit.EndsAt = GetWorld()->GetTimeSeconds() + 25.0;
			Pit.Line = FString::Printf(TEXT("%s to act."), *Seat.Name);
			return;
		}
	}
	FinishBlackjack(Table);
}

void ADCCCasinoCascade::FinishBlackjack(int32 Table)
{
	FDCCCasinoTableNet& Pit = Tables[Table];
	const bool bDealerNatural = DCCCasinoHandValue(Pit.Dealer0, Pit.Dealer1, 0, 0, 0) == 21;
	if (!bDealerNatural)
	{
		int32 Slot = Pit.Dealer1 ? (Pit.Dealer2 ? (Pit.Dealer3 ? (Pit.Dealer4 ? -1 : 4) : 3) : 2) : 1;
		while (Slot >= 0 && DCCCasinoHandValue(Pit.Dealer0, Pit.Dealer1, Pit.Dealer2, Pit.Dealer3, Pit.Dealer4) < 17)
		{
			SetDealerCard(Pit, Slot, DrawRank());
			++Slot;
			if (Slot > 4)
			{
				break;
			}
		}
	}
	Pit.bShowDealer = 1;
	const int32 Dealer = DCCCasinoHandValue(Pit.Dealer0, Pit.Dealer1, Pit.Dealer2, Pit.Dealer3, Pit.Dealer4);
	FString Line = FString::Printf(TEXT("June shows %d."), Dealer);
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		if (Seat.Name.IsEmpty() || !Seat.bInHand)
		{
			continue;
		}
		ADCCCharacterBase* Who = nullptr;
		for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
		{
			if (CrawlerName(*It) == Seat.Name)
			{
				Who = *It;
				break;
			}
		}
		const int32 Value = DCCCasinoHandValue(Seat.Card0, Seat.Card1, Seat.Card2, Seat.Card3, Seat.Card4);
		const bool bNatural = Seat.Card2 == 0 && Value == 21;
		const bool bBust = Value > 21;
		if (bBust || (!bNatural && Dealer <= 21 && Dealer > Value))
		{
			Tell(Who, FString::Printf(TEXT("%d. The house keeps it."), Value));
		}
		else if ((bNatural && bDealerNatural) || (Value == Dealer && Dealer <= 21))
		{
			if (Seat.Stake > 0)
			{
				Give(Who, Seat.Stake, TEXT("push"));
			}
			else
			{
				Tell(Who, TEXT("Push."));
			}
		}
		else
		{
			int32 Pay = 4;
			if (Seat.Stake > 0)
			{
				Pay = bNatural ? Seat.Stake * 2 + Seat.Stake / 2 : Seat.Stake * 2;
			}
			else if (bNatural)
			{
				Pay = 6;
			}
			Give(Who, Pay, bNatural ? FString(TEXT("twenty-one")) : FString(TEXT("the hand")));
		}
		Line += FString::Printf(TEXT(" %s %d."), *Seat.Name, Value);
	}
	Pit.Line = Line;
	Pit.Phase = 3;
	Pit.Turn = 255;
	Pit.EndsAt = GetWorld()->GetTimeSeconds() + 7.0;
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] blackjack: %s"), *Line);
}

void ADCCCasinoCascade::ResolveHockey(int32 Table)
{
	FDCCCasinoTableNet& Pit = Tables[Table];
	auto Side = [&](int32 First, bool& bHouse)
	{
		int32 Best = 1000;
		bHouse = true;
		bool bShot = false;
		for (int32 i = 0; i < 3; ++i)
		{
			const FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + First + i];
			if (Seat.Name.IsEmpty() || !Seat.bInHand)
			{
				continue;
			}
			bHouse = false;
			if (Seat.bDone)
			{
				bShot = true;
				Best = FMath::Min(Best, int32(Seat.Score));
			}
		}
		if (bHouse)
		{
			return 18;
		}
		return bShot ? Best : 100;
	};
	bool bHouseA = false;
	bool bHouseB = false;
	const int32 ErrA = Side(0, bHouseA);
	const int32 ErrB = Side(3, bHouseB);
	if (ErrA < ErrB)
	{
		++Pit.ScoreA;
	}
	else if (ErrB < ErrA)
	{
		++Pit.ScoreB;
	}
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		Seat.bDone = 0;
		Seat.Score = 0;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Pit.ScoreA >= 3 || Pit.ScoreB >= 3)
	{
		const bool bGold = Pit.ScoreA >= 3;
		Pit.Line = FString::Printf(TEXT("%s takes it, %d to %d."), bGold ? TEXT("Gold") : TEXT("Blue"), Pit.ScoreA, Pit.ScoreB);
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const bool bWin = bGold ? s < 3 : s >= 3;
			FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
			if (Seat.Name.IsEmpty() || !Seat.bInHand || !bWin)
			{
				continue;
			}
			for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
			{
				if (CrawlerName(*It) == Seat.Name)
				{
					Give(*It, 8, TEXT("air hockey"));
					break;
				}
			}
		}
		Pit.Phase = 3;
		Pit.EndsAt = Now + 6.0;
		UE_LOG(LogTemp, Log, TEXT("[DCC Casino] hockey: %s"), *Pit.Line);
		return;
	}
	Pit.EndsAt = Now + 5.0;
	Pit.Line = FString::Printf(TEXT("Gold %d, blue %d."), Pit.ScoreA, Pit.ScoreB);
}

void ADCCCasinoCascade::FinishShow(int32 Table)
{
	if (Occupied(Table) <= 0)
	{
		Tables[Table] = FDCCCasinoTableNet();
		return;
	}
	FDCCCasinoTableNet Cleared;
	Cleared.Phase = Table == 0 ? 2 : 1;
	Cleared.EndsAt = GetWorld()->GetTimeSeconds() + (Table == 4 ? 40.0 : 6.0);
	Cleared.Line = Tables[Table].Line;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		const FString Name = Seat.Name;
		Seat = FDCCCasinoSeatNet();
		Seat.Name = Name;
	}
	Tables[Table] = Cleared;
}

void ADCCCasinoCascade::Choose(ADCCCharacterBase* Who, int32 ChoiceId)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	if (ChoiceId == 0)
	{
		Leave(Who);
		return;
	}
	int32 Table = -1;
	int32 SeatIndex = -1;
	const int32 Index = FindSeat(Who->GetPlayerState(), Table, SeatIndex);
	if (Index < 0 || !Tables.IsValidIndex(Table))
	{
		return;
	}
	FDCCCasinoTableNet& Pit = Tables[Table];
	FDCCCasinoSeatNet& Seat = Seats[Index];
	const double Now = GetWorld()->GetTimeSeconds();

	if (Table == 0 && ChoiceId == 1)
	{
		const double At = Mark(Now, SeatIndex);
		const int32 Points = SkeePoints(At);
		Seat.Score = int16(Seat.Score + Points);
		++Seat.Rolls;
		Seat.LastBall = uint8(Points);
		++Seat.BallSerial;
		Tell(Who, FString::Printf(TEXT("%d."), Points));
		if (Seat.Rolls >= 3)
		{
			const int32 Tickets = FMath::Max(1, int32(Seat.Score) / 10);
			Give(Who, Tickets, FString::Printf(TEXT("skee-ball %d"), int32(Seat.Score)));
			Pit.Line = FString::Printf(TEXT("%s rolled %d."), *Seat.Name, int32(Seat.Score));
			Seat.Score = 0;
			Seat.Rolls = 0;
			UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s skee %d tickets"), *Seat.Name, Tickets);
		}
		ForceNetUpdate();
		return;
	}

	if (Pit.Phase == 1 && (ChoiceId == 10 || ChoiceId == 11 || ChoiceId == 12 || (ChoiceId >= 20 && ChoiceId <= 25)))
	{
		Seat.Choice = uint8(ChoiceId);
		bool bAll = true;
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const FDCCCasinoSeatNet& Other = Seats[Table * SeatCount + s];
			if (!Other.Name.IsEmpty() && Other.Choice == 255)
			{
				bAll = false;
			}
		}
		if (bAll && Table != 4) // the wheel goes on its lever
		{
			StartHand(Table);
		}
		ForceNetUpdate();
		return;
	}

	if (Pit.Phase == 1 && ChoiceId == 1)
	{
		StartHand(Table);
		ForceNetUpdate();
		return;
	}

	if (Table == 1 && Pit.Phase == 2 && ChoiceId == 1 && Seat.bInHand && !Seat.bDone)
	{
		const bool bGold = SeatIndex < 3;
		const double At = Mark(Now, 0);
		const double Sweet = bGold ? 0.22 : 0.78;
		Seat.Score = int16(FMath::Clamp(FMath::RoundToInt(FMath::Abs(At - Sweet) * 100.0), 0, 100));
		Seat.bDone = 1;
		bool bGoldShot = false;
		bool bBlueShot = false;
		bool bGoldPlayer = false;
		bool bBluePlayer = false;
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const FDCCCasinoSeatNet& Other = Seats[Table * SeatCount + s];
			if (Other.Name.IsEmpty() || !Other.bInHand)
			{
				continue;
			}
			if (s < 3)
			{
				bGoldPlayer = true;
				bGoldShot |= Other.bDone != 0;
			}
			else
			{
				bBluePlayer = true;
				bBlueShot |= Other.bDone != 0;
			}
		}
		if ((!bGoldPlayer || bGoldShot) && (!bBluePlayer || bBlueShot))
		{
			ResolveHockey(Table);
		}
		ForceNetUpdate();
		return;
	}

	if (Table == 3 && Pit.Phase == 2 && Pit.Turn == SeatIndex && Seat.bInHand && !Seat.bDone)
	{
		if (ChoiceId == 30)
		{
			int32 Slot = -1;
			if (Seat.Card0 == 0) Slot = 0;
			else if (Seat.Card1 == 0) Slot = 1;
			else if (Seat.Card2 == 0) Slot = 2;
			else if (Seat.Card3 == 0) Slot = 3;
			else if (Seat.Card4 == 0) Slot = 4;
			if (Slot >= 0)
			{
				SetSeatCard(Seat, Slot, DrawRank());
			}
			const int32 Value = DCCCasinoHandValue(Seat.Card0, Seat.Card1, Seat.Card2, Seat.Card3, Seat.Card4);
			if (Value > 21)
			{
				Seat.bDone = 1;
				Tell(Who, FString::Printf(TEXT("Bust, %d."), Value));
				AdvanceBlackjack(Table);
			}
			else if (Value == 21 || Slot < 0)
			{
				Seat.bDone = 1;
				AdvanceBlackjack(Table);
			}
			else
			{
				Pit.EndsAt = Now + 25.0;
			}
		}
		else if (ChoiceId == 31)
		{
			Seat.bDone = 1;
			AdvanceBlackjack(Table);
		}
		ForceNetUpdate();
	}
}

void ADCCCasinoCascade::ServerTickGame()
{
	EnsureArrays();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	ServerClock = Now;
	for (int32 t = 0; t < TableCount; ++t)
	{
		FDCCCasinoTableNet& Pit = Tables[t];
		if (t < SlotCount && SlotRevealAt[t] > 0.0 && Now >= SlotRevealAt[t])
		{
			// The reels have stopped on every screen: pay, or say so.
			SlotRevealAt[t] = 0.0;
			Give(SlotOwed[t].Who.Get(), SlotOwed[t].Pay, SlotOwed[t].Text);
			SlotOwed[t] = FPendingPay();
		}
		if (RevealAt[t] > 0.0 && (Now >= RevealAt[t] || Occupied(t) == 0))
		{
			RevealAt[t] = 0.0;
			if (!PendingLine[t].IsEmpty())
			{
				Pit.Line = PendingLine[t];
			}
			for (const FPendingPay& Owed : PendingPays[t])
			{
				Give(Owed.Who.Get(), Owed.Pay, Owed.Text);
			}
			PendingPays[t].Reset();
			PendingLine[t].Reset();
		}
		if (Pit.Phase == 0)
		{
			continue;
		}
		if (Occupied(t) == 0)
		{
			Pit = FDCCCasinoTableNet();
			continue;
		}
		if (Pit.Phase == 1 && Now >= Pit.EndsAt)
		{
			StartHand(t);
		}
		else if (Pit.Phase == 2 && t == 1 && Now >= Pit.EndsAt)
		{
			ResolveHockey(t);
		}
		else if (Pit.Phase == 2 && t == 3 && Now >= Pit.EndsAt && Pit.Turn != 255)
		{
			Seats[t * SeatCount + Pit.Turn].bDone = 1;
			AdvanceBlackjack(t);
		}
		else if (Pit.Phase == 3 && Now >= Pit.EndsAt)
		{
			FinishShow(t);
		}
	}
}

void ADCCCasinoCascade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFloorBuilt)
	{
		BuildFloor();
	}
	AnimateFloor(DeltaSeconds);
	if (HasAuthority())
	{
		ServerTickGame();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FRoom Layout = Room();
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Who = *It;
		UCharacterMovementComponent* Move = Who->GetCharacterMovement();
		if (!Move)
		{
			continue;
		}
		int32 Table = -1;
		int32 Seat = -1;
		if (FindSeat(Who->GetPlayerState(), Table, Seat) >= 0)
		{
			if (!Held.Contains(Who))
			{
				Move->StopMovementImmediately();
				Move->DisableMovement();
				Held.Add(Who);
			}
			if (HasAuthority())
			{
				const FSpot& Spot = Layout.Seats[Table][Seat];
				if (FVector::Dist2D(Who->GetActorLocation(), Spot.At) > 45.0f)
				{
					Who->SetActorLocation(Spot.At, false, nullptr, ETeleportType::TeleportPhysics);
				}
				Who->SetActorRotation(Spot.Face);
			}
		}
		else if (Held.Contains(Who))
		{
			Release(Who);
		}
	}
}

void ADCCCasinoCascade::FillOptions(const ADCCCharacterBase* Who, TArray<FString>& Labels, TArray<int32>& Ids) const
{
	Labels.Reset();
	Ids.Reset();
	int32 Table = -1;
	int32 Seat = -1;
	if (!Who || FindSeat(Who->GetPlayerState(), Table, Seat) < 0 || !Tables.IsValidIndex(Table))
	{
		return;
	}
	const FDCCCasinoTableNet& Pit = Tables[Table];
	const FDCCCasinoSeatNet& Mine = Seats[Table * SeatCount + Seat];
	auto Add = [&](const TCHAR* Label, int32 Id)
	{
		FString Row(Label);
		if (Mine.Choice == Id)
		{
			Row += TEXT("  (yours)");
		}
		Labels.Add(Row);
		Ids.Add(Id);
	};

	if (Table == 0)
	{
		Add(TEXT("Roll"), 1);
		Add(TEXT("Leave"), 0);
		return;
	}
	if (Pit.Phase == 1)
	{
		if (Table == 2)
		{
			Add(TEXT("Red"), 10);
			Add(TEXT("Black"), 11);
			Add(TEXT("Green"), 12);
			Add(TEXT("Spin"), 1);
		}
		else if (Table == 4)
		{
			for (int32 i = 0; i < 6; ++i)
			{
				Add(WheelWord[i], 20 + i);
			}
			Add(TEXT("Pull the lever"), 1);
		}
		else if (Table == 1)
		{
			Add(TEXT("Drop the puck"), 1);
		}
		else
		{
			Add(TEXT("Deal"), 1);
		}
		Add(TEXT("Leave"), 0);
		return;
	}
	if (Pit.Phase == 2 && Table == 1)
	{
		Add(TEXT("Shoot"), 1);
		Add(TEXT("Leave"), 0);
		return;
	}
	if (Pit.Phase == 2 && Table == 3 && Pit.Turn == Seat && Mine.bInHand && !Mine.bDone)
	{
		Add(TEXT("Hit"), 30);
		Add(TEXT("Stand"), 31);
	}
	Add(TEXT("Leave"), 0);
}

int32 ADCCCasinoCascade::LocalIndex(const ADCCCharacterBase* Who) const
{
	const int32* Found = MenuIndex.Find(Who);
	return Found ? *Found : 0;
}

void ADCCCasinoCascade::LocalScroll(const ADCCCharacterBase* Who, int32 Delta)
{
	TArray<FString> Labels;
	TArray<int32> Ids;
	FillOptions(Who, Labels, Ids);
	if (Ids.IsEmpty())
	{
		return;
	}
	int32& Index = MenuIndex.FindOrAdd(Who);
	Index = ((Index + Delta) % Ids.Num() + Ids.Num()) % Ids.Num();
}

void ADCCCasinoCascade::LocalConfirm(ADCCCharacterBase* Who)
{
	if (!Who)
	{
		return;
	}
	TArray<FString> Labels;
	TArray<int32> Ids;
	FillOptions(Who, Labels, Ids);
	if (Ids.IsEmpty())
	{
		return;
	}
	int32& Index = MenuIndex.FindOrAdd(Who);
	Index = FMath::Clamp(Index, 0, Ids.Num() - 1);
	if (Who->HasAuthority())
	{
		Who->Server_CasinoAction_Implementation(2, Ids[Index]);
	}
	else
	{
		Who->Server_CasinoAction(2, Ids[Index]);
	}
}

void ADCCCharacterBase::Server_CasinoAction_Implementation(uint8 Action, int32 Arg)
{
	ADCCCasinoCascade* Casino = ADCCCasinoCascade::Get(GetWorld());
	if (!Casino)
	{
		return;
	}
	switch (Action)
	{
	case 1: Casino->Join(this, Arg); break;
	case 2: Casino->Choose(this, Arg); break;
	case 3: Casino->Leave(this); break;
	case 4: Casino->Buy(this, Arg); break;
	case 5: Casino->PullSlot(this, Arg); break;
	case 6: Casino->BuyCard(this); break;
	case 7: Casino->ClaimCard(this); break;
	default: break;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GCasinoGo(
	TEXT("DCC.CasinoGo"),
	TEXT("Dev: stand in the Casino Cascade doorway, looking in. Optional delay in seconds."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const float Delay = Args.Num() > 0 ? FCString::Atof(*Args.Last()) : 0.0f;
		auto Go = [](UWorld* W)
		{
			if (!W)
			{
				return;
			}
			ADCCArcadeLobby* Lobby = nullptr;
			for (TActorIterator<ADCCArcadeLobby> It(W); It; ++It)
			{
				Lobby = *It;
				break;
			}
			APlayerController* PC = W->GetFirstPlayerController();
			if (!Lobby || !PC || !PC->GetPawn())
			{
				return;
			}
			float L = 0.0f;
			float Wd = 0.0f;
			ADCCCasinoCascade::DenBounds(L, Wd);
			const FRotator Face(-4.0f, 6.0f, 0.0f);
			// Inside the doorway, a step south of center, so the east-wall wheel is not behind her head.
			APawn* Pawn = PC->GetPawn();
			Pawn->SetActorLocation(Lobby->GetActorLocation() + FVector(L - 280.0f, -140.0f, 120.0f), false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->SetActorRotation(Face);
			PC->SetControlRotation(Face);
		};
		if (Delay <= 0.0f)
		{
			Go(World);
			return;
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> Weak(World);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Go]()
		{
			if (Weak.IsValid())
			{
				Go(Weak.Get());
			}
		}), Delay, false);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCasinoMachine(
	TEXT("DCC.CasinoMachine"),
	TEXT("Dev: walk up to a casino machine (0-3 slots, 4 the card machine) and use it. Delay is the last argument."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const int32 Which = FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, 0, ADCCCasinoCascade::SlotCount);
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args.Last()) : 0.0f;
		TWeakObjectPtr<UWorld> Weak(World);
		auto Go = [Weak, Which]()
		{
			UWorld* W = Weak.Get();
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			ADCCCharacterBase* Who = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
			ADCCCasinoCascade* Casino = W ? ADCCCasinoCascade::Get(W) : nullptr;
			if (!Who || !Casino)
			{
				return;
			}
			const FVector At = ADCCCasinoCascade::MachineAt(ADCCCasinoCascade::RoomFor(Casino->GetActorLocation()), Which)
				+ FVector(0.0f, -190.0f, 100.0f);
			Who->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			const FRotator Face(-12.0f, 90.0f, 0.0f);
			Who->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
			PC->SetControlRotation(Face);
			Who->Server_CasinoAction_Implementation(Which == ADCCCasinoCascade::SlotCount ? 6 : 5, Which);
		};
		if (Delay <= 0.0f)
		{
			Go();
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Go), Delay, false);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCasinoSit(
	TEXT("DCC.CasinoSit"),
	TEXT("Dev: sit the local crawler at a casino game (0 skee, 1 hockey, 2 roulette, 3 blackjack, 4 wheel). Delay is the last argument."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const int32 Which = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args.Last()) : 0.0f;
		TWeakObjectPtr<UWorld> Weak(World);
		auto Go = [Weak, Which]()
		{
			UWorld* W = Weak.Get();
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			ADCCCharacterBase* Who = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
			ADCCCasinoCascade* Casino = W ? ADCCCasinoCascade::Get(W) : nullptr;
			if (Who && Casino)
			{
				const int32 Table = FMath::Clamp(Which, 0, ADCCCasinoCascade::TableCount - 1);
				// Walk up to the middle seat first: Join wants you at the table.
				Who->SetActorLocation(ADCCCasinoCascade::RoomFor(Casino->GetActorLocation()).Seats[Table][2].At, false, nullptr, ETeleportType::TeleportPhysics);
				Casino->Join(Who, Table);
			}
		};
		if (Delay <= 0.0f)
		{
			Go();
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Go), Delay, false);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCasinoSpot(
	TEXT("DCC.CasinoSpot"),
	TEXT("Dev: stand at a casino game, looking at it. Station 0 skee, 1 hockey, 2 roulette, 3 blackjack, 4 wheel. Delay is the last argument."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const int32 Which = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args.Last()) : 0.0f;
		auto Go = [Which](UWorld* W)
		{
			if (!W)
			{
				return;
			}
			ADCCArcadeLobby* Lobby = nullptr;
			for (TActorIterator<ADCCArcadeLobby> It(W); It; ++It)
			{
				Lobby = *It;
				break;
			}
			APlayerController* PC = W->GetFirstPlayerController();
			if (!Lobby || !PC || !PC->GetPawn())
			{
				return;
			}
			const ADCCCasinoCascade::FRoom R = ADCCCasinoCascade::RoomFor(Lobby->GetActorLocation());
			const int32 Index = FMath::Clamp(Which, 0, ADCCCasinoCascade::TableCount - 1);
			// Stand on the open side of the machine, far enough that the camera sees the game.
			FVector Back(0.0f, -1.0f, 0.0f);
			float Dist = 700.0f;
			float LookZ = 80.0f;
			if (Index == 0)
			{
				Dist = 1080.0f;
				LookZ = 150.0f;
			}
			else if (Index == 1)
			{
				// From the aisle, looking south. The south wall is too close for a camera behind her.
				Back = FVector(0.0f, 1.0f, 0.0f);
				Dist = 860.0f;
				LookZ = 110.0f;
			}
			else if (Index == 3)
			{
				Back = FVector(0.0f, 1.0f, 0.0f);
				Dist = 680.0f;
			}
			else if (Index == 4)
			{
				Back = FVector(-1.0f, -0.15f, 0.0f).GetSafeNormal();
				Dist = 1400.0f;
				LookZ = 480.0f;
			}
			FVector At = R.Tables[Index] + Back * Dist;
			At.Z = 120.0f;
			const FVector Look(R.Tables[Index].X, R.Tables[Index].Y, LookZ);
			FRotator Face = (Look - FVector(At.X, At.Y, 170.0f)).Rotation();
			Face.Roll = 0.0f;
			APawn* Pawn = PC->GetPawn();
			Pawn->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->SetActorRotation(Face);
			PC->SetControlRotation(Face);
			if (UCharacterMovementComponent* Move = Pawn->FindComponentByClass<UCharacterMovementComponent>())
			{
				Move->StopMovementImmediately();
			}
		};
		if (Delay <= 0.0f)
		{
			Go(World);
			return;
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> Weak(World);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Go]()
		{
			if (Weak.IsValid())
			{
				Go(Weak.Get());
			}
		}), Delay, false);
	}));
