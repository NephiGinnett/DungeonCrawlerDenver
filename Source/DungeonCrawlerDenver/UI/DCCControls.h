#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * Keys a player can change (2026-11-16, Zoe: "can you make it all adjustable to different keys"), from the SETTINGS
 * tab of the Tab menu. The HUD reads them every frame, so a change works at once and works from the truck too.
 * Saved per PC in GameUserSettings.ini, [DCCKeys].
 *
 * Keys that already do something else are refused, so a binding never silently does two things at once.
 */
namespace DCCControls
{
	enum EAction : uint8
	{
		Grapple = 0,   // retired 2026-11-16: the hook is a belt slot. Kept so saved key numbers do not shift.
		Spray,         // your spray tag from a truck (on foot it is a belt slot)
		Reactions,     // the reactions and moves menu
		QuickReact1,   // QuickReact1 + n sends reaction n + 1 (Cheer ... Cold): none bound by default
		QuickReactLast = QuickReact1 + 7,
		Count
	};

	const TCHAR* Name(uint8 Action);
	/** Shown in SETTINGS and read by the HUD (the retired ones are not). */
	inline bool IsLive(uint8 Action) { return Action != Grapple; }
	FKey Get(uint8 Action);
	FKey Default(uint8 Action);
	void Set(uint8 Action, const FKey& Key);

	/** What already uses Key in the game's fixed controls, or empty if nothing does. */
	FString TakenBy(const FKey& Key);
	/** The action (other than Except) this key is already set to, or Count. */
	uint8 BoundTo(const FKey& Key, uint8 Except);
}
