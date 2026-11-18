#include "Characters/DCCCreatureLook.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace DCCCreatureLook
{
	void Tint(ADCCCharacterBase* Character, const FLinearColor& Colour, const FLinearColor& Edge, float Rim)
	{
		USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
		UMaterialInterface* Shade = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DCC/Materials/M_CharacterShade.M_CharacterShade"));
		if (!Body || !Shade)
		{
			return;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Shade, Character);
		Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Colour.R, Colour.G, Colour.B, 1.0f));
		Mid->SetVectorParameterValue(TEXT("Edge"), Edge);
		Mid->SetScalarParameterValue(TEXT("RimStrength"), Rim);
		Body->SetOverlayMaterial(Mid);
	}

	void Hologram(ADCCCharacterBase* Character, const FLinearColor& Colour, float Intensity, float Tiling)
	{
		USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
		UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cyberpunk_City/Materials/Master/M_hologram.M_hologram"));
		if (!Body || !Holo)
		{
			return;
		}
		UMaterialInstanceDynamic* Spirit = UMaterialInstanceDynamic::Create(Holo, Character);
		Spirit->SetVectorParameterValue(TEXT("Emissive Color"), Colour);
		Spirit->SetScalarParameterValue(TEXT("Emissive Intensity"), Intensity);
		Spirit->SetScalarParameterValue(TEXT("Tiling Hologram"), Tiling);
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			Body->SetMaterial(Slot, Spirit);
		}
		// Something must sit in the overlay slot, or the arena's own shade would be put on top of the glow.
		Tint(Character, FLinearColor::White, FLinearColor::White, 0.0f);
	}

	void Shimmer(ADCCCharacterBase* Character, const FLinearColor& Colour, float Intensity)
	{
		USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
		UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cyberpunk_City/Materials/Master/M_hologram.M_hologram"));
		if (!Body || !Holo)
		{
			return;
		}
		UMaterialInstanceDynamic* Sheen = UMaterialInstanceDynamic::Create(Holo, Character);
		Sheen->SetVectorParameterValue(TEXT("Emissive Color"), Colour);
		Sheen->SetScalarParameterValue(TEXT("Emissive Intensity"), Intensity);
		Sheen->SetScalarParameterValue(TEXT("Tiling Hologram"), 18.0f);
		Body->SetOverlayMaterial(Sheen);
	}

	UPointLightComponent* Aura(ADCCCharacterBase* Character, const FLinearColor& Colour, float Intensity, float Radius, float Height)
	{
		if (!Character || !Character->GetRootComponent())
		{
			return nullptr;
		}
		UPointLightComponent* Glow = NewObject<UPointLightComponent>(Character);
		Glow->SetupAttachment(Character->GetRootComponent());
		Glow->SetRelativeLocation(FVector(0, 0, Height));
		Glow->SetLightColor(Colour);
		Glow->SetIntensity(Intensity);
		Glow->SetAttenuationRadius(Radius);
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
		return Glow;
	}
}
