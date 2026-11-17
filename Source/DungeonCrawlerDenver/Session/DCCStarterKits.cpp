#include "Session/DCCStarterKits.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/DCCInventoryComponent.h"
#include "Session/DCCProfile.h"
#include "Session/DCCSaveSystem.h"
#include "Skills/DCCSkillComponent.h"

namespace DCCStarterKits
{
	const TArray<FKit>& All()
	{
		static const TArray<FKit> Kits = {
			{ TEXT("BRUISER"), TEXT("Hit hard, up close. A wall sword, a tray to block with, and an apron that has seen worse."),
				FLinearColor(1.0f, 0.45f, 0.3f), { TEXT("WallSword"), TEXT("ServiceTray"), TEXT("GreaseApron") }, {}, TEXT("BandageRollMostlySterile"), 2, 25 },
			{ TEXT("TANK"), TEXT("Stand in front. A skillet, a shield and a hi-vis vest: hard to hurt, hard to miss."),
				FLinearColor(0.4f, 0.75f, 1.0f), { TEXT("CastIronSkillet"), TEXT("FeatureWallShield"), TEXT("HighVisibilitySafetyVest") }, {}, TEXT("TraumaKitParamedic"), 1, 25 },
			{ TEXT("MAGIC"), TEXT("Burn things from across the room. Static Burst, a flare gun, and a blazer that makes you smarter."),
				FLinearColor(0.7f, 0.45f, 1.0f), { TEXT("FlareGunEmergency"), TEXT("ManagersBlazer"), TEXT("Torch") }, { TEXT("StaticBurst") }, TEXT("EnergyTonic"), 2, 25 },
			{ TEXT("HEALER"), TEXT("Keep the crew standing. First Aid, a stack of kits, and a bar gun for keeping your distance."),
				FLinearColor(0.4f, 1.0f, 0.6f), { TEXT("SodaGun"), TEXT("HighVisibilitySafetyVest"), TEXT("NameTagDave") }, { TEXT("FirstAid") }, TEXT("FirstAidKitCompliant"), 3, 25 },
			{ TEXT("RANGER"), TEXT("Pick them off. A confetti crossbow, a sun hat, and a folder full of complaints to hide behind."),
				FLinearColor(1.0f, 0.85f, 0.35f), { TEXT("ConfettiCrossbow"), TEXT("PatioSunHat"), TEXT("ComplaintFolderOverstuffed") }, {}, TEXT("EnergyTonic"), 1, 25 },
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
		DCCSave::Save(Crawler);
		if (Crawler->Wallet)
		{
			Crawler->Wallet->Tell(FString::Printf(TEXT("%s kit: equipped. Open your menu (Tab) and look at Inventory to see it all."), Kit.Name));
		}
		return true;
	}
}
