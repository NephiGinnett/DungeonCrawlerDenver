#include "Skills/DCCSpellBook.h"

#include "NiagaraSystem.h"

namespace DCCSpellBook
{
	namespace
	{
		const TCHAR* Fire = TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire.NS_fire");
		const TCHAR* Sparks = TEXT("/Game/Legendary_Forge/Fx/Sparks/NS_sparks.NS_sparks");
		const TCHAR* Smoke = TEXT("/Game/Legendary_Forge/Fx/Smoke/NS_smoke.NS_smoke");
		const TCHAR* Snow = TEXT("/Game/Ice_Station/FX/NS_snow.NS_snow");
		const TCHAR* Motes = TEXT("/Game/Sun_Temple_Kit/FX/NS_light_particules_01.NS_light_particules_01");
		const TCHAR* Hit = TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage");

		FDCCSkillDef Spell(const TCHAR* Id, const TCHAR* Name, const TCHAR* Flavor, EDCCSkillArchetype Archetype,
			EDCCSkillTargeting Targeting, EDCCAbility Ability, int32 Mana, float Cooldown, int32 Die,
			float RangeOrRadius, float ConeHalf, const TCHAR* CastFx, const TCHAR* ImpactFx)
		{
			FDCCSkillDef S;
			S.SkillId = Id;
			S.DisplayName = FText::FromString(Name);
			S.FlavorText = FText::FromString(Flavor);
			S.Archetype = Archetype;
			S.Targeting = Targeting;
			S.CastingAbility = Ability;
			S.ManaCost = Mana;
			S.CooldownSeconds = Cooldown;
			S.PowerDie = Die;
			S.Range = Targeting == EDCCSkillTargeting::SingleTarget ? RangeOrRadius : 600.0f;
			S.Radius = Targeting == EDCCSkillTargeting::SingleTarget ? 300.0f : RangeOrRadius;
			S.ConeHalfAngleDegrees = ConeHalf;
			S.DifficultyClass = 0;          // a spell you were taught simply works once paid for
			S.bCanAttemptUntrained = false; // and nobody else can cast it
			if (CastFx) { S.CastEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(CastFx)); }
			if (ImpactFx) { S.ImpactEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(ImpactFx)); }
			return S;
		}
	}

	const TArray<FDCCSkillDef>& All()
	{
		using A = EDCCSkillArchetype;
		using T = EDCCSkillTargeting;
		using S = EDCCAbility;
		static const TArray<FDCCSkillDef> Book = {
			// Repo Driver
			Spell(TEXT("FinalNotice"), TEXT("Final Notice"), TEXT("Delivered in person. Signature not required."),
				A::Damage, T::SingleTarget, S::Strength, 3, 9.0f, 12, 700.0f, 45.0f, nullptr, Hit),
			Spell(TEXT("HostileTakeover"), TEXT("Hostile Takeover"), TEXT("Everything in front of you is under new management."),
				A::Damage, T::Cone, S::Strength, 3, 10.0f, 10, 480.0f, 55.0f, Smoke, Hit),
			Spell(TEXT("TotalRepossession"), TEXT("Total Repossession"), TEXT("The whole room. Every last thing in it."),
				A::Damage, T::AreaOfEffect, S::Strength, 6, 24.0f, 16, 650.0f, 45.0f, Smoke, Hit),
			// Shared
			Spell(TEXT("SecondWind"), TEXT("Second Wind"), TEXT("Not done. Not even close. Probably."),
				A::Heal, T::Self, S::Constitution, 2, 16.0f, 12, 0.0f, 45.0f, Motes, nullptr),
			Spell(TEXT("HoldTheLine"), TEXT("Hold the Line"), TEXT("Plant your feet. Everything near you regrets it."),
				A::Damage, T::AreaOfEffect, S::Strength, 3, 11.0f, 10, 400.0f, 45.0f, Smoke, Hit),
			// Ridge Hand
			Spell(TEXT("AvalancheWarning"), TEXT("Avalanche Warning"), TEXT("The warning was the courtesy."),
				A::Damage, T::AreaOfEffect, S::Constitution, 3, 12.0f, 10, 460.0f, 45.0f, Snow, Hit),
			Spell(TEXT("Whiteout"), TEXT("Whiteout"), TEXT("Visibility zero. Damage, considerable."),
				A::Damage, T::AreaOfEffect, S::Constitution, 6, 24.0f, 14, 760.0f, 45.0f, Snow, Hit),
			// Call Sign
			Spell(TEXT("FeedbackLoop"), TEXT("Feedback Loop"), TEXT("Point the mic at them. Turn it up."),
				A::Damage, T::SingleTarget, S::Intelligence, 3, 7.0f, 12, 1100.0f, 45.0f, Sparks, Sparks),
			Spell(TEXT("RadioSilence"), TEXT("Radio Silence"), TEXT("Every channel at once, then nothing."),
				A::Damage, T::AreaOfEffect, S::Intelligence, 4, 12.0f, 10, 540.0f, 45.0f, Sparks, Sparks),
			Spell(TEXT("Interference"), TEXT("Interference"), TEXT("A wall of static, aimed."),
				A::Damage, T::Cone, S::Intelligence, 3, 9.0f, 10, 820.0f, 38.0f, Sparks, Sparks),
			Spell(TEXT("StationIdentification"), TEXT("Station Identification"), TEXT("You are listening to the last thing they hear."),
				A::Damage, T::AreaOfEffect, S::Intelligence, 6, 22.0f, 18, 760.0f, 45.0f, Sparks, Sparks),
			// Line Coach
			Spell(TEXT("Timeout"), TEXT("Timeout"), TEXT("Everybody in. Breathe. Again."),
				A::Heal, T::AreaOfEffect, S::Charisma, 3, 16.0f, 8, 650.0f, 45.0f, Motes, Motes),
			Spell(TEXT("PenaltyFlag"), TEXT("Penalty Flag"), TEXT("Thrown hard enough to count as a tackle."),
				A::Damage, T::SingleTarget, S::Charisma, 2, 8.0f, 10, 900.0f, 45.0f, nullptr, Hit),
			Spell(TEXT("TwoMinuteDrill"), TEXT("Two-Minute Drill"), TEXT("Fast hands, fast heals, no huddle."),
				A::Heal, T::AreaOfEffect, S::Charisma, 4, 18.0f, 10, 900.0f, 45.0f, Motes, Motes),
			Spell(TEXT("HalftimeSpeech"), TEXT("Halftime Speech"), TEXT("Nobody remembers the words. Everybody stands up."),
				A::Heal, T::AreaOfEffect, S::Charisma, 6, 28.0f, 16, 1000.0f, 45.0f, Motes, Motes),
			// Mile Runner
			Spell(TEXT("HotLap"), TEXT("Hot Lap"), TEXT("A line of fire down the racing line."),
				A::Damage, T::Cone, S::Dexterity, 2, 6.0f, 10, 1200.0f, 22.0f, Fire, Fire),
			Spell(TEXT("PhotoFinish"), TEXT("Photo Finish"), TEXT("By a nose. Theirs, mostly."),
				A::Damage, T::SingleTarget, S::Dexterity, 3, 9.0f, 12, 1500.0f, 45.0f, nullptr, Hit),
			Spell(TEXT("Slipstream"), TEXT("Slipstream"), TEXT("Whatever is behind you is in front of them now."),
				A::Damage, T::Cone, S::Dexterity, 3, 10.0f, 12, 1400.0f, 28.0f, Smoke, Hit),
			Spell(TEXT("VictoryLap"), TEXT("Victory Lap"), TEXT("One full circle. Nobody standing inside it."),
				A::Damage, T::AreaOfEffect, S::Dexterity, 6, 22.0f, 16, 700.0f, 45.0f, Fire, Fire),
			// Gate Warden
			Spell(TEXT("Lockdown"), TEXT("Lockdown"), TEXT("Nobody leaves. Nobody leaves well."),
				A::Damage, T::AreaOfEffect, S::Strength, 6, 24.0f, 16, 700.0f, 45.0f, Smoke, Hit),
			// Floor Walker
			Spell(TEXT("CleanUpAisleFive"), TEXT("Clean-Up on Aisle Five"), TEXT("Mop first, questions never."),
				A::Damage, T::Cone, S::Strength, 2, 7.0f, 8, 620.0f, 50.0f, nullptr, Hit),
			Spell(TEXT("CustomerService"), TEXT("Customer Service"), TEXT("We value your continued survival."),
				A::Heal, T::AreaOfEffect, S::Charisma, 3, 15.0f, 8, 650.0f, 45.0f, Motes, Motes),
			Spell(TEXT("GrandReopening"), TEXT("Grand Reopening"), TEXT("Under new management, again, with fireworks."),
				A::Damage, T::AreaOfEffect, S::Charisma, 6, 22.0f, 14, 700.0f, 45.0f, Fire, Fire),
		};
		return Book;
	}

	bool Find(FName SkillId, FDCCSkillDef& Out)
	{
		for (const FDCCSkillDef& Spell : All())
		{
			if (Spell.SkillId == SkillId)
			{
				Out = Spell;
				return true;
			}
		}
		return false;
	}
}
