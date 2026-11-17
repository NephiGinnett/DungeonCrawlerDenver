#include "Arcade/DCCWalletComponent.h"
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
	if (DCCProfile::MergeFromClient(Key, Current, Base))
	{
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
	FDCCEquipmentDef Def;
	if (HeadId.IsNone() || !Crawler->Equipment->FindItem(HeadId, Def))
	{
		return;
	}
	// Into the bag first, so a full bag leaves the gear on rather than losing it.
	if (Crawler->Inventory->AddItemRecord(ADCCPickup_Equipment::StaticClass(), Def.DisplayName, Def.MassKilograms, HeadId)
		&& Crawler->Equipment->Unequip(EDCCEquipmentSlot::Head))
	{
		Client_Message(FString::Printf(TEXT("Your %s went in your bag."), *Def.DisplayName.ToString()), true);
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
