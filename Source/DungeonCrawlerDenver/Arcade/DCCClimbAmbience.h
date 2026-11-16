#pragma once

#include "CoreMinimal.h"

class ADCCArcadeClimb;

/**
 * Fourteener's sound and feel (2026-11-13), run from ADCCArcadeClimb::ClientTick on every machine.
 *
 * Zoe asked for: falls that drop instead of glide; a real soundscape (birds, wind, fire, the song now
 * and then); and hazards you HEAR instead of read - a rockfall cracks loose, rumbles down and lands
 * where it is in the world (faint if far, loud and shaking the screen if close), a gust whooshes past.
 * The text warnings are gone (ADCCArcadeClimb::Warn).
 *
 * No pack ships birds, wind or rockfall sounds, so those are synthesized at runtime (the same
 * USoundWaveProcedural trick as DCCFireworks); the fire is StarterContent's Fire01_Cue and the song
 * is /Game/Songs/Swept_Away. Kept in its own file, apart from DCCArcadeGames.cpp, so it can change
 * without touching the other games.
 */
class FDCCClimbAmbience
{
public:
	static void Tick(ADCCArcadeClimb* Arena, float DeltaSeconds);
};
