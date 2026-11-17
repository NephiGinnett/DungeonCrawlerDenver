// Item pop-ups: a video-game loot card for anything a crawler gains.
//
// Borrowed from Elyra's Tales (TheFamiliar/game/scripts/notices.gd and the
// rarity colours in enchanting.gd): cards queue, show a few seconds each, and
// fade. They are coloured by rarity and show what the item does. A new piece
// of gear also says how it compares with what is already in that slot, which
// is the question a player actually has when loot drops.
//
// Everything here is client-side and read-only. The HUD diffs the owning
// crawler's replicated state each frame (bag, potions, loot boxes, equipped
// slots, gold) against what it saw last frame. Whatever is new becomes a card.
// That covers every way an item arrives (ground pickups, chests, corpses, tale
// floor payouts, achievement rewards) without any of them knowing about the HUD.

#include "UI/DCCHUD.h"
#include "UI/DCCItemVisuals.h"

#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Items/DCCConsumableTypes.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCLootBoxTypes.h"
#include "Items/DCCPickup.h"
#include "Stats/DCCStatsComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace DCCItemToast
{
	constexpr double ShowSeconds = 4.5;
	constexpr double SlideSeconds = 0.25;
	constexpr double FadeSeconds = 0.4;
	constexpr int32 MaxShowing = 3;
	constexpr int32 MaxQueued = 12;
	constexpr float CardWidth = 380.0f;

	FLinearColor RarityColour(EDCCLootBoxTier Tier)
	{
		switch (Tier)
		{
		case EDCCLootBoxTier::Bronze:    return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D08A4E")));
		case EDCCLootBoxTier::Silver:    return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C9D3DF")));
		case EDCCLootBoxTier::Gold:      return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFD36B")));
		case EDCCLootBoxTier::Platinum:  return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7FE3FF")));
		case EDCCLootBoxTier::Legendary: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FF9A3C")));
		case EDCCLootBoxTier::Celestial: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F58CFF")));
		default:                         return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C8C0B4")));
		}
	}

	FString RarityName(EDCCLootBoxTier Tier)
	{
		return Tier == EDCCLootBoxTier::None
			? FString(TEXT("Common"))
			: UEnum::GetDisplayValueAsText(Tier).ToString();
	}

	uint8 IconForSlot(EDCCEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EDCCEquipmentSlot::MainHand: return Sword;
		case EDCCEquipmentSlot::OffHand:  return Shield;
		case EDCCEquipmentSlot::Head:     return Helmet;
		case EDCCEquipmentSlot::Body:     return Tunic;
		case EDCCEquipmentSlot::Trinket:  return Ring;
		default:                          return Bag;
		}
	}

	const TCHAR* AbilityShort(EDCCAbility Ability)
	{
		switch (Ability)
		{
		case EDCCAbility::Strength:     return TEXT("STR");
		case EDCCAbility::Intelligence: return TEXT("INT");
		case EDCCAbility::Constitution: return TEXT("CON");
		case EDCCAbility::Dexterity:    return TEXT("DEX");
		case EDCCAbility::Charisma:     return TEXT("CHA");
		default:                        return TEXT("?");
		}
	}

	/** Same rough yardstick allies use to decide whether to swap gear. */
	int32 Score(const FDCCEquipmentDef& Def)
	{
		return Def.BonusStrength + Def.BonusIntelligence + Def.BonusConstitution + Def.BonusDexterity + Def.BonusCharisma
			+ Def.WeaponDamageDie / 2 + Def.EvadeBonus * 2;
	}

	void AddStatLines(const FDCCEquipmentDef& Def, TArray<FString>& Out)
	{
		if (Def.WeaponDamageDie > 0)
		{
			Out.Add(FString::Printf(TEXT("1d%d %s damage"), Def.WeaponDamageDie,
				*UEnum::GetDisplayValueAsText(Def.WeaponDamageType).ToString()));
		}

		FString Bonuses;
		const TPair<int32, const TCHAR*> Pairs[] = {
			{ Def.BonusStrength, TEXT("STR") }, { Def.BonusIntelligence, TEXT("INT") },
			{ Def.BonusConstitution, TEXT("CON") }, { Def.BonusDexterity, TEXT("DEX") },
			{ Def.BonusCharisma, TEXT("CHA") }
		};
		for (const TPair<int32, const TCHAR*>& Pair : Pairs)
		{
			if (Pair.Key != 0)
			{
				Bonuses += FString::Printf(TEXT("%s%+d %s"), Bonuses.IsEmpty() ? TEXT("") : TEXT("   "), Pair.Key, Pair.Value);
			}
		}
		if (!Bonuses.IsEmpty())
		{
			Out.Add(Bonuses);
		}

		if (Def.EvadeBonus != 0)
		{
			Out.Add(FString::Printf(TEXT("%+d Evade"), Def.EvadeBonus));
		}
		if (Def.AttackRangeOverride > 0.0f)
		{
			Out.Add(FString::Printf(TEXT("Reach %.0f m"), Def.AttackRangeOverride / 100.0f));
		}
		if (Def.ResistanceLevel != EDCCResistanceLevel::Normal)
		{
			Out.Add(FString::Printf(TEXT("%s: %s"),
				*UEnum::GetDisplayValueAsText(Def.ResistanceDamageType).ToString(),
				*UEnum::GetDisplayValueAsText(Def.ResistanceLevel).ToString()));
		}
		if (Def.bIsCursed)
		{
			Out.Add(TEXT("Cursed. It will not come off."));
		}
		if (Out.IsEmpty() && !Def.Description.IsEmpty())
		{
			Out.Add(Def.Description.ToString());
		}
	}

	FDCCItemToastCard ForEquipment(const UDCCEquipmentComponent* Equipment, FName ItemId, bool bEquipped, FName Replaced)
	{
		FDCCItemToastCard Card;
		Card.Header = bEquipped ? TEXT("EQUIPPED") : TEXT("NEW ITEM");

		FDCCEquipmentDef Def;
		if (!Equipment || !Equipment->FindItem(ItemId, Def))
		{
			Card.Name = ItemId.ToString();
			Card.Colour = RarityColour(EDCCLootBoxTier::None);
			return Card;
		}

		Card.Name = Def.DisplayName.IsEmpty() ? ItemId.ToString() : Def.DisplayName.ToString();
		Card.Colour = RarityColour(Def.Rarity);
		Card.Icon = IconForSlot(Def.Slot);
		Card.Subtitle = FString::Printf(TEXT("%s  -  %s"), *RarityName(Def.Rarity),
			*UEnum::GetDisplayValueAsText(Def.Slot).ToString());
		AddStatLines(Def, Card.Lines);

		// What the player wants to know when loot drops: is it better?
		const FName Current = bEquipped ? Replaced : Equipment->GetEquippedId(Def.Slot);
		FDCCEquipmentDef CurrentDef;
		if (Current.IsNone() || Current == ItemId)
		{
			if (!bEquipped)
			{
				Card.Compare = TEXT("Your slot is empty. Equip it from the Tab menu.");
				Card.CompareSign = 1;
			}
		}
		else if (Equipment->FindItem(Current, CurrentDef))
		{
			const FString CurrentName = CurrentDef.DisplayName.IsEmpty() ? Current.ToString() : CurrentDef.DisplayName.ToString();
			const int32 Delta = Score(Def) - Score(CurrentDef);
			if (bEquipped)
			{
				Card.Compare = FString::Printf(TEXT("Replaces %s"), *CurrentName);
				Card.CompareSign = Delta > 0 ? 1 : Delta < 0 ? -1 : 0;
			}
			else if (Delta > 0)
			{
				Card.Compare = FString::Printf(TEXT("UPGRADE over %s"), *CurrentName);
				Card.CompareSign = 1;
			}
			else if (Delta < 0)
			{
				Card.Compare = FString::Printf(TEXT("Weaker than %s"), *CurrentName);
				Card.CompareSign = -1;
			}
			else
			{
				Card.Compare = FString::Printf(TEXT("About the same as %s"), *CurrentName);
			}
		}
		return Card;
	}
}

void ADCCHUD::WatchForNewItems(ADCCCharacterBase* Crawler)
{
	using namespace DCCItemToast;

	UDCCInventoryComponent* Inventory = Crawler ? Crawler->Inventory.Get() : nullptr;
	UDCCEquipmentComponent* Equipment = Crawler ? Crawler->Equipment.Get() : nullptr;

	// A new body (spawn, respawn, possession) is a new baseline: everything it
	// already carries is old news, not a pile of cards.
	// The loot box screen shows its own reveal; cards on top of it would be noise.
	const bool bBaseline = ItemToastWatched.Get() != Crawler || bLootScreenOpen;
	if (ItemToastWatched.Get() != Crawler)
	{
		ItemToastWatched = Crawler;
		ItemToastQueue.Reset();
	}
	if (!Crawler)
	{
		return;
	}

	// The bag. New records are appended, so anything past the old count is new.
	if (Inventory)
	{
		const int32 Count = Inventory->Items.Num();
		if (!bBaseline && Count > SeenItemCount)
		{
			for (int32 i = SeenItemCount; i < Count; ++i)
			{
				const FDCCInventoryItem& Item = Inventory->Items[i];
				if (!Item.EquipmentItemId.IsNone())
				{
					PushItemToast(ForEquipment(Equipment, Item.EquipmentItemId, /*bEquipped=*/false, NAME_None));
					continue;
				}

				FDCCItemToastCard Card;
				Card.Header = TEXT("NEW ITEM");
				Card.Name = Item.DisplayName.ToString();
				Card.Colour = RarityColour(EDCCLootBoxTier::None);
				Card.Icon = Bag;
				Card.Subtitle = TEXT("Stowed in your bag");
				if (Item.MassKilograms > 0.0f)
				{
					Card.Lines.Add(FString::Printf(TEXT("%.1f kg"), Item.MassKilograms));
				}
				if (const ADCCPickup* Defaults = Item.PickupClass ? Item.PickupClass->GetDefaultObject<ADCCPickup>() : nullptr)
				{
					if (Defaults->GoldValue > 0)
					{
						Card.Lines.Add(FString::Printf(TEXT("Worth %d gold"), Defaults->GoldValue));
					}
				}
				PushItemToast(MoveTemp(Card));
			}
		}
		SeenItemCount = Count;

		// Potions and other consumables stack by id; a bigger stack is a find.
		TMap<FName, int32> Consumables;
		for (const FDCCStowedConsumable& Stack : Inventory->StowedConsumables)
		{
			Consumables.FindOrAdd(Stack.ItemId) += Stack.Quantity;
		}
		if (!bBaseline)
		{
			for (const TPair<FName, int32>& Pair : Consumables)
			{
				const int32 Before = SeenConsumables.FindRef(Pair.Key);
				if (Pair.Value <= Before)
				{
					continue;
				}

				FDCCItemToastCard Card;
				Card.Header = TEXT("CONSUMABLE");
				Card.Icon = Potion;
				Card.Colour = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7AD07A")));
				FDCCConsumableDef Def;
				const bool bKnown = Inventory->FindConsumable(Pair.Key, Def);
				Card.Name = bKnown && !Def.DisplayName.IsEmpty() ? Def.DisplayName.ToString() : Pair.Key.ToString();
				const int32 Gained = Pair.Value - Before;
				Card.Subtitle = Gained > 1
					? FString::Printf(TEXT("x%d  -  you have %d"), Gained, Pair.Value)
					: FString::Printf(TEXT("You have %d"), Pair.Value);
				if (bKnown)
				{
					for (const FDCCConsumableEffect& Effect : Def.Effects)
					{
						if (Effect.InstantHealthChange > 0)
						{
							Card.Lines.Add(FString::Printf(TEXT("Restores %d health"), Effect.InstantHealthChange));
						}
						else if (Effect.InstantHealthChange < 0)
						{
							Card.Lines.Add(FString::Printf(TEXT("Deals %d damage to you"), -Effect.InstantHealthChange));
						}
						if (Effect.ModifierAmount != 0)
						{
							Card.Lines.Add(FString::Printf(TEXT("%+d %s for %.0f s"), Effect.ModifierAmount,
								AbilityShort(Effect.ModifiedAbility), Effect.ModifierDurationSeconds));
						}
					}
					if (Card.Lines.IsEmpty() && !Def.Description.IsEmpty())
					{
						Card.Lines.Add(Def.Description.ToString());
					}
				}
				PushItemToast(MoveTemp(Card));
			}
		}
		SeenConsumables = MoveTemp(Consumables);

		// Loot boxes, by tier.
		const TArray<int32>& Boxes = Inventory->LootBoxCounts;
		if (!bBaseline)
		{
			for (int32 Tier = 1; Tier < Boxes.Num(); ++Tier)
			{
				const int32 Before = SeenLootBoxes.IsValidIndex(Tier) ? SeenLootBoxes[Tier] : 0;
				if (Boxes[Tier] <= Before)
				{
					continue;
				}

				const EDCCLootBoxTier AsTier = static_cast<EDCCLootBoxTier>(Tier);
				FDCCItemToastCard Card;
				Card.Header = TEXT("LOOT BOX");
				Card.Icon = LootBox;
				Card.Colour = RarityColour(AsTier);
				Card.Name = FString::Printf(TEXT("%s Box"), *RarityName(AsTier));
				Card.Subtitle = FString::Printf(TEXT("You have %d"), Boxes[Tier]);
				Card.Lines.Add(TEXT("Sponsored. Tab, then LOOT BOXES, to open them."));
				PushItemToast(MoveTemp(Card));
			}
		}
		SeenLootBoxes = Boxes;
	}

	// Equipped slots. A slot that now holds something different was just equipped.
	if (Equipment)
	{
		TMap<uint8, FName> Now;
		for (const FDCCEquippedItem& Entry : Equipment->Equipped)
		{
			Now.Add(static_cast<uint8>(Entry.Slot), Entry.ItemId);
		}
		if (!bBaseline)
		{
			for (const TPair<uint8, FName>& Pair : Now)
			{
				const FName Before = SeenEquipped.FindRef(Pair.Key);
				if (!Pair.Value.IsNone() && Pair.Value != Before)
				{
					PushItemToast(ForEquipment(Equipment, Pair.Value, /*bEquipped=*/true, Before));
				}
			}
		}
		SeenEquipped = MoveTemp(Now);
	}

	// Gold. Several coins in a row fold into one card instead of a stack of them.
	if (Crawler->Stats)
	{
		const int32 Gold = Crawler->Stats->Gold;
		if (!bBaseline && Gold > SeenGold)
		{
			FDCCItemToastCard Card;
			Card.Header = TEXT("GOLD");
			Card.Icon = Coin;
			Card.Colour = RarityColour(EDCCLootBoxTier::Gold);
			Card.GoldAmount = Gold - SeenGold;
			PushItemToast(MoveTemp(Card));
		}
		SeenGold = Gold;
	}
}

void ADCCHUD::PushItemToast(FDCCItemToastCard&& Card)
{
	using namespace DCCItemToast;

	if (Card.GoldAmount > 0)
	{
		const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
		auto Fold = [&Card, Now](FDCCItemToastCard& Existing, bool bShowing)
		{
			if (Existing.GoldAmount <= 0)
			{
				return false;
			}
			// Only into a card that is not already on its way out.
			if (bShowing && Now - Existing.ShownAt > ShowSeconds - 1.0)
			{
				return false;
			}
			Existing.GoldAmount += Card.GoldAmount;
			if (bShowing)
			{
				Existing.ShownAt = FMath::Max(Existing.ShownAt, Now - SlideSeconds);
			}
			return true;
		};
		for (FDCCItemToastCard& Existing : ItemToastsShowing)
		{
			if (Fold(Existing, true))
			{
				return;
			}
		}
		for (FDCCItemToastCard& Existing : ItemToastQueue)
		{
			if (Fold(Existing, false))
			{
				return;
			}
		}
	}

	// Loot-all on a big chest should not keep cards coming for a minute.
	if (ItemToastQueue.Num() >= MaxQueued)
	{
		ItemToastQueue.RemoveAt(0);
	}
	ItemToastQueue.Add(MoveTemp(Card));
}

void ADCCHUD::DrawItemToasts()
{
	using namespace DCCItemToast;

	const UWorld* World = GetWorld();
	if (!Canvas || !World)
	{
		return;
	}

	// Real time, so cards still time out while the game is paused for planning.
	const double Now = World->GetRealTimeSeconds();

	ItemToastsShowing.RemoveAll([Now](const FDCCItemToastCard& Card)
	{
		return Now - Card.ShownAt >= ShowSeconds;
	});

	while (ItemToastsShowing.Num() < MaxShowing && !ItemToastQueue.IsEmpty())
	{
		FDCCItemToastCard Next = ItemToastQueue[0];
		ItemToastQueue.RemoveAt(0);
		// Staggered, so three cards from one Loot All arrive one after another.
		Next.ShownAt = Now + ItemToastsShowing.Num() * 0.18;
		ItemToastsShowing.Add(MoveTemp(Next));
	}

	UFont* SmallFont = GEngine->GetSmallFont();
	UFont* MediumFont = GEngine->GetMediumFont();

	constexpr float Pad = 12.0f;
	constexpr float IconSize = 58.0f;
	const float RestX = Canvas->SizeX - CardWidth - 32.0f;
	float Y = Canvas->SizeY * 0.34f;

	for (const FDCCItemToastCard& Card : ItemToastsShowing)
	{
		const double Age = Now - Card.ShownAt;
		if (Age < 0.0)
		{
			continue;
		}

		const float SlideT = FMath::Clamp(static_cast<float>(Age / SlideSeconds), 0.0f, 1.0f);
		const float Ease = 1.0f - FMath::Pow(1.0f - SlideT, 3.0f);
		const float Alpha = FMath::Min(SlideT, FMath::Clamp(static_cast<float>((ShowSeconds - Age) / FadeSeconds), 0.0f, 1.0f));
		const float X = RestX + (1.0f - Ease) * (CardWidth + 48.0f);

		const FString Name = Card.GoldAmount > 0 ? FString::Printf(TEXT("+%d gold"), Card.GoldAmount) : Card.Name;

		// Measure first, so the panel is exactly as tall as what is on it.
		const float TextX = X + Pad + IconSize + Pad;
		const float TextW = CardWidth - (TextX - X) - Pad;
		float W = 0.0f, HeaderH = 0.0f, NameH = 0.0f, LineH = 0.0f;
		GetTextSize(Card.Header, W, HeaderH, SmallFont, 1.0f);
		GetTextSize(Name, W, NameH, MediumFont, 1.0f);
		GetTextSize(TEXT("Ag"), W, LineH, SmallFont, 1.0f);

		int32 TextRows = (Card.Subtitle.IsEmpty() ? 0 : 1) + Card.Lines.Num() + (Card.Compare.IsEmpty() ? 0 : 1);
		const float TextBlockH = HeaderH + NameH + 4.0f + TextRows * (LineH + 1.0f);
		const float CardH = FMath::Max(IconSize, TextBlockH) + Pad * 2.0f;

		const FLinearColor C = Card.Colour;
		auto Tint = [Alpha](FLinearColor Colour, float A)
		{
			Colour.A = A * Alpha;
			return Colour;
		};

		// A soft rarity glow, the dark card, a bright rarity edge on the left.
		DrawRect(Tint(C, 0.16f), X - 3.0f, Y - 3.0f, CardWidth + 6.0f, CardH + 6.0f);
		DrawRect(FLinearColor(0.035f, 0.03f, 0.06f, 0.93f * Alpha), X, Y, CardWidth, CardH);
		DrawRect(Tint(C, 0.95f), X, Y, 5.0f, CardH);
		DrawRect(Tint(C, 0.55f), X, Y, CardWidth, 1.0f);
		DrawRect(Tint(C, 0.55f), X, Y + CardH - 1.0f, CardWidth, 1.0f);
		DrawRect(Tint(C, 0.55f), X + CardWidth - 1.0f, Y, 1.0f, CardH);

		// The icon in a rarity frame, like an inventory slot.
		const float IconX = X + Pad + 2.0f;
		const float IconY = Y + (CardH - IconSize) * 0.5f;
		DrawRect(Tint(C, 0.14f), IconX, IconY, IconSize, IconSize);
		DrawRect(Tint(C, 0.85f), IconX, IconY, IconSize, 2.0f);
		DrawRect(Tint(C, 0.85f), IconX, IconY + IconSize - 2.0f, IconSize, 2.0f);
		DrawRect(Tint(C, 0.85f), IconX, IconY, 2.0f, IconSize);
		DrawRect(Tint(C, 0.85f), IconX + IconSize - 2.0f, IconY, 2.0f, IconSize);
		DrawItemIcon(Card.Icon, IconX, IconY, IconSize, C, Alpha);

		float TY = Y + Pad;
		DrawText(Card.Header, Tint(C, 0.85f), TextX, TY, SmallFont, 1.0f);
		TY += HeaderH;
		DrawText(Name, Tint(C * 0.6f + FLinearColor::White * 0.4f, 1.0f), TextX, TY, MediumFont, 1.0f);
		TY += NameH + 4.0f;

		if (!Card.Subtitle.IsEmpty())
		{
			DrawText(Card.Subtitle, FLinearColor(0.62f, 0.60f, 0.66f, Alpha), TextX, TY, SmallFont, 1.0f);
			TY += LineH + 1.0f;
		}
		for (const FString& Line : Card.Lines)
		{
			DrawText(Line, FLinearColor(0.93f, 0.90f, 0.85f, Alpha), TextX, TY, SmallFont, 1.0f);
			TY += LineH + 1.0f;
		}
		if (!Card.Compare.IsEmpty())
		{
			const FLinearColor CompareColour = Card.CompareSign > 0 ? FLinearColor(0.45f, 0.95f, 0.45f, Alpha)
				: Card.CompareSign < 0 ? FLinearColor(1.0f, 0.45f, 0.40f, Alpha)
				: FLinearColor(0.75f, 0.75f, 0.75f, Alpha);
			DrawText(Card.Compare, CompareColour, TextX, TY, SmallFont, 1.0f);
		}

		(void)TextW;
		Y += CardH + 10.0f;
	}
}

void ADCCHUD::DrawItemIcon(uint8 Icon, float X, float Y, float Size, const FLinearColor& Colour, float Alpha)
{
	using namespace DCCItemToast;

	FLinearColor Ink = Colour * 0.5f + FLinearColor::White * 0.5f;
	Ink.A = Alpha;

	// Icons are drawn in a 0..1 box, then scaled into the frame.
	auto P = [X, Y, Size](float U, float V) { return FVector2D(X + U * Size, Y + V * Size); };
	auto Line = [&](float U0, float V0, float U1, float V1, float Thickness)
	{
		const FVector2D A = P(U0, V0), B = P(U1, V1);
		DrawLine(A.X, A.Y, B.X, B.Y, Ink, Thickness);
	};
	auto Poly = [&](std::initializer_list<FVector2D> Points, float Thickness)
	{
		const TArray<FVector2D> Pts(Points);
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			const FVector2D& A = Pts[i];
			const FVector2D& B = Pts[(i + 1) % Pts.Num()];
			Line(A.X, A.Y, B.X, B.Y, Thickness);
		}
	};
	auto Arc = [&](float CU, float CV, float R, float FromDeg, float ToDeg, float Thickness)
	{
		constexpr int32 Segments = 18;
		for (int32 i = 0; i < Segments; ++i)
		{
			const float A0 = FMath::DegreesToRadians(FMath::Lerp(FromDeg, ToDeg, static_cast<float>(i) / Segments));
			const float A1 = FMath::DegreesToRadians(FMath::Lerp(FromDeg, ToDeg, static_cast<float>(i + 1) / Segments));
			Line(CU + FMath::Cos(A0) * R, CV + FMath::Sin(A0) * R, CU + FMath::Cos(A1) * R, CV + FMath::Sin(A1) * R, Thickness);
		}
	};

	switch (Icon)
	{
	case Sword:
		Line(0.33f, 0.67f, 0.80f, 0.20f, 4.0f);   // blade
		Line(0.20f, 0.54f, 0.46f, 0.80f, 3.0f);   // crossguard
		Line(0.33f, 0.67f, 0.20f, 0.80f, 3.0f);   // grip
		Line(0.15f, 0.85f, 0.21f, 0.79f, 5.0f);   // pommel
		break;
	case Shield:
		Poly({ FVector2D(0.24f, 0.20f), FVector2D(0.76f, 0.20f), FVector2D(0.76f, 0.50f),
			FVector2D(0.50f, 0.84f), FVector2D(0.24f, 0.50f) }, 3.0f);
		Line(0.50f, 0.24f, 0.50f, 0.76f, 2.0f);
		Line(0.28f, 0.42f, 0.72f, 0.42f, 2.0f);
		break;
	case Helmet:
		Arc(0.50f, 0.58f, 0.30f, 180.0f, 360.0f, 3.0f);
		Line(0.16f, 0.58f, 0.84f, 0.58f, 3.0f);
		Line(0.20f, 0.58f, 0.20f, 0.76f, 3.0f);
		Line(0.80f, 0.58f, 0.80f, 0.76f, 3.0f);
		Line(0.50f, 0.28f, 0.50f, 0.58f, 2.0f);
		break;
	case Tunic:
		Poly({ FVector2D(0.34f, 0.20f), FVector2D(0.66f, 0.20f), FVector2D(0.84f, 0.36f), FVector2D(0.74f, 0.46f),
			FVector2D(0.70f, 0.82f), FVector2D(0.30f, 0.82f), FVector2D(0.26f, 0.46f), FVector2D(0.16f, 0.36f) }, 3.0f);
		Line(0.42f, 0.20f, 0.50f, 0.32f, 2.0f);
		Line(0.58f, 0.20f, 0.50f, 0.32f, 2.0f);
		break;
	case Ring:
		Arc(0.50f, 0.60f, 0.22f, 0.0f, 360.0f, 3.0f);
		Poly({ FVector2D(0.50f, 0.18f), FVector2D(0.60f, 0.28f), FVector2D(0.50f, 0.38f), FVector2D(0.40f, 0.28f) }, 2.5f);
		break;
	case Potion:
		Arc(0.50f, 0.62f, 0.24f, -55.0f, 235.0f, 3.0f);
		Line(0.43f, 0.20f, 0.43f, 0.42f, 3.0f);
		Line(0.57f, 0.20f, 0.57f, 0.42f, 3.0f);
		Line(0.39f, 0.18f, 0.61f, 0.18f, 4.0f);
		Line(0.32f, 0.62f, 0.68f, 0.62f, 2.0f);
		break;
	case LootBox:
		Poly({ FVector2D(0.20f, 0.42f), FVector2D(0.80f, 0.42f), FVector2D(0.80f, 0.80f), FVector2D(0.20f, 0.80f) }, 3.0f);
		Poly({ FVector2D(0.16f, 0.30f), FVector2D(0.84f, 0.30f), FVector2D(0.84f, 0.42f), FVector2D(0.16f, 0.42f) }, 3.0f);
		Line(0.50f, 0.30f, 0.50f, 0.80f, 2.0f);
		Line(0.50f, 0.10f, 0.50f, 0.20f, 2.0f);   // sparkle
		Line(0.30f, 0.14f, 0.36f, 0.22f, 2.0f);
		Line(0.70f, 0.14f, 0.64f, 0.22f, 2.0f);
		break;
	case Coin:
		Arc(0.50f, 0.50f, 0.30f, 0.0f, 360.0f, 3.0f);
		Arc(0.50f, 0.50f, 0.21f, 0.0f, 360.0f, 1.5f);
		Line(0.50f, 0.36f, 0.50f, 0.64f, 3.0f);
		break;
	case Bag:
	default:
		Poly({ FVector2D(0.24f, 0.36f), FVector2D(0.76f, 0.36f), FVector2D(0.82f, 0.82f), FVector2D(0.18f, 0.82f) }, 3.0f);
		Arc(0.50f, 0.36f, 0.14f, 180.0f, 360.0f, 3.0f);
		Line(0.24f, 0.52f, 0.76f, 0.52f, 2.0f);
		break;
	}
}
