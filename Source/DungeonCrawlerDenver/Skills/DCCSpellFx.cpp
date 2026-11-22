#include "Skills/DCCSpellFx.h"

#include "Camera/PlayerCameraManager.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Skills/DCCSkillTypes.h"

namespace
{
	enum class EStyle : uint8 { Beam, JitterBeam, Bolt, Stamp, Fan, Ring, Cage, Dome, Rain, Orbit, Pillar, Fountain, Swirl, Broadcast };

	struct FLook
	{
		const TCHAR* Id;
		EStyle Style;
		FLinearColor A;
		FLinearColor B;
		const TCHAR* Word;
		const TCHAR* Clip; // under /Game/DCC/Characters/Shared/AnimsFixed/
	};

	const FLinearColor Red(1.0f, 0.12f, 0.08f), Gold(1.0f, 0.75f, 0.15f), Ink(0.15f, 0.08f, 0.02f), Green(0.3f, 1.0f, 0.45f),
		Dust(0.85f, 0.55f, 0.25f), Snow(0.85f, 0.95f, 1.0f), Ice(0.45f, 0.75f, 1.0f), Violet(0.75f, 0.3f, 1.0f),
		Pink(1.0f, 0.3f, 0.8f), Static(0.65f, 0.6f, 0.85f), White(1.0f, 1.0f, 1.0f), Sky(0.35f, 0.65f, 1.0f),
		Fire(1.0f, 0.45f, 0.08f), Ember(1.0f, 0.15f, 0.02f), Cyan(0.3f, 1.0f, 0.95f), Water(0.25f, 0.55f, 1.0f), Yellow(1.0f, 0.95f, 0.2f);

	const TArray<FLook>& Looks()
	{
		static const TArray<FLook> List = {
			// The class spell book
			{ TEXT("FinalNotice"), EStyle::Stamp, Red, Ink, TEXT("PAST DUE"), TEXT("A_DCC_Overhead") },
			{ TEXT("HostileTakeover"), EStyle::Fan, Gold, Ink, TEXT("SOLD!"), TEXT("A_DCC_DW_ChargedSlash") },
			{ TEXT("TotalRepossession"), EStyle::Cage, Gold, Red, TEXT("REPOSSESSED"), TEXT("A_DCC_DW_AirSquat") },
			{ TEXT("SecondWind"), EStyle::Swirl, Green, White, TEXT("NOT DONE YET"), TEXT("A_DCC_OA_BoxingWarmup") },
			{ TEXT("HoldTheLine"), EStyle::Ring, Dust, Gold, TEXT("HOLD!"), TEXT("A_DCC_AO_AirSquat") },
			{ TEXT("AvalancheWarning"), EStyle::Rain, Snow, Ice, TEXT("LOOK UP"), TEXT("A_DCC_OA_Alert") },
			{ TEXT("Whiteout"), EStyle::Dome, Snow, Ice, TEXT("WHITEOUT"), TEXT("A_DCC_AO_SpinJump") },
			{ TEXT("FeedbackLoop"), EStyle::JitterBeam, Violet, Pink, TEXT("SKREEEE"), TEXT("A_DCC_DW_CallGesture") },
			{ TEXT("RadioSilence"), EStyle::Ring, Violet, Static, TEXT("..."), TEXT("A_DCC_HW_AgreeGesture") },
			{ TEXT("Interference"), EStyle::Fan, Static, Violet, TEXT("KSSHHH"), TEXT("A_DCC_DW_CallGesture") },
			{ TEXT("StationIdentification"), EStyle::Broadcast, Violet, Pink, TEXT("YOU'RE LISTENING TO DCC-FM"), TEXT("A_DCC_HW_BoomDance") },
			{ TEXT("Timeout"), EStyle::Pillar, White, Yellow, TEXT("TIMEOUT!"), TEXT("A_DCC_HW_AgreeGesture") },
			{ TEXT("PenaltyFlag"), EStyle::Bolt, Yellow, Gold, TEXT("FLAG ON THE PLAY"), TEXT("A_DCC_AO_ArcheryShot") },
			{ TEXT("TwoMinuteDrill"), EStyle::Pillar, Sky, White, TEXT("HUSTLE!"), TEXT("A_DCC_DW_CardioDance") },
			{ TEXT("HalftimeSpeech"), EStyle::Fountain, Gold, White, TEXT("GO TEAM"), TEXT("A_DCC_HW_BoomDance") },
			{ TEXT("HotLap"), EStyle::Fan, Fire, Ember, TEXT("VROOM"), TEXT("A_DCC_AO_ArcheryShot") },
			{ TEXT("PhotoFinish"), EStyle::Beam, White, Sky, TEXT("*CLICK*"), TEXT("A_DCC_AO_ArcheryAim") },
			{ TEXT("Slipstream"), EStyle::Fan, Cyan, White, TEXT("WHOOSH"), TEXT("A_DCC_AO_ArcheryShot") },
			{ TEXT("VictoryLap"), EStyle::Orbit, Fire, Gold, TEXT("VICTORY LAP"), TEXT("A_DCC_AO_SpinJump") },
			{ TEXT("Lockdown"), EStyle::Cage, Red, Ember, TEXT("LOCKDOWN"), TEXT("A_DCC_Overhead") },
			{ TEXT("CleanUpAisleFive"), EStyle::Fan, Water, White, TEXT("WET FLOOR"), TEXT("A_DCC_Slash") },
			{ TEXT("CustomerService"), EStyle::Pillar, Cyan, White, TEXT("HAVE A NICE DAY"), TEXT("A_DCC_HW_AgreeGesture") },
			{ TEXT("GrandReopening"), EStyle::Fountain, Pink, Gold, TEXT("GRAND REOPENING"), TEXT("A_DCC_AO_AllNightDance") },
			// The table's own spells, which had only the shared particles
			{ TEXT("StaticBurst"), EStyle::Ring, Sky, White, TEXT("ZAP"), TEXT("A_DCC_DW_CallGesture") },
			{ TEXT("BallLightning"), EStyle::Bolt, Sky, White, TEXT("KRAKOOM"), TEXT("A_DCC_DW_CallGesture") },
			{ TEXT("FireFingers"), EStyle::Fan, Fire, Yellow, TEXT("FWOOSH"), TEXT("A_DCC_DW_CallGesture") },
			{ TEXT("LienEnforcement"), EStyle::Stamp, Dust, Ink, TEXT("LIEN"), TEXT("A_DCC_Overhead") },
			{ TEXT("MarkAndCollect"), EStyle::Bolt, Gold, Red, TEXT("MARKED"), TEXT("A_DCC_AO_ArcheryShot") },
			{ TEXT("FirstAid"), EStyle::Pillar, Green, White, TEXT("+"), TEXT("A_DCC_HW_AgreeGesture") },
		};
		return List;
	}

	const FLook* FindLook(FName Id)
	{
		for (const FLook& Look : Looks())
		{
			if (Id == Look.Id) { return &Look; }
		}
		return nullptr;
	}

	FVector Chest(const AActor* Actor)
	{
		return Actor ? Actor->GetActorLocation() + FVector(0, 0, 40) : FVector::ZeroVector;
	}

	FVector Feet(const AActor* Actor)
	{
		return Actor ? Actor->GetActorLocation() - FVector(0, 0, 85) : FVector::ZeroVector;
	}
}

ADCCSpellFx::ADCCSpellFx()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

const TCHAR* ADCCSpellFx::CastClipFor(FName SkillId)
{
	const FLook* Look = FindLook(SkillId);
	return Look ? Look->Clip : nullptr;
}

bool ADCCSpellFx::Play(UWorld* World, ADCCCharacterBase* Caster, const FDCCSkillDef& Skill, const TArray<AActor*>& Hit)
{
	const FLook* Look = FindLook(Skill.SkillId);
	if (!World || !Caster || !Look)
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCSpellFx* Fx = World->SpawnActor<ADCCSpellFx>(ADCCSpellFx::StaticClass(), FTransform(Caster->GetActorLocation()), Params);
	if (!Fx)
	{
		return false;
	}
	const FVector From = Chest(Caster) + Caster->GetActorForwardVector() * 50.0f;
	const FVector Ground = Feet(Caster);
	const FVector Dir = Caster->GetActorForwardVector();
	const float Reach = Skill.Targeting == EDCCSkillTargeting::SingleTarget ? Skill.Range : Skill.Radius;
	TArray<FVector> Spots;
	for (const AActor* A : Hit) { if (A) { Spots.Add(Chest(A)); } }
	const FVector Aim = Spots.Num() > 0 ? Spots[0] : From + Dir * FMath::Min(Reach, 900.0f);
	const FLinearColor A = Look->A, B = Look->B;

	switch (Look->Style)
	{
	case EStyle::Beam:
	case EStyle::JitterBeam:
		Fx->Beam(From, Aim, A, B, 0.0f, Look->Style == EStyle::JitterBeam);
		Fx->AddFlash(From, A, 0.0f, 0.25f, 60000.0f, 900.0f); // the camera flash, for the photo finish
		break;
	case EStyle::Bolt:
		Fx->Bolt(From, Aim, A, B, 0.0f, Skill.SkillId == TEXT("BallLightning") ? 90.0f : 50.0f);
		break;
	case EStyle::Stamp:
		Fx->Stamp(Spots.Num() > 0 ? Spots[0] - FVector(0, 0, 125) : Ground + Dir * 400.0f, A, B, 0.05f);
		break;
	case EStyle::Fan:
		Fx->Fan(From, Dir, FMath::Max(Reach, 400.0f), Skill.ConeHalfAngleDegrees, A, B, Skill.ConeHalfAngleDegrees < 30.0f ? 6 : 9);
		break;
	case EStyle::Ring:
		Fx->Ring(Ground, Skill.Radius, A, B, 0.0f, Skill.SkillId == TEXT("RadioSilence") ? 3 : 2);
		break;
	case EStyle::Cage:
		Fx->Cage(Ground, Skill.Radius, A, B);
		break;
	case EStyle::Dome:
		Fx->Dome(Ground, Skill.Radius, A, B);
		Fx->Rain(Ground, Skill.Radius, B, A, 10, Spots);
		break;
	case EStyle::Rain:
		Fx->Rain(Ground, Skill.Radius, A, B, 12, Spots);
		Fx->Ring(Ground, Skill.Radius, B, A, 0.45f, 1);
		break;
	case EStyle::Orbit:
		Fx->Orbit(Ground + FVector(0, 0, 60), Skill.Radius * 0.7f, A, B, 6, 0.0f, 0.8f);
		Fx->Ring(Ground, Skill.Radius, A, B, 0.6f, 1);
		break;
	case EStyle::Pillar:
		if (Spots.Num() == 0) { Spots.Add(Chest(Caster)); }
		for (int32 i = 0; i < Spots.Num(); ++i)
		{
			Fx->Pillar(Spots[i] - FVector(0, 0, 125), A, B, 0.05f * i, 520.0f);
		}
		break;
	case EStyle::Swirl:
		Fx->Orbit(Ground, 90.0f, A, B, 8, 260.0f, 1.0f);
		Fx->Pillar(Ground, A, B, 0.2f, 300.0f);
		break;
	case EStyle::Fountain:
		Fx->Fountain(Ground, 26, 0.0f);
		for (int32 i = 0; i < Spots.Num(); ++i)
		{
			Fx->Pillar(Spots[i] - FVector(0, 0, 125), A, B, 0.1f + 0.04f * i, 420.0f);
		}
		if (Skill.Archetype == EDCCSkillArchetype::Damage)
		{
			Fx->Ring(Ground, Skill.Radius, A, B, 0.2f, 2);
		}
		break;
	case EStyle::Broadcast:
		Fx->Pillar(Ground, A, B, 0.0f, 900.0f);
		Fx->Ring(Ground, Skill.Radius, A, B, 0.15f, 3);
		Fx->Rain(Ground, Skill.Radius, B, A, 8, Spots);
		break;
	}
	if (Look->Word && Look->Word[0])
	{
		Fx->AddWord(Look->Word, Caster->GetActorLocation() + FVector(0, 0, 160), A, 0.05f, 1.2f, 34.0f);
	}
	return true;
}

// ---- the building blocks ---------------------------------------------------------------------------------------------

ADCCSpellFx::FPiece& ADCCSpellFx::AddPiece(const TCHAR* Shape, const FVector& P0, const FVector& P1, const FVector& S0, const FVector& S1,
	float T0, float T1, const FLinearColor& Colour, float Glow, const FRotator& Rot)
{
	FPiece& P = Pieces.AddDefaulted_GetRef();
	P.P0 = P0; P.P1 = P1; P.S0 = S0; P.S1 = S1; P.T0 = T0; P.T1 = T1; P.Colour = Colour; P.Glow = Glow; P.Rot = Rot;
	Life = FMath::Max(Life, T1);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
	// No static cache: a raw pointer the GC cannot see goes bad after a travel (see DCCAttackStyle::FindClip).
	// An opaque glow (the Megastructure pack's emissive master): the see-through hologram drew nothing on these shapes.
	UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Megastructure_Scifi_World/Materials/Master/M_emissive.M_emissive"));
	if (!Mesh)
	{
		return P;
	}
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetStaticMesh(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCastShadow(false);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetUsingAbsoluteLocation(true);
	Comp->SetUsingAbsoluteRotation(true);
	Comp->SetUsingAbsoluteScale(true);
	Comp->RegisterComponent();
	Comp->SetVisibility(false);
	if (Holo)
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Holo, Comp);
		Mid->SetVectorParameterValue(TEXT("Emissive Color"), Colour * (Glow * 0.4f));
		Comp->SetMaterial(0, Mid);
		P.Mid = Mid;
	}
	P.Mesh = Comp;
	return P;
}

void ADCCSpellFx::AddFlash(const FVector& At, const FLinearColor& Colour, float T0, float T1, float Peak, float Radius)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(GetRootComponent());
	Light->SetUsingAbsoluteLocation(true);
	Light->SetWorldLocation(At);
	Light->SetLightColor(Colour);
	Light->SetAttenuationRadius(Radius);
	Light->SetIntensity(0.0f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	Flashes.Add({ Light, T0, T1, Peak });
	Life = FMath::Max(Life, T1);
}

void ADCCSpellFx::AddWord(const FString& Text, const FVector& At, const FLinearColor& Colour, float T0, float T1, float Size)
{
	UTextRenderComponent* Comp = NewObject<UTextRenderComponent>(this);
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetUsingAbsoluteLocation(true);
	Comp->SetUsingAbsoluteRotation(true);
	Comp->SetText(FText::FromString(Text));
	Comp->SetTextRenderColor(Colour.ToFColor(true));
	Comp->SetWorldSize(Size);
	Comp->SetHorizontalAlignment(EHTA_Center);
	Comp->SetVerticalAlignment(EVRTA_TextCenter);
	Comp->SetWorldLocation(At);
	Comp->RegisterComponent();
	Comp->SetVisibility(false);
	Words.Add({ Comp, At, T0, T1 });
	Life = FMath::Max(Life, T1);
}

void ADCCSpellFx::Burst(const FVector& At, const FLinearColor& Colour, float Start, float Size)
{
	AddPiece(TEXT("Sphere"), At, At, FVector(Size * 0.004f), FVector(Size * 0.03f), Start, Start + 0.35f, Colour, 10.0f);
	AddFlash(At, Colour, Start, Start + 0.3f, 25000.0f, Size * 8.0f);
}

void ADCCSpellFx::Beam(const FVector& From, const FVector& To, const FLinearColor& A, const FLinearColor& B, float Start, bool bJitter)
{
	auto Segment = [this, Start](const FVector& P, const FVector& Q, const FLinearColor& C, float Thick)
	{
		const FVector D = Q - P;
		const FRotator R = FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator();
		const FVector Mid = (P + Q) * 0.5f;
		AddPiece(TEXT("Cylinder"), Mid, Mid, FVector(Thick, Thick, D.Size() / 100.0f), FVector(Thick * 0.2f, Thick * 0.2f, D.Size() / 100.0f),
			Start, Start + 0.4f, C, 12.0f, R);
	};
	if (bJitter)
	{
		// A shaking line in four kinks: feedback, not a laser.
		FVector Prev = From;
		for (int32 i = 1; i <= 4; ++i)
		{
			FVector Next = FMath::Lerp(From, To, i / 4.0f);
			if (i < 4) { Next += FVector(FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-40.0f, 40.0f)); }
			Segment(Prev, Next, i % 2 ? A : B, 0.18f);
			Prev = Next;
		}
	}
	else
	{
		Segment(From, To, A, 0.3f);
		Segment(From, To, B, 0.12f);
	}
	Burst(To, A, Start + 0.05f, 160.0f);
}

void ADCCSpellFx::Bolt(const FVector& From, const FVector& To, const FLinearColor& A, const FLinearColor& B, float Start, float Size)
{
	const float Fly = FMath::Clamp(FVector::Dist(From, To) / 2600.0f, 0.12f, 0.4f);
	for (int32 i = 0; i < 4; ++i)
	{
		const float Lag = i * 0.035f;
		const float S = Size * (1.0f - i * 0.2f) / 100.0f;
		AddPiece(TEXT("Sphere"), From, To, FVector(S), FVector(S), Start + Lag, Start + Lag + Fly, i == 0 ? A : B, 12.0f - i * 2.0f);
	}
	Burst(To, A, Start + Fly, Size * 3.0f);
}

void ADCCSpellFx::Stamp(const FVector& At, const FLinearColor& A, const FLinearColor& B, float Start)
{
	// A rubber stamp the size of a car, slammed down from above.
	AddPiece(TEXT("Cube"), At + FVector(0, 0, 700), At + FVector(0, 0, 30), FVector(2.4f, 2.4f, 0.4f), FVector(2.6f, 2.6f, 0.4f),
		Start, Start + 0.22f, A, 9.0f);
	AddPiece(TEXT("Cube"), At + FVector(0, 0, 30), At + FVector(0, 0, 25), FVector(2.6f, 2.6f, 0.4f), FVector(2.8f, 2.8f, 0.1f),
		Start + 0.22f, Start + 0.75f, A, 6.0f);
	AddPiece(TEXT("Cube"), At + FVector(0, 0, 760), At + FVector(0, 0, 90), FVector(0.5f, 0.5f, 1.2f), FVector(0.5f, 0.5f, 1.2f),
		Start, Start + 0.22f, B, 3.0f);
	Ring(At, 320.0f, A, B, Start + 0.22f, 1);
	AddFlash(At + FVector(0, 0, 60), A, Start + 0.22f, Start + 0.5f, 40000.0f, 900.0f);
}

void ADCCSpellFx::Fan(const FVector& From, const FVector& Dir, float Range, float HalfAngle, const FLinearColor& A, const FLinearColor& B, int32 Count)
{
	for (int32 i = 0; i < Count; ++i)
	{
		const float Alpha = Count > 1 ? i / float(Count - 1) : 0.5f;
		const float Yaw = FMath::Lerp(-HalfAngle, HalfAngle, Alpha);
		const FVector D = Dir.RotateAngleAxis(Yaw, FVector::UpVector);
		const FRotator R = D.Rotation();
		const float Delay = FMath::Abs(Alpha - 0.5f) * 0.12f;
		AddPiece(TEXT("Cube"), From + D * 40.0f, From + D * Range, FVector(0.3f, 0.06f, 0.06f), FVector(1.6f, 0.5f, 0.12f),
			Delay, Delay + 0.38f, i % 2 ? A : B, 10.0f, R);
	}
	AddFlash(From + Dir * Range * 0.5f, A, 0.0f, 0.35f, 30000.0f, Range);
}

void ADCCSpellFx::Ring(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B, float Start, int32 Pulses)
{
	for (int32 p = 0; p < Pulses; ++p)
	{
		const float T = Start + p * 0.18f;
		// A thin rim of light racing out along the floor (opaque, so never a filled disc), and orbs riding it.
		RimAt(At + FVector(0, 0, 6), 40.0f, Radius, T, T + 0.45f, p % 2 ? B : A);
		for (int32 k = 0; k < 14; ++k)
		{
			const float Ang = k * (2.0f * PI / 14.0f) + p * 0.2f;
			const FVector Out(FMath::Cos(Ang), FMath::Sin(Ang), 0.0f);
			AddPiece(TEXT("Sphere"), At + FVector(0, 0, 30), At + Out * Radius + FVector(0, 0, 30), FVector(0.25f), FVector(0.12f),
				T, T + 0.45f, p % 2 ? A : B, 10.0f);
		}
	}
	AddFlash(At + FVector(0, 0, 80), A, Start, Start + 0.4f + 0.18f * Pulses, 30000.0f, Radius * 1.5f);
}

void ADCCSpellFx::RimAt(const FVector& At, float From, float To, float T0, float T1, const FLinearColor& Colour)
{
	// 24 short bars round a circle, each pushed from radius From to radius To, turned along the circle.
	constexpr int32 Bars = 24;
	for (int32 k = 0; k < Bars; ++k)
	{
		const float Ang = k * (2.0f * PI / Bars);
		const FVector Out(FMath::Cos(Ang), FMath::Sin(Ang), 0.0f);
		const FRotator Along = FVector(-Out.Y, Out.X, 0.0f).Rotation();
		const float Len0 = 2.0f * PI * FMath::Max(From, 10.0f) / Bars / 100.0f;
		const float Len1 = 2.0f * PI * FMath::Max(To, 10.0f) / Bars / 100.0f;
		AddPiece(TEXT("Cube"), At + Out * From, At + Out * To, FVector(Len0, 0.06f, 0.06f), FVector(Len1, 0.1f, 0.05f), T0, T1, Colour, 9.0f, Along);
	}
}

void ADCCSpellFx::Cage(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B)
{
	// Bars come up out of the floor round the whole area and stand for a second. Nobody leaves.
	for (int32 k = 0; k < 12; ++k)
	{
		const float Ang = k * (2.0f * PI / 12.0f);
		const FVector P = At + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.0f) * Radius;
		AddPiece(TEXT("Cylinder"), P - FVector(0, 0, 150), P + FVector(0, 0, 150), FVector(0.25f, 0.25f, 3.0f), FVector(0.25f, 0.25f, 3.0f),
			0.05f + k * 0.02f, 1.2f, k % 2 ? A : B, 8.0f);
	}
	RimAt(At + FVector(0, 0, 300), Radius, Radius, 0.3f, 1.2f, A);
	Ring(At, Radius, A, B, 0.0f, 1);
	AddFlash(At + FVector(0, 0, 200), A, 0.1f, 1.1f, 25000.0f, Radius * 1.8f);
}

void ADCCSpellFx::Dome(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B)
{
	// A dome of light drawn as rings stacked into a hemisphere, growing out from the caster.
	for (int32 Band = 0; Band < 4; ++Band)
	{
		const float Lat = Band * (PI * 0.5f / 4.0f);
		RimAt(At + FVector(0, 0, FMath::Sin(Lat) * Radius * 0.7f), 30.0f, FMath::Cos(Lat) * Radius, Band * 0.05f, 0.75f, Band % 2 ? B : A);
	}
	AddFlash(At + FVector(0, 0, 100), A, 0.0f, 0.8f, 40000.0f, Radius * 2.0f);
}

void ADCCSpellFx::Rain(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B, int32 Count, const TArray<FVector>& Spots)
{
	for (int32 i = 0; i < Count; ++i)
	{
		FVector Land = i < Spots.Num() ? Spots[i] - FVector(0, 0, 110)
			: At + FVector(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius), 0.0f);
		const float T = 0.1f + i * 0.05f;
		const float S = FMath::FRandRange(0.5f, 0.9f);
		AddPiece(TEXT("Sphere"), Land + FVector(FMath::FRandRange(-80.0f, 80.0f), FMath::FRandRange(-80.0f, 80.0f), 1000.0f), Land + FVector(0, 0, 30),
			FVector(S), FVector(S * 0.8f), T, T + 0.32f, i % 2 ? A : B, 8.0f);
		RimAt(Land + FVector(0, 0, 6), 10.0f, 110.0f, T + 0.32f, T + 0.65f, A);
	}
}

void ADCCSpellFx::Orbit(const FVector& At, float Radius, const FLinearColor& A, const FLinearColor& B, int32 Count, float Rise, float Seconds)
{
	for (int32 i = 0; i < Count; ++i)
	{
		FPiece& P = AddPiece(TEXT("Sphere"), At, At, FVector(0.45f), FVector(0.2f), 0.0f, Seconds, i % 2 ? A : B, 10.0f);
		P.bOrbit = true;
		P.Radius = Radius;
		P.A0 = i * (2.0f * PI / Count);
		P.Spin = 2.0f * PI * 1.6f / Seconds;
		P.RiseZ = Rise;
	}
	AddFlash(At, A, 0.0f, Seconds, 20000.0f, Radius * 2.5f);
}

void ADCCSpellFx::Pillar(const FVector& At, const FLinearColor& A, const FLinearColor& B, float Start, float Height)
{
	// A slim column (it must not hide who it is healing) and a ring at its foot.
	AddPiece(TEXT("Cylinder"), At + FVector(0, 0, Height * 0.5f), At + FVector(0, 0, Height * 0.5f), FVector(0.5f, 0.5f, 0.1f),
		FVector(0.12f, 0.12f, Height / 100.0f), Start, Start + 0.8f, A, 6.0f);
	RimAt(At + FVector(0, 0, 6), 30.0f, 110.0f, Start, Start + 0.6f, B);
	for (int32 k = 0; k < 6; ++k)
	{
		const FVector Off(FMath::FRandRange(-70.0f, 70.0f), FMath::FRandRange(-70.0f, 70.0f), 0.0f);
		AddPiece(TEXT("Sphere"), At + Off, At + Off + FVector(0, 0, Height), FVector(0.15f), FVector(0.05f),
			Start + k * 0.06f, Start + k * 0.06f + 0.7f, B, 10.0f);
	}
	AddFlash(At + FVector(0, 0, 120), A, Start, Start + 0.8f, 20000.0f, 600.0f);
}

void ADCCSpellFx::Fountain(const FVector& At, int32 Count, float Start)
{
	static const FLinearColor Confetti[] = { FLinearColor(1, 0.2f, 0.4f), FLinearColor(1, 0.85f, 0.2f), FLinearColor(0.3f, 0.9f, 1),
		FLinearColor(0.5f, 1, 0.4f), FLinearColor(0.8f, 0.4f, 1) };
	for (int32 i = 0; i < Count; ++i)
	{
		const float Ang = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Out = FMath::FRandRange(150.0f, 520.0f);
		const FVector End = At + FVector(FMath::Cos(Ang) * Out, FMath::Sin(Ang) * Out, FMath::FRandRange(380.0f, 760.0f));
		const float T = Start + FMath::FRandRange(0.0f, 0.25f);
		AddPiece(TEXT("Cube"), At + FVector(0, 0, 60), End, FVector(0.14f, 0.14f, 0.03f), FVector(0.14f, 0.14f, 0.03f),
			T, T + 0.9f, Confetti[i % 5], 10.0f, FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0));
	}
	AddFlash(At + FVector(0, 0, 200), Confetti[1], Start, Start + 0.9f, 30000.0f, 1200.0f);
}

void ADCCSpellFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age > Life + 0.05f)
	{
		Destroy();
		return;
	}
	for (FPiece& P : Pieces)
	{
		if (!P.Mesh)
		{
			continue;
		}
		const bool bLive = Age >= P.T0 && Age <= P.T1;
		P.Mesh->SetVisibility(bLive);
		if (!bLive)
		{
			continue;
		}
		const float Raw = (Age - P.T0) / FMath::Max(0.01f, P.T1 - P.T0);
		const float Ease = 1.0f - FMath::Square(1.0f - Raw); // fast out, settling in
		FVector At = FMath::Lerp(P.P0, P.P1, Ease);
		if (P.bOrbit)
		{
			const float Ang = P.A0 + P.Spin * (Age - P.T0);
			At = P.P0 + FVector(FMath::Cos(Ang) * P.Radius, FMath::Sin(Ang) * P.Radius, P.RiseZ * Raw);
		}
		P.Mesh->SetWorldLocationAndRotation(At, P.Rot);
		P.Mesh->SetWorldScale3D(FMath::Lerp(P.S0, P.S1, Ease));
		if (P.Mid)
		{
			// Bright, then fading through the last 40%.
			const float Fade = Raw < 0.6f ? 1.0f : FMath::Clamp((1.0f - Raw) / 0.4f, 0.0f, 1.0f);
			P.Mid->SetVectorParameterValue(TEXT("Emissive Color"), P.Colour * (P.Glow * 0.4f * Fade));
		}
	}
	for (FFlash& F : Flashes)
	{
		if (!F.Light) { continue; }
		const float Raw = (Age - F.T0) / FMath::Max(0.01f, F.T1 - F.T0);
		F.Light->SetIntensity(Raw < 0.0f || Raw > 1.0f ? 0.0f : F.Peak * (1.0f - Raw));
	}
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	for (FWord& W : Words)
	{
		if (!W.Text) { continue; }
		const bool bLive = Age >= W.T0 && Age <= W.T1;
		W.Text->SetVisibility(bLive);
		if (!bLive) { continue; }
		const float Raw = (Age - W.T0) / FMath::Max(0.01f, W.T1 - W.T0);
		// Pops in big, settles, floats up a little.
		const float Pop = Raw < 0.12f ? FMath::Lerp(1.6f, 1.0f, Raw / 0.12f) : 1.0f;
		const FVector At = W.P0 + FVector(0, 0, 60.0f * Raw);
		W.Text->SetWorldLocation(At);
		W.Text->SetWorldScale3D(FVector(Pop));
		if (!Eye.IsZero())
		{
			W.Text->SetWorldRotation((Eye - At).GetSafeNormal2D().Rotation());
		}
	}
}
