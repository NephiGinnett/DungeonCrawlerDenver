#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "GameFramework/PlayerState.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCLootBoxTypes.h"
#include "Session/DCCIntegrity.h"
#include "Session/DCCProfile.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * Voice Drop credits (2026-11-22, Zoe: "players to be able to upload their voice lines and even friends who haven't
 * played the game ... have the game award those tickets ... when the voice line is shipped"). Friends record on the
 * VoiceDrop site; Tools/voice_inbox.py brings approved takes into the game and writes Content/DCC/VoiceCredits/
 * credits.json. Here, on the host, each credit is paid once to the player whose name matches, the first time they play.
 */
namespace DCCVoiceCredits
{
	struct FCredit
	{
		FString Id;
		FString Player;
		FString Speaker;
		FString Text;
		int32 Tickets = 0;
		/** Besides tickets (2026-11-23): a loot box tier, a nameplate title, an item ("HotMic"). */
		FString Box;
		FString Title;
		FString Item;
	};

	const TArray<FCredit>& All()
	{
		static TArray<FCredit> Credits;
		static bool bLoaded = false;
		if (bLoaded)
		{
			return Credits;
		}
		bLoaded = true;
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("DCC/VoiceCredits/credits.json");
		TSharedPtr<FJsonObject> Root;
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)
			&& Root.IsValid() && Root->TryGetArrayField(TEXT("credits"), Rows))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Rows)
			{
				const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Row.IsValid())
				{
					continue;
				}
				FCredit Credit;
				Credit.Id = Row->GetStringField(TEXT("id"));
				Credit.Player = Row->GetStringField(TEXT("player")).TrimStartAndEnd();
				Credit.Speaker = Row->GetStringField(TEXT("speaker"));
				Credit.Text = Row->GetStringField(TEXT("text"));
				Credit.Tickets = static_cast<int32>(Row->GetNumberField(TEXT("tickets")));
				Row->TryGetStringField(TEXT("box"), Credit.Box);
				Row->TryGetStringField(TEXT("title"), Credit.Title);
				Row->TryGetStringField(TEXT("item"), Credit.Item);
				if (!Credit.Id.IsEmpty() && !Credit.Player.IsEmpty()
					&& (Credit.Tickets > 0 || !Credit.Box.IsEmpty() || !Credit.Title.IsEmpty() || !Credit.Item.IsEmpty()))
				{
					Credits.Add(Credit);
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[DCC Voice] %d shipped voice line credit(s) on file."), Credits.Num());
		return Credits;
	}

	/** Server: pay this player any credit of theirs not paid yet. The profile remembers which were paid. */
	void PayOut(APlayerState* State, const FString& Key)
	{
		for (const FCredit& Credit : All())
		{
			if (!Credit.Player.Equals(Key, ESearchCase::IgnoreCase))
			{
				continue;
			}
			const FName Stat(*FString::Printf(TEXT("Credit_%s"), *Credit.Id));
			if (DCCProfile::GetForKey(Key, TEXT("Voice"), Stat, 0.0f) > 0.5f)
			{
				continue;
			}
			DCCProfile::SetForKey(Key, TEXT("Voice"), Stat, 1.0f);
			const FString Quote = Credit.Text.Len() > 50 ? Credit.Text.Left(47) + TEXT("...") : Credit.Text;
			if (Credit.Tickets > 0)
			{
				UDCCWalletComponent::AwardTickets(State, Credit.Tickets,
					FString::Printf(TEXT("your voice line shipped (%s: \"%s\")"), *Credit.Speaker, *Quote));
			}
			ADCCCharacterBase* Crawler = State ? Cast<ADCCCharacterBase>(State->GetPawn()) : nullptr;
			if (!Credit.Box.IsEmpty() && Crawler && Crawler->Inventory)
			{
				static const TCHAR* Tiers[] = { TEXT("None"), TEXT("Bronze"), TEXT("Silver"), TEXT("Gold"), TEXT("Platinum"), TEXT("Legendary"), TEXT("Celestial") };
				for (int32 t = 1; t < UE_ARRAY_COUNT(Tiers); ++t)
				{
					if (Credit.Box.Equals(Tiers[t], ESearchCase::IgnoreCase))
					{
						Crawler->Inventory->GrantLootBox(static_cast<EDCCLootBoxTier>(t));
						if (Crawler->Wallet)
						{
							Crawler->Wallet->Tell(FString::Printf(TEXT("%s: a %s loot box. Open it from Tab, LOOT BOXES."), *Credit.Speaker, Tiers[t]));
						}
					}
				}
			}
			if (!Credit.Title.IsEmpty())
			{
				DCCProfile::SetForKey(Key, TEXT("Titles"), FName(*Credit.Title), 1.0f);
				if (Crawler && Crawler->Wallet)
				{
					Crawler->Wallet->NameTitle = Credit.Title;
					Crawler->RefreshNameplateNow();
					Crawler->Wallet->Tell(FString::Printf(TEXT("New nameplate title: %s"), *Credit.Title));
				}
			}
			if (Credit.Item.Equals(TEXT("HotMic"), ESearchCase::IgnoreCase))
			{
				DCCProfile::GrantCosmetic(Key, UDCCWalletComponent::HotMicId);
				if (Crawler && Crawler->Wallet)
				{
					Crawler->Wallet->RefreshFromProfile();
					Crawler->Wallet->Tell(TEXT("THE HOT MIC (Epic) is yours: you are the voice of Hiss Cranson. It is on your belt."));
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[DCC Voice] paid %s %d tickets for %s %s"), *Key, Credit.Tickets, *Credit.Speaker, *Credit.Id);
		}
	}
}
#include "Combat/DCCGameState.h"
#include "Arcade/DCCAudience.h"
#include "GameFramework/GameStateBase.h"
#include "Characters/DCCAllyCharacters.h"
#include "Arcade/DCCSummons.h"
#include "Characters/DCCReactions.h"
#include "UI/DCCHUD.h"
#include "Session/DCCStarterKits.h"

#include "Bench/DCCBenchData.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCArcadeHaunt.h"
#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCClimbItems.h"
#include "Arcade/DCCRallyItems.h"
#include "Vehicles/DCCRover.h"
#include "Characters/DCCCharacterBase.h"
#include "Session/DCCProfile.h"
#include "Session/DCCSession.h"
#include "Arcade/DCCChopShop.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCPickup_Equipment.h"
#include "Session/DCCSaveSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Arcade/DCCSpray.h"
#include "Arcade/DCCBounties.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	UStaticMesh* ShapeMesh(const TCHAR* Shape)
	{
		static TMap<FString, TWeakObjectPtr<UStaticMesh>> Cache;
		const FString Key(Shape);
		if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(Key))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
		Cache.Add(Key, Mesh);
		return Mesh;
	}
}

UDCCWalletComponent::UDCCWalletComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only while wearing something
	SetIsReplicatedByDefault(true);
}

void UDCCWalletComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Tickets and what you own matter only to you; what you wear, everyone sees.
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, Tickets, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, Fame, COND_OwnerOnly);
	DOREPLIFETIME(UDCCWalletComponent, FameLevel); // everyone: the talk show board and the host read it
	DOREPLIFETIME(UDCCWalletComponent, NameTitle);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, HotMicLiveUntil, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, HotMicReadyAt, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, Owned, COND_OwnerOnly);
	DOREPLIFETIME(UDCCWalletComponent, Worn);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, ClimbTools, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, ClimbBoosts, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, CarriedArtifacts, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, ClimbHunger, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, TrophyIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, BountiesDone, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, BountiesPaid, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, TrophyCounts, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, Scrap, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, ChopLevels, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCCWalletComponent, MineCredits, COND_OwnerOnly);
}

void UDCCWalletComponent::BeginPlay()
{
	Super::BeginPlay();
	// Bounties are read off the profile every couple of seconds on the server: cheap, and nothing has to remember to
	// call it. A timer, not the tick, because the tick is switched off when nothing cosmetic is worn.
	if (GetOwner() && GetOwner()->HasAuthority() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(BountyTimer, this, &UDCCWalletComponent::RefreshBounties, 2.0f, true, 1.0f);
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner()))
		{
			if (Crawler->Equipment)
			{
				Crawler->Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UDCCWalletComponent::OnGearChanged);
			}
		}
		// The pawn is possessed (and has a name) a moment after it spawns.
		GetWorld()->GetTimerManager().SetTimer(LoadRetry, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const APawn* Pawn = Cast<APawn>(GetOwner());
			if (Pawn && Pawn->GetPlayerState() && !Pawn->GetPlayerState()->GetPlayerName().IsEmpty())
			{
				LoadFromProfile();
			}
			if (!LoadedKey.IsEmpty() || ++LoadAttempts > 40)
			{
				GetWorld()->GetTimerManager().ClearTimer(LoadRetry);
			}
		}), 0.5f, true, 0.2f);
		// Every few seconds: if this visitor's profile changed here, send them the new copy (their PC keeps it).
		GetWorld()->GetTimerManager().SetTimer(SyncTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const APawn* Pawn = Cast<APawn>(GetOwner());
			if (!bProfileSent || !Pawn || Pawn->IsLocallyControlled())
			{
				return; // not synced yet, or the host's own crawler (already on this PC)
			}
			const FString Key = ProfileKey();
			if (DCCProfile::TakeDirty(Key))
			{
				Client_ReceiveProfile(DCCProfile::Export(Key));
			}
		}), 4.0f, true, 4.0f);
	}
	else if (GetNetMode() == NM_Client)
	{
		// A visitor: once our crawler has its name, carry our profile to the host (DCCProfile::MergeFromClient).
		GetWorld()->GetTimerManager().SetTimer(SyncTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const APawn* Pawn = Cast<APawn>(GetOwner());
			if (Pawn && Pawn->IsLocallyControlled() && Pawn->GetPlayerState() && !Pawn->GetPlayerState()->GetPlayerName().IsEmpty())
			{
				const FString Key = Pawn->GetPlayerState()->GetPlayerName();
				Server_SyncProfile(DCCProfile::Export(Key), DCCProfile::ExportBase(Key));
				UE_LOG(LogTemp, Log, TEXT("[DCC Profile] sent %s's profile to the host."), *Key);
				GetWorld()->GetTimerManager().ClearTimer(SyncTimer);
			}
			else if (++SyncAttempts > 60)
			{
				GetWorld()->GetTimerManager().ClearTimer(SyncTimer);
			}
		}), 0.5f, true, 0.5f);
	}
}

void UDCCWalletComponent::Server_SyncProfile_Implementation(const TArray<uint8>& Current, const TArray<uint8>& Base)
{
	const FString Key = ProfileKey();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Key.IsEmpty() || !Pawn || Pawn->IsLocallyControlled() || Current.Num() > 256 * 1024 || Base.Num() > 256 * 1024)
	{
		return;
	}
	FString Suspicion;
	if (DCCProfile::MergeFromClient(Key, Current, Base, &Suspicion))
	{
		if (!Suspicion.IsEmpty())
		{
			DCCIntegrity::Flag(Cast<ADCCCharacterBase>(GetOwner()), Suspicion);
		}
		PushToClients(Key); // tickets, items and the rest, now including what they earned away from this PC
		bProfileSent = true;
		DCCProfile::TakeDirty(Key);
		Client_ReceiveProfile(DCCProfile::Export(Key));
	}
}

void UDCCWalletComponent::Client_ReceiveProfile_Implementation(const TArray<uint8>& Merged)
{
	if (GetNetMode() == NM_Client)
	{
		DCCProfile::AdoptFromHost(Merged);
	}
}

void UDCCWalletComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FLivePart& Live : LiveParts)
	{
		if (UStaticMeshComponent* Part = Live.Component.Get())
		{
			Part->DestroyComponent();
		}
	}
	LiveParts.Reset();
	Super::EndPlay(EndPlayReason);
}

FString UDCCWalletComponent::ProfileKey() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsPlayerControlled() ? DCCProfile::KeyFor(Pawn->GetPlayerState()) : FString();
}

FName UDCCWalletComponent::WornIn(FName Slot) const
{
	const FDCCWornCosmetic* Found = Worn.FindByPredicate([Slot](const FDCCWornCosmetic& W) { return W.Slot == Slot; });
	return Found ? Found->Id : NAME_None;
}

void UDCCWalletComponent::PushToClients(const FString& Key)
{
	Tickets = DCCProfile::GetTickets(Key);
	if (DCCProfile::GetForKey(Key, TEXT("Titles"), TEXT("OPEN MIC"), 0.0f) > 0.5f)
	{
		NameTitle = TEXT("OPEN MIC");
	}
	Fame = FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Audience"), TEXT("Fame"), 0.0f));
	FameLevel = FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Audience"), TEXT("FameLevel"), 0.0f));
	Owned = DCCProfile::GetCosmetics(Key);
	Worn.Reset();
	for (const TPair<FName, FName>& Pair : DCCProfile::GetWearing(Key))
	{
		if (DCCCosmetics::Find(Pair.Value) && Owned.Contains(Pair.Value))
		{
			Worn.Add({ Pair.Key, Pair.Value });
		}
	}
	OnRep_Worn(); // the server draws too
	RefreshTrophies();
	Scrap = DCCChopShop::GetScrap(Key);
	MineCredits = DCCChopShop::GetCredits(Key);
	ChopLevels.Reset();
	for (const DCCChopShop::FUpgrade& Upgrade : DCCChopShop::All())
	{
		ChopLevels.Add(uint8(DCCChopShop::GetLevel(Key, Upgrade.Id)));
	}
}

void UDCCWalletComponent::RefreshTrophies()
{
	const FString Key = ProfileKey();
	TrophyIds.Reset();
	TrophyCounts.Reset();
	if (Key.IsEmpty())
	{
		return;
	}
	for (const TPair<FName, int32>& Pair : DCCProfile::GetTrophies(Key))
	{
		TrophyIds.Add(Pair.Key);
		TrophyCounts.Add(Pair.Value);
	}
}

void UDCCWalletComponent::Server_SellTrophy_Implementation(FName ArtifactId)
{
	if (!DCCIntegrity::Allow(Cast<ADCCCharacterBase>(GetOwner()), TEXT("shop"), 5.0f, 10.0f))
	{
		return;
	}
	const FString Key = ProfileKey();
	int32 Value = 0;
	const TCHAR* Name = nullptr;
	if (const DCCClimbItems::FItem* Climb = DCCClimbItems::Find(ArtifactId))
	{
		if (Climb->Kind == DCCClimbItems::EKind::Artifact && Climb->SellValue > 0)
		{
			Value = Climb->SellValue;
			Name = Climb->Name;
		}
	}
	if (!Name)
	{
		if (const DCCRallyItems::FItem* Rally = DCCRallyItems::Find(ArtifactId))
		{
			if (Rally->Kind == DCCRallyItems::EKind::Seized && Rally->SellValue > 0)
			{
				Value = Rally->SellValue;
				Name = Rally->Name;
			}
		}
	}
	if (Key.IsEmpty() || !Name || !DCCProfile::TakeTrophy(Key, ArtifactId))
	{
		return;
	}
	Tickets = DCCProfile::AddTickets(Key, Value);
	RefreshTrophies();
	Client_Message(FString::Printf(TEXT("Sold %s for %d tickets."), Name, Value), false);
}

void UDCCWalletComponent::LoadFromProfile()
{
	const FString Key = ProfileKey();
	if (Key.IsEmpty())
	{
		return;
	}
	LoadedKey = Key;
	PushToClients(Key);
}

void UDCCWalletComponent::Server_Spray_Implementation(FVector At, FVector Normal, FVector ViewDir, const TArray<uint8>& RGB)
{
	APawn* Crawler = Cast<APawn>(GetOwner());
	DCCSpray::ServerSpray(Crawler, Crawler ? Crawler->GetPlayerState() : nullptr, At, Normal, ViewDir, RGB);
}

void UDCCWalletComponent::Server_Buy_Implementation(FName CosmeticId)
{
	if (!DCCIntegrity::Allow(Cast<ADCCCharacterBase>(GetOwner()), TEXT("shop"), 5.0f, 10.0f))
	{
		return;
	}
	const FString Key = ProfileKey();
	if (Key.IsEmpty())
	{
		return;
	}
	// The spray can (DCCSpray): owned on the profile like a cosmetic, but worn by nobody. The picture is picked on the
	// buyer's own PC before this is sent.
	if (CosmeticId == DCCSpray::CanId)
	{
		if (DCCProfile::GetCosmetics(Key).Contains(CosmeticId))
		{
			Client_Message(TEXT("New picture saved. Spray it from your belt: press its number, then click."), false);
			return;
		}
		if (!DCCProfile::SpendTickets(Key, DCCSpray::Price))
		{
			Client_Message(FString::Printf(TEXT("The spray can costs %d tickets. You have %d."), DCCSpray::Price, DCCProfile::GetTickets(Key)), false);
			return;
		}
		Tickets = DCCProfile::GetTickets(Key);
		DCCProfile::GrantCosmetic(Key, CosmeticId);
		Owned = DCCProfile::GetCosmetics(Key);
		Client_Message(TEXT("Bought a spray can. It is on your belt: press its number, then click whatever you are looking at. From a truck, press Z."), false);
		return;
	}
	if (const DCCCosmetics::FBoothWeapon* Gun = DCCCosmetics::BoothWeapons().FindByPredicate(
		[CosmeticId](const DCCCosmetics::FBoothWeapon& Row) { return Row.Id == CosmeticId; }))
	{
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
		if (!Crawler || !Crawler->Inventory || !Crawler->Equipment)
		{
			return;
		}
		auto OwnsIt = [Crawler, CosmeticId]()
		{
			if (Crawler->Equipment->GetEquippedId(EDCCEquipmentSlot::MainHand) == CosmeticId)
			{
				return true;
			}
			for (const FDCCInventoryItem& Row : Crawler->Inventory->Items)
			{
				if (Row.EquipmentItemId == CosmeticId)
				{
					return true;
				}
			}
			return false;
		};
		// Booth guns are owned on the profile, like cosmetics (2026-11-14). They used to live only as one
		// inventory row plus the main hand, so buying a second gun replaced the first and it was gone for good.
		// A gun already in hand or in the bag from before counts as owned, so nothing bought is lost.
		if (DCCProfile::GetCosmetics(Key).Contains(CosmeticId) || OwnsIt())
		{
			DCCProfile::GrantCosmetic(Key, CosmeticId);
			Owned = DCCProfile::GetCosmetics(Key);
			Crawler->Equipment->Equip(CosmeticId);
			DCCSave::Save(Crawler);
			Client_Message(FString::Printf(TEXT("The %s is in your hands. Left click shoots. Right mouse aims."), Gun->Name), false);
			return;
		}
		FDCCEquipmentDef Def;
		if (!Crawler->Equipment->FindItem(CosmeticId, Def))
		{
			Client_Message(FString::Printf(TEXT("%s is not on the shelf yet."), Gun->Name), false);
			return;
		}
		if (!DCCProfile::SpendTickets(Key, Gun->Price))
		{
			Client_Message(FString::Printf(TEXT("%s costs %d tickets. You have %d."), Gun->Name, Gun->Price, DCCProfile::GetTickets(Key)), false);
			return;
		}
		Tickets = DCCProfile::GetTickets(Key);
		DCCProfile::GrantCosmetic(Key, CosmeticId);
		Owned = DCCProfile::GetCosmetics(Key);
		Crawler->Equipment->Equip(CosmeticId);
		DCCSave::Save(Crawler);
		Client_Message(FString::Printf(TEXT("Bought %s. Left click shoots. Right mouse aims."), Gun->Name), false);
		return;
	}
	const DCCCosmetics::FItem* Item = DCCCosmetics::Find(CosmeticId);
	if (!Item)
	{
		return;
	}
	if (DCCProfile::GetCosmetics(Key).Contains(CosmeticId))
	{
		Server_Wear_Implementation(CosmeticId);
		return;
	}
	if (!DCCProfile::SpendTickets(Key, Item->Price))
	{
		Client_Message(FString::Printf(TEXT("%s costs %d tickets. You have %d."), Item->Name, Item->Price, DCCProfile::GetTickets(Key)), false);
		return;
	}
	DCCProfile::GrantCosmetic(Key, CosmeticId);
	if (Item->Slot == TEXT("Head"))
	{
		StowHeadGear();
	}
	DCCProfile::SetWearing(Key, Item->Slot, CosmeticId); // bought it, so put it on
	PushToClients(Key);
	Client_Message(FString::Printf(TEXT("Bought %s. You're wearing it."), Item->Name), false);
}

void UDCCWalletComponent::Server_Wear_Implementation(FName CosmeticId)
{
	const FString Key = ProfileKey();
	const DCCCosmetics::FItem* Item = DCCCosmetics::Find(CosmeticId);
	if (Key.IsEmpty() || !Item || !DCCProfile::GetCosmetics(Key).Contains(CosmeticId))
	{
		return;
	}
	if (Item->Slot == TEXT("Head"))
	{
		StowHeadGear();
	}
	DCCProfile::SetWearing(Key, Item->Slot, CosmeticId);
	PushToClients(Key);
	Client_Message(FString::Printf(TEXT("Wearing %s."), Item->Name), false);
}

void UDCCWalletComponent::StowHeadGear()
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	if (!Crawler || !Crawler->Equipment || !Crawler->Inventory)
	{
		return;
	}
	const FName HeadId = Crawler->Equipment->GetEquippedId(EDCCEquipmentSlot::Head);
	if (HeadId.IsNone())
	{
		return;
	}
	FDCCEquipmentDef Def;
	const bool bFound = Crawler->Equipment->FindItem(HeadId, Def);
	const FString Name = (bFound && !Def.DisplayName.IsEmpty()) ? Def.DisplayName.ToString() : HeadId.ToString();
	// Unequip puts it in the bag. Adding it here first would leave two copies.
	if (Crawler->Equipment->Unequip(EDCCEquipmentSlot::Head))
	{
		Client_Message(FString::Printf(TEXT("Your %s went in your bag."), *Name), true);
	}
}

void UDCCWalletComponent::OnGearChanged(UDCCEquipmentComponent* Changed)
{
	const FString Key = ProfileKey();
	if (Key.IsEmpty() || !Changed || Changed->GetEquippedId(EDCCEquipmentSlot::Head).IsNone())
	{
		return;
	}
	const FName Hat = DCCProfile::GetWearing(Key).FindRef(TEXT("Head"));
	if (Hat.IsNone())
	{
		return;
	}
	DCCProfile::SetWearing(Key, TEXT("Head"), NAME_None);
	PushToClients(Key);
	const DCCCosmetics::FItem* Item = DCCCosmetics::Find(Hat);
	Client_Message(FString::Printf(TEXT("Took off your %s. One thing on your head at a time."), Item ? Item->Name : TEXT("hat")), true);
}

void UDCCWalletComponent::Server_TakeOff_Implementation(FName Slot)
{
	const FString Key = ProfileKey();
	if (Key.IsEmpty())
	{
		return;
	}
	DCCProfile::SetWearing(Key, Slot, NAME_None);
	PushToClients(Key);
	Client_Message(FString::Printf(TEXT("Took off your %s."), *DCCCosmetics::SlotLabel(Slot).ToLower()), false);
}

void UDCCWalletComponent::Server_SetMineLamp_Implementation(bool bOn)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (!State)
	{
		return;
	}
	for (TActorIterator<ADCCArcadeHaunt> It(GetWorld()); It; ++It)
	{
		It->SetLamp(State->GetPlayerName(), bOn);
	}
	Client_Message(bOn ? TEXT("Lamp on.") : TEXT("Lamp off. Walk carefully."), true);
}

void UDCCWalletComponent::Server_ArcadeVote_Implementation(bool bPlayAgain)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}
	for (TActorIterator<ADCCArcadeArena> It(GetWorld()); It; ++It)
	{
		It->CastVote(PC, bPlayAgain);
	}
}

void UDCCWalletComponent::AwardTickets(APlayerState* PlayerState, int32 Amount, const FString& Why)
{
	const FString Key = DCCProfile::KeyFor(PlayerState);
	if (Key.IsEmpty() || Amount <= 0)
	{
		return;
	}
	const int32 Balance = DCCProfile::AddTickets(Key, Amount);
	const FString Line = FString::Printf(TEXT("+%d tickets: %s"), Amount, *Why);
	if (APawn* Pawn = PlayerState->GetPawn())
	{
		if (UDCCWalletComponent* Wallet = Pawn->FindComponentByClass<UDCCWalletComponent>())
		{
			Wallet->Tickets = Balance;
			Wallet->Client_Message(Line, true);
		}
		else if (ADCCRover* Rover = Cast<ADCCRover>(Pawn))
		{
			// Seated: the crawler is unpossessed, so a wallet RPC on them never arrives.
			// The car is what the driver is possessing.
			if (Rover->Driver && Rover->Driver->Wallet)
			{
				Rover->Driver->Wallet->Tickets = Balance;
			}
			Rover->Client_RallyNote(Line);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Tickets] %s +%d (%s) = %d"), *Key, Amount, *Why, Balance);
}

void UDCCWalletComponent::Client_Message_Implementation(const FString& Message, bool bNotice)
{
	ShopMessage = Message;
	if (const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner()))
	{
		Crawler->ShowLocalNotice(FText::FromString(Message));
	}
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void UDCCWalletComponent::OnRep_Worn()
{
	RebuildVisuals();
}

void UDCCWalletComponent::ApplyOutfit(const TArray<FName>& CosmeticIds)
{
	Worn.Reset();
	for (const FName& Id : CosmeticIds)
	{
		if (const DCCCosmetics::FItem* Item = DCCCosmetics::Find(Id))
		{
			Worn.Add({ Item->Slot, Id });
		}
	}
	RebuildVisuals();
}

void UDCCWalletComponent::RebuildVisuals()
{
	for (FLivePart& Live : LiveParts)
	{
		if (UStaticMeshComponent* Part = Live.Component.Get())
		{
			Part->DestroyComponent();
		}
	}
	LiveParts.Reset();

	AActor* Owner = GetOwner();
	if (!Owner || (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer))
	{
		return;
	}

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	for (const FDCCWornCosmetic& Entry : Worn)
	{
		const DCCCosmetics::FItem* Item = DCCCosmetics::Find(Entry.Id);
		if (!Item)
		{
			continue;
		}
		for (const DCCCosmetics::FPart& Part : Item->Parts)
		{
			UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
			Component->SetStaticMesh(ShapeMesh(Part.Shape));
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCastShadow(false);
			Component->SetGenerateOverlapEvents(false);
			Component->SetUsingAbsoluteLocation(true);
			Component->SetUsingAbsoluteRotation(true);
			Component->SetUsingAbsoluteScale(true);
			Component->SetupAttachment(Owner->GetRootComponent());
			Component->RegisterComponent();
			Component->SetWorldScale3D(Part.Size / 100.0f);
			if (Base)
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Component);
				Mid->SetVectorParameterValue(TEXT("Color"), Part.Colour);
				Mid->SetVectorParameterValue(TEXT("BaseColor"), Part.Colour);
				Component->SetMaterial(0, Mid);
			}
			LiveParts.Add({ Component, Part, Item->Anchor, Item->Slot == DCCCosmetics::Aura, FName(*(FString(TEXT("Cosmetic.")) + Entry.Id.ToString())) });
		}
	}
	SetComponentTickEnabled(!LiveParts.IsEmpty());
	UpdateVisuals();
}

void UDCCWalletComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateVisuals();
}

bool UDCCWalletComponent::BountyDone(FName Id) const
{
	const int32 i = DCCBounties::All().IndexOfByPredicate([Id](const DCCBounties::FBounty& B) { return B.Id == Id; });
	return i != INDEX_NONE && (BountiesDone & (1 << i)) != 0;
}

bool UDCCWalletComponent::BountyPaid(FName Id) const
{
	const int32 i = DCCBounties::All().IndexOfByPredicate([Id](const DCCBounties::FBounty& B) { return B.Id == Id; });
	return i != INDEX_NONE && (BountiesPaid & (1 << i)) != 0;
}

void UDCCWalletComponent::RefreshBounties()
{
	const FString Key = ProfileKey();
	if (Key.IsEmpty())
	{
		return;
	}
	// Shipped voice lines (DCCVoiceCredits above). A visitor only once their profile has merged here, so the payment
	// cannot be overwritten by the copy their PC brings.
	if (const APawn* Pawn = Cast<APawn>(GetOwner()); Pawn && (Pawn->IsLocallyControlled() || bProfileSent))
	{
		DCCVoiceCredits::PayOut(Pawn->GetPlayerState(), Key);
	}
	int32 Done = 0, Paid = 0;
	const TArray<DCCBounties::FBounty>& All = DCCBounties::All();
	for (int32 i = 0; i < All.Num() && i < 31; ++i)
	{
		if (DCCBounties::IsDone(Key, All[i].Id)) { Done |= 1 << i; }
		if (DCCBounties::IsPaid(Key, All[i].Id)) { Paid |= 1 << i; }
	}
	// Tell them the moment one finishes (not on the first read, or every join would announce old ones).
	if (bBountiesRead)
	{
		const int32 New = Done & ~BountiesDone & ~Paid;
		for (int32 i = 0; i < All.Num() && i < 31; ++i)
		{
			if (New & (1 << i))
			{
				Tell(FString::Printf(TEXT("Bounty done: %s (+%d tickets). Claim it at the Prize Counter, BOUNTIES tab."), All[i].Name, All[i].Reward));
				if (ADCCCharacterBase* Owner = Cast<ADCCCharacterBase>(GetOwner()))
				{
					Owner->ReactTo(DCCReactions::MomentWin, false);
				}
			}
		}
	}
	bBountiesRead = true;
	BountiesDone = Done;
	BountiesPaid = Paid;
}

void UDCCWalletComponent::Server_ClaimBounty_Implementation(FName BountyId)
{
	if (!DCCIntegrity::Allow(Cast<ADCCCharacterBase>(GetOwner()), TEXT("shop"), 5.0f, 10.0f))
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && DCCBounties::Claim(Pawn->GetPlayerState(), BountyId))
	{
		Tickets = DCCProfile::GetTickets(ProfileKey());
		RefreshBounties();
	}
}

void UDCCWalletComponent::UpdateVisuals()
{
	const AActor* Owner = GetOwner();
	if (!Owner || LiveParts.IsEmpty())
	{
		return;
	}

	// Anchors follow the bones (so a hat bobs with the head), but every part is
	// placed in the character's own facing, never the bone's: the Meshy bodies'
	// bones all point different ways.
	const USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	const FVector Root = Owner->GetActorLocation();
	auto SocketOr = [Mesh](FName Socket, const FVector& Fallback)
	{
		return Mesh && Mesh->DoesSocketExist(Socket) ? Mesh->GetSocketLocation(Socket) : Fallback;
	};
	// How far the head (and upper back) have turned from their rest pose, in
	// world space (2026-11-13: the crown used to stay level while the head turned).
	// A delta from rest rather than the bone's own rotation, because each Meshy
	// body's bones point a different way at rest.
	const USkeletalMesh* RestAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (RestAsset != RefAsset.Get())
	{
		RefAsset = RestAsset;
		HeadBone = Mesh ? Mesh->GetSocketBoneName(TEXT("headSocket")) : NAME_None;
		BackBone = Mesh ? Mesh->GetSocketBoneName(TEXT("spine_03Socket")) : NAME_None;
		RefHead = RefBack = FQuat::Identity;
		if (RestAsset)
		{
			const FReferenceSkeleton& Ref = RestAsset->GetRefSkeleton();
			const int32 H = Ref.FindBoneIndex(HeadBone), B = Ref.FindBoneIndex(BackBone);
			if (H != INDEX_NONE) { RefHead = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, H).GetRotation(); }
			if (B != INDEX_NONE) { RefBack = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, B).GetRotation(); }
		}
	}
	auto TurnedFromRest = [Mesh](FName Bone, const FQuat& Rest) -> FQuat
	{
		if (!Mesh || Bone.IsNone() || Mesh->GetBoneIndex(Bone) == INDEX_NONE)
		{
			return FQuat::Identity;
		}
		const FQuat Now = Mesh->GetSocketTransform(Bone, RTS_Component).GetRotation();
		const FQuat Component = Mesh->GetComponentQuat();
		return Component * (Now * Rest.Inverse()) * Component.Inverse();
	};
	const FQuat HeadTurn = TurnedFromRest(HeadBone, RefHead);
	const FQuat BackTurn = TurnedFromRest(BackBone, RefBack);
	const FVector HeadTop = SocketOr(TEXT("headSocket"), Root + FVector(0, 0, 70)) + HeadTurn.RotateVector(FVector(0, 0, 18));
	const FVector UpperBack = SocketOr(TEXT("spine_03Socket"), Root + FVector(0, 0, 40));
	const FRotator Facing(0.0f, Owner->GetActorRotation().Yaw, 0.0f);
	const FQuat FacingQuat(Facing);
	const float Spin = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) * 70.0f : 0.0f;
	const USkeletalMesh* BodyAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FString BodyPath = BodyAsset ? BodyAsset->GetPathName() : FString();

	for (const FLivePart& Live : LiveParts)
	{
		UStaticMeshComponent* Component = Live.Component.Get();
		if (!Component)
		{
			continue;
		}
		FVector Offset = Live.Part.Offset;
		if (Live.bSpins)
		{
			Offset = FRotator(0.0f, Spin, 0.0f).RotateVector(Offset);
		}
		const bool bHead = Live.Anchor == DCCCosmetics::EAnchor::HeadTop;
		const FVector Anchor = bHead ? HeadTop : UpperBack;
		// Auras circle the body and stay level; everything else turns with its bone.
		const FQuat Turn = Live.bSpins ? FQuat::Identity : (bHead ? HeadTurn : BackTurn);

		// The gear bench's nudge for this item (this body first, then every body):
		// moves, turns and scales the whole item about its anchor, in the character's facing.
		DCCBench::FNudge Nudge;
		DCCBench::FindGrip(BodyPath, Live.BenchKey, Nudge);
		const FQuat NudgeQuat(Nudge.Rotation);
		const FVector Local = Nudge.Location + NudgeQuat.RotateVector(Offset * Nudge.Scale);
		Component->SetWorldScale3D(Live.Part.Size / 100.0f * Nudge.Scale);
		Component->SetWorldLocationAndRotation(Anchor + Turn.RotateVector(FacingQuat.RotateVector(Local)), Turn * FacingQuat * NudgeQuat * FQuat(Live.Part.Rotation));
	}
}

// ---------------------------------------------------------------------------
// The Chop Shop and the crawler password (2026-11-14)
// ---------------------------------------------------------------------------

int32 UDCCWalletComponent::ChopLevel(FName UpgradeId) const
{
	const TArray<DCCChopShop::FUpgrade>& All = DCCChopShop::All();
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (All[i].Id == UpgradeId)
		{
			return ChopLevels.IsValidIndex(i) ? ChopLevels[i] : 0;
		}
	}
	return 0;
}

void UDCCWalletComponent::Server_BuyChop_Implementation(FName UpgradeId)
{
	if (!DCCIntegrity::Allow(Cast<ADCCCharacterBase>(GetOwner()), TEXT("shop"), 5.0f, 10.0f))
	{
		return;
	}
	const FString Key = ProfileKey();
	if (Key.IsEmpty())
	{
		return;
	}
	FString Message;
	DCCChopShop::BuyNext(Key, UpgradeId, Message);
	PushToClients(Key);
	if (!Message.IsEmpty())
	{
		Client_Message(Message, false);
	}
}

void UDCCWalletComponent::AwardCredits(APlayerState* PlayerState, int32 Amount, const FString& Why)
{
	const FString Key = DCCProfile::KeyFor(PlayerState);
	if (Key.IsEmpty() || Amount <= 0)
	{
		return;
	}
	const int32 Balance = DCCChopShop::AddCredits(Key, Amount);
	if (APawn* Pawn = PlayerState->GetPawn())
	{
		if (UDCCWalletComponent* Wallet = Pawn->FindComponentByClass<UDCCWalletComponent>())
		{
			Wallet->MineCredits = Balance;
			Wallet->Client_Message(FString::Printf(TEXT("+%d credits: %s"), Amount, *Why), true);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Credits] %s +%d (%s) = %d"), *Key, Amount, *Why, Balance);
}

void UDCCWalletComponent::Server_ThrowLightstick_Implementation(FVector Eye, FVector Dir)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (!State)
	{
		return;
	}
	for (TActorIterator<ADCCArcadeHaunt> It(GetWorld()); It; ++It)
	{
		It->ThrowLightstick(State->GetPlayerName(), Eye, Dir.GetSafeNormal());
	}
}

void UDCCWalletComponent::AwardScrap(APlayerState* PlayerState, int32 Amount, const FString& Why)
{
	const FString Key = DCCProfile::KeyFor(PlayerState);
	if (Key.IsEmpty() || Amount <= 0)
	{
		return;
	}
	const int32 Balance = DCCChopShop::AddScrap(Key, Amount);
	const FString Line = FString::Printf(TEXT("+%d scrap: %s"), Amount, *Why);
	if (APawn* Pawn = PlayerState->GetPawn())
	{
		if (UDCCWalletComponent* Wallet = Pawn->FindComponentByClass<UDCCWalletComponent>())
		{
			Wallet->Scrap = Balance;
			Wallet->Client_Message(Line, true);
		}
		else if (ADCCRover* Rover = Cast<ADCCRover>(Pawn))
		{
			// Seated: the crawler is unpossessed, so the note goes through the car (see AwardTickets).
			if (Rover->Driver && Rover->Driver->Wallet)
			{
				Rover->Driver->Wallet->Scrap = Balance;
			}
			Rover->Client_RallyNote(Line);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Scrap] %s +%d (%s) = %d"), *Key, Amount, *Why, Balance);
}

void UDCCWalletComponent::Server_ChangePassword_Implementation(const FString& OldHash, const FString& NewHash)
{
	const FString Key = ProfileKey();
	const bool bChanged = !Key.IsEmpty() && DCCProfile::ChangePassword(Key, OldHash, NewHash);
	UE_LOG(LogTemp, Log, TEXT("[DCC Crawlers] password change for %s: %s"), *Key, bChanged ? TEXT("done") : TEXT("refused"));
	Client_PasswordResult(bChanged, bChanged ? NewHash : FString());
}

void UDCCWalletComponent::Client_PasswordResult_Implementation(bool bChanged, const FString& NewHash)
{
	bPasswordChanged = bChanged;
	PasswordMessage = bChanged ? TEXT("Password changed. This PC will remember it.") : TEXT("That is not your current password.");
	if (bChanged)
	{
		// What this PC sends the host from now on.
		DCCSession::SavePasswordHash(DCCSession::GetPreferredCrawler(), NewHash);
	}
}

void UDCCWalletComponent::Server_BuySupply_Implementation(FName ItemId)
{
	if (!DCCIntegrity::Allow(Cast<ADCCCharacterBase>(GetOwner()), TEXT("shop"), 5.0f, 10.0f))
	{
		return;
	}
	const FString Key = ProfileKey();
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	const DCCClimbItems::FItem* Item = DCCClimbItems::Find(ItemId);
	const int32 Price = DCCClimbItems::SupplyPrice(ItemId);
	ADCCArcadeClimb* Climb = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADCCArcadeClimb> It(World); It; ++It) { Climb = *It; break; }
	}
	if (Key.IsEmpty() || !Crawler || !Item || Price <= 0)
	{
		return;
	}
	if (!Climb)
	{
		Client_Message(TEXT("The Trading Post is in Fort Tiner, in a Fourteener round."), true);
		return;
	}
	if (!Climb->IsAtTradingPost(Crawler->GetActorLocation()))
	{
		Client_Message(TEXT("Walk back to the Trading Post in Fort Tiner to buy."), true);
		return;
	}
	if (!DCCProfile::SpendTickets(Key, Price))
	{
		Client_Message(FString::Printf(TEXT("%s is %d tickets. Not enough."), Item->Name, Price), true);
		return;
	}
	Tickets = DCCProfile::GetTickets(Key);
	Climb->GiveItem(Crawler, ItemId, true);
	Client_Message(FString::Printf(TEXT("Bought: %s (-%d tickets). %s"), Item->Name, Price, Item->Blurb), false);
}

void UDCCWalletComponent::Server_CageVote_Implementation(bool bUp)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	for (TActorIterator<ADCCArcadeHaunt> It(GetWorld()); It; ++It)
	{
		It->VoteCageUp(PC, bUp);
		break;
	}
}

void UDCCWalletComponent::Client_OfferStarterKit_Implementation()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		Hud->OpenStarterKit();
	}
}

void UDCCWalletComponent::Server_ChooseStarterKit_Implementation(int32 Index)
{
	DCCStarterKits::Grant(Cast<ADCCCharacterBase>(GetOwner()), Index);
}

void UDCCWalletComponent::Server_Grapple_Implementation()
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	for (TActorIterator<ADCCArcadeClimb> It(GetWorld()); It && Crawler; ++It)
	{
		It->BeltGrapple(Crawler);
		return;
	}
}

void UDCCWalletComponent::Client_GrappleThrown_Implementation(float Seconds)
{
	GrappleReadyLocal = GetWorld() ? GetWorld()->GetTimeSeconds() + Seconds : 0.0f;
}

// ---- The summoning tubes (2026-11-17, Arcade/DCCSummons.h) ----------------------------------------------------------

void UDCCWalletComponent::Server_Summon_Implementation(uint8 Tube)
{
	if (!DCCIntegrity::Allow(Cast<ADCCCharacterBase>(GetOwner()), TEXT("summon"), 1.0f, 3.0f))
	{
		return;
	}
	DoSummon(Tube, false);
}

void UDCCWalletComponent::DoSummon(uint8 Tube, bool bFree)
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	UWorld* World = GetWorld();
	if (!Crawler || !World || !Crawler->IsAlive())
	{
		return;
	}
	ADCCArcadeLobby* Lobby = nullptr;
	for (TActorIterator<ADCCArcadeLobby> It(World); It; ++It) { Lobby = *It; break; }
	if (!Lobby || Tube >= ADCCArcadeLobby::TubeCount || (!bFree && Lobby->PodiumNear(Crawler->GetActorLocation()) != int32(Tube)))
	{
		return; // only at that tube's podium
	}
	const FString Key = DCCProfile::KeyFor(Crawler->GetPlayerState());
	TArray<DCCSummons::FSummon>& Mine = DCCSummons::Party().FindOrAdd(Key);
	if (Mine.Num() >= DCCSummons::MaxAllies)
	{
		Client_Message(FString::Printf(TEXT("You already have %d allies with you. One has to fall before another comes down."), DCCSummons::MaxAllies), false);
		return;
	}
	if (Lobby->IsTubeBusy(Tube))
	{
		Client_Message(TEXT("That tube is still busy. Give it a second."), false);
		return;
	}
	if (!bFree && !DCCProfile::SpendTickets(Key, DCCSummons::Price))
	{
		Client_Message(FString::Printf(TEXT("A summon costs %d tickets. You have %d."), DCCSummons::Price, DCCProfile::GetTickets(Key)), false);
		return;
	}
	Tickets = DCCProfile::GetTickets(Key);

	TArray<uint8> Tiers = Lobby->FriendTiers;
	if (Tiers.IsEmpty())
	{
		DCCSummons::ComputeTiers(Tiers);
	}
	const DCCSummons::FSummon Pull = DCCSummons::Roll(Tiers, Key, Crawler->Equipment ? Crawler->Equipment->EquipmentTable.Get() : nullptr);
	if (Pull.Friend.IsNone())
	{
		if (!bFree) { DCCProfile::AddTickets(Key, DCCSummons::Price); } // nobody to summon: give the tickets back
		Tickets = DCCProfile::GetTickets(Key);
		return;
	}
	Lobby->StartTube(Tube, Pull.Tier);
	if (ADCCSummonedAlly* Ally = DCCSummons::SpawnAlly(World, Pull, Crawler, Lobby->TubeCenter(Tube) + FVector(0, 0, 70.0f), 640.0f, Lobby->TubeExit(Tube)))
	{
		Mine.Add(Pull);
		DCCSummons::Track(World, Key, Ally);
	}
	Client_SummonResult(Pull.Friend, Pull.Tier, Pull.Variant, Pull.Weapon);
	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::FromString(FString::Printf(TEXT("%s pulled a %s%s %s out of F.I.A.T."),
		*Key, Pull.Variant ? *FString(DCCSummons::VariantName(Pull.Variant)).ToUpper().Append(TEXT(" ")) : TEXT(""), DCCSummons::TierName(Pull.Tier), *Pull.Friend.ToString())));
}

const FName UDCCWalletComponent::HotMicId(TEXT("HotMic"));

void UDCCWalletComponent::RefreshFromProfile()
{
	PushToClients(ProfileKey());
}

void UDCCWalletComponent::Server_HotMic_Implementation()
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!Crawler || !GS || !Owns(HotMicId) || !Crawler->IsAlive())
	{
		return;
	}
	const double Now = GS->GetServerWorldTimeSeconds();
	if (Now < HotMicReadyAt)
	{
		return;
	}
	HotMicLiveUntil = Now + 8.0;
	HotMicReadyAt = Now + 45.0;
	Crawler->Multicast_HotMic(false);
	DCCAudience::Moment(Crawler, Crawler, DCCAudience::EMoment::Trick);
}

bool UDCCWalletComponent::ConsumeHotMic()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || HotMicLiveUntil <= 0.0 || GS->GetServerWorldTimeSeconds() > HotMicLiveUntil)
	{
		return false;
	}
	HotMicLiveUntil = 0.0;
	return true;
}

void UDCCWalletComponent::Client_FameLevel_Implementation(int32 Level, uint8 Tier)
{
	const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	const APlayerController* PC = Crawler ? Cast<APlayerController>(Crawler->GetController()) : nullptr;
	if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		static const TCHAR* Names[] = { TEXT("?"), TEXT("Bronze"), TEXT("Silver"), TEXT("Gold"), TEXT("Platinum"), TEXT("Legendary"), TEXT("Celestial") };
		Hud->PushAchievement(FString::Printf(TEXT("FAME LEVEL %d"), Level),
			FString::Printf(TEXT("%s loot box. Open it from Tab, LOOT BOXES."), Names[FMath::Clamp<int32>(Tier, 0, 6)]));
	}
}

void UDCCWalletComponent::Server_SummonTen_Implementation(uint8 Tube)
{
	DoSummonTen(Tube, false);
}

void UDCCWalletComponent::DoSummonTen(uint8 Tube, bool bFree)
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	UWorld* World = GetWorld();
	if (!Crawler || !World || !Crawler->IsAlive() || !DCCIntegrity::Allow(Crawler, TEXT("summon"), 1.0f, 3.0f))
	{
		return;
	}
	ADCCArcadeLobby* Lobby = nullptr;
	for (TActorIterator<ADCCArcadeLobby> It(World); It; ++It) { Lobby = *It; break; }
	if (!Lobby || Tube >= ADCCArcadeLobby::TubeCount || (!bFree && Lobby->PodiumNear(Crawler->GetActorLocation()) != int32(Tube)))
	{
		return;
	}
	const FString Key = DCCProfile::KeyFor(Crawler->GetPlayerState());
	const int32 Free = DCCSummons::MaxAllies - DCCSummons::Party().FindOrAdd(Key).Num();
	if (Free <= 0)
	{
		Client_Message(TEXT("Your party is full. One has to fall before another comes down."), false);
		return;
	}
	if (TenTube >= 0)
	{
		Client_Message(TEXT("Ten friends are already waiting in the tube. Pick who stays first."), false);
		return;
	}
	if (!bFree && !DCCProfile::SpendTickets(Key, DCCSummons::TenPrice))
	{
		Client_Message(FString::Printf(TEXT("A ten-pull costs %d tickets. You have %d."), DCCSummons::TenPrice, DCCProfile::GetTickets(Key)), false);
		return;
	}
	Tickets = DCCProfile::GetTickets(Key);

	TArray<uint8> Tiers = Lobby->FriendTiers;
	if (Tiers.IsEmpty())
	{
		DCCSummons::ComputeTiers(Tiers);
	}
	const UDataTable* Table = Crawler->Equipment ? Crawler->Equipment->EquipmentTable.Get() : nullptr;
	TArray<DCCSummons::FSummon> Pulls;
	bool bRare = false;
	for (int32 i = 0; i < 10; ++i)
	{
		Pulls.Add(DCCSummons::Roll(Tiers, Key, Table));
		bRare |= Pulls.Last().Tier >= 1;
	}
	// The guarantee: if nothing came up Rare or better, the last one is rerolled until it does.
	for (int32 Try = 0; !bRare && Try < 60; ++Try)
	{
		Pulls[9] = DCCSummons::Roll(Tiers, Key, Table);
		bRare = Pulls[9].Tier >= 1;
	}
	if (Pulls[0].Friend.IsNone())
	{
		if (!bFree) { DCCProfile::AddTickets(Key, DCCSummons::TenPrice); } // nobody to summon: the tickets back
		Tickets = DCCProfile::GetTickets(Key);
		return;
	}
	TenFriends.Reset(); TenTiers.Reset(); TenVariants.Reset(); TenWeapons.Reset();
	for (const DCCSummons::FSummon& P : Pulls)
	{
		TenFriends.Add(P.Friend);
		TenTiers.Add(P.Tier);
		TenVariants.Add(P.Variant);
		TenWeapons.Add(P.Weapon);
	}
	TenTube = Tube;
	Client_SummonTen(TenFriends, TenTiers, TenVariants, TenWeapons, Free);
	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::FromString(FString::Printf(
		TEXT("%s fed F.I.A.T. %d tickets at once. Ten friends are crammed in the tube, waiting to hear who gets to stay."), *Key, DCCSummons::TenPrice)));
}

void UDCCWalletComponent::Server_KeepSummons_Implementation(const TArray<uint8>& Keep)
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	UWorld* World = GetWorld();
	if (!Crawler || !World || TenTube < 0 || TenFriends.Num() != 10)
	{
		return;
	}
	ADCCArcadeLobby* Lobby = nullptr;
	for (TActorIterator<ADCCArcadeLobby> It(World); It; ++It) { Lobby = *It; break; }
	const uint8 Tube = uint8(TenTube);
	TenTube = -1;
	const FString Key = DCCProfile::KeyFor(Crawler->GetPlayerState());
	TArray<DCCSummons::FSummon>& Mine = DCCSummons::Party().FindOrAdd(Key);
	TSet<uint8> Picked;
	uint8 Best = 0;
	int32 Kept = 0;
	for (const uint8 Index : Keep)
	{
		if (Index >= 10 || Picked.Contains(Index) || Mine.Num() >= DCCSummons::MaxAllies || !Lobby)
		{
			continue;
		}
		Picked.Add(Index);
		DCCSummons::FSummon Pull;
		Pull.Friend = TenFriends[Index];
		Pull.Tier = TenTiers[Index];
		Pull.Variant = TenVariants[Index];
		Pull.Weapon = TenWeapons[Index];
		const FVector Spot = Lobby->TubeCenter(Tube) + FVector(0, 0, 70.0f) + FVector(Kept * 90.0f, 0.0f, 0.0f);
		if (ADCCSummonedAlly* Ally = DCCSummons::SpawnAlly(World, Pull, Crawler, Spot, 640.0f + Kept * 200.0f, Lobby->TubeExit(Tube)))
		{
			Mine.Add(Pull);
			DCCSummons::Track(World, Key, Ally);
			Client_SummonResult(Pull.Friend, Pull.Tier, Pull.Variant, Pull.Weapon);
			Best = FMath::Max(Best, Pull.Tier);
			++Kept;
		}
	}
	if (Lobby && Kept > 0)
	{
		Lobby->StartTube(Tube, Best);
	}
	const int32 Gone = 10 - Kept;
	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::FromString(Kept > 0
		? FString::Printf(TEXT("%s kept %d. The other %d evaporated with a small, polite pop. The tube smells like ozone and regret."), *Key, Kept, Gone)
		: FString::Printf(TEXT("%s kept nobody. All ten evaporated. The audience is not sure whether that was cold or strategic."), *Key)));
	TenFriends.Reset(); TenTiers.Reset(); TenVariants.Reset(); TenWeapons.Reset();
}

void UDCCWalletComponent::Client_SummonTen_Implementation(const TArray<FName>& Friends, const TArray<uint8>& Tiers, const TArray<uint8>& Variants,
	const TArray<FName>& Weapons, int32 Slots)
{
	const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	const APlayerController* PC = Crawler ? Cast<APlayerController>(Crawler->GetController()) : nullptr;
	if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		TArray<FString> WeaponNames;
		for (const FName& Weapon : Weapons)
		{
			FDCCEquipmentDef Def;
			WeaponNames.Add(!Weapon.IsNone() && Crawler->Equipment && Crawler->Equipment->FindItem(Weapon, Def)
				? (Def.DisplayName.IsEmpty() ? Weapon.ToString() : Def.DisplayName.ToString()) : FString());
		}
		Hud->ShowSummonTen(Friends, Tiers, Variants, WeaponNames, Slots);
	}
}

void UDCCWalletComponent::Client_SummonResult_Implementation(FName Friend, uint8 Tier, uint8 Variant, FName Weapon)
{
	const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	const APlayerController* PC = Crawler ? Cast<APlayerController>(Crawler->GetController()) : nullptr;
	if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		FString WeaponName;
		FDCCEquipmentDef Def;
		if (!Weapon.IsNone() && Crawler->Equipment && Crawler->Equipment->FindItem(Weapon, Def))
		{
			WeaponName = Def.DisplayName.IsEmpty() ? Weapon.ToString() : Def.DisplayName.ToString();
		}
		Hud->ShowSummonResult(Friend, Tier, Variant, WeaponName);
	}
}
