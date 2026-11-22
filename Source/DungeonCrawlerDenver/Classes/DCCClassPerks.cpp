#include "Classes/DCCClassPerks.h"

#include "Characters/DCCCharacterBase.h"
#include "Classes/DCCBasicClasses.h"
#include "Classes/DCCClassComponent.h"
#include "Skills/DCCSkillTypes.h"
#include "Stats/DCCStatsComponent.h"

namespace DCCClassPerks
{
	namespace
	{
		// Shorthand for the table below: P(name, blurb, melee, ranged, spell, heal, taken, mana, cooldown, radius, killheal, xp)
		FPerk P(const TCHAR* Name, const TCHAR* Blurb, int32 Melee, int32 Ranged, int32 Spell, int32 Heal, int32 Taken,
			int32 Mana, int32 Cooldown, int32 Radius, int32 KillHeal, int32 Xp)
		{
			return FPerk{ Name, Blurb, Melee, Ranged, Spell, Heal, Taken, Mana, Cooldown, Radius, KillHeal, Xp };
		}

		// [class][tier][option]. Class order is DCCBasicClasses::All().
		const TArray<TArray<TArray<FPerk>>>& Trees()
		{
			static const TArray<TArray<TArray<FPerk>>> Table = {
				// Repo Driver
				{
					{ P(TEXT("Aggressive Collections"), TEXT("You no longer knock. Your Yelp rating has entered a death spiral."), 15, 0, 0, -10, 0, 0, 0, 0, 0, 0),
					  P(TEXT("Courtesy Call"), TEXT("Please disregard this beating if payment has already been sent."), 0, 0, 0, 0, 8, 0, 0, 0, 3, 0) },
					{ P(TEXT("Lien on Life"), TEXT("Every kill is collateral. The System admires your accounting."), 5, 0, 0, 0, 0, 0, 0, 0, 6, 0),
					  P(TEXT("Tow Zone"), TEXT("Unattended enemies will be towed at the owner's expense."), 0, 0, 0, 0, 0, 0, -10, 25, 0, 0) },
					{ P(TEXT("Repo Man Energy"), TEXT("You have not slept since the Bush administration. It shows. It helps."), 20, 0, 0, 0, -5, 0, 0, 0, 0, 0),
					  P(TEXT("Paperwork Shield"), TEXT("Nobody hits a man holding a clipboard. Legally, it is a weapon."), 0, 0, 0, 0, 12, -15, 0, 0, 0, 0) },
					{ P(TEXT("Self-Help Repossession"), TEXT("The courts call it a gray area. The audience calls it content."), 0, 0, 25, 0, 0, -20, 0, 0, 0, 0),
					  P(TEXT("The Hook Remembers"), TEXT("Every car you ever took is watching from the yard. They are proud of you."), 15, 0, 0, 0, 0, 0, 0, 0, 0, 15) },
				},
				// Ridge Hand
				{
					{ P(TEXT("Switchback Calves"), TEXT("Your calves are now load-bearing. Please do not lean on them."), 0, 0, 0, 0, 8, 0, -5, 0, 0, 0),
					  P(TEXT("Summit Selfie"), TEXT("Sponsors love a view. They do not love you, but they love a view."), 0, 0, 0, 10, 0, 0, 0, 0, 0, 15) },
					{ P(TEXT("Altitude Sickness (Theirs)"), TEXT("At this elevation, enemies forget how to breathe. You remember."), 12, 0, 0, 0, 0, 0, 0, 15, 0, 0),
					  P(TEXT("Bear Spray Sommelier"), TEXT("Notes of capsaicin, pine, and regret."), 0, 0, 15, 0, 0, 0, 0, 0, 3, 0) },
					{ P(TEXT("Fourteen Thousand Feet of Spite"), TEXT("You did not climb this far to be slapped by a mid-level goblin."), -5, 0, 0, 0, 10, 0, 0, 0, 0, 0),
					  P(TEXT("Trail Magic"), TEXT("Someone left Gatorade in a cooler. It was you. You are the magic."), 0, 0, 0, 25, 0, -15, 0, 0, 0, 0) },
					{ P(TEXT("Avalanche Insurance"), TEXT("The premium is high. The payout is everyone standing near you."), 0, 0, 15, 0, 0, 0, 0, 30, 0, 0),
					  P(TEXT("Cairn Builder"), TEXT("You stack their bones into little trail markers. The System finds this wholesome."), 0, 0, 0, 0, 0, 0, 0, 0, 8, 10) },
				},
				// Call Sign
				{
					{ P(TEXT("Shock Jock"), TEXT("You say things on air that cannot be unsaid. Neither can the damage."), 0, 0, 15, -10, 0, 0, 0, 0, 0, 0),
					  P(TEXT("Smooth Late-Night Voice"), TEXT("Your spells cost less. The mana finds you soothing."), 0, 0, 0, 0, 0, -20, -5, 0, 0, 0) },
					{ P(TEXT("Dead Air Is Death"), TEXT("Silence is unacceptable. Something must always be on fire."), 0, 0, 0, 0, 0, 0, -15, 0, 0, 0),
					  P(TEXT("Caller Number Nine"), TEXT("You are the ninth caller. You win nothing. You win everything."), 0, 0, 5, 0, 0, 0, 0, 0, 0, 20) },
					{ P(TEXT("Feedback Shrieker"), TEXT("Point the mic at the speakers. Point the speakers at the problem."), 0, 0, 10, 0, -5, 0, 0, 25, 0, 0),
					  P(TEXT("Public Broadcasting"), TEXT("Made possible by viewers like you. Viewers like you are bleeding."), 0, 0, 0, 20, 0, -15, 0, 0, 0, 0) },
					{ P(TEXT("Emergency Alert System"), TEXT("THIS IS NOT A TEST. It is a test. You are failing it loudly and on purpose."), 0, 0, 30, 0, 0, 15, 0, 0, 0, 0),
					  P(TEXT("Syndicated"), TEXT("Your show airs in four markets now. So do your spells."), 0, 0, 0, 0, 0, 0, -20, 10, 0, 0) },
				},
				// Line Coach
				{
					{ P(TEXT("Participation Trophy"), TEXT("Everyone gets one. Everyone is alive to get one. Mostly."), 0, 0, 0, 15, 0, 0, 0, 0, 2, 0),
					  P(TEXT("Tough Love"), TEXT("Walk it off. No, really, walk it off, there is a thing behind you."), 12, 0, 0, -5, 0, 0, 0, 0, 0, 0) },
					{ P(TEXT("Halftime Oranges"), TEXT("Sliced with love. Sliced with a knife you found on a corpse."), 0, 0, 0, 10, 0, -20, 0, 0, 0, 0),
					  P(TEXT("Run It Back"), TEXT("Again! From the top! With feeling! With less dying!"), 0, 0, 0, 0, 0, 0, -15, 0, 0, 0) },
					{ P(TEXT("Team Mom"), TEXT("The System has flagged you for excessive snacks. It is not going to do anything about it."), 0, 0, 0, 25, 0, 0, 0, 20, 0, 0),
					  P(TEXT("Clipboard of Shame"), TEXT("You write their names down. Somewhere, they feel it."), 0, 0, 20, 0, 5, 0, 0, 0, 0, 0) },
					{ P(TEXT("Gatorade Bath"), TEXT("Expensive, sticky, and somehow medically effective."), 0, 0, 0, 30, 0, 10, 0, 0, 0, 0),
					  P(TEXT("Bench the Starter"), TEXT("Someone has to sit out. It is the monster. The monster sits out forever."), 0, 0, 0, 0, 0, 0, 0, 0, 5, 20) },
				},
				// Mile Runner
				{
					{ P(TEXT("Carb Loaded"), TEXT("You ate four bowls of pasta. You are a projectile now."), 0, 15, 0, 0, -5, 0, 0, 0, 0, 0),
					  P(TEXT("Negative Split"), TEXT("Every attack faster than the last. Your coach is weeping."), 0, 5, 0, 0, 0, 0, -10, 0, 0, 0) },
					{ P(TEXT("Runner's High"), TEXT("The endorphins are real. The hallucinations are a free bonus."), 0, 0, 0, 0, 0, 0, 0, 0, 5, 10),
					  P(TEXT("Chafe Resistant"), TEXT("You applied the cream. You applied all of it. You are untouchable."), 0, 0, 0, 0, 10, 0, 0, 0, 0, 0) },
					{ P(TEXT("Pace Car"), TEXT("Everyone behind you goes faster. Everyone in front of you goes home in a bag."), 0, 0, 20, 0, 0, 0, 0, 15, 0, 0),
					  P(TEXT("Personal Best"), TEXT("Your running app notified twelve people. None of them care. The audience does."), 0, 20, 0, 0, 0, -10, 0, 0, 0, 0) },
					{ P(TEXT("Mile High Lungs"), TEXT("Denver altitude training. Your lungs are basically wings at this point."), 0, 15, 15, 0, 0, 0, 0, 0, 0, 0),
					  P(TEXT("Finish Line Fireworks"), TEXT("You cross the line. The line explodes. The sponsors are thrilled."), 0, 0, 0, 0, 0, 0, -10, 30, 0, 0) },
				},
				// Gate Warden
				{
					{ P(TEXT("No Shirt, No Shoes"), TEXT("No service. No mercy. No refunds."), 12, 0, 0, 0, 4, 0, 0, 0, 0, 0),
					  P(TEXT("Velvet Rope"), TEXT("You are not on the list. Nobody is on the list. There is no list."), 0, 0, 0, 0, 10, 0, 0, 0, 0, 0) },
					{ P(TEXT("Bouncer's Shove"), TEXT("You are not hitting them. You are relocating them, firmly."), 8, 0, 0, 0, 0, 0, 0, 20, 0, 0),
					  P(TEXT("Stamp of Approval"), TEXT("Your hand stamp glows under black light and closes small wounds."), 0, 0, 0, 10, 0, 0, 0, 0, 5, 0) },
					{ P(TEXT("Last Call"), TEXT("Everyone out. Everyone. You too, the thing made of teeth."), 8, 0, 0, 0, 0, 0, -15, 0, 0, 0),
					  P(TEXT("Fire Code Capacity"), TEXT("This room holds forty. You will see to that personally."), 0, 0, 0, 0, 10, 0, 0, 0, 0, 10) },
					{ P(TEXT("Lifetime Ban"), TEXT("Your name goes on the wall. Theirs goes on the floor."), 25, 0, 0, -10, 0, 0, 0, 0, 0, 0),
					  P(TEXT("Gatekeeper's Apprentice"), TEXT("The real Gatekeeper sent you a fruit basket. It is ticking."), 0, 0, 20, 0, 0, 0, 0, 20, 0, 0) },
				},
				// Floor Walker
				{
					{ P(TEXT("Employee of the Month"), TEXT("Your photo is on the wall. Someone drew horns on it. Morale is high."), 6, 6, 6, 0, 0, 0, 0, 0, 0, 0),
					  P(TEXT("Clearance Rack"), TEXT("Every spell, twenty percent off. Final sale. No returns."), 0, 0, 0, 0, 0, -20, 0, 0, 0, 0) },
					{ P(TEXT("Price Check"), TEXT("Everything has a value. Mostly the monsters. Mostly to you."), 0, 0, 0, 0, 0, 0, 0, 0, 0, 20),
					  P(TEXT("Wet Floor Sign"), TEXT("Caution. You are the slippery one now."), 0, 0, 0, 0, 8, 0, 0, 0, 3, 0) },
					{ P(TEXT("Loss Prevention"), TEXT("Shrinkage is down twelve percent. The shrinkage was the monsters."), 8, 0, 0, 0, 0, 0, 0, 0, 6, 0),
					  P(TEXT("Blue Light Special"), TEXT("Attention shoppers. Aisle five. Run."), 0, 0, 10, 0, 0, 0, -15, 0, 0, 0) },
					{ P(TEXT("Regional Manager"), TEXT("You manage four stores and a dungeon. You are tired in a way that helps."), 10, 10, 10, 10, 0, 0, 0, 0, 0, 0),
					  P(TEXT("Going Out of Business"), TEXT("Everything must go. That includes you, eventually. Not today."), 0, 0, 30, 0, -10, 0, 0, 0, 0, 0) },
				},
			};
			return Table;
		}

		const TArray<FName>* PickedIds(const ADCCCharacterBase* Crawler)
		{
			const UDCCClassComponent* Comp = Crawler ? Crawler->FindComponentByClass<UDCCClassComponent>() : nullptr;
			return Comp ? &Comp->Perks : nullptr;
		}
	}

	int32 TierLevel(int32 Tier)
	{
		return 4 * (Tier + 1);
	}

	const FPerk* Get(int32 ClassIndex, int32 Tier, int32 Option)
	{
		const auto& T = Trees();
		if (!T.IsValidIndex(ClassIndex) || !T[ClassIndex].IsValidIndex(Tier) || !T[ClassIndex][Tier].IsValidIndex(Option))
		{
			return nullptr;
		}
		return &T[ClassIndex][Tier][Option];
	}

	FName IdFor(int32 ClassIndex, int32 Tier, int32 Option)
	{
		return FName(*FString::Printf(TEXT("C%dT%dO%d"), ClassIndex, Tier, Option));
	}

	int32 Picked(const ADCCCharacterBase* Crawler, int32 Tier)
	{
		const int32 ClassIndex = DCCBasicClasses::IndexOf(Crawler);
		const TArray<FName>* Ids = PickedIds(Crawler);
		if (ClassIndex < 0 || !Ids)
		{
			return -1;
		}
		for (int32 Option = 0; Option < 2; ++Option)
		{
			if (Ids->Contains(IdFor(ClassIndex, Tier, Option)))
			{
				return Option;
			}
		}
		return -1;
	}

	int32 Unpicked(const ADCCCharacterBase* Crawler)
	{
		if (!Crawler || !Crawler->Stats || DCCBasicClasses::IndexOf(Crawler) < 0)
		{
			return 0;
		}
		int32 Count = 0;
		for (int32 Tier = 0; Tier < TierCount; ++Tier)
		{
			Count += Crawler->Stats->Level >= TierLevel(Tier) && Picked(Crawler, Tier) < 0 ? 1 : 0;
		}
		return Count;
	}

	FString EffectLine(const FPerk& Perk)
	{
		TArray<FString> Parts;
		auto Pct = [&Parts](int32 V, const TCHAR* What) { if (V != 0) { Parts.Add(FString::Printf(TEXT("%s %s%d%%"), What, V > 0 ? TEXT("+") : TEXT(""), V)); } };
		Pct(Perk.Melee, TEXT("Melee"));
		Pct(Perk.Ranged, TEXT("Ranged"));
		Pct(Perk.Spell, TEXT("Spells"));
		Pct(Perk.Heal, TEXT("Heals"));
		if (Perk.Taken != 0) { Parts.Add(FString::Printf(TEXT("Hits taken %s%d%%"), Perk.Taken > 0 ? TEXT("-") : TEXT("+"), FMath::Abs(Perk.Taken))); }
		Pct(Perk.Mana, TEXT("Mana cost"));
		Pct(Perk.Cooldown, TEXT("Cooldowns"));
		Pct(Perk.Radius, TEXT("Spell area"));
		if (Perk.KillHeal != 0) { Parts.Add(FString::Printf(TEXT("+%d HP per kill"), Perk.KillHeal)); }
		Pct(Perk.Xp, TEXT("XP"));
		return FString::Join(Parts, TEXT("   "));
	}

	int32 Sum(const ADCCCharacterBase* Crawler, int32 FPerk::* Field)
	{
		const int32 ClassIndex = DCCBasicClasses::IndexOf(Crawler);
		if (ClassIndex < 0 || !Crawler->Stats)
		{
			return 0;
		}
		int32 Total = 0;
		for (int32 Tier = 0; Tier < TierCount; ++Tier)
		{
			// A pick only counts while the crawler is at that tier's level (it always is, unless levels are ever lost).
			const int32 Option = Picked(Crawler, Tier);
			if (Option >= 0 && Crawler->Stats->Level >= TierLevel(Tier))
			{
				if (const FPerk* Perk = Get(ClassIndex, Tier, Option))
				{
					Total += Perk->*Field;
				}
			}
		}
		return Total;
	}

	void AdjustSkill(const ADCCCharacterBase* Crawler, FDCCSkillDef& Skill)
	{
		if (!Crawler || !Crawler->IsPlayerControlled())
		{
			return;
		}
		const int32 Mana = Sum(Crawler, &FPerk::Mana);
		const int32 Cooldown = Sum(Crawler, &FPerk::Cooldown);
		const int32 Radius = Sum(Crawler, &FPerk::Radius);
		if (Mana != 0 && Skill.ManaCost > 0)
		{
			Skill.ManaCost = FMath::Max(1, FMath::RoundToInt(Skill.ManaCost * (100 + Mana) / 100.0f));
		}
		if (Cooldown != 0)
		{
			Skill.CooldownSeconds = FMath::Max(0.5f, Skill.CooldownSeconds * (100 + Cooldown) / 100.0f);
		}
		if (Radius != 0)
		{
			Skill.Radius *= (100 + Radius) / 100.0f;
		}
	}
}
