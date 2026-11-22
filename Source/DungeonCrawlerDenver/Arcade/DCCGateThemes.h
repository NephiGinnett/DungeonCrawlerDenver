#pragma once

#include "CoreMinimal.h"
#include "Stats/DCCDamageTypes.h"

class ADCCEnemyCharacter;

/**
 * Gate Run themes (2026-11-22, Zoe: "we need much more stylized arenas, variety, etc."). Every gate rolls one from its
 * seed, so every machine builds the same place. A theme is the look (floor, wall and trim materials, light, the lid),
 * what stands in the rooms (pillars, wall clutter, a centrepiece in fight rooms), a hazard on the floor that hurts
 * crawlers and foes alike, and which creatures the gate favours. The rooms' rules are the same in every theme.
 */
namespace DCCGateThemes
{
	struct FTheme
	{
		const TCHAR* Name;           // "THE STORM DRAIN"
		const TCHAR* Blurb;          // one line under it on the plaza board
		const TCHAR* Floor;          // room floors
		const TCHAR* FloorAlt;       // special rooms (boss, duel, shrine)
		const TCHAR* Corridor;
		const TCHAR* Wall;
		const TCHAR* WallAlt;
		FLinearColor Accent;         // trim, signs, the plaza board
		FLinearColor LightTint;      // mixed into every room light
		float LightScale;
		FLinearColor Lid;            // the ceiling
		const TCHAR* StripMaterial;  // the glow along the foot of the walls (null: plain accent colour)
		const TCHAR* Pillar;         // a mesh, or null for a painted cylinder
		const TCHAR* PillarMaterial; // for the painted cylinder
		TArray<const TCHAR*> WallProps;
		TArray<const TCHAR*> Centrepieces;
		const TCHAR* Fx;             // a Niagara system lit at some wall props (null: none)
		// The floor hazard.
		const TCHAR* HazardName;     // "sludge"
		const TCHAR* HazardMaterial; // null: plain colour
		FLinearColor HazardColour;
		EDCCDamageType HazardType;
		bool bRoundHazard;
		TArray<TSubclassOf<ADCCEnemyCharacter>> Favoured;
	};

	const TArray<FTheme>& All();
	/** The same theme for the same seed on every machine. */
	int32 IndexFor(int32 Seed);
	const FTheme& For(int32 Seed);
}
