// The loot box screen (O): every sealed box lined up in a row, hold to open.
//
// Modelled on a hero-shooter loot box screen (Zoe's request, 2026-11-12):
//   - All owned boxes sit in a row along the bottom, coloured by tier. The
//     selected one is bigger, bobs, and shakes harder the longer you hold.
//   - Hold Space (or hold the mouse on the selected box) to open it. Keep
//     holding and the next box starts charging as soon as the reveal lands,
//     so one long hold opens a whole stack.
//   - Hold R to open every box at once: one big burst and a grid of cards.
//   - The reveal: the box shakes, bursts in its tier colour, and the contents
//     fly out as cards, each coloured by its own rarity.
//
// The server does the opening (ADCCCharacterBase::Server_OpenLootBoxBatch):
// gear lands in the bag, gold in the purse, boons apply at once. This file
// only asks, waits, and shows what came back. It draws on the HUD Canvas;
// UDCCLootScreenInput only captures input while the screen is up.

#include "UI/DCCHUD.h"

#include "UI/DCCItemVisuals.h"
#include "UI/DCCLootScreenInput.h"
#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCLootBoxTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr double ChargeSeconds = 0.75;
	constexpr double ChargeAllSeconds = 1.2;
	constexpr double BurstAt = 0.45;
	constexpr double NextBoxDelay = 1.7;
	constexpr double BatchNextDelay = 2.8;
	constexpr double RequestTimeout = 4.0;
	constexpr int32 MaxBatchCards = 32;

	const FLinearColor Dim(0.62f, 0.60f, 0.66f);
	const FLinearColor Ink(0.94f, 0.91f, 0.86f);

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return 1.0f - FMath::Pow(1.0f - T, 3.0f);
	}

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}

	/** Everything the reveal needs to draw one card. */
	struct FRevealCard
	{
		FString Name;
		FString Subtitle;
		FString Detail;
		FLinearColor Colour = FLinearColor::White;
		uint8 Icon = DCCItemToast::Bag;
	};

	FRevealCard ResolveCard(const FDCCLootRevealEntry& Entry, EDCCLootBoxTier BoxTier, const UDCCEquipmentComponent* Equipment)
	{
		using namespace DCCItemToast;
		FRevealCard Card;

		switch (Entry.Kind)
		{
		case EDCCLootRevealKind::Gold:
			Card.Name = FString::Printf(TEXT("+%d gold"), Entry.Amount);
			Card.Subtitle = TEXT("Straight to your purse");
			Card.Colour = RarityColour(EDCCLootBoxTier::Gold);
			Card.Icon = Coin;
			return Card;

		case EDCCLootRevealKind::Boon:
			Card.Name = Entry.Name.ToString();
			Card.Subtitle = TEXT("Boon - already working");
			Card.Detail = Entry.Detail.ToString();
			Card.Colour = FLinearColor(0.50f, 0.95f, 0.85f);
			Card.Icon = Potion;
			return Card;

		default:
			break;
		}

		FDCCEquipmentDef Def;
		if (!Entry.ItemId.IsNone() && Equipment && Equipment->FindItem(Entry.ItemId, Def))
		{
			Card.Name = Def.DisplayName.IsEmpty() ? Entry.ItemId.ToString() : Def.DisplayName.ToString();
			Card.Subtitle = FString::Printf(TEXT("%s  -  %s"), *RarityName(Def.Rarity),
				*UEnum::GetDisplayValueAsText(Def.Slot).ToString());
			Card.Colour = RarityColour(Def.Rarity);
			Card.Icon = IconForSlot(Def.Slot);

			if (Def.WeaponDamageDie > 0)
			{
				Card.Detail = FString::Printf(TEXT("1d%d %s"), Def.WeaponDamageDie,
					*UEnum::GetDisplayValueAsText(Def.WeaponDamageType).ToString());
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
					Bonuses += FString::Printf(TEXT("%s%+d %s"), Bonuses.IsEmpty() ? TEXT("") : TEXT("  "), Pair.Key, Pair.Value);
				}
			}
			if (Def.EvadeBonus != 0)
			{
				Bonuses += FString::Printf(TEXT("%s%+d Evade"), Bonuses.IsEmpty() ? TEXT("") : TEXT("  "), Def.EvadeBonus);
			}
			if (!Bonuses.IsEmpty())
			{
				Card.Detail = Card.Detail.IsEmpty() ? Bonuses : Card.Detail + TEXT("   ") + Bonuses;
			}
			return Card;
		}

		Card.Name = Entry.Name.IsEmpty() ? Entry.ItemId.ToString() : Entry.Name.ToString();
		Card.Subtitle = TEXT("Item");
		Card.Detail = Entry.Detail.ToString();
		Card.Colour = RarityColour(BoxTier);
		Card.Icon = Bag;
		return Card;
	}
}

void ADCCHUD::ToggleLootBoxScreen()
{
	if (!PlayerOwner)
	{
		return;
	}

	if (bLootScreenOpen)
	{
		bLootScreenOpen = false;
		bLootHoldOne = false;
		bLootHoldAll = false;
		LootHoldStart = -1.0;
		LootHoldAllStart = -1.0;
		if (LootScreenInput)
		{
			LootScreenInput->RemoveFromParent();
		}
		ApplyCursorForOpenPanels();
		return;
	}

	if (!LootScreenInput)
	{
		LootScreenInput = CreateWidget<UDCCLootScreenInput>(PlayerOwner, UDCCLootScreenInput::StaticClass());
		if (!LootScreenInput)
		{
			return;
		}
		LootScreenInput->Hud = this;
	}

	const int32 TierCount = static_cast<int32>(EDCCLootBoxTier::MAX);
	LootInFlight.Init(0, TierCount);
	LootSeenCounts.Init(0, TierCount);
	if (const ADCCCharacterBase* Crawler = GetOwningCrawler())
	{
		if (const UDCCInventoryComponent* Inventory = Crawler->Inventory)
		{
			for (int32 Tier = 0; Tier < TierCount; ++Tier)
			{
				LootSeenCounts[Tier] = Inventory->GetLootBoxCount(static_cast<EDCCLootBoxTier>(Tier));
			}
		}
	}

	bLootScreenOpen = true;
	LootRequestSentAt = -1.0;
	LootNextChargeAt = 0.0;
	LootRevealStart = -1.0;
	LootScroll = static_cast<float>(LootSelected);

	LootScreenInput->AddToViewport(60);
	ApplyCursorForOpenPanels();
}

void ADCCHUD::BuildLootRow(TArray<EDCCLootBoxTier>& OutRow) const
{
	OutRow.Reset();
	const ADCCCharacterBase* Crawler = GetOwningCrawler();
	const UDCCInventoryComponent* Inventory = Crawler ? Crawler->Inventory.Get() : nullptr;
	if (!Inventory)
	{
		return;
	}

	// Cheapest first, so a stack builds towards its best box.
	for (uint8 TierIndex = 1; TierIndex < static_cast<uint8>(EDCCLootBoxTier::MAX); ++TierIndex)
	{
		const EDCCLootBoxTier Tier = static_cast<EDCCLootBoxTier>(TierIndex);
		const int32 InFlight = LootInFlight.IsValidIndex(TierIndex) ? LootInFlight[TierIndex] : 0;
		const int32 Count = FMath::Max(0, Inventory->GetLootBoxCount(Tier) - InFlight);
		for (int32 i = 0; i < Count; ++i)
		{
			OutRow.Add(Tier);
		}
	}
}

void ADCCHUD::SendLootOpen(EDCCLootBoxTier Tier, int32 Count, bool bAll)
{
	ADCCCharacterBase* Crawler = GetOwningCrawler();
	if (!Crawler || !Crawler->Inventory)
	{
		return;
	}

	// Taken out of the row now, so it does not sit there while the server answers.
	if (bAll)
	{
		for (int32 TierIndex = 1; TierIndex < LootInFlight.Num(); ++TierIndex)
		{
			LootInFlight[TierIndex] = Crawler->Inventory->GetLootBoxCount(static_cast<EDCCLootBoxTier>(TierIndex));
		}
	}
	else if (LootInFlight.IsValidIndex(static_cast<int32>(Tier)))
	{
		LootInFlight[static_cast<int32>(Tier)] += Count;
	}

	LootRequestSentAt = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	Crawler->RequestOpenLootBoxes(bAll ? EDCCLootBoxTier::None : Tier, Count);
}

void ADCCHUD::ReceiveLootBoxReveal(const FDCCLootBoxReveal& Reveal)
{
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	LootRequestSentAt = -1.0;

	if (Reveal.BoxCount <= 0)
	{
		for (int32& InFlight : LootInFlight)
		{
			InFlight = 0;
		}
		LootNotice = Reveal.Message.ToString();
		LootNoticeUntil = Now + 4.0;
		LootNextChargeAt = Now + 0.5;
		return;
	}

	LootReveal = Reveal;
	LootRevealStart = Now;
	LootNextChargeAt = Now + (Reveal.BoxCount > 1 ? BatchNextDelay : NextBoxDelay);
}

bool ADCCHUD::HandleLootScreenKey(const FKey& Key, bool bPressed)
{
	if (!bLootScreenOpen)
	{
		return false;
	}

	if (Key == EKeys::Escape || Key == EKeys::O || Key == EKeys::Tab)
	{
		if (bPressed)
		{
			ToggleLootBoxScreen();
		}
		return true;
	}

	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left)
	{
		if (bPressed)
		{
			LootSelected = FMath::Max(0, LootSelected - 1);
		}
		return true;
	}
	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right)
	{
		if (bPressed)
		{
			++LootSelected; // clamped against the row when drawn
		}
		return true;
	}

	if (Key == EKeys::LeftMouseButton && bPressed)
	{
		// Clicking another box selects it; holding on the selected one opens it.
		float MouseX = 0.0f, MouseY = 0.0f;
		if (PlayerOwner && PlayerOwner->GetMousePosition(MouseX, MouseY))
		{
			for (int32 i = 0; i < LootBoxRects.Num(); ++i)
			{
				if (LootBoxRects[i].bIsValid && LootBoxRects[i].IsInside(FVector2D(MouseX, MouseY)))
				{
					if (i != LootSelected)
					{
						LootSelected = i;
						return true;
					}
					break;
				}
			}
		}
		bLootHoldOne = true;
		return true;
	}

	if (Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::LeftMouseButton || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		bLootHoldOne = bPressed;
		if (!bPressed)
		{
			LootHoldStart = -1.0;
		}
		return true;
	}

	if (Key == EKeys::R || Key == EKeys::Gamepad_FaceButton_Top)
	{
		bLootHoldAll = bPressed;
		if (!bPressed)
		{
			LootHoldAllStart = -1.0;
		}
		return true;
	}

	return false;
}

void ADCCHUD::DrawGiftBox(float CentreX, float BaseY, float Size, const FLinearColor& Colour, float Alpha, float Glow)
{
	const float S = Size;
	const FLinearColor Body = WithAlpha(Colour * 0.62f, Alpha);
	const FLinearColor Lid = WithAlpha(Colour * 0.85f + FLinearColor::White * 0.1f, Alpha);
	const FLinearColor Ribbon = WithAlpha(FLinearColor(0.97f, 0.95f, 0.90f), 0.92f * Alpha);
	const FLinearColor Edge = WithAlpha(Colour * 0.35f, Alpha);

	if (Glow > 0.0f)
	{
		const float G = S * (0.25f + Glow * 0.35f);
		DrawRect(WithAlpha(Colour, 0.10f * Glow * Alpha), CentreX - S * 0.5f - G, BaseY - S * 0.95f - G, S + G * 2.0f, S * 0.95f + G * 2.0f);
		DrawRect(WithAlpha(Colour, 0.12f * Glow * Alpha), CentreX - S * 0.5f - G * 0.5f, BaseY - S * 0.95f - G * 0.5f, S + G, S * 0.95f + G);
	}

	// Shadow on the "floor".
	DrawRect(WithAlpha(FLinearColor::Black, 0.35f * Alpha), CentreX - S * 0.55f, BaseY + 2.0f, S * 1.1f, S * 0.06f);

	const float BodyTop = BaseY - S * 0.72f;
	const float LidTop = BaseY - S * 0.92f;
	DrawRect(Body, CentreX - S * 0.5f, BodyTop, S, S * 0.72f);
	DrawRect(Lid, CentreX - S * 0.56f, LidTop, S * 1.12f, S * 0.22f);
	DrawRect(Ribbon, CentreX - S * 0.07f, LidTop, S * 0.14f, S * 0.92f);
	DrawRect(Ribbon, CentreX - S * 0.5f, BodyTop + S * 0.24f, S, S * 0.10f);

	// Bow.
	const float BowY = LidTop;
	DrawLine(CentreX, BowY, CentreX - S * 0.22f, BowY - S * 0.16f, Ribbon, 3.0f);
	DrawLine(CentreX - S * 0.22f, BowY - S * 0.16f, CentreX - S * 0.18f, BowY, Ribbon, 3.0f);
	DrawLine(CentreX, BowY, CentreX + S * 0.22f, BowY - S * 0.16f, Ribbon, 3.0f);
	DrawLine(CentreX + S * 0.22f, BowY - S * 0.16f, CentreX + S * 0.18f, BowY, Ribbon, 3.0f);

	// Outline, so a dark tier still reads against the dark background.
	DrawLine(CentreX - S * 0.5f, BodyTop, CentreX - S * 0.5f, BaseY, Edge, 2.0f);
	DrawLine(CentreX + S * 0.5f, BodyTop, CentreX + S * 0.5f, BaseY, Edge, 2.0f);
	DrawLine(CentreX - S * 0.5f, BaseY, CentreX + S * 0.5f, BaseY, Edge, 2.0f);
}

void ADCCHUD::DrawLootReveal(double Now)
{
	using namespace DCCItemToast;

	const float T = static_cast<float>(Now - LootRevealStart);
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.40f;
	const FLinearColor TierColour = RarityColour(LootReveal.Tier);
	const bool bBatch = LootReveal.BoxCount > 1 || LootReveal.Entries.Num() > 6;

	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();

	// 1. The box, alone in the middle, shaking itself apart.
	if (T < BurstAt)
	{
		const float U = T / BurstAt;
		const float Shake = 4.0f + U * 14.0f;
		const float Jx = FMath::Sin(T * 71.0f) * Shake;
		const float Jy = FMath::Cos(T * 59.0f) * Shake * 0.6f;
		DrawGiftBox(CX + Jx, CY + 70.0f + Jy, 120.0f + U * 40.0f, TierColour, 1.0f, 0.4f + U * 0.6f);
		return;
	}

	// 2. The burst: a flash and rays in the box's colour.
	const float B = T - static_cast<float>(BurstAt);
	if (B < 0.9f)
	{
		const float Fade = 1.0f - B / 0.9f;
		DrawRect(WithAlpha(TierColour * 0.6f + FLinearColor::White * 0.4f, 0.28f * Fade * Fade), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

		const int32 Rays = 28;
		const float Inner = 40.0f + B * 220.0f;
		const float Outer = Inner + 60.0f + B * 360.0f;
		for (int32 i = 0; i < Rays; ++i)
		{
			const float A = (static_cast<float>(i) / Rays) * 2.0f * PI + B * 0.6f;
			const float C = FMath::Cos(A), S = FMath::Sin(A);
			DrawLine(CX + C * Inner, CY + S * Inner, CX + C * Outer, CY + S * Outer, WithAlpha(TierColour, 0.75f * Fade), 3.0f);
		}
	}

	// 3. The header.
	const FString Header = bBatch
		? FString::Printf(TEXT("%d BOXES OPENED"), LootReveal.BoxCount)
		: FString::Printf(TEXT("%s BOX"), *RarityName(LootReveal.Tier).ToUpper());
	{
		const float A = EaseOut(B / 0.3f);
		float W = 0.0f, H = 0.0f;
		GetTextSize(Header, W, H, Large, 1.4f);
		DrawText(Header, WithAlpha(TierColour * 0.7f + FLinearColor::White * 0.3f, A), CX - W * 0.5f, Canvas->SizeY * 0.13f, Large, 1.4f);
	}

	// 4. The cards fly out of the box into a row (or a grid, for a batch).
	const ADCCCharacterBase* Crawler = GetOwningCrawler();
	const UDCCEquipmentComponent* Equipment = Crawler ? Crawler->Equipment.Get() : nullptr;

	const int32 Total = LootReveal.Entries.Num();
	if (Total == 0)
	{
		const FString Empty = TEXT("Empty. The sponsor apologises for nothing.");
		float W = 0.0f, H = 0.0f;
		GetTextSize(Empty, W, H, Medium, 1.0f);
		DrawText(Empty, WithAlpha(Dim, EaseOut(B / 0.3f)), CX - W * 0.5f, CY, Medium, 1.0f);
		return;
	}

	const int32 Shown = bBatch ? FMath::Min(Total, MaxBatchCards) : Total;
	const int32 Columns = bBatch ? FMath::Min(Shown, 8) : Shown;
	const int32 Rows = FMath::DivideAndRoundUp(Shown, Columns);
	const float CardW = bBatch ? 170.0f : 210.0f;
	const float CardH = bBatch ? 74.0f : 250.0f;
	const float Gap = bBatch ? 10.0f : 22.0f;
	const float GridW = Columns * CardW + (Columns - 1) * Gap;
	const float GridH = Rows * CardH + (Rows - 1) * Gap;
	const float Left = CX - GridW * 0.5f;
	const float Top = bBatch ? FMath::Max(Canvas->SizeY * 0.22f, CY - GridH * 0.5f) : CY - CardH * 0.5f;
	const float Stagger = bBatch ? 0.035f : 0.14f;

	for (int32 i = 0; i < Shown; ++i)
	{
		const float U = EaseOut((B - i * Stagger) / 0.28f);
		if (U <= 0.0f)
		{
			continue;
		}

		const int32 Col = i % Columns;
		const int32 Row = i / Columns;
		const float TargetX = Left + Col * (CardW + Gap);
		const float TargetY = Top + Row * (CardH + Gap);
		const float X = FMath::Lerp(CX - CardW * 0.5f, TargetX, U);
		const float Y = FMath::Lerp(CY - CardH * 0.3f, TargetY, U);
		const float Alpha = U;

		const bool bMore = bBatch && i == Shown - 1 && Total > Shown;
		FRevealCard Card = bMore ? FRevealCard() : ResolveCard(LootReveal.Entries[i], LootReveal.Tier, Equipment);
		if (bMore)
		{
			Card.Name = FString::Printf(TEXT("+%d more"), Total - Shown + 1);
			Card.Subtitle = TEXT("All in your bag");
			Card.Colour = Dim;
		}
		const FLinearColor C = Card.Colour;

		// Frame: glow, dark card, a bright rarity band.
		DrawRect(WithAlpha(C, 0.18f * Alpha), X - 4.0f, Y - 4.0f, CardW + 8.0f, CardH + 8.0f);
		DrawRect(FLinearColor(0.035f, 0.03f, 0.06f, 0.95f * Alpha), X, Y, CardW, CardH);
		DrawRect(WithAlpha(C, 0.95f * Alpha), X, Y, CardW, bBatch ? 3.0f : 6.0f);
		DrawRect(WithAlpha(C, 0.5f * Alpha), X, Y + CardH - 1.0f, CardW, 1.0f);
		DrawRect(WithAlpha(C, 0.5f * Alpha), X, Y, 1.0f, CardH);
		DrawRect(WithAlpha(C, 0.5f * Alpha), X + CardW - 1.0f, Y, 1.0f, CardH);

		const FLinearColor NameColour = WithAlpha(C * 0.6f + FLinearColor::White * 0.4f, Alpha);

		if (bBatch)
		{
			const float IconSize = 44.0f;
			const float IconX = X + 10.0f, IconY = Y + (CardH - IconSize) * 0.5f;
			DrawRect(WithAlpha(C, 0.14f * Alpha), IconX, IconY, IconSize, IconSize);
			if (!bMore)
			{
				DrawItemIcon(Card.Icon, IconX, IconY, IconSize, C, Alpha);
			}
			const float TX = IconX + IconSize + 10.0f;
			DrawText(Card.Name, NameColour, TX, Y + 14.0f, Small, 1.0f);
			DrawText(Card.Subtitle, WithAlpha(Dim, Alpha), TX, Y + 34.0f, Small, 1.0f);
			continue;
		}

		const float IconSize = 96.0f;
		const float IconX = X + (CardW - IconSize) * 0.5f;
		const float IconY = Y + 26.0f;
		DrawRect(WithAlpha(C, 0.12f * Alpha), IconX, IconY, IconSize, IconSize);
		DrawItemIcon(Card.Icon, IconX, IconY, IconSize, C, Alpha);

		float TY = IconY + IconSize + 14.0f;
		auto Centred = [&](const FString& Line, const FLinearColor& Colour, UFont* Font)
		{
			if (Line.IsEmpty())
			{
				return;
			}
			float W = 0.0f, H = 0.0f;
			GetTextSize(Line, W, H, Font, 1.0f);
			// A long name drops to the small font rather than spilling off the card.
			UFont* Use = W > CardW - 16.0f && Font != Small ? Small : Font;
			if (Use != Font)
			{
				GetTextSize(Line, W, H, Use, 1.0f);
			}
			DrawText(Line, Colour, X + FMath::Max(8.0f, (CardW - W) * 0.5f), TY, Use, 1.0f);
			TY += H + 4.0f;
		};
		Centred(Card.Name, NameColour, Medium);
		Centred(Card.Subtitle, WithAlpha(Dim, Alpha), Small);
		Centred(Card.Detail, WithAlpha(Ink, Alpha), Small);
	}
}

void ADCCHUD::DrawLootBoxScreen()
{
	if (!bLootScreenOpen || !Canvas || !GetWorld())
	{
		return;
	}

	using namespace DCCItemToast;

	UWorld* World = GetWorld();
	const double Now = World->GetRealTimeSeconds();
	const float Dt = FMath::Clamp(World->DeltaRealTimeSeconds, 0.0f, 0.1f);

	ADCCCharacterBase* Crawler = GetOwningCrawler();
	const UDCCInventoryComponent* Inventory = Crawler ? Crawler->Inventory.Get() : nullptr;

	// Boxes the server has now taken off our count are no longer "in flight".
	if (Inventory)
	{
		for (int32 Tier = 0; Tier < LootSeenCounts.Num(); ++Tier)
		{
			const int32 Count = Inventory->GetLootBoxCount(static_cast<EDCCLootBoxTier>(Tier));
			if (Count < LootSeenCounts[Tier] && LootInFlight.IsValidIndex(Tier))
			{
				LootInFlight[Tier] = FMath::Max(0, LootInFlight[Tier] - (LootSeenCounts[Tier] - Count));
			}
			LootSeenCounts[Tier] = Count;
		}
	}

	int32 InFlightTotal = 0;
	for (const int32 InFlight : LootInFlight)
	{
		InFlightTotal += InFlight;
	}
	if (LootRequestSentAt > 0.0 && Now - LootRequestSentAt > RequestTimeout)
	{
		for (int32& InFlight : LootInFlight)
		{
			InFlight = 0;
		}
		InFlightTotal = 0;
		LootRequestSentAt = -1.0;
		LootNotice = TEXT("The sponsor did not answer. Try again.");
		LootNoticeUntil = Now + 3.0;
	}

	TArray<EDCCLootBoxTier> Row;
	BuildLootRow(Row);
	LootSelected = Row.IsEmpty() ? 0 : FMath::Clamp(LootSelected, 0, Row.Num() - 1);
	LootScroll = FMath::FInterpTo(LootScroll, static_cast<float>(LootSelected), Dt, 12.0f);

	// --- Holding to open ------------------------------------------------------
	const bool bBusy = InFlightTotal > 0 || LootRequestSentAt > 0.0 || Now < LootNextChargeAt;
	float ChargeOne = 0.0f, ChargeAll = 0.0f;

	if (bLootHoldAll && !bBusy && !Row.IsEmpty())
	{
		if (LootHoldAllStart < 0.0)
		{
			LootHoldAllStart = Now;
		}
		ChargeAll = static_cast<float>((Now - LootHoldAllStart) / ChargeAllSeconds);
		if (ChargeAll >= 1.0f)
		{
			LootHoldAllStart = -1.0;
			ChargeAll = 0.0f;
			SendLootOpen(EDCCLootBoxTier::None, Row.Num(), /*bAll=*/true);
		}
	}
	else if (!bLootHoldAll || bBusy)
	{
		LootHoldAllStart = -1.0;
	}

	if (bLootHoldOne && !bLootHoldAll && !bBusy && Row.IsValidIndex(LootSelected))
	{
		if (LootHoldStart < 0.0)
		{
			LootHoldStart = Now;
		}
		ChargeOne = static_cast<float>((Now - LootHoldStart) / ChargeSeconds);
		if (ChargeOne >= 1.0f)
		{
			LootHoldStart = -1.0;
			ChargeOne = 0.0f;
			SendLootOpen(Row[LootSelected], 1, /*bAll=*/false);
		}
	}
	else
	{
		// Reset while busy too, so a held key starts the next box from zero
		// once the reveal has had its moment. That is what makes one long
		// hold open a whole stack, one box at a time.
		LootHoldStart = -1.0;
	}

	// --- Backdrop and title --------------------------------------------------
	const float SW = Canvas->SizeX, SH = Canvas->SizeY;
	DrawRect(FLinearColor(0.015f, 0.012f, 0.03f, 0.92f), 0.0f, 0.0f, SW, SH);
	// A soft stage light behind the reveal area.
	for (int32 i = 0; i < 6; ++i)
	{
		const float Inset = i * SW * 0.05f;
		DrawRect(FLinearColor(0.20f, 0.16f, 0.30f, 0.025f), Inset, SH * 0.10f + i * 8.0f, SW - Inset * 2.0f, SH * 0.62f - i * 16.0f);
	}

	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();

	{
		const FString Title = TEXT("LOOT BOXES");
		float W = 0.0f, H = 0.0f;
		GetTextSize(Title, W, H, Large, 1.1f);
		DrawText(Title, FLinearColor(1.0f, 0.83f, 0.42f), 32.0f, 26.0f, Large, 1.1f);

		FString Summary = FString::Printf(TEXT("%d sealed"), Row.Num());
		for (uint8 TierIndex = 1; TierIndex < static_cast<uint8>(EDCCLootBoxTier::MAX); ++TierIndex)
		{
			int32 N = 0;
			for (const EDCCLootBoxTier Tier : Row)
			{
				N += Tier == static_cast<EDCCLootBoxTier>(TierIndex) ? 1 : 0;
			}
			if (N > 0)
			{
				Summary += FString::Printf(TEXT("   %s %d"), *RarityName(static_cast<EDCCLootBoxTier>(TierIndex)), N);
			}
		}
		DrawText(Summary, Dim, 34.0f, 30.0f + H, Small, 1.0f);
	}

	// --- The reveal ------------------------------------------------------------
	if (LootRevealStart > 0.0)
	{
		DrawLootReveal(Now);
	}
	else if (Row.IsEmpty())
	{
		const FString Empty = TEXT("No sealed boxes. Achievements, bosses and the occasional sponsor send them.");
		float W = 0.0f, H = 0.0f;
		GetTextSize(Empty, W, H, Medium, 1.0f);
		DrawText(Empty, Dim, (SW - W) * 0.5f, SH * 0.42f, Medium, 1.0f);
	}

	// --- The row of boxes ------------------------------------------------------
	const float BaseY = SH * 0.86f;
	const float Spacing = 132.0f;
	LootBoxRects.SetNum(Row.Num());

	for (int32 i = 0; i < Row.Num(); ++i)
	{
		LootBoxRects[i] = FBox2D(ForceInit);

		const float Offset = (i - LootScroll) * Spacing;
		if (FMath::Abs(Offset) > SW * 0.5f + Spacing)
		{
			continue;
		}

		const bool bSelected = i == LootSelected;
		const float Distance = FMath::Abs(Offset) / (SW * 0.55f);
		const float Alpha = FMath::Clamp(1.0f - Distance, 0.25f, 1.0f);
		float Size = bSelected ? 112.0f : 76.0f;
		float X = SW * 0.5f + Offset;
		float Y = BaseY;

		float Glow = bSelected ? 0.35f : 0.0f;
		if (bSelected)
		{
			Y += FMath::Sin(static_cast<float>(Now) * 3.0f) * 4.0f;
			const float Shake = ChargeOne * 7.0f;
			X += FMath::Sin(static_cast<float>(Now) * 63.0f) * Shake;
			Y += FMath::Cos(static_cast<float>(Now) * 51.0f) * Shake;
			Glow += ChargeOne * 0.65f;
		}
		if (ChargeAll > 0.0f)
		{
			const float Shake = ChargeAll * 5.0f;
			X += FMath::Sin(static_cast<float>(Now) * 57.0f + i) * Shake;
			Y += FMath::Cos(static_cast<float>(Now) * 49.0f + i) * Shake;
			Glow = FMath::Max(Glow, ChargeAll * 0.7f);
		}

		const FLinearColor Colour = RarityColour(Row[i]);
		DrawGiftBox(X, Y, Size, Colour, Alpha, Glow);
		LootBoxRects[i] = FBox2D(FVector2D(X - Size * 0.6f, Y - Size), FVector2D(X + Size * 0.6f, Y + 8.0f));

		if (bSelected)
		{
			const FString Label = FString::Printf(TEXT("%s BOX"), *RarityName(Row[i]).ToUpper());
			float W = 0.0f, H = 0.0f;
			GetTextSize(Label, W, H, Medium, 1.0f);
			DrawText(Label, Colour * 0.7f + FLinearColor::White * 0.3f, X - W * 0.5f, BaseY + 14.0f, Medium, 1.0f);

			// The charge bar fills while Space (or the mouse) is held.
			const float BarW = 150.0f;
			DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), X - BarW * 0.5f, BaseY + 16.0f + H, BarW, 6.0f);
			if (ChargeOne > 0.0f)
			{
				DrawRect(Colour, X - BarW * 0.5f, BaseY + 16.0f + H, BarW * FMath::Clamp(ChargeOne, 0.0f, 1.0f), 6.0f);
			}
		}
	}

	if (ChargeAll > 0.0f)
	{
		const float BarW = SW * 0.4f;
		const FString Label = TEXT("OPENING EVERYTHING");
		float W = 0.0f, H = 0.0f;
		GetTextSize(Label, W, H, Small, 1.0f);
		DrawText(Label, FLinearColor(1.0f, 0.83f, 0.42f), (SW - W) * 0.5f, BaseY - 150.0f - H, Small, 1.0f);
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), (SW - BarW) * 0.5f, BaseY - 146.0f, BarW, 8.0f);
		DrawRect(FLinearColor(1.0f, 0.83f, 0.42f), (SW - BarW) * 0.5f, BaseY - 146.0f, BarW * FMath::Clamp(ChargeAll, 0.0f, 1.0f), 8.0f);
	}

	// --- Notices and hints ------------------------------------------------------
	if (Now < LootNoticeUntil && !LootNotice.IsEmpty())
	{
		float W = 0.0f, H = 0.0f;
		GetTextSize(LootNotice, W, H, Medium, 1.0f);
		DrawText(LootNotice, FLinearColor(1.0f, 0.45f, 0.40f), (SW - W) * 0.5f, BaseY - 190.0f, Medium, 1.0f);
	}

	const FString Hints = Row.IsEmpty()
		? TEXT("O / Esc: close")
		: TEXT("Hold SPACE or hold the mouse on a box: open (keep holding for the next)   |   Hold R: open all   |   A / D: choose   |   O / Esc: close");
	float HW = 0.0f, HH = 0.0f;
	GetTextSize(Hints, HW, HH, Small, 1.0f);
	DrawText(Hints, Dim, (SW - HW) * 0.5f, SH - HH - 14.0f, Small, 1.0f);
}

// DCC.GiveBoxes [count]: every player gets [count] boxes of each tier (default 2).
// For testing the screen and for the DM handing out rewards at the table.
static void DCCGiveBoxesCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	const int32 Count = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 20) : 2;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
		if (Crawler && Crawler->Inventory)
		{
			for (uint8 TierIndex = 1; TierIndex < static_cast<uint8>(EDCCLootBoxTier::MAX); ++TierIndex)
			{
				Crawler->Inventory->GrantLootBox(static_cast<EDCCLootBoxTier>(TierIndex), Count);
			}
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GGiveBoxesCmd(
	TEXT("DCC.GiveBoxes"),
	TEXT("DCC.GiveBoxes [count]: give every crawler [count] loot boxes of each tier (default 2)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DCCGiveBoxesCommand));
