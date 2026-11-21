#include "Classes/DCCBasicClasses.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Session/DCCStarterKits.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"

namespace DCCBasicClasses
{
	const TArray<FClass>& All()
	{
		// Order is the starter-kit index. Do not reorder: a saved pick is that index.
		// Percents are per level, on a whole kind of action. The last two are wide on purpose
		// and smaller, so a specialist is still better at the one thing they do.
		static const TArray<FClass> Classes = {
			{ TEXT("Repo Driver"), TEXT("melee hits, every level"),
				FLinearColor(1.0f, 0.72f, 0.15f), 8, 0, 0, 0, 0,
				{ { 1, TEXT("Lien Enforcement"), TEXT("LienEnforcement"), TEXT("The hook, up close.") },
				  { 2, TEXT("Martial Arts"), TEXT("MartialArts"), TEXT("When the cable is busy.") },
				  { 3, TEXT("Mark and Collect"), TEXT("MarkAndCollect"), TEXT("The car, then the yard.") },
				  { 4, TEXT("Mechanics"), TEXT("Mechanics"), TEXT("The winch, the door, the engine.") },
				  { 5, TEXT("Athletics"), TEXT("Athletics"), TEXT("Out of the truck and after them.") } } },
			{ TEXT("Ridge Hand"), TEXT("you take less of every hit"),
				FLinearColor(0.4f, 0.75f, 1.0f), 0, 0, 0, 0, 6,
				{ { 1, TEXT("Athletics"), TEXT("Athletics"), TEXT("The ridge is a long day.") },
				  { 2, TEXT("Climbing"), TEXT("Climbing"), TEXT("The wall is the trail.") },
				  { 3, TEXT("Vault"), TEXT("Vault"), TEXT("Over the thing in the way.") },
				  { 4, TEXT("Perception"), TEXT("Perception"), TEXT("See the weather before it arrives.") },
				  { 5, TEXT("Navigation"), TEXT("Navigation"), TEXT("Know which way is down.") } } },
			{ TEXT("Call Sign"), TEXT("spells hit harder"),
				FLinearColor(0.7f, 0.45f, 1.0f), 0, 0, 10, 0, 0,
				{ { 1, TEXT("Static Burst"), TEXT("StaticBurst"), TEXT("The mic pops. So do they.") },
				  { 2, TEXT("Fire Fingers"), TEXT("FireFingers"), TEXT("The first spell that sticks.") },
				  { 3, TEXT("Ball Lightning"), TEXT("BallLightning"), TEXT("Throw the storm.") },
				  { 4, TEXT("Weather-Reading"), TEXT("WeatherReading"), TEXT("Read the room before you speak.") },
				  { 5, TEXT("Lore: Creepypasta"), TEXT("LoreCreepypasta"), TEXT("You have heard this episode.") } } },
			{ TEXT("Line Coach"), TEXT("heals land bigger"),
				FLinearColor(0.4f, 1.0f, 0.6f), 0, 0, 0, 12, 0,
				{ { 1, TEXT("First Aid"), TEXT("FirstAid"), TEXT("Get them off the floor.") },
				  { 2, TEXT("Athletics"), TEXT("Athletics"), TEXT("Reach them before the clock does.") },
				  { 3, TEXT("Pep Talk"), TEXT("QuickWit"), TEXT("Talk them back into it.") },
				  { 4, TEXT("Intimidate"), TEXT("FirstImpressions"), TEXT("Call the play loud enough.") },
				  { 5, TEXT("Perception"), TEXT("Perception"), TEXT("See who is about to drop.") } } },
			{ TEXT("Mile Runner"), TEXT("ranged shots get sharper"),
				FLinearColor(1.0f, 0.35f, 0.7f), 0, 8, 0, 0, 0,
				{ { 1, TEXT("Perception"), TEXT("Perception"), TEXT("Pick the line.") },
				  { 2, TEXT("Targeted Throw"), TEXT("TargetedThrow"), TEXT("Put it through the gap.") },
				  { 3, TEXT("Navigation"), TEXT("Navigation"), TEXT("Which exit is still open.") },
				  { 4, TEXT("Mechanics"), TEXT("Mechanics"), TEXT("The car, when it starts lying.") },
				  { 5, TEXT("Mark and Collect"), TEXT("MarkAndCollect"), TEXT("The shot that closes it.") } } },
			{ TEXT("Gate Warden"), TEXT("melee, and you soak a little"),
				FLinearColor(0.35f, 0.85f, 1.0f), 4, 0, 0, 0, 4,
				{ { 1, TEXT("Perception"), TEXT("Perception"), TEXT("Read the room from the door.") },
				  { 2, TEXT("Martial Arts"), TEXT("MartialArts"), TEXT("The room still has to be cleared.") },
				  { 3, TEXT("Lien Enforcement"), TEXT("LienEnforcement"), TEXT("Some of them owe the house.") },
				  { 4, TEXT("Vault"), TEXT("Vault"), TEXT("Over the thing in the way.") },
				  { 5, TEXT("Heavy Armor"), TEXT("HeavyArmorTraining"), TEXT("The vest earns its keep.") } } },
			{ TEXT("Floor Walker"), TEXT("a little of every kind"),
				FLinearColor(0.95f, 0.8f, 0.45f), 3, 3, 3, 3, 0,
				{ { 1, TEXT("Martial Arts"), TEXT("MartialArts"), TEXT("A swing, when that is the ask.") },
				  { 2, TEXT("First Aid"), TEXT("FirstAid"), TEXT("A hand, when someone is down.") },
				  { 3, TEXT("Fire Fingers"), TEXT("FireFingers"), TEXT("A spell, when the room is far.") },
				  { 4, TEXT("Perception"), TEXT("Perception"), TEXT("Notice the thing the floor hid.") },
				  { 5, TEXT("Athletics"), TEXT("Athletics"), TEXT("Still walking when the floor is not.") } } },
		};
		return Classes;
	}

	int32 IndexOf(const ADCCCharacterBase* Crawler)
	{
		const int32 Chosen = DCCStarterKits::Chosen(Crawler);
		return Chosen > 0 ? Chosen - 1 : INDEX_NONE;
	}

	const FClass* Find(const ADCCCharacterBase* Crawler)
	{
		const int32 Index = IndexOf(Crawler);
		return All().IsValidIndex(Index) ? &All()[Index] : nullptr;
	}

	static int32 ClassLevel(const ADCCCharacterBase* Crawler)
	{
		if (!Find(Crawler) || !Crawler->Stats)
		{
			return 0;
		}
		return FMath::Clamp(Crawler->Stats->Level, 1, 5);
	}

	static int32 Percent(const ADCCCharacterBase* Crawler, int32 FClass::* PerLevel)
	{
		const FClass* Class = Find(Crawler);
		const int32 Level = ClassLevel(Crawler);
		return (Class && Level > 0) ? Class->*PerLevel * Level : 0;
	}

	void ApplyLearned(ADCCCharacterBase* Crawler)
	{
		const FClass* Class = Find(Crawler);
		if (!Crawler || !Crawler->HasAuthority() || !Crawler->IsPlayerControlled() || !Class || !Crawler->Skills)
		{
			return;
		}
		const int32 Level = ClassLevel(Crawler);
		FString Learned;
		for (const FNode& Node : Class->Nodes)
		{
			if (Node.Level > Level || !Node.SkillId || Node.SkillId[0] == 0)
			{
				continue;
			}
			if (Crawler->Skills->GrantSkill(FName(Node.SkillId), 1))
			{
				if (!Learned.IsEmpty())
				{
					Learned += TEXT(", ");
				}
				Learned += Node.Title;
			}
		}
		if (Learned.IsEmpty())
		{
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("[DCC Class] %s is a %s (level %d) and learned %s"),
			*Crawler->GetName(), Class->Name, Level, *Learned);
		if (Crawler->Wallet)
		{
			Crawler->Wallet->Tell(FString::Printf(TEXT("%s  level %d  learned %s"), Class->Name, Level, *Learned));
		}
	}

	int32 MeleePercent(const ADCCCharacterBase* Crawler) { return Percent(Crawler, &FClass::MeleePerLevel); }
	int32 RangedPercent(const ADCCCharacterBase* Crawler) { return Percent(Crawler, &FClass::RangedPerLevel); }
	int32 SpellPercent(const ADCCCharacterBase* Crawler) { return Percent(Crawler, &FClass::SpellPerLevel); }
	int32 HealPercent(const ADCCCharacterBase* Crawler) { return Percent(Crawler, &FClass::HealPerLevel); }

	float DamageTakenScale(const ADCCCharacterBase* Crawler)
	{
		const int32 Reduced = Percent(Crawler, &FClass::TakenPerLevel);
		return Reduced > 0 ? 1.0f - Reduced / 100.0f : 1.0f;
	}

	FString BonusLine(const ADCCCharacterBase* Crawler)
	{
		const FClass* Class = Find(Crawler);
		const int32 Level = ClassLevel(Crawler);
		if (!Class || Level <= 0)
		{
			return FString();
		}
		const int32 Melee = Class->MeleePerLevel * Level;
		const int32 Ranged = Class->RangedPerLevel * Level;
		const int32 Spell = Class->SpellPerLevel * Level;
		const int32 Heal = Class->HealPerLevel * Level;
		const int32 Taken = Class->TakenPerLevel * Level;
		if (Melee > 0 && Melee == Ranged && Ranged == Spell && Spell == Heal)
		{
			return FString::Printf(TEXT("A bit of each +%d%%"), Melee);
		}
		TArray<FString> Parts;
		if (Melee > 0) { Parts.Add(FString::Printf(TEXT("Melee +%d%%"), Melee)); }
		if (Ranged > 0) { Parts.Add(FString::Printf(TEXT("Ranged +%d%%"), Ranged)); }
		if (Spell > 0) { Parts.Add(FString::Printf(TEXT("Spells +%d%%"), Spell)); }
		if (Heal > 0) { Parts.Add(FString::Printf(TEXT("Heals +%d%%"), Heal)); }
		if (Taken > 0) { Parts.Add(FString::Printf(TEXT("Hits taken -%d%%"), Taken)); }
		return FString::Join(Parts, TEXT("   "));
	}
}
