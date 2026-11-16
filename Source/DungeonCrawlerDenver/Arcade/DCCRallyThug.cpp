#include "Arcade/DCCRallyThug.h"

#include "Animation/AnimationAsset.h"
#include "Components/AudioComponent.h"
#include "Dialogue/DCCVoiceLines.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
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
	const TCHAR* const Anims = TEXT("/Game/Characters/Mannequins/Anims/");

	UAnimationAsset* Clip(const TCHAR* Path)
	{
		const FString Full = FString(Anims) + Path;
		return LoadObject<UAnimationAsset>(nullptr, *(Full + TEXT(".") + FPaths::GetBaseFilename(Full)));
	}

	// ---- sound: made here, like the rest of Repo Rally's effects (DCCRallyAmbience) ----------------------
	constexpr int32 Rate = 22050;

	void Bang(TArray<int16>& Out, bool bShot)
	{
		// A shot: a sharp crack of noise with a low body under it. A punch: a dull thump, mostly low.
		FRandomStream R(bShot ? 91 : 17);
		const int32 N = int32(Rate * (bShot ? 0.32f : 0.16f));
		Out.SetNumUninitialized(N);
		float Lp = 0.0f;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = float(i) / Rate;
			const float Noise = R.FRandRange(-1.0f, 1.0f);
			Lp += (Noise - Lp) * (bShot ? 0.35f : 0.06f);
			const float Body = FMath::Sin(2.0f * PI * (bShot ? 95.0f : 70.0f) * T) * FMath::Exp(-T * (bShot ? 18.0f : 30.0f));
			const float Crack = (bShot ? Noise : Lp) * FMath::Exp(-T * (bShot ? 26.0f : 40.0f));
			const float V = FMath::Clamp(Crack * 0.8f + Body * 0.6f, -1.0f, 1.0f);
			Out[i] = int16(V * 30000.0f);
		}
	}

	void PlayPcm(UWorld* World, const TArray<int16>& Pcm, const FVector& At, float Volume)
	{
		static TStrongObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			Att.Reset(NewObject<USoundAttenuation>(GetTransientPackage()));
			Att->Attenuation.bAttenuate = true;
			Att->Attenuation.bSpatialize = true;
			Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Att->Attenuation.AttenuationShapeExtents = FVector(1200.0f, 0, 0);
			Att->Attenuation.FalloffDistance = 9000.0f;
		}
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(World);
		W->SetSampleRate(Rate);
		W->NumChannels = 1;
		W->Duration = float(Pcm.Num()) / Rate;
		W->SoundGroup = SOUNDGROUP_Effects;
		W->bLooping = false;
		W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		UGameplayStatics::PlaySoundAtLocation(World, W, At, Volume, 1.0f, 0.0f, Att.Get());
	}

	/** A glowing stick from A to B that lives a moment. Local only. */
	void Streak(UWorld* World, const FVector& A, const FVector& B, const FLinearColor& Colour, float Thick, float Life)
	{
		static TStrongObjectPtr<UStaticMesh> Cylinder;
		static TStrongObjectPtr<UMaterialInterface> Glow;
		if (!Cylinder.IsValid())
		{
			Cylinder.Reset(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			Glow.Reset(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_light.MI_light")));
		}
		const FVector D = B - A;
		const float Len = D.Size();
		if (!Cylinder.IsValid() || Len < 1.0f)
		{
			return;
		}
		// The engine cylinder is 100 units tall along Z: point Z down the shot.
		const FRotator Rot = FRotationMatrix::MakeFromZ(D / Len).Rotator();
		const FTransform Xf(Rot, (A + B) * 0.5f, FVector(Thick / 100.0f, Thick / 100.0f, Len / 100.0f));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Xf, Params);
		if (!Actor)
		{
			return;
		}
		Actor->SetReplicates(false);
		UStaticMeshComponent* C = Actor->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Cylinder.Get());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		if (Glow.IsValid())
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Glow.Get(), Actor);
			M->SetVectorParameterValue(TEXT("Color"), Colour);
			M->SetVectorParameterValue(TEXT("Emissive Color"), Colour);
			C->SetMaterial(0, M);
		}
		Actor->SetActorTransform(Xf);
		Actor->SetLifeSpan(Life);
	}

	struct FOwnerLine
	{
		FString Speaker;
		FString Row;
		FString Text;
	};

	/** Used only when the Voice Studio file is missing. Keep these in step with gen_impound_lines.py. */
	const FOwnerLine FallbackLines[] = {
		{ TEXT("Pryce"), TEXT("RR_Pryce_01"), TEXT("Hey! That is my car, you son of a bitch! I am paid through Friday!") },
		{ TEXT("Pryce"), TEXT("RR_Pryce_02"), TEXT("Put the hook down! That is how I get to work, and I have a shift at six!") },
		{ TEXT("Nia"), TEXT("RR_Nia_01"), TEXT("Get off my car! My kid's seat is in the back, you asshole!") },
		{ TEXT("Nia"), TEXT("RR_Nia_02"), TEXT("I am not a skip! The notice went to the old address! You don't get to take it!") },
		{ TEXT("Holt"), TEXT("RR_Holt_01"), TEXT("I paid that note! The receipt is in the glove box you're dragging down the street!") },
		{ TEXT("Holt"), TEXT("RR_Holt_02"), TEXT("Mile High can kiss my ass! Touch that bumper and I am not moving!") },
	};

	bool IsOwnerSpeaker(const FString& Id)
	{
		return Id == TEXT("Pryce") || Id == TEXT("Nia") || Id == TEXT("Holt");
	}

	void LoadOwnerLines(TArray<FOwnerLine>& Out)
	{
		const FString Candidates[] = {
			FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Voice/NPC/impound_lines.json")),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Voice/NPC/impound_lines.json")),
		};
		for (const FString& Path : Candidates)
		{
			FString Json;
			TSharedPtr<FJsonObject> Root;
			if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
			{
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("lines")))
			{
				const TSharedPtr<FJsonObject> O = Value->AsObject();
				if (!O.IsValid())
				{
					continue;
				}
				const FString Speaker = O->GetStringField(TEXT("speaker"));
				if (!IsOwnerSpeaker(Speaker))
				{
					continue;
				}
				const FString Text = O->GetStringField(TEXT("text"));
				const FString Row = O->GetStringField(TEXT("row"));
				if (!Text.IsEmpty() && !Row.IsEmpty())
				{
					Out.Add({ Speaker, Row, Text });
				}
			}
			if (!Out.IsEmpty())
			{
				return;
			}
		}
		for (const FOwnerLine& Line : FallbackLines)
		{
			Out.Add(Line);
		}
	}

	void Flash(UWorld* World, const FVector& At, const FLinearColor& Colour, float Intensity, float Radius, float Life)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APointLight* L = World->SpawnActor<APointLight>(At, FRotator::ZeroRotator, Params))
		{
			L->SetReplicates(false);
			L->SetMobility(EComponentMobility::Movable);
			L->PointLightComponent->SetLightColor(Colour);
			L->PointLightComponent->SetIntensity(Intensity);
			L->PointLightComponent->SetAttenuationRadius(Radius);
			L->PointLightComponent->SetCastShadows(false);
			L->SetLifeSpan(Life);
		}
	}
}

ADCCRallyThug::ADCCRallyThug()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (Manny.Succeeded())
	{
		Body->SetSkeletalMeshAsset(Manny.Object);
	}
	// The mannequin's root is at its feet and it faces +Y: turn it to face the actor's forward.
	Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	Gun = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gun"));
	Gun->SetupAttachment(Body, TEXT("hand_rSocket"));
	Gun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Gun->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/DCC/Weapons/BoilerplateRifle/SM_BoilerplateRifle.SM_BoilerplateRifle"));
	if (Rifle.Succeeded())
	{
		Gun->SetStaticMesh(Rifle.Object);
	}
	Gun->SetVisibility(false);
}

void ADCCRallyThug::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCRallyThug, Kind);
	DOREPLIFETIME(ADCCRallyThug, Plate);
	DOREPLIFETIME(ADCCRallyThug, Mood);
}

void ADCCRallyThug::BeginPlay()
{
	Super::BeginPlay();
	if (Kind == Shooter && Gun && Gun->GetStaticMesh())
	{
		// Fit the rifle to about 90 cm, whatever size the Meshy import came in at.
		const FVector Size = Gun->GetStaticMesh()->GetBoundingBox().GetSize();
		const float Longest = FMath::Max3(Size.X, Size.Y, Size.Z);
		if (Longest > 1.0f)
		{
			Gun->SetRelativeScale3D(FVector(90.0f / Longest));
		}
		Gun->SetVisibility(true);
	}
	// A real person from NPC Forge (Tools/npc_forge.py, 2026-11-14), picked from the plate so the same car always
	// has the same owner. Falls back to the mannequins if the NPCs are not imported.
	{
		static const TCHAR* Forge[] = {
			TEXT("casual_300"), TEXT("worker_301"), TEXT("office_302"), TEXT("casual_303"), TEXT("worker_304"), TEXT("office_305"),
			TEXT("casual_306"), TEXT("worker_307"), TEXT("office_308"), TEXT("casual_309"), TEXT("worker_310"), TEXT("office_311"),
		};
		const int32 Pick = int32(GetTypeHash(Plate) % UE_ARRAY_COUNT(Forge));
		const FString Path = FString::Printf(TEXT("/Game/DCC/Characters/NPCs/NPC_%s/SK_NPC_%s.SK_NPC_%s"), Forge[Pick], Forge[Pick], Forge[Pick]);
		if (USkeletalMesh* Person = LoadObject<USkeletalMesh>(nullptr, *Path))
		{
			Body->SetSkeletalMeshAsset(Person);
			bForgeBody = true;
			if (Gun)
			{
				Gun->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
			}
		}
	}
	if (!bForgeBody && Kind != Shooter)
	{
		// Owners who fight are the Quinn body, so the two kinds read apart at a glance.
		if (USkeletalMesh* Quinn = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")))
		{
			Body->SetSkeletalMeshAsset(Quinn);
		}
	}
	ApplyMood();
	TryShout();
}

void ADCCRallyThug::YellFor(const FString& Plate, FString& OutSpeaker, FString& OutRow, FString& OutText)
{
	TArray<FOwnerLine> Lines;
	LoadOwnerLines(Lines);
	const FString Key = Plate.IsEmpty() ? FString(TEXT("owner")) : Plate;
	const FOwnerLine& Line = Lines[int32(GetTypeHash(Key) % uint32(Lines.Num()))];
	OutSpeaker = Line.Speaker;
	OutRow = Line.Row;
	OutText = Line.Text;
}

void ADCCRallyThug::TryShout()
{
	if (bShouted || Plate.IsEmpty() || !GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bShouted = true;
	FString Speaker, Row;
	YellFor(Plate, Speaker, Row, ShoutText);
	// Long enough to read a shouted sentence, then the tag is enough.
	ShoutUntil = GetWorld()->GetTimeSeconds() + 8.0;
	const FString Wav = DCCVoice::FindLine(FName(*Speaker), FName(*Row));
	if (!Wav.IsEmpty())
	{
		DCCVoice::PlayAt(this, Wav, GetActorLocation() + FVector(0, 0, 160), 1.5f);
	}
}

void ADCCRallyThug::SetMood(uint8 NewMood)
{
	if (Mood != NewMood)
	{
		Mood = NewMood;
		ApplyMood();
	}
}

void ADCCRallyThug::OnRep_Mood()
{
	ApplyMood();
}

void ADCCRallyThug::ApplyMood()
{
	if (!Body || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const bool bGun = Kind == Shooter;
	UAnimationAsset* Anim = nullptr;
	if (bForgeBody)
	{
		// The same clips, retargeted onto the NPC Forge skeleton by DCC.NpcRetarget.
		const TCHAR* Name = Mood == Run ? (bGun ? TEXT("NPC_MF_Rifle_Jog_Fwd") : TEXT("NPC_MF_Unarmed_Jog_Fwd"))
			: Mood == Attack ? (bGun ? TEXT("NPC_MF_Rifle_Idle_ADS") : TEXT("NPC_MM_Attack_01"))
			: Mood == Down ? TEXT("NPC_MM_Death_Back_01") : TEXT("NPC_MM_Idle");
		const FString Path = FString::Printf(TEXT("/Game/DCC/Characters/NPCs/Anims/%s.%s"), Name, Name);
		if (UAnimationAsset* Clip = LoadObject<UAnimationAsset>(nullptr, *Path))
		{
			Body->PlayAnimation(Clip, Mood != Down);
			return;
		}
	}
	bool bLoop = true;
	switch (Mood)
	{
	case Run:
		Anim = Clip(bGun ? TEXT("Rifle/Jog/MF_Rifle_Jog_Fwd") : TEXT("Unarmed/Jog/MF_Unarmed_Jog_Fwd"));
		break;
	case Attack:
		Anim = Clip(bGun ? TEXT("Rifle/MF_Rifle_Idle_ADS") : TEXT("Unarmed/Attack/MM_Attack_01"));
		break;
	case Down:
		Anim = Clip(TEXT("Death/MM_Death_Back_01"));
		bLoop = false;
		break;
	default:
		Anim = Clip(TEXT("Unarmed/MM_Idle"));
		break;
	}
	if (Anim)
	{
		Body->PlayAnimation(Anim, bLoop);
	}
}

FVector ADCCRallyThug::MuzzleLocation() const
{
	if (Gun && Gun->IsVisible())
	{
		return Gun->GetComponentLocation() + GetActorForwardVector() * 50.0f;
	}
	return GetActorLocation() + FVector(0, 0, 130) + GetActorForwardVector() * 60.0f;
}

void ADCCRallyThug::PlayHit(UWorld* World, const FVector& From, const FVector& To, uint8 HitKind)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	static TArray<int16> ShotPcm, PunchPcm;
	if (ShotPcm.IsEmpty())
	{
		Bang(ShotPcm, true);
		Bang(PunchPcm, false);
	}
	if (HitKind == 0)
	{
		Flash(World, To, FLinearColor(1.0f, 0.55f, 0.2f), 6000.0f, 500.0f, 0.08f);
		PlayPcm(World, PunchPcm, To, 1.0f);
		return;
	}
	const FLinearColor Tracer(1.0f, 0.75f, 0.3f);
	Streak(World, From, To, Tracer * 6.0f, 6.0f, 0.09f);
	Flash(World, From, FLinearColor(1.0f, 0.7f, 0.3f), 30000.0f, 900.0f, 0.06f);
	if (HitKind == 1)
	{
		Flash(World, To, FLinearColor(1.0f, 0.4f, 0.15f), 12000.0f, 600.0f, 0.1f);
	}
	PlayPcm(World, ShotPcm, From, 0.9f);
}
