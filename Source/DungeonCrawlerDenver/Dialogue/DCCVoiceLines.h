#pragma once

#include "CoreMinimal.h"

class UAudioComponent;

/**
 * NPC voice lines straight from the Voice Studio's exports (2026-11-13).
 *
 * The studio writes 16-bit PCM WAVs to SourceArt/Voice/NPC/<Speaker>/VO_<Row>.wav.
 * The game reads and plays them at runtime, so a friend's new recording is heard
 * the next time the line is said: no editor import, no rebuild. A line with no
 * file is simply silent (its text still shows).
 *
 * Packaged builds: SourceArt is not shipped. Copy SourceArt/Voice/NPC to
 * <Game>/Voice/NPC next to the packaged game, which is the second place looked in.
 */
namespace DCCVoice
{
	/** The recording for this line, if one exists on disk. Empty if none. */
	FString FindLine(FName Speaker, FName Row);

	/** Plays a WAV at a place in the world, heard within about 25 m. Returns the playing component (or null). */
	UAudioComponent* PlayAt(const UObject* WorldContext, const FString& WavPath, const FVector& Location, float Volume = 1.0f);
}
