#pragma once

#include "CoreMinimal.h"

/**
 * How a swing looks. Derived from the gear row's name. One line in ClipFor() is a new animation.
 * Stab, slash, overhead and blunt use the Mixamo clips baked onto the shared crawler (2026-11-19).
 * Shoot and throw still borrow the archery shot. Punch borrows the boxing warm-up.
 * Swords walk with the sword-and-shield set. Axes and hammers keep the body's own loops.
 */
namespace DCCAttackStyle
{
	enum EStyle : uint8
	{
		Stab,
		Slash,
		Overhead,
		Blunt,
		Punch,
		Shoot,
		Throw,
		/** A one-handed chop or bash (hatchet, hammer, pan, mace): the one-handed slash, with the body's own loops. */
		Chop
	};

	EStyle FromId(FName ItemId);
	const TCHAR* Name(EStyle Style);
	/** Asset path and the play rate. bBorrowed is false when the clip is one of the baked Mixamo swings. */
	void ClipFor(EStyle Style, FString& OutPath, float& OutRate, bool& bBorrowed);
}

class UAnimationAsset;

namespace DCCAttackStyle
{
	/** Idle, walk and run for a two-hander or a sword. Empty when this weapon keeps the body's own loops. */
	void WeaponLoops(EStyle Style, UAnimationAsset*& Idle, UAnimationAsset*& Walk, UAnimationAsset*& Run);
	/** Loads a combat clip once. A miss is remembered, so a missing file is not looked up every frame. */
	UAnimationAsset* FindClip(const TCHAR* Path);
	UAnimationAsset* HitClip();
	UAnimationAsset* CastClip();
	UAnimationAsset* WaveClip();
}
