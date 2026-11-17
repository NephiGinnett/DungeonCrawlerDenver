#include "Arcade/DCCMountainBird.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr int32 Rate = 22050;

	/** A raptor's scream: a falling, rasping whistle. Made here, like the rest of the arcade's effects. */
	const TArray<int16>& ScreamPcm()
	{
		static TArray<int16> Pcm;
		if (Pcm.IsEmpty())
		{
			FRandomStream R(41);
			const int32 N = int32(Rate * 0.9f);
			Pcm.SetNumUninitialized(N);
			float Phase = 0.0f;
			for (int32 i = 0; i < N; ++i)
			{
				const float T = float(i) / Rate;
				const float Freq = FMath::Lerp(2600.0f, 1500.0f, FMath::Pow(T / 0.9f, 0.7f)) + 140.0f * FMath::Sin(T * 2.0f * PI * 23.0f);
				Phase += 2.0f * PI * Freq / Rate;
				const float Env = FMath::Min(1.0f, T / 0.04f) * FMath::Exp(-T * 2.2f);
				const float Rasp = R.FRandRange(-1.0f, 1.0f) * 0.35f;
				const float V = (FMath::Sin(Phase) * 0.7f + FMath::Sin(Phase * 2.01f) * 0.2f + Rasp) * Env;
				Pcm[i] = int16(FMath::Clamp(V, -1.0f, 1.0f) * 26000.0f);
			}
		}
		return Pcm;
	}

	void Play(UWorld* World, const TArray<int16>& Pcm, const FVector& At)
	{
		static TStrongObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			Att.Reset(NewObject<USoundAttenuation>(GetTransientPackage()));
			Att->Attenuation.bAttenuate = true;
			Att->Attenuation.bSpatialize = true;
			Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Att->Attenuation.AttenuationShapeExtents = FVector(3000.0f, 0, 0);
			Att->Attenuation.FalloffDistance = 20000.0f; // heard from anywhere on the face
		}
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(Rate);
		W->NumChannels = 1;
		W->Duration = float(Pcm.Num()) / Rate;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		UGameplayStatics::PlaySoundAtLocation(World, W, At, 1.0f, 1.0f, 0.0f, Att.Get());
	}
}

ADCCMountainBird::ADCCMountainBird()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.0f);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Bird(TEXT("/Game/Legendary_Forge/Meshes/SM_bird.SM_bird"));
	if (Bird.Succeeded())
	{
		Body->SetStaticMesh(Bird.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	AimLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimLine"));
	AimLine->SetupAttachment(Root);
	AimLine->SetUsingAbsoluteLocation(true);
	AimLine->SetUsingAbsoluteRotation(true);
	AimLine->SetUsingAbsoluteScale(true);
	AimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimLine->SetCastShadow(false);
	AimLine->SetVisibility(false);
	if (Cylinder.Succeeded())
	{
		AimLine->SetStaticMesh(Cylinder.Object);
	}

	HeldRock = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldRock"));
	HeldRock->SetupAttachment(Root);
	HeldRock->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	HeldRock->SetRelativeScale3D(FVector(0.9f, 0.8f, 0.75f));
	HeldRock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldRock->SetVisibility(false);
	if (Sphere.Succeeded())
	{
		HeldRock->SetStaticMesh(Sphere.Object);
	}
}

void ADCCMountainBird::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCMountainBird, State);
	DOREPLIFETIME(ADCCMountainBird, DiveTarget);
}

void ADCCMountainBird::BeginPlay()
{
	Super::BeginPlay();
	// A 6 m wingspan, whatever size the seagull came in at: big enough to read from the bottom of the mountain.
	if (UStaticMesh* Mesh = Body->GetStaticMesh())
	{
		const FVector Size = Mesh->GetBoundingBox().GetSize();
		const float Span = FMath::Max3(float(Size.X), float(Size.Y), 1.0f);
		Body->SetRelativeScale3D(FVector(600.0f / Span));
	}
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Red = UMaterialInstanceDynamic::Create(Base, this);
		// Not red any more (2026-11-16): the tell is the bird's shadow on the rock, so it is near black.
		Red->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.01f, 0.012f));
		AimLine->SetMaterial(0, Red);
		UMaterialInstanceDynamic* Stone = UMaterialInstanceDynamic::Create(Base, this);
		Stone->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.32f, 0.28f, 0.25f));
		HeldRock->SetMaterial(0, Stone);
	}
	BodyRest = Body->GetRelativeRotation();
}

void ADCCMountainBird::SetState(uint8 NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnStateChanged(); // replication does not call OnRep on the server
	}
}

void ADCCMountainBird::OnRep_State()
{
	OnStateChanged();
}

void ADCCMountainBird::OnStateChanged()
{
	HeldRock->SetVisibility(State == Carry);
	StateStart = Age;
	if (State == WindUp && GetNetMode() != NM_DedicatedServer)
	{
		Play(GetWorld(), ScreamPcm(), GetActorLocation());
	}
}

void ADCCMountainBird::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// The "flap": a soaring bird rocks and bobs a little. Harder while hovering for a dive.
	const float Rock = State == WindUp ? 14.0f * FMath::Sin(Age * 11.0f) : 5.0f * FMath::Sin(Age * 2.3f);
	Body->SetRelativeRotation(BodyRest + FRotator(0.0f, 0.0f, Rock));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, (State == WindUp ? 35.0f : 18.0f) * FMath::Sin(Age * (State == WindUp ? 11.0f : 2.3f))));

	// The tell (2026-11-16, Zoe: "instead of the red line where they're coming how about a shadow passing by?"): the
	// bird's shadow on the rock. Through the wind-up it sweeps down the face toward where it will strike, growing as
	// the bird closes; through the dive it sits on the spot. Look up when a shadow crosses you.
	const bool bAim = State == WindUp || State == Dive;
	AimLine->SetVisibility(bAim);
	if (bAim)
	{
		const FVector Target = FVector(DiveTarget);
		FVector Out = (GetActorLocation() - Target).GetSafeNormal2D(); // away from the rock, toward the bird
		if (Out.IsNearlyZero()) { Out = FVector(1, 0, 0); }
		const FVector Side = FVector::CrossProduct(Out, FVector::UpVector);
		const float T = State == Dive ? 1.0f : FMath::Clamp((Age - StateStart) / 1.6f, 0.0f, 1.0f);
		const float Ease = FMath::SmoothStep(0.0f, 1.0f, T);
		// From high and to one side, down across the rock, onto the target. The disc lies flat on the face.
		const FVector Start = Target + FVector(0, 0, 650.0f) + Side * 420.0f;
		const FVector At = FMath::Lerp(Start, Target, Ease) - Out * 48.0f;
		const float Span = FMath::Lerp(180.0f, 360.0f, Ease) * (State == Dive ? 1.1f : 1.0f);
		AimLine->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Out).Rotator(), At, FVector(Span / 100.0f, Span * 0.55f / 100.0f, 0.02f)));
	}
}
