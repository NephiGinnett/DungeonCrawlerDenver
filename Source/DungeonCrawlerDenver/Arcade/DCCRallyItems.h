#pragma once

#include "CoreMinimal.h"

/**
 * Repo Rally's items (2026-11-13). Found on glowing pads around the impound
 * oval, all of them in the pit crate just past the start, and one piece of
 * seized goods in the infield. Nothing here is Dungeon Crawler Denver gear.
 *
 *   Tool      kept, used with [Q] ([F] picks which — [T] is the car radio)
 *   Boost     works the moment the car drives over it
 *   Seized    carried in the passenger seat. Finish the race and it goes into
 *             the trophy vault. Fail to finish and the lot keeps it.
 */
namespace DCCRallyItems
{
	enum class EKind : uint8 { Tool, Boost, Seized };

	struct FItem
	{
		FName Id;
		const TCHAR* Name;
		const TCHAR* Blurb;
		EKind Kind;
		/** Tickets at the Prize Counter. 0 = a keepsake, not for sale. */
		int32 SellValue = 0;
		FLinearColor Colour;
	};

	const TArray<FItem>& All();
	const FItem* Find(FName Id);

	/** What a track pad can hold: tools and boosts, every one useful in a race. */
	TArray<FName> PadPool();
	/** What the infield cache can hold. */
	TArray<FName> SeizedPool();

	extern const FName Hook, Net;
	extern const FName TowCable, WheelBoot, Lien;
	extern const FName Nitro, StuddedTires, QuietPlates;
	extern const FName PinkSlip, SeizedChain, LotManifest, VINPlate;
}
