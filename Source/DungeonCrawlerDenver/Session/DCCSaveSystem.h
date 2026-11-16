#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;
class UWorld;

/**
 * Crawler saves (2026-11-13). Before this, levels, gold, gear, skills and loot
 * boxes all vanished when the game closed.
 *
 * The host keeps everyone's save, keyed by crawler (CJ, Scott...), so a player
 * is themselves whenever they join that host. Server only throughout.
 *
 *   Load    when a crawler's identity is restored on possession
 *           (UDCCOnboardingComponent), before anything can save over it.
 *   Save    every 60 s, on leaving, when a run ends, after a shop trade, when
 *           the pawn is destroyed, and when the world shuts down.
 *
 * A pawn only ever saves if it was the one that loaded that crawler. That stops
 * a fresh, empty pawn overwriting a good save, and a dying corpse overwriting
 * the new pawn's progress after a respawn.
 *
 * Console: DCC.Save (save everyone now), DCC.WipeSave <Crawler> (start over).
 */
namespace DCCSave
{
	/** Load this crawler's save into the pawn (or start fresh), and mark the pawn as its owner. */
	void Load(ADCCCharacterBase* Crawler);

	/** Save one crawler. False if it has no save identity or is not the owning pawn. */
	bool Save(ADCCCharacterBase* Crawler);

	/** Save every player crawler in the world. */
	void SaveAll(UWorld* World, const TCHAR* Reason);

	/** "CJ", or the player's name when no crawler identity is set. Empty for non-players. */
	FString SaveKeyFor(const ADCCCharacterBase* Crawler);
}
