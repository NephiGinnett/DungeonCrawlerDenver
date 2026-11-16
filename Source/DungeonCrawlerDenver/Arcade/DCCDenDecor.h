#pragma once

#include "CoreMinimal.h"

class ADCCArcadeLobby;

/**
 * The Den's vote pads, dressed as their games (2026-11-13: Zoe wanted players to "get the vibe before
 * they vote"). Each pad gets a small scene of real pack models behind it, on and beside the sign block,
 * so nothing gets in the way of standing on the pad:
 *
 *   Dungeon Crawler Denver  a treasure chest, a weapon rack, a burning brazier, a statue
 *   Dead Air                a static-green TV (the livestream), a statue, a chain, a ghost light
 *   Fourteener              a rock outcrop with ice, a pine, a tent, a coil of rope
 *   Repo Rally              a car, an amber beacon, a tow chain, traffic cones
 *   Neon Mile               a neon sign, a palm, a neon street lamp, glowing floor strips
 *   Stress Test             a bullseye on the pad and a hot light over the sign
 *
 * Missing packs cost decoration, never a crash (ADCCArcadeArena::Prop). Kept out of DCCArcadeGames.cpp
 * so the games' code is untouched.
 */
class FDCCDenDecor
{
public:
	static void Dress(ADCCArcadeLobby* Den, FName GameId, const FVector& Pad, const FLinearColor& Colour);
};
