#include "Skills/DCCSpellBook.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace DCCSpellBook
{
	namespace
	{
		// Free Spells. Auras and buffs loop, so every spawn is switched off after a short life.
		const TCHAR* AuraAir = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Air.NS_Free_Spells_Aura_Air");
		const TCHAR* AuraFire = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Fire.NS_Free_Spells_Aura_Fire");
		const TCHAR* AuraHeal = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Healing.NS_Free_Spells_Aura_Healing");
		const TCHAR* AuraLightning = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Lightning.NS_Free_Spells_Aura_Lightning");
		const TCHAR* AuraNotes = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Notes.NS_Free_Spells_Aura_Notes");
		const TCHAR* AuraSoul = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Soul.NS_Free_Spells_Aura_Soul");
		const TCHAR* BuffAir = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Air.NS_Free_Spells_Buff_Air");
		const TCHAR* BuffFire = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Fire.NS_Free_Spells_Buff_Fire");
		const TCHAR* BuffHeal = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Healing.NS_Free_Spells_Buff_Healing");
		const TCHAR* BuffLightning = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Lightning.NS_Free_Spells_Buff_Lightning");
		const TCHAR* BuffNotes = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Notes.NS_Free_Spells_Buff_Notes");
		const TCHAR* Circle = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle.NS_Free_Spells_Circle");
		const TCHAR* Explosion = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion.NS_Free_Spells_Explosion");
		const TCHAR* LevelUp = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_LevelUp.NS_Free_Spells_LevelUp");
		const TCHAR* Shockwave = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave.NS_Free_Spells_Shockwave");
		const TCHAR* SoulPull = TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Purple.NS_Absorption_Purple");

		struct FFx
		{
			const TCHAR* Id;
			const TCHAR* Cast;
			const TCHAR* Impact;
			float Z;
			float CastScale;
			float ImpactScale;
		};

		// Scales are from the 2026-11-25 pose-bench sheet (Saved/TestShots/spell_fx2), shot at scale 1 on Zoe.
		// A fire aura, a lightning buff and Level Up fill the room. A shockwave and a slash are about two body-heights.
		// Ordinary casts stay near 0.3 so they sit on the crawler. The level-20 shows stay large on purpose.
		// There is no point-spawn ice system. Frost is a spline, so ice spells wear the air aura and a shockwave.
		const FFx& Row(int32 Index, int32& Count)
		{
			static const FFx Table[] = {
				// Starter strikes that the classes teach and the skill table never received.
				{ TEXT("StaticBurst"), AuraLightning, BuffLightning, 70.0f, 0.28f, 0.22f },
				{ TEXT("BallLightning"), AuraLightning, Explosion, 80.0f, 0.40f, 0.80f },
				{ TEXT("LienEnforcement"), Circle, Shockwave, 40.0f, 0.40f, 0.45f },
				{ TEXT("MarkAndCollect"), AuraSoul, Shockwave, 50.0f, 0.30f, 0.50f },
				// Class spell book.
				{ TEXT("FinalNotice"), Circle, Shockwave, 30.0f, 0.40f, 0.45f },
				{ TEXT("HostileTakeover"), BuffAir, Shockwave, 40.0f, 0.32f, 0.55f },
				{ TEXT("TotalRepossession"), Circle, Explosion, 40.0f, 1.00f, 1.15f },
				{ TEXT("SecondWind"), AuraHeal, BuffHeal, 80.0f, 0.32f, 0.30f },
				{ TEXT("HoldTheLine"), Circle, Shockwave, 20.0f, 0.45f, 0.55f },
				{ TEXT("AvalancheWarning"), AuraAir, Shockwave, 30.0f, 0.35f, 0.65f },
				{ TEXT("Whiteout"), AuraAir, Shockwave, 40.0f, 1.05f, 1.20f },
				{ TEXT("FeedbackLoop"), AuraLightning, BuffLightning, 70.0f, 0.32f, 0.28f },
				{ TEXT("RadioSilence"), AuraNotes, Shockwave, 30.0f, 0.35f, 0.60f },
				{ TEXT("Interference"), AuraLightning, BuffLightning, 60.0f, 0.30f, 0.28f },
				{ TEXT("StationIdentification"), AuraNotes, Explosion, 50.0f, 1.15f, 1.25f },
				{ TEXT("Timeout"), AuraHeal, BuffHeal, 70.0f, 0.32f, 0.30f },
				{ TEXT("PenaltyFlag"), BuffAir, Shockwave, 50.0f, 0.28f, 0.45f },
				{ TEXT("TwoMinuteDrill"), AuraHeal, LevelUp, 60.0f, 0.35f, 0.55f },
				{ TEXT("HalftimeSpeech"), LevelUp, BuffHeal, 40.0f, 1.15f, 1.10f },
				{ TEXT("HotLap"), AuraFire, BuffFire, 50.0f, 0.32f, 0.28f },
				{ TEXT("PhotoFinish"), BuffAir, Shockwave, 70.0f, 0.25f, 0.40f },
				{ TEXT("Slipstream"), AuraAir, Shockwave, 40.0f, 0.32f, 0.55f },
				{ TEXT("VictoryLap"), AuraFire, Explosion, 40.0f, 1.05f, 1.10f },
				{ TEXT("Lockdown"), Circle, Explosion, 30.0f, 1.00f, 1.10f },
				{ TEXT("CleanUpAisleFive"), AuraAir, Shockwave, 20.0f, 0.28f, 0.50f },
				{ TEXT("CustomerService"), AuraHeal, BuffHeal, 70.0f, 0.32f, 0.30f },
				{ TEXT("GrandReopening"), BuffFire, Explosion, 40.0f, 1.10f, 1.20f },
				// Older table spells. The table row still wins for numbers. This only replaces the look.
				{ TEXT("FireFingers"), AuraFire, BuffFire, 60.0f, 0.35f, 0.32f },
				{ TEXT("FirstAid"), AuraHeal, BuffHeal, 70.0f, 0.30f, 0.28f },
				{ TEXT("SoulCollector"), AuraSoul, SoulPull, 70.0f, 0.35f, 0.50f },
				{ TEXT("FieldDressing"), AuraHeal, BuffHeal, 50.0f, 0.28f, 0.26f },
				{ TEXT("HypeTheCrowd"), AuraNotes, BuffNotes, 80.0f, 0.35f, 0.32f },
				{ TEXT("SummonInterns"), Circle, LevelUp, 20.0f, 0.45f, 0.45f },
				{ TEXT("SummonRegulars"), Circle, LevelUp, 20.0f, 0.50f, 0.50f },
			};
			Count = UE_ARRAY_COUNT(Table);
			return Table[Index];
		}

		const FFx* FindFx(FName SkillId)
		{
			int32 Count = 0;
			Row(0, Count);
			for (int32 i = 0; i < Count; ++i)
			{
				int32 N = 0;
				const FFx& Fx = Row(i, N);
				if (SkillId == Fx.Id)
				{
					return &Fx;
				}
			}
			return nullptr;
		}

		FDCCSkillDef Spell(const TCHAR* Id, const TCHAR* Name, const TCHAR* Flavor, EDCCSkillArchetype Archetype,
			EDCCSkillTargeting Targeting, EDCCAbility Ability, int32 Mana, float Cooldown, int32 Die,
			float RangeOrRadius, float ConeHalf, int32 Difficulty = 0, bool bUntrained = false)
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
			S.DifficultyClass = Difficulty;
			S.bCanAttemptUntrained = bUntrained;
			ApplyPresentation(S);
			return S;
		}
	}

	void ApplyPresentation(FDCCSkillDef& Skill)
	{
		const FFx* Fx = FindFx(Skill.SkillId);
		if (!Fx)
		{
			return;
		}
		if (Fx->Cast)
		{
			Skill.CastEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Fx->Cast));
		}
		if (Fx->Impact)
		{
			Skill.ImpactEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Fx->Impact));
		}
		Skill.ImpactEffectZOffset = Fx->Z;
	}

	void ScalesFor(FName SkillId, float& OutCastScale, float& OutImpactScale)
	{
		OutCastScale = 1.0f;
		OutImpactScale = 1.0f;
		if (const FFx* Fx = FindFx(SkillId))
		{
			OutCastScale = Fx->CastScale;
			OutImpactScale = Fx->ImpactScale;
		}
	}

	void StopAfter(UNiagaraComponent* Component, float Seconds)
	{
		if (!Component || Seconds <= 0.0f)
		{
			return;
		}
		UWorld* World = Component->GetWorld();
		if (!World)
		{
			return;
		}
		TWeakObjectPtr<UNiagaraComponent> Weak(Component);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, [Weak]()
		{
			if (UNiagaraComponent* Live = Weak.Get())
			{
				Live->Deactivate();
			}
		}, Seconds, false);
	}

	void SpawnPuff(const UObject* WorldContext, UNiagaraSystem* System, const FVector& At, float Scale, float LifeSeconds, float MaxDistance)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || !System || World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		if (MaxDistance > 0.0f)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				FVector View;
				FRotator Rot;
				PC->GetPlayerViewPoint(View, Rot);
				if (FVector::Dist(View, At) > MaxDistance)
				{
					return;
				}
			}
		}
		UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, System, At, FRotator::ZeroRotator, FVector(Scale), true);
		StopAfter(Comp, LifeSeconds);
	}

	const TCHAR* DeathPuffPath()
	{
		return TEXT("/Game/NiagaraExamples/FX_Explosions/NS_Explosion_Small.NS_Explosion_Small");
	}

	const TCHAR* HitPuffPath(EDCCDamageType Type)
	{
		switch (Type)
		{
		case EDCCDamageType::Fire:
			return TEXT("/Game/NiagaraExamples/FX_Explosions/NS_Explosion_Small.NS_Explosion_Small");
		case EDCCDamageType::Piercing:
			return TEXT("/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Slash.NS_Free_Magic_Slash");
		case EDCCDamageType::Bludgeoning:
		case EDCCDamageType::Physical:
		default:
			return TEXT("/Game/NiagaraExamples/FX_Weapons/Impacts/NS_Impact_Concrete.NS_Impact_Concrete");
		}
	}

	const TArray<FDCCSkillDef>& All()
	{
		using A = EDCCSkillArchetype;
		using T = EDCCSkillTargeting;
		using S = EDCCAbility;
		static const TArray<FDCCSkillDef> Book = {
			// Taught from level 1. They were a CSV that never landed in the skill table.
			Spell(TEXT("StaticBurst"), TEXT("Static Burst"), TEXT("A jolt of borrowed dread."),
				A::Damage, T::SingleTarget, S::Intelligence, 0, 5.0f, 6, 600.0f, 45.0f, 10, true),
			Spell(TEXT("BallLightning"), TEXT("Ball Lightning"), TEXT("A crackling orb, thrown with more confidence than the insurance covers."),
				A::Damage, T::SingleTarget, S::Dexterity, 0, 6.0f, 6, 700.0f, 45.0f, 10, true),
			Spell(TEXT("MarkAndCollect"), TEXT("Mark and Collect"), TEXT("The closer. Hits harder against someone already rattled."),
				A::Damage, T::SingleTarget, S::Strength, 0, 7.0f, 8, 250.0f, 45.0f, 10, true),
			Spell(TEXT("LienEnforcement"), TEXT("Lien Enforcement"), TEXT("The answer to a missed payment."),
				A::Damage, T::SingleTarget, S::Strength, 0, 5.0f, 6, 250.0f, 45.0f, 10, true),
			// Repo Driver
			Spell(TEXT("FinalNotice"), TEXT("Final Notice"), TEXT("Delivered in person. Signature not required."),
				A::Damage, T::SingleTarget, S::Strength, 3, 9.0f, 12, 700.0f, 45.0f),
			Spell(TEXT("HostileTakeover"), TEXT("Hostile Takeover"), TEXT("Everything in front of you is under new management."),
				A::Damage, T::Cone, S::Strength, 3, 10.0f, 10, 480.0f, 55.0f),
			Spell(TEXT("TotalRepossession"), TEXT("Total Repossession"), TEXT("The whole room. Every last thing in it."),
				A::Damage, T::AreaOfEffect, S::Strength, 6, 24.0f, 16, 650.0f, 45.0f),
			// Shared
			Spell(TEXT("SecondWind"), TEXT("Second Wind"), TEXT("Not done. Not even close. Probably."),
				A::Heal, T::Self, S::Constitution, 2, 16.0f, 12, 0.0f, 45.0f),
			Spell(TEXT("HoldTheLine"), TEXT("Hold the Line"), TEXT("Plant your feet. Everything near you regrets it."),
				A::Damage, T::AreaOfEffect, S::Strength, 3, 11.0f, 10, 400.0f, 45.0f),
			// Ridge Hand
			Spell(TEXT("AvalancheWarning"), TEXT("Avalanche Warning"), TEXT("The warning was the courtesy."),
				A::Damage, T::AreaOfEffect, S::Constitution, 3, 12.0f, 10, 460.0f, 45.0f),
			Spell(TEXT("Whiteout"), TEXT("Whiteout"), TEXT("Visibility zero. Damage, considerable."),
				A::Damage, T::AreaOfEffect, S::Constitution, 6, 24.0f, 14, 760.0f, 45.0f),
			// Call Sign
			Spell(TEXT("FeedbackLoop"), TEXT("Feedback Loop"), TEXT("Point the mic at them. Turn it up."),
				A::Damage, T::SingleTarget, S::Intelligence, 3, 7.0f, 12, 1100.0f, 45.0f),
			Spell(TEXT("RadioSilence"), TEXT("Radio Silence"), TEXT("Every channel at once, then nothing."),
				A::Damage, T::AreaOfEffect, S::Intelligence, 4, 12.0f, 10, 540.0f, 45.0f),
			Spell(TEXT("Interference"), TEXT("Interference"), TEXT("A wall of static, aimed."),
				A::Damage, T::Cone, S::Intelligence, 3, 9.0f, 10, 820.0f, 38.0f),
			Spell(TEXT("StationIdentification"), TEXT("Station Identification"), TEXT("You are listening to the last thing they hear."),
				A::Damage, T::AreaOfEffect, S::Intelligence, 6, 22.0f, 18, 760.0f, 45.0f),
			// Line Coach
			Spell(TEXT("Timeout"), TEXT("Timeout"), TEXT("Everybody in. Breathe. Again."),
				A::Heal, T::AreaOfEffect, S::Charisma, 3, 16.0f, 8, 650.0f, 45.0f),
			Spell(TEXT("PenaltyFlag"), TEXT("Penalty Flag"), TEXT("Thrown hard enough to count as a tackle."),
				A::Damage, T::SingleTarget, S::Charisma, 2, 8.0f, 10, 900.0f, 45.0f),
			Spell(TEXT("TwoMinuteDrill"), TEXT("Two-Minute Drill"), TEXT("Fast hands, fast heals, no huddle."),
				A::Heal, T::AreaOfEffect, S::Charisma, 4, 18.0f, 10, 900.0f, 45.0f),
			Spell(TEXT("HalftimeSpeech"), TEXT("Halftime Speech"), TEXT("Nobody remembers the words. Everybody stands up."),
				A::Heal, T::AreaOfEffect, S::Charisma, 6, 28.0f, 16, 1000.0f, 45.0f),
			// Mile Runner
			Spell(TEXT("HotLap"), TEXT("Hot Lap"), TEXT("A line of fire down the racing line."),
				A::Damage, T::Cone, S::Dexterity, 2, 6.0f, 10, 1200.0f, 22.0f),
			Spell(TEXT("PhotoFinish"), TEXT("Photo Finish"), TEXT("By a nose. Theirs, mostly."),
				A::Damage, T::SingleTarget, S::Dexterity, 3, 9.0f, 12, 1500.0f, 45.0f),
			Spell(TEXT("Slipstream"), TEXT("Slipstream"), TEXT("Whatever is behind you is in front of them now."),
				A::Damage, T::Cone, S::Dexterity, 3, 10.0f, 12, 1400.0f, 28.0f),
			Spell(TEXT("VictoryLap"), TEXT("Victory Lap"), TEXT("One full circle. Nobody standing inside it."),
				A::Damage, T::AreaOfEffect, S::Dexterity, 6, 22.0f, 16, 700.0f, 45.0f),
			// Gate Warden
			Spell(TEXT("Lockdown"), TEXT("Lockdown"), TEXT("Nobody leaves. Nobody leaves well."),
				A::Damage, T::AreaOfEffect, S::Strength, 6, 24.0f, 16, 700.0f, 45.0f),
			// Floor Walker
			Spell(TEXT("CleanUpAisleFive"), TEXT("Clean-Up on Aisle Five"), TEXT("Mop first, questions never."),
				A::Damage, T::Cone, S::Strength, 2, 7.0f, 8, 620.0f, 50.0f),
			Spell(TEXT("CustomerService"), TEXT("Customer Service"), TEXT("We value your continued survival."),
				A::Heal, T::AreaOfEffect, S::Charisma, 3, 15.0f, 8, 650.0f, 45.0f),
			Spell(TEXT("GrandReopening"), TEXT("Grand Reopening"), TEXT("Under new management, again, with fireworks."),
				A::Damage, T::AreaOfEffect, S::Charisma, 6, 22.0f, 14, 700.0f, 45.0f),
		};
		return Book;
	}

	bool Find(FName SkillId, FDCCSkillDef& Out)
	{
		for (const FDCCSkillDef& SpellDef : All())
		{
			if (SpellDef.SkillId == SkillId)
			{
				Out = SpellDef;
				return true;
			}
		}
		return false;
	}
}
