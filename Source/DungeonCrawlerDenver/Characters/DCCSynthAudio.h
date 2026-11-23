#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;
class USoundAttenuation;
class UWorld;

/**
 * Sounds made in code, played safely (2026-11-23).
 *
 * A USoundWaveProcedural never ends by itself: every one started with PlaySoundAtLocation keeps a voice open forever,
 * and frequent ones (footsteps, hits) pile up until they steal the room's song (the casino found this first). PlayPcm
 * spawns the sound and stops it when its samples run out.
 */
namespace DCCSynth
{
	/** Play 16-bit mono samples at Rate. At == null plays it 2D (to this machine's ears). Att may be null. */
	DUNGEONCRAWLERDENVER_API void PlayPcm(UWorld* World, const TArray<int16>& Pcm, int32 Rate, const FVector* At, float Volume, float Pitch,
		USoundAttenuation* Att = nullptr);

	/** A sphere falloff that lives for the whole game. */
	DUNGEONCRAWLERDENVER_API USoundAttenuation* Falloff(float Inner, float Distance);
}

/**
 * Footsteps for everyone, in every game (2026-11-23, Zoe: "a quiet walking sound"). A soft scuff every stride on the
 * ground, louder for big bodies, quieter for your own. Cosmetic, on every machine, within earshot of this one's player.
 */
namespace DCCFootsteps
{
	void Tick(ADCCCharacterBase* Character, float DeltaSeconds);
}
