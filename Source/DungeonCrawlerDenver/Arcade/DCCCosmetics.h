#pragma once

#include "CoreMinimal.h"

/**
 * Arcade cosmetics (2026-11-13): hats, packs, armour-ish shoulders and auras
 * bought with tickets in the Den shop. They live in the arcade profile (DCCProfile),
 * not the DCD character save. A few of them do a small thing while worn (the jetpack
 * hops, the pack eases a climb, the plates turn a hit, the helmet lights the way).
 * The rest are still just looks.
 *
 * Each one is a few engine basic shapes (cone, cylinder, sphere, cube), so the
 * catalogue works with no art imported. A part hangs off an anchor (the head or
 * the upper back) and is placed in the CHARACTER's frame (X forward, Y right,
 * Z up), not the bone's, because each Meshy body's bones point a different way.
 */
namespace DCCCosmetics
{
	/** Slots, one item each. */
	extern const FName Head;
	extern const FName Back;
	extern const FName Shoulders;
	extern const FName Aura;
	/** Damage skins (UI/DCCDamageSkins): how your damage numbers look. No parts on the body. */
	extern const FName Numbers;

	enum class EAnchor : uint8 { HeadTop, UpperBack };

	struct FPart
	{
		const TCHAR* Shape;      // "Cone", "Cylinder", "Sphere" or "Cube" (/Engine/BasicShapes)
		FVector Size;            // cm
		FVector Offset;          // cm, character frame, from the anchor
		FRotator Rotation;       // relative to the character's facing
		FLinearColor Colour;
	};

	struct FItem
	{
		FName Id;
		const TCHAR* Name;
		const TCHAR* Blurb;
		FName Slot;
		int32 Price = 0;
		EAnchor Anchor = EAnchor::HeadTop;
		TArray<FPart> Parts;
	};

	/** Everything the shop sells, cheapest first within each slot. */
	const TArray<FItem>& Catalog();
	const FItem* Find(FName Id);

	/** Display name for a slot ("Head", "Back"...). */
	FString SlotLabel(FName Slot);

	/** Ranged gear the prize counter sells for tickets. Rows in DT_DCCEquipment, not cosmetics. */
	struct FBoothWeapon
	{
		FName Id;
		const TCHAR* Name;
		const TCHAR* Blurb;
		int32 Price = 0;
	};
	const TArray<FBoothWeapon>& BoothWeapons();
}
