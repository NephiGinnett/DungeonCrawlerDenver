#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;
class UPointLightComponent;

/**
 * How the arcade's creatures are made to look like what they are (2026-11-16): a tint over the body, a see-through
 * hologram body, and a light that rides along. Shared by Gate Run's cast and Dead Air's creatures, which each used to
 * carry their own copy. Call from BeginPlay (they make components and material instances).
 */
namespace DCCCreatureLook
{
	/** The character-shade overlay (it multiplies what is behind it), pushed toward a colour, with a coloured rim. */
	void Tint(ADCCCharacterBase* Character, const FLinearColor& Colour, const FLinearColor& Edge, float Rim);

	/** See-through: the ghosts' hologram material in a colour, on every slot of the body. */
	void Hologram(ADCCCharacterBase* Character, const FLinearColor& Colour, float Intensity, float Tiling = 12.0f);

	/** A glowing shimmer over the regular body (the hologram material as an overlay, so the model shows through). */
	void Shimmer(ADCCCharacterBase* Character, const FLinearColor& Colour, float Intensity);
	/** A light that rides along on a character, at Height above its root. */
	UPointLightComponent* Aura(ADCCCharacterBase* Character, const FLinearColor& Colour, float Intensity, float Radius, float Height);
}
