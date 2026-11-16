#include "Arcade/DCCRallyItems.h"

namespace DCCRallyItems
{
	const FName Hook(TEXT("Hook"));
	const FName Net(TEXT("Net"));
	const FName TowCable(TEXT("TowCable"));
	const FName WheelBoot(TEXT("WheelBoot"));
	const FName Lien(TEXT("Lien"));
	const FName Nitro(TEXT("Nitro"));
	const FName StuddedTires(TEXT("StuddedTires"));
	const FName QuietPlates(TEXT("QuietPlates"));
	const FName PinkSlip(TEXT("PinkSlip"));
	const FName SeizedChain(TEXT("SeizedChain"));
	const FName LotManifest(TEXT("LotManifest"));
	const FName VINPlate(TEXT("VINPlate"));

	const TArray<FItem>& All()
	{
		static const TArray<FItem> Items = {
			{ Hook, TEXT("Hook"), TEXT("Right mouse aims. Q fires. Pull a marked car onto your back bumper. The wrong car is a fine."), EKind::Tool, 0, FLinearColor(0.72f, 0.55f, 0.32f) },
			{ Net, TEXT("Net"), TEXT("Throw it ahead. A car that drives through it slows down, and a slow car reels onto the bumper easier."), EKind::Tool, 0, FLinearColor(0.85f, 0.42f, 0.15f) },
			{ TowCable, TEXT("Tow Cable"), TEXT("Yanks the car ahead of you. They lose most of their speed."), EKind::Tool, 0, FLinearColor(0.75f, 0.78f, 0.82f) },
			{ WheelBoot, TEXT("Wheel Boot"), TEXT("Drop it behind you. The next car to drive over it is clamped."), EKind::Tool, 0, FLinearColor(0.85f, 0.55f, 0.2f) },
			{ Lien, TEXT("Lien"), TEXT("Serve the car leading you. The paperwork spins them out."), EKind::Tool, 0, FLinearColor(0.95f, 0.9f, 0.75f) },
			{ Nitro, TEXT("Nitro"), TEXT("The tank, not the button. A long boost, ready now."), EKind::Boost, 0, FLinearColor(0.3f, 0.85f, 1.0f) },
			{ StuddedTires, TEXT("Studded Tires"), TEXT("The next wall, spike strip, boot or tow truck doesn't stop you."), EKind::Boost, 0, FLinearColor(0.7f, 0.7f, 0.75f) },
			{ QuietPlates, TEXT("Quiet Plates"), TEXT("The repo men lose interest in shoving you for a while."), EKind::Boost, 0, FLinearColor(0.45f, 0.8f, 0.55f) },
			{ PinkSlip, TEXT("Pink Slip"), TEXT("Signed over to Mile High Recovery Solutions. The signature is yours. You don't remember signing."),
				EKind::Seized, 0, FLinearColor(0.95f, 0.55f, 0.7f) },
			{ SeizedChain, TEXT("Seized Chain"), TEXT("Heavy, still warm. The Prize Counter pays for these and asks no questions."),
				EKind::Seized, 80, FLinearColor(1.0f, 0.82f, 0.35f) },
			{ LotManifest, TEXT("Lot Manifest"), TEXT("Today's clipboard. The mountain is on it. So are you."),
				EKind::Seized, 0, FLinearColor(0.85f, 0.82f, 0.7f) },
			{ VINPlate, TEXT("VIN Plate"), TEXT("Pried off something they already sold. Worth a little at the counter. Worth more as proof."),
				EKind::Seized, 45, FLinearColor(0.8f, 0.84f, 0.9f) },
		};
		return Items;
	}

	const FItem* Find(FName Id)
	{
		return All().FindByPredicate([Id](const FItem& Item) { return Item.Id == Id; });
	}

	TArray<FName> PadPool()
	{
		TArray<FName> Pool;
		for (const FItem& Item : All())
		{
			if (Item.Kind != EKind::Seized)
			{
				Pool.Add(Item.Id);
			}
		}
		return Pool;
	}

	TArray<FName> SeizedPool()
	{
		TArray<FName> Pool;
		for (const FItem& Item : All())
		{
			if (Item.Kind == EKind::Seized)
			{
				Pool.Add(Item.Id);
			}
		}
		return Pool;
	}
}
