#include "Arcade/DCCClimbItems.h"

namespace DCCClimbItems
{
	const FName GrapplingHook(TEXT("GrapplingHook"));
	const FName Piton(TEXT("Piton"));
	const FName TrailMix(TEXT("TrailMix"));
	const FName ChalkBag(TEXT("ChalkBag"));
	const FName Crampons(TEXT("Crampons"));
	const FName WindCloak(TEXT("WindCloak"));
	const FName HardHat(TEXT("HardHat"));
	const FName SafetyLine(TEXT("SafetyLine"));
	const FName GranolaBar(TEXT("GranolaBar"));
	const FName GreenChileBurrito(TEXT("GreenChileBurrito"));
	const FName Jerky(TEXT("Jerky"));
	const FName Apple(TEXT("Apple"));
	const FName CeremonialKnife(TEXT("CeremonialKnife"));
	const FName PlatinumOre(TEXT("PlatinumOre"));
	const FName AssayLedger(TEXT("AssayLedger"));
	const FName ClaimLantern(TEXT("ClaimLantern"));

	const TArray<FItem>& All()
	{
		static const TArray<FItem> Items = {
			// Tools: a belt slot each (its number key uses it).
			{ GrapplingHook, TEXT("Grappling Hook"), TEXT("While climbing: yank yourself 6 m up the wall."), EKind::Tool, 0, FLinearColor(0.7f, 0.75f, 0.8f) },
			{ Piton, TEXT("Piton"), TEXT("Drive it in: if you fall, you come back here."), EKind::Tool, 0, FLinearColor(0.85f, 0.85f, 0.9f) },
			{ TrailMix, TEXT("Trail Mix"), TEXT("Eat it: stamina back to full, even on the wall. A little less hungry."), EKind::Tool, 0, FLinearColor(0.85f, 0.6f, 0.3f), 20.0f },
			// Food (2026-11-13): hunger caps your stamina; eating lifts the cap back up.
			{ GranolaBar, TEXT("Granola Bar"), TEXT("Eat it: less hungry, and a little stamina back."), EKind::Tool, 0, FLinearColor(0.8f, 0.65f, 0.35f), 35.0f },
			{ GreenChileBurrito, TEXT("Green Chile Burrito"), TEXT("Eat it: hunger gone. Denver's finest, foil and all."), EKind::Tool, 0, FLinearColor(0.45f, 0.75f, 0.3f), 80.0f },
			{ Jerky, TEXT("Jerky"), TEXT("Eat it: less hungry. Chewy."), EKind::Tool, 0, FLinearColor(0.55f, 0.3f, 0.2f), 30.0f },
			{ Apple, TEXT("Apple"), TEXT("Eat it: a little less hungry."), EKind::Tool, 0, FLinearColor(0.9f, 0.2f, 0.2f), 20.0f },
			// Boosts: work the moment you pick them up.
			{ ChalkBag, TEXT("Chalk Bag"), TEXT("Climbing costs 40% less stamina for 90 seconds."), EKind::Boost, 0, FLinearColor(0.95f, 0.95f, 0.95f) },
			{ Crampons, TEXT("Crampons"), TEXT("Ice costs you nothing extra for the rest of the round."), EKind::Boost, 0, FLinearColor(0.6f, 0.85f, 1.0f) },
			{ WindCloak, TEXT("Wind Cloak"), TEXT("Gusts cost you nothing extra for the rest of the round."), EKind::Boost, 0, FLinearColor(0.5f, 0.8f, 0.6f) },
			{ HardHat, TEXT("Hard Hat"), TEXT("The next rock that hits you bounces off. You keep your grip."), EKind::Boost, 0, FLinearColor(1.0f, 0.8f, 0.1f) },
			{ SafetyLine, TEXT("Safety Line"), TEXT("Catches your next fall: no damage."), EKind::Boost, 0, FLinearColor(1.0f, 0.45f, 0.2f) },
			// Artifacts: carried; lost if you die; banked as trophies if you live to the end of the round.
			{ CeremonialKnife, TEXT("Ceremonial Knife"), TEXT("Older than any claim on this mountain. Mile High Recovery Solutions would very much like it to stay lost. Proof the ground is historic."),
				EKind::Artifact, 0, FLinearColor(0.9f, 0.75f, 0.35f) },
			{ PlatinumOre, TEXT("Platinum Ore"), TEXT("A fist-sized nugget, worth a fortune at the Prize Counter if you get it down alive."),
				EKind::Artifact, 150, FLinearColor(0.85f, 0.88f, 0.95f) },
			{ AssayLedger, TEXT("Assayer's Ledger, 1859"), TEXT("Columns of numbers that do not add up. Someone salted this claim. Evidence."),
				EKind::Artifact, 0, FLinearColor(0.55f, 0.35f, 0.2f) },
			{ ClaimLantern, TEXT("Prospector's Lantern"), TEXT("Still smells of lamp oil. Etched: 'FT 1859'. A keepsake from the first Fort Tiner."),
				EKind::Artifact, 40, FLinearColor(1.0f, 0.65f, 0.25f) },
		};
		return Items;
	}

	int32 SupplyPrice(FName Id)
	{
		// Cheap enough to buy something every round, dear enough that the ledge chests still matter. Food is
		// cheapest: hunger is the clock, and running out should be a choice, not a trap.
		static const TMap<FName, int32> Prices = {
			{ Apple, 2 }, { Jerky, 4 }, { GranolaBar, 5 }, { GreenChileBurrito, 9 }, { TrailMix, 8 },
			{ Piton, 10 }, { ChalkBag, 10 }, { HardHat, 12 }, { SafetyLine, 14 }, { Crampons, 16 }, { WindCloak, 16 },
			// No GrapplingHook: it is on everyone's belt now (DCCChopShop::Grapple), upgraded rather than bought.
		};
		const int32* Price = Prices.Find(Id);
		return Price ? *Price : 0;
	}

	const FItem* Find(FName Id)
	{
		return All().FindByPredicate([Id](const FItem& Item) { return Item.Id == Id; });
	}

	TArray<FName> ChestPool()
	{
		TArray<FName> Pool;
		for (const FItem& Item : All())
		{
			if (Item.Kind != EKind::Artifact)
			{
				Pool.Add(Item.Id);
			}
		}
		return Pool;
	}

	TArray<FName> FoodPool()
	{
		TArray<FName> Pool;
		for (const FItem& Item : All())
		{
			if (Item.Food > 0.0f)
			{
				Pool.Add(Item.Id);
			}
		}
		return Pool;
	}

	TArray<FName> ArtifactPool()
	{
		TArray<FName> Pool;
		for (const FItem& Item : All())
		{
			if (Item.Kind == EKind::Artifact)
			{
				Pool.Add(Item.Id);
			}
		}
		return Pool;
	}
}
