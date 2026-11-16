#pragma once

#include "CoreMinimal.h"

class ADCCArcadeKart;

/**
 * Repo Rally's sound and street feel (2026-11-13), run from ADCCArcadeKart::ClientTick on every machine.
 *
 *  - Boost pads: the arena lists them (BoostPads) when it builds the city; driving over one grants a boost,
 *    on the driver's machine (felt at once) and the server (owns it), like Neon Mile's.
 *  - The city at night: a low traffic rumble and a high synth hum underneath everything, traffic whooshing
 *    past (pitched by speed), distant horns, a siren now and then.
 * The songs themselves are not in this bed. They play on the cab radio (KMHR 5280,
 * ADCCRover::TuneIn), Repo Rally first and the other tracks after it.
 * Synthesized at runtime like FDCCClimbAmbience; no pack ships city audio.
 */
class FDCCRallyAmbience
{
public:
	static void Tick(ADCCArcadeKart* Arena, float DeltaSeconds);
};
