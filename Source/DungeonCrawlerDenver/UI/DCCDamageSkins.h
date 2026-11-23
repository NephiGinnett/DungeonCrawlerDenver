#pragma once

#include "CoreMinimal.h"

/**
 * Damage numbers you can tell apart (2026-11-23, Zoe: "color code the damage numbers or find some way of
 * differentiating people's damage ... maybe we could even have 'damage skins' ... It's just a cosmetic").
 *
 * Every player's numbers come out in their own colour (PlayerColour), and your own are a size bigger. A damage skin
 * is a Den prize-counter cosmetic in the "Numbers" slot (DCCCosmetics): it changes how your numbers look and move
 * for everyone. Purely cosmetic. Drawn by ADCCFloatingText.
 */
namespace DCCDamageSkins
{
	enum class EMotion : uint8
	{
		Rise,     // straight up
		Flicker,  // up fast, shimmering (fire)
		Drift,    // slow, sinking a little at the end (ice)
		Wobble,   // side to side (toxic)
		Pop,      // bursts big, settles (gold)
		Glitch,   // jumps about (static)
		Stamp,    // slams down, holds, fades (the repo notice)
	};

	struct FSkin
	{
		FName Id;
		const TCHAR* Name;
		const TCHAR* Blurb;
		int32 Price = 0;
		FLinearColor Main;
		FLinearColor Glow;
		float Scale = 1.0f;
		const TCHAR* Prefix = TEXT("");
		const TCHAR* Suffix = TEXT("");
		EMotion Motion = EMotion::Rise;
	};

	const TArray<FSkin>& All();
	const FSkin* Find(FName Id);

	/** A player's own colour: the roster's are fixed (CJ orange, Scott blue...), anyone else gets one from their name. */
	FLinearColor PlayerColour(const FString& Name);
}
