#include "Session/DCCStarterKits.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/DCCInventoryComponent.h"
#include "Session/DCCProfile.h"
#include "Session/DCCSaveSystem.h"
#include "Classes/DCCBasicClasses.h"
#include "Skills/DCCSkillComponent.h"

namespace DCCStarterKits
{
	const TArray<FKit>& All()
	{
		// Index is the saved pick. Renames stay in the old slots. New jobs go on the end.
		static const TArray<FKit> Kits = {
			{ TEXT("Repo Driver"), TEXT("Repo Rally. A wall sword, a tray and an apron. Up close, every swing hits harder."),
				FLinearColor(1.0f, 0.72f, 0.15f), { TEXT("WallSword"), TEXT("ServiceTray"), TEXT("GreaseApron") }, {}, TEXT("BandageRollMostlySterile"), 2, 25 },
			{ TEXT("Ridge Hand"), TEXT("Fourteener. A skillet, a shield and a hi-vis vest. You take less of every hit."),
				FLinearColor(0.4f, 0.75f, 1.0f), { TEXT("CastIronSkillet"), TEXT("FeatureWallShield"), TEXT("HighVisibilitySafetyVest") }, {}, TEXT("TraumaKitParamedic"), 1, 25 },
			{ TEXT("Call Sign"), TEXT("Dead Air. Static Burst, a flare gun and a blazer. Your spells hit harder."),
				FLinearColor(0.7f, 0.45f, 1.0f), { TEXT("FlareGunEmergency"), TEXT("ManagersBlazer"), TEXT("Torch") }, { TEXT("StaticBurst") }, TEXT("EnergyTonic"), 2, 25 },
			{ TEXT("Line Coach"), TEXT("Stress Test. First Aid, kits and a bar gun. Your heals land bigger."),
				FLinearColor(0.4f, 1.0f, 0.6f), { TEXT("SodaGun"), TEXT("HighVisibilitySafetyVest"), TEXT("NameTagDave") }, { TEXT("FirstAid") }, TEXT("FirstAidKitCompliant"), 3, 25 },
			{ TEXT("Mile Runner"), TEXT("Neon Mile. A confetti crossbow, a sun hat and a folder. Ranged shots get sharper."),
				FLinearColor(1.0f, 0.35f, 0.7f), { TEXT("ConfettiCrossbow"), TEXT("PatioSunHat"), TEXT("ComplaintFolderOverstuffed") }, {}, TEXT("EnergyTonic"), 1, 25 },
			{ TEXT("Gate Warden"), TEXT("Gate Run. A fire axe, a tray and a vest. Swings hit harder, and hits land softer."),
				FLinearColor(0.35f, 0.85f, 1.0f), { TEXT("FireAxe"), TEXT("ServiceTray"), TEXT("HighVisibilitySafetyVest") }, {}, TEXT("BandageRollMostlySterile"), 2, 25 },
			{ TEXT("Floor Walker"), TEXT("The dungeon. A mallet, a torch and an apron. A little more on swings, shots, spells and heals."),
				FLinearColor(0.95f, 0.8f, 0.45f), { TEXT("TenderiserMallet"), TEXT("Torch"), TEXT("GreaseApron") }, {}, TEXT("EnergyTonic"), 2, 25 },
		};
		return Kits;
	}

	int32 Chosen(const ADCCCharacterBase* Crawler)
	{
		const FString Key = Crawler ? DCCProfile::KeyFor(Crawler->GetPlayerState()) : FString();
		return Key.IsEmpty() ? 0 : FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Dungeon"), TEXT("StarterKit"), 0.0f));
	}

	bool Grant(ADCCCharacterBase* Crawler, int32 Index)
	{
		if (!Crawler || !Crawler->HasAuthority() || !All().IsValidIndex(Index) || Chosen(Crawler) != 0)
		{
			return false;
		}
		const FString Key = DCCProfile::KeyFor(Crawler->GetPlayerState());
		if (Key.IsEmpty())
		{
			return false;
		}
		const FKit& Kit = All()[Index];
		for (const FName& Id : Kit.Gear)
		{
			if (Crawler->Equipment) { Crawler->Equipment->Equip(Id); }
		}
		for (const FName& Id : Kit.Skills)
		{
			if (Crawler->Skills) { Crawler->Skills->GrantSkill(Id, 1); }
		}
		if (Crawler->Inventory && !Kit.Consumable.IsNone() && Kit.ConsumableCount > 0)
		{
			Crawler->Inventory->AddConsumable(Kit.Consumable, Kit.ConsumableCount);
		}
		DCCProfile::SetForKey(Key, TEXT("Dungeon"), TEXT("StarterKit"), float(Index + 1));
		if (Kit.Tickets > 0)
		{
			UDCCWalletComponent::AwardTickets(Crawler->GetPlayerState(), Kit.Tickets, TEXT("starter kit"));
		}
		DCCBasicClasses::ApplyLearned(Crawler);
		DCCSave::Save(Crawler);
		if (Crawler->Wallet)
		{
			Crawler->Wallet->Tell(FString::Printf(TEXT("%s kit: equipped. Open your menu (Tab) and look at Inventory to see it all."), Kit.Name));
		}
		return true;
	}
}
