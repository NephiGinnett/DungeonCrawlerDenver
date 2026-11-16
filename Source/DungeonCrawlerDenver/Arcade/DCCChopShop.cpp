#include "Arcade/DCCChopShop.h"

#include "Session/DCCProfile.h"

namespace DCCChopShop
{
	const FName Plating(TEXT("Plating"));
	const FName Tank(TEXT("Tank"));
	const FName Engine(TEXT("Engine"));
	const FName Line(TEXT("Line"));
	const FName PitCrew(TEXT("PitCrew"));
	const FName AirTank(TEXT("AirTank"));
	const FName MineLamp(TEXT("MineLamp"));
	const FName Lightsticks(TEXT("Lightsticks"));
	const FName HardHat(TEXT("HardHat"));
	const FName Harness(TEXT("Harness"));
	const FName GasMeter(TEXT("GasMeter"));
	const FName TapeReel(TEXT("TapeReel"));

	const TArray<FUpgrade>& All()
	{
		static const TArray<FUpgrade> List = {
			{ Plating, TEXT("Skid Plating"), TEXT("Welded plate over the soft parts. Crashes, owners, bullets and rams take less hull. A flying rover shakes a hit off sooner too."),
				TEXT("-20% damage"), 3, 40, FLinearColor(0.75f, 0.8f, 0.9f) },
			{ Engine, TEXT("Bored-Out Engine"), TEXT("More truck per pedal, and more rover when you are in the air. You will feel the next level."),
				TEXT("+15% speed"), 3, 60, FLinearColor(1.0f, 0.45f, 0.3f) },
			{ Tank, TEXT("Long-Range Tank"), TEXT("A bigger tank where the spare used to be. It holds 30 more, the shift drinks slower, and flight lasts longer."),
				TEXT("-20% burn, +30 tank"), 3, 35, FLinearColor(1.0f, 0.8f, 0.3f) },
			{ Line, TEXT("Long Line"), TEXT("More cable on the drum. The magnet reaches further."),
				TEXT("+25% hook range"), 3, 45, FLinearColor(0.3f, 0.9f, 1.0f) },
			{ PitCrew, TEXT("Pit Crew on Retainer"), TEXT("Somebody is already holding the hose. Stations fuel and patch you faster."),
				TEXT("+50% pit speed"), 2, 50, FLinearColor(0.45f, 1.0f, 0.55f) },
			// ---- Mine Outfitters (Dead Air, credits) ----------------------------------------------------------
			{ AirTank, TEXT("Air Tanks"), TEXT("The deep stopes have no air worth the name. Without tanks you get about half way down Mount Fourteener's workings. Each level takes you deeper; three reach the thing in the deepest stope."),
				TEXT("+20% depth"), 3, 30, FLinearColor(0.4f, 0.8f, 1.0f), true },
			{ MineLamp, TEXT("Better Lamp"), TEXT("From the carbide lamp you were issued, to a brighter carbide, to an electric cap lamp, to an arc lamp that throws light down a whole drift."),
				TEXT("brighter, further, wider"), 3, 25, FLinearColor(1.0f, 0.85f, 0.5f), true },
			{ Lightsticks, TEXT("Lightsticks"), TEXT("G throws one. It glows green where it lands, for the rest of the round, and nothing hides in its light: a ghost near one stays in frame."),
				TEXT("+2 a round"), 3, 20, FLinearColor(0.45f, 1.0f, 0.5f), true },
			{ HardHat, TEXT("Hard Hat"), TEXT("A real one, not the one the stream sent. Falling rock, runaway carts and powder take less out of you."),
				TEXT("-30% from rock, carts, blasts"), 2, 30, FLinearColor(1.0f, 0.75f, 0.2f), true },
			{ Harness, TEXT("Rope & Harness"), TEXT("Clipped to the old timbers. When rotten planks give way, the rope catches you instead of the sump."),
				TEXT("no more falls"), 1, 45, FLinearColor(0.9f, 0.6f, 0.4f), true },
			{ GasMeter, TEXT("Gas Meter"), TEXT("Level 1 reads firedamp and bad air before your lamp turns blue. Level 2 is a better meter and a damp rag: flashes and bad air hurt half as much."),
				TEXT("warns, then protects"), 2, 30, FLinearColor(0.5f, 0.75f, 1.0f), true },
			{ TapeReel, TEXT("Spare Reels"), TEXT("Thirty more seconds of tape a level. The deep stopes are where the footage is."),
				TEXT("+30 s tape"), 3, 20, FLinearColor(1.0f, 0.45f, 0.6f), true },
		};
		return List;
	}

	const FUpgrade* Find(FName Id)
	{
		return All().FindByPredicate([Id](const FUpgrade& U) { return U.Id == Id; });
	}

	int32 PriceFor(const FUpgrade& Upgrade, int32 Level)
	{
		return Level >= Upgrade.MaxLevel ? 0 : Upgrade.BasePrice * (Level + 1);
	}

	int32 GetScrap(const FString& Key)
	{
		return FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Chop"), TEXT("Scrap"), 0.0f));
	}

	int32 AddScrap(const FString& Key, int32 Delta)
	{
		const int32 Balance = FMath::Max(0, GetScrap(Key) + Delta);
		DCCProfile::SetForKey(Key, TEXT("Chop"), TEXT("Scrap"), float(Balance));
		return Balance;
	}

	int32 GetLevel(const FString& Key, FName UpgradeId)
	{
		const FUpgrade* Upgrade = Find(UpgradeId);
		return Upgrade ? FMath::Clamp(FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Chop"), UpgradeId, 0.0f)), 0, Upgrade->MaxLevel) : 0;
	}

	int32 GetCredits(const FString& Key)
	{
		return FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Mine"), TEXT("Credits"), 0.0f));
	}

	int32 AddCredits(const FString& Key, int32 Delta)
	{
		const int32 Balance = FMath::Max(0, GetCredits(Key) + Delta);
		DCCProfile::SetForKey(Key, TEXT("Mine"), TEXT("Credits"), float(Balance));
		return Balance;
	}

	bool BuyNext(const FString& Key, FName UpgradeId, FString& OutMessage)
	{
		const FUpgrade* Upgrade = Find(UpgradeId);
		if (!Upgrade || Key.IsEmpty())
		{
			return false;
		}
		const int32 Level = GetLevel(Key, UpgradeId);
		if (Level >= Upgrade->MaxLevel)
		{
			OutMessage = FString::Printf(TEXT("%s is as far as it goes."), Upgrade->Name);
			return false;
		}
		const int32 Price = PriceFor(*Upgrade, Level);
		if (Upgrade->bMine)
		{
			const int32 Credits = GetCredits(Key);
			if (Credits < Price)
			{
				OutMessage = FString::Printf(TEXT("%s level %d costs %d credits. You have %d. Dead Air pays credits for footage, notes and stories."), Upgrade->Name, Level + 1, Price, Credits);
				return false;
			}
			AddCredits(Key, -Price);
			DCCProfile::SetForKey(Key, TEXT("Chop"), UpgradeId, float(Level + 1));
			OutMessage = FString::Printf(TEXT("%s is now level %d. It goes down the shaft with you next round."), Upgrade->Name, Level + 1);
			return true;
		}
		const int32 Scrap = GetScrap(Key);
		if (Scrap < Price)
		{
			OutMessage = FString::Printf(TEXT("%s level %d costs %d scrap. You have %d. Repo Rally and Neon Mile pay scrap."), Upgrade->Name, Level + 1, Price, Scrap);
			return false;
		}
		AddScrap(Key, -Price);
		DCCProfile::SetForKey(Key, TEXT("Chop"), UpgradeId, float(Level + 1));
		OutMessage = FString::Printf(TEXT("%s is now level %d. On the truck next shift, and on the rover the next time you fly."), Upgrade->Name, Level + 1);
		return true;
	}
}
