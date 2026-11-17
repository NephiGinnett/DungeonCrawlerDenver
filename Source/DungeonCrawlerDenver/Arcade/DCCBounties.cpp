#include "Arcade/DCCBounties.h"

#include "Arcade/DCCClimbItems.h"
#include "Arcade/DCCCosmetics.h"
#include "Arcade/DCCSpray.h"
#include "Arcade/DCCWalletComponent.h"
#include "GameFramework/PlayerState.h"
#include "Session/DCCProfile.h"

namespace DCCBounties
{
	const TArray<FBounty>& All()
	{
		// Dead Air and Fourteener send you to each other: the mine is inside the mountain.
		static const TArray<FBounty> Bounties = {
			{ TEXT("FirstPrize"), TEXT("Dressed for It"), TEXT("Buy anything here at the Prize Counter. The camera adds ten pounds; a hat subtracts eleven."),
				TEXT("THE DEN"), FLinearColor(1.0f, 0.83f, 0.42f), 20 },
			{ TEXT("HighCamp"), TEXT("High Camp"), TEXT("Reach High Camp on Fourteener, 11,560 ft, where the mountain narrows."),
				TEXT("FOURTEENER"), FLinearColor(0.55f, 0.8f, 1.0f), 30 },
			{ TEXT("Souvenir"), TEXT("Souvenir"), TEXT("Carry a Fourteener artifact down alive. It goes in your trophy vault."),
				TEXT("FOURTEENER"), FLinearColor(0.55f, 0.8f, 1.0f), 40 },
			{ TEXT("SummitTag"), TEXT("Proof"), TEXT("Spray your tag in the top tenth of Fourteener. Buy a spray can first (Prizes)."),
				TEXT("FOURTEENER"), FLinearColor(0.55f, 0.8f, 1.0f), 60 },
			{ TEXT("GoingViral"), TEXT("Going Viral"), TEXT("Upload 2,000 views from one Dead Air shift. Ghosts, traps and each other's suffering rate best."),
				TEXT("DEAD AIR"), FLinearColor(0.55f, 1.0f, 0.65f), 40 },
			{ TEXT("CageTag"), TEXT("Property Defacement"), TEXT("Spray your tag on the cage in Dead Air. The chat pays for it too."),
				TEXT("DEAD AIR"), FLinearColor(0.55f, 1.0f, 0.65f), 40 },
			{ TEXT("ShiftsOver"), TEXT("Shift's Over"), TEXT("Be in the mine when the shift boss goes down, in the deepest stope of Dead Air."),
				TEXT("DEAD AIR"), FLinearColor(0.55f, 1.0f, 0.65f), 80 },
		};
		return Bounties;
	}

	const FBounty* Find(FName Id)
	{
		return All().FindByPredicate([Id](const FBounty& B) { return B.Id == Id; });
	}

	bool IsDone(const FString& Key, FName Id)
	{
		if (Key.IsEmpty() || !Find(Id))
		{
			return false;
		}
		if (DCCProfile::GetForKey(Key, TEXT("Bounty"), Id) > 0.0f)
		{
			return true; // flagged by a game
		}
		if (Id == TEXT("HighCamp"))
		{
			return DCCProfile::GetForKey(Key, TEXT("Climb"), TEXT("BestHeight")) >= 11500.0f;
		}
		if (Id == TEXT("GoingViral"))
		{
			return DCCProfile::GetForKey(Key, TEXT("Haunt"), TEXT("BestViews")) >= 2000.0f;
		}
		if (Id == TEXT("Souvenir"))
		{
			for (const TPair<FName, int32>& Trophy : DCCProfile::GetTrophies(Key))
			{
				const DCCClimbItems::FItem* Item = DCCClimbItems::Find(Trophy.Key);
				if (Item && Item->Kind == DCCClimbItems::EKind::Artifact && Trophy.Value > 0)
				{
					return true;
				}
			}
			return false;
		}
		if (Id == TEXT("FirstPrize"))
		{
			for (const FName& Owned : DCCProfile::GetCosmetics(Key))
			{
				if (Owned != DCCSpray::CanId)
				{
					return true; // a cosmetic, a booth gun or anything else from the counter
				}
			}
			return false;
		}
		return false;
	}

	bool IsPaid(const FString& Key, FName Id)
	{
		return !Key.IsEmpty() && DCCProfile::GetForKey(Key, TEXT("BountyPaid"), Id) > 0.0f;
	}

	void Flag(const APlayerState* PlayerState, FName Id)
	{
		const FString Key = DCCProfile::KeyFor(PlayerState);
		if (!Key.IsEmpty() && Find(Id))
		{
			DCCProfile::SetForKey(Key, TEXT("Bounty"), Id, 1.0f);
		}
	}

	bool Claim(APlayerState* PlayerState, FName Id)
	{
		const FString Key = DCCProfile::KeyFor(PlayerState);
		const FBounty* Bounty = Find(Id);
		if (!Bounty || !IsDone(Key, Id) || IsPaid(Key, Id))
		{
			return false;
		}
		DCCProfile::SetForKey(Key, TEXT("BountyPaid"), Id, 1.0f);
		UDCCWalletComponent::AwardTickets(PlayerState, Bounty->Reward, FString::Printf(TEXT("bounty: %s"), Bounty->Name));
		return true;
	}
}
