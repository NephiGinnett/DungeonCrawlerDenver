#pragma once

#include "CoreMinimal.h"

/**
 * How a swing looks. Derived from the gear row's name. One line in ClipFor() is a new animation.
 * Slash and Overhead borrow Charged Slash, Punch borrows Boxing Warm-up, Shoot borrows Archery Shot,
 * Stab plays that slash faster. Blunt borrows the slash too. Throw borrows the archery shot.
 * Mixamo clips are a later swap in this one function.
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
		Throw
	};

	EStyle FromId(FName ItemId);
	const TCHAR* Name(EStyle Style);
	/** Asset path (may be a borrowed clip) and the play rate. */
	void ClipFor(EStyle Style, FString& OutPath, float& OutRate, bool& bBorrowed);
}
