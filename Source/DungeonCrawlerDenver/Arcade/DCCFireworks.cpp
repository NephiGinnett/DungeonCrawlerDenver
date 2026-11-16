#include "Arcade/DCCFireworks.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	const float ShowSeconds = 7.5f;
	const float BannerSeconds = 6.0f;
	const float SparkLife = 1.8f;
	const int32 Rate = 22050;

	USoundWaveProcedural* MakeWave(UWorld* World, const TArray<int16>& Samples)
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(Rate);
		Wave->NumChannels = 1;
		Wave->Duration = float(Samples.Num()) / float(Rate);
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		return Wave;
	}

	void Normalise(const TArray<float>& In, TArray<int16>& Out, float Peak = 0.85f)
	{
		float Max = 0.0001f;
		for (float V : In) { Max = FMath::Max(Max, FMath::Abs(V)); }
		Out.SetNumUninitialized(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			Out[i] = int16(FMath::Clamp(In[i] / Max * Peak, -1.0f, 1.0f) * 32000.0f);
		}
	}

	/** A short brass-ish fanfare: sawtooth-like harmonics, a little vibrato, a chord at the end. */
	void Fanfare(TArray<int16>& Out)
	{
		struct FNote { float Hz, Start, Length; };
		const FNote Notes[] = {
			{ 392.0f, 0.00f, 0.17f }, { 523.3f, 0.19f, 0.17f }, { 659.3f, 0.38f, 0.17f }, { 784.0f, 0.57f, 0.45f },
			{ 659.3f, 1.06f, 0.14f }, { 784.0f, 1.22f, 1.10f }, { 523.3f, 1.22f, 1.10f }, { 659.3f, 1.22f, 1.10f },
			{ 1046.5f, 1.40f, 0.92f },
		};
		const float Total = 2.45f;
		TArray<float> Mix;
		Mix.SetNumZeroed(int32(Total * Rate));
		for (const FNote& N : Notes)
		{
			const int32 From = int32(N.Start * Rate), Count = int32(N.Length * Rate);
			for (int32 i = 0; i < Count && From + i < Mix.Num(); ++i)
			{
				const float T = float(i) / Rate;
				const float Attack = FMath::Min(1.0f, T / 0.025f);
				const float Release = FMath::Min(1.0f, (N.Length - T) / 0.08f);
				const float Vibrato = T > 0.15f ? 1.0f + 0.006f * FMath::Sin(2.0f * PI * 5.5f * T) : 1.0f;
				float V = 0.0f;
				for (int32 k = 1; k <= 7; ++k)
				{
					V += FMath::Sin(2.0f * PI * N.Hz * k * Vibrato * T) / float(k) * (k == 1 ? 1.0f : 0.8f);
				}
				Mix[From + i] += V * Attack * Release * 0.3f;
			}
		}
		Normalise(Mix, Out, 0.8f);
	}

	/** A firework: a thump, a bright burst of noise, then crackle. */
	void PopSound(TArray<int16>& Out, int32 Seed)
	{
		FRandomStream R(Seed);
		const float Total = 1.3f;
		TArray<float> Mix;
		Mix.SetNumZeroed(int32(Total * Rate));
		for (int32 i = 0; i < Mix.Num(); ++i)
		{
			const float T = float(i) / Rate;
			float V = FMath::Sin(2.0f * PI * 55.0f * T) * FMath::Exp(-T / 0.12f) * 0.9f;   // thump
			V += R.FRandRange(-1.0f, 1.0f) * FMath::Exp(-T / 0.07f) * 0.8f;                  // bang
			Mix[i] = V;
		}
		// Crackle: sparse little clicks, thinning out.
		for (int32 c = 0; c < 70; ++c)
		{
			const float At = 0.18f + FMath::Pow(R.FRand(), 1.6f) * 1.0f;
			const int32 From = int32(At * Rate);
			const float Loud = R.FRandRange(0.15f, 0.45f) * (1.2f - At);
			for (int32 i = 0; i < 90 && From + i < Mix.Num(); ++i)
			{
				Mix[From + i] += R.FRandRange(-1.0f, 1.0f) * Loud * FMath::Exp(-i / 20.0f);
			}
		}
		Normalise(Mix, Out, 0.7f);
	}

	const FLinearColor Palette[] = {
		FLinearColor(1.0f, 0.25f, 0.2f), FLinearColor(1.0f, 0.8f, 0.2f), FLinearColor(0.3f, 0.6f, 1.0f),
		FLinearColor(0.4f, 1.0f, 0.4f), FLinearColor(0.9f, 0.4f, 1.0f), FLinearColor(1.0f, 1.0f, 1.0f),
	};
}

ADCCFireworks::ADCCFireworks()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADCCFireworks::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCFireworks, Seed);
	DOREPLIFETIME(ADCCFireworks, Climber);
}

void ADCCFireworks::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SetLifeSpan(ShowSeconds + 3.0f);
	}
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	FRandomStream R(Seed);
	const int32 Count = 10;
	for (int32 b = 0; b < Count; ++b)
	{
		FBurst Burst;
		Burst.LaunchAt = 0.3f + b * 0.6f + R.FRandRange(0.0f, 0.3f);
		Burst.Center = GetActorLocation() + FVector(R.FRandRange(-1000, 1000), R.FRandRange(-1000, 1000), R.FRandRange(1000, 1900));
		Burst.Colour = Palette[R.RandRange(0, UE_ARRAY_COUNT(Palette) - 1)];
		const int32 Sparks = 48;
		for (int32 s = 0; s < Sparks; ++s)
		{
			Burst.Velocities.Add(R.GetUnitVector() * R.FRandRange(650.0f, 1000.0f));
		}

		auto MakeIsm = [this, Sphere, Material, &Burst]()
		{
			UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this);
			Ism->SetupAttachment(GetRootComponent());
			Ism->SetUsingAbsoluteLocation(true);
			Ism->SetStaticMesh(Sphere);
			Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Ism->SetCastShadow(false);
			Ism->RegisterComponent();
			if (Material)
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Material, Ism);
				Mid->SetVectorParameterValue(TEXT("Color"), Burst.Colour);
				Mid->SetVectorParameterValue(TEXT("BaseColor"), Burst.Colour);
				Ism->SetMaterial(0, Mid);
			}
			Ism->SetVisibility(false);
			return Ism;
		};
		Burst.Rocket = MakeIsm();
		Burst.Rocket->AddInstance(FTransform(FQuat::Identity, GetActorLocation(), FVector(0.12f)), true);
		Burst.Sparks = MakeIsm();
		for (int32 s = 0; s < Sparks; ++s)
		{
			Burst.Sparks->AddInstance(FTransform(FQuat::Identity, Burst.Center, FVector(0.2f)), true);
		}

		Burst.Flash = NewObject<UPointLightComponent>(this);
		Burst.Flash->SetupAttachment(GetRootComponent());
		Burst.Flash->SetUsingAbsoluteLocation(true);
		Burst.Flash->SetWorldLocation(Burst.Center);
		Burst.Flash->SetLightColor(Burst.Colour);
		Burst.Flash->SetAttenuationRadius(4500.0f);
		Burst.Flash->SetCastShadows(false);
		Burst.Flash->SetIntensity(0.0f);
		Burst.Flash->RegisterComponent();
		Bursts.Add(Burst);
	}

	// The fanfare, for everyone: a summit is the whole party's moment.
	TArray<int16> Samples;
	Fanfare(Samples);
	UGameplayStatics::PlaySound2D(World, MakeWave(World, Samples), 0.75f);
}

void ADCCFireworks::Pop(FBurst& Burst)
{
	Burst.bPopped = true;
	UWorld* World = GetWorld();
	TArray<int16> Samples;
	PopSound(Samples, Seed + int32(Burst.LaunchAt * 1000.0f));
	UGameplayStatics::PlaySound2D(World, MakeWave(World, Samples), 0.55f, FMath::FRandRange(0.85f, 1.15f));
}

void ADCCFireworks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float RocketTime = 0.7f;
	for (FBurst& Burst : Bursts)
	{
		const float T = Age - Burst.LaunchAt;
		if (T < 0.0f || !Burst.Sparks)
		{
			continue;
		}
		if (T < RocketTime)
		{
			// The rocket: up from the peak to where it will burst, slowing as it goes.
			const float A = T / RocketTime;
			const FVector At = FMath::Lerp(GetActorLocation(), Burst.Center, 1.0f - FMath::Square(1.0f - A));
			Burst.Rocket->SetVisibility(true);
			Burst.Rocket->UpdateInstanceTransform(0, FTransform(FQuat::Identity, At, FVector(0.12f)), true, true);
			continue;
		}
		Burst.Rocket->SetVisibility(false);
		if (!Burst.bPopped)
		{
			Pop(Burst);
			Burst.Sparks->SetVisibility(true);
		}
		const float S = T - RocketTime;
		if (S > SparkLife)
		{
			Burst.Sparks->SetVisibility(false);
			Burst.Flash->SetIntensity(0.0f);
			continue;
		}
		const float Spread = (1.0f - FMath::Exp(-2.2f * S)) / 2.2f; // drag: fast out, then hanging
		const float Size = 0.22f * (1.0f - S / SparkLife) + 0.02f;
		for (int32 i = 0; i < Burst.Velocities.Num(); ++i)
		{
			const FVector At = Burst.Center + Burst.Velocities[i] * Spread + FVector(0, 0, -160.0f * S * S);
			Burst.Sparks->UpdateInstanceTransform(i, FTransform(FQuat::Identity, At, FVector(Size)), true, i == Burst.Velocities.Num() - 1, true);
		}
		Burst.Flash->SetIntensity(80000.0f * FMath::Exp(-S / 0.35f));
	}
}

void ADCCFireworks::DrawBanners(UWorld* World, UCanvas* Canvas)
{
	if (!World || !Canvas)
	{
		return;
	}
	float Y = Canvas->ClipY * 0.2f;
	for (TActorIterator<ADCCFireworks> It(World); It; ++It)
	{
		const float A = It->Age;
		if (A > BannerSeconds)
		{
			continue;
		}
		const float Alpha = FMath::Min(1.0f, A / 0.25f) * FMath::Min(1.0f, (BannerSeconds - A) / 0.8f);
		const float Bounce = 1.0f + 0.25f * FMath::Exp(-A * 5.0f) * FMath::Cos(A * 18.0f);
		const FString Text = FString::Printf(TEXT("%s REACHED THE SUMMIT!"), *It->Climber.ToUpper());
		UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
		const float Scale = 2.6f * Bounce * (Canvas->ClipY / 1080.0f);
		float W = 0.0f, H = 0.0f;
		Canvas->TextSize(Font, Text, W, H, Scale, Scale);
		FCanvasTextItem Item(FVector2D((Canvas->ClipX - W) * 0.5f, Y), FText::FromString(Text), Font, FLinearColor(1.0f, 0.82f, 0.25f, Alpha));
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f * Alpha), FVector2D(3.0f, 3.0f));
		Canvas->DrawItem(Item);
		Y += H + 8.0f;
	}
}
