#include "Bench/DCCGearBenchWidget.h"
#include "Arcade/DCCArcade.h"

#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Arcade/DCCCosmetics.h"
#include "Arcade/DCCWalletComponent.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "UI/DCCHUD.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Gold(1.0f, 0.83f, 0.42f);
	const FLinearColor Ink(0.94f, 0.91f, 0.86f);
	const FLinearColor Dim(0.62f, 0.60f, 0.66f);

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	TSharedRef<SWidget> Heading(const FText& Text)
	{
		return SNew(SBox).Padding(FMargin(0, 12, 0, 4))
			[
				SNew(STextBlock).Text(Text).Font(Font(12, true)).ColorAndOpacity(Gold)
			];
	}

	TSharedRef<SWidget> Small(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(Font(9)).ColorAndOpacity(Dim).AutoWrapText(true);
	}

	using FCombo = SComboBox<TSharedPtr<FString>>;

	/** A dropdown over a list of strings, showing whatever Current returns. */
	TSharedRef<FCombo> MakeCombo(TSharedPtr<FCombo>& Out, TArray<TSharedPtr<FString>>* Options,
		TFunction<FString()> Current, TFunction<void(int32)> OnPick)
	{
		SAssignNew(Out, FCombo)
			.OptionsSource(Options)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())).Font(Font(10));
			})
			.OnSelectionChanged_Lambda([Options, OnPick](TSharedPtr<FString> Item, ESelectInfo::Type How)
			{
				if (Item.IsValid() && How != ESelectInfo::Direct)
				{
					OnPick(Options->IndexOfByKey(Item));
				}
			})
			[
				SNew(STextBlock).Text_Lambda([Current]() { return FText::FromString(Current()); }).Font(Font(10))
			];
		return Out.ToSharedRef();
	}

	/** One labelled number: drag it, type it, or nudge with the arrows. */
	TSharedRef<SWidget> Field(const TCHAR* Label, float Delta, TFunction<float()> Get, TFunction<void(float)> Set)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
			[
				SNew(SBox).WidthOverride(14.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(10, true)).ColorAndOpacity(Dim)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SSpinBox<float>)
				.Delta(Delta)
				.MinValue(TOptional<float>())
				.MaxValue(TOptional<float>())
				.Font(Font(10))
				.Value_Lambda([Get]() { return Get(); })
				.OnValueChanged_Lambda([Set](float V) { Set(V); })
				.OnValueCommitted_Lambda([Set](float V, ETextCommit::Type) { Set(V); })
			];
	}

	TSharedRef<SWidget> Row3(TSharedRef<SWidget> A, TSharedRef<SWidget> B, TSharedRef<SWidget> C)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)[A]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)[B]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[C];
	}

	TSharedRef<SButton> Button(const FText& Label, TFunction<void()> OnClick)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(8.0f, 3.0f))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label).Font(Font(10, true))
			];
	}
}

// ---------------------------------------------------------------------------
// Begin / end
// ---------------------------------------------------------------------------

void UDCCGearBenchWidget::Begin(ADCCCharacterBase* InCrawler)
{
	Crawler = InCrawler;
	APlayerController* PC = GetOwningPlayer();
	if (!InCrawler || !PC || !GetWorld())
	{
		return;
	}

	DCCBench::EnsureLoaded();
	InCrawler->bBenchPosing = true;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = GetWorld()->SpawnActor<ACameraActor>(InCrawler->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (Camera)
	{
		Camera->SetReplicates(false);
		Yaw = InCrawler->GetActorRotation().Yaw + 180.0f; // facing them
		PC->SetViewTargetWithBlend(Camera, 0.35f);
	}

	RefreshLists();

	// Start on whatever they are already holding in their right hand, if anything.
	if (InCrawler->Equipment)
	{
		const FName Held = InCrawler->Equipment->GetEquippedId(EDCCEquipmentSlot::MainHand);
		if (!Held.IsNone())
		{
			SelectedItem = Held;
		}
	}
	LoadEdit();
	Status = TEXT("Pick an animation, equip something, then nudge. Save writes DCCBench.json.");
}

void UDCCGearBenchWidget::End()
{
	ADCCCharacterBase* Body = Crawler.Get();
	APlayerController* PC = GetOwningPlayer();
	if (Body)
	{
		Body->bBenchPosing = false;
		if (USkeletalMeshComponent* Mesh = Body->GetMesh())
		{
			Mesh->SetPlayRate(1.0f);
		}
		Body->RestartLoopAnimation();
	}
	if (PC && Body)
	{
		PC->SetViewTargetWithBlend(Body, 0.35f);
	}
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
}

USkeletalMeshComponent* UDCCGearBenchWidget::GetBodyComponent() const
{
	return Crawler.IsValid() ? Crawler->GetMesh() : nullptr;
}

FString UDCCGearBenchWidget::GetBodyPath() const
{
	const USkeletalMeshComponent* Mesh = GetBodyComponent();
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	return Asset ? Asset->GetPathName() : FString();
}

// ---------------------------------------------------------------------------
// Lists
// ---------------------------------------------------------------------------

void UDCCGearBenchWidget::RefreshLists()
{
	ADCCCharacterBase* Body = Crawler.Get();
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

	BodyOptions.Reset();
	if (Body && Body->Onboarding)
	{
		TArray<FName> Ids;
		Body->Onboarding->CrawlerMeshes.GetKeys(Ids);
		for (const FName& Id : Ids)
		{
			BodyOptions.Add(MakeShared<FString>(Id.ToString()));
		}
	}

	SocketOptions.Reset();
	BoneOptions.Reset();
	if (Asset)
	{
		for (int32 i = 0; i < Asset->NumSockets(); ++i)
		{
			if (const USkeletalMeshSocket* Socket = Asset->GetSocketByIndex(i))
			{
				SocketOptions.Add(MakeShared<FString>(Socket->SocketName.ToString()));
			}
		}
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			BoneOptions.Add(MakeShared<FString>(Ref.GetBoneName(i).ToString()));
		}
		if (SelectedBone.IsNone() && Ref.GetNum() > 0)
		{
			SelectedBone = Ref.GetBoneName(0);
		}
	}

	RebuildItemOptions();
	RebuildWornList();

	// Every animation on this body's skeleton, from the asset registry, so new
	// clips show up without touching this code.
	AllAnimPaths.Reset();
	if (Asset && Asset->GetSkeleton())
	{
		if (IAssetRegistry* Registry = IAssetRegistry::Get())
		{
			const FString SkeletonPath = Asset->GetSkeleton()->GetPathName();
			TArray<FAssetData> Assets;
			Registry->GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Assets, true);
			for (const FAssetData& Data : Assets)
			{
				// Shared/Anims holds the pre-2026-11-13 clips, keyed in each source rig's own
				// bone axes (the twisted-hips bug). Their re-baked twins live in AnimsFixed.
				if (Data.PackagePath == FName(TEXT("/Game/DCC/Characters/Shared/Anims")))
				{
					continue;
				}
				const FString Tag = Data.GetTagValueRef<FString>(TEXT("Skeleton"));
				if (Tag.Contains(SkeletonPath))
				{
					AllAnimPaths.Add(Data.GetSoftObjectPath());
				}
			}
			AllAnimPaths.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B)
			{
				return A.GetAssetName() < B.GetAssetName();
			});
		}
	}
	RefreshAnimOptions();

	for (TSharedPtr<FCombo> Combo : { BodyCombo, ItemCombo, SocketCombo, BoneCombo })
	{
		if (Combo.IsValid())
		{
			Combo->RefreshOptions();
		}
	}
}

void UDCCGearBenchWidget::RefreshAnimOptions()
{
	AnimOptions.Reset();
	AnimPaths.Reset();
	for (const FSoftObjectPath& Path : AllAnimPaths)
	{
		const FString Name = Path.GetAssetName();
		if (AnimFilter.IsEmpty() || Name.Contains(AnimFilter))
		{
			AnimOptions.Add(MakeShared<FString>(Name));
			AnimPaths.Add(Path);
		}
	}
	if (AnimCombo.IsValid())
	{
		AnimCombo->RefreshOptions();
	}
}

// ---------------------------------------------------------------------------
// Choices
// ---------------------------------------------------------------------------

void UDCCGearBenchWidget::SelectBody(FName CrawlerId)
{
	ADCCCharacterBase* Body = Crawler.Get();
	if (!Body || !Body->Onboarding)
	{
		return;
	}
	if (!Body->HasAuthority())
	{
		Status = TEXT("Only the host (or a solo game) can swap bodies.");
		return;
	}

	Body->Onboarding->CrawlerId = CrawlerId;
	Body->Onboarding->ApplyCrawlerLook();
	Body->bBenchPosing = true;
	RefreshLists();

	// The new body may be on a different skeleton; replay the clip if it fits.
	if (CurrentAnim)
	{
		if (USkeletalMeshComponent* Mesh = GetBodyComponent())
		{
			Mesh->PlayAnimation(CurrentAnim, true);
			Mesh->SetPlayRate(bPlaying ? PlayRate : 0.0f);
		}
	}
	LoadEdit();
	Status = FString::Printf(TEXT("Wearing %s's body."), *CrawlerId.ToString());
}

void UDCCGearBenchWidget::SelectAnim(int32 Index)
{
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	if (!Mesh || !AnimPaths.IsValidIndex(Index))
	{
		return;
	}
	CurrentAnim = Cast<UAnimSequence>(AnimPaths[Index].TryLoad());
	if (!CurrentAnim)
	{
		Status = TEXT("That clip would not load.");
		return;
	}
	Mesh->PlayAnimation(CurrentAnim, true);
	Mesh->SetPlayRate(bPlaying ? PlayRate : 0.0f);
	Status = FString::Printf(TEXT("Playing %s (%.2f s)."), *CurrentAnim->GetName(), CurrentAnim->GetPlayLength());
}

void UDCCGearBenchWidget::RebuildItemOptions()
{
	struct FEntry { FName Id; FString Label; FString Category; };
	TArray<FEntry> Entries;
	ADCCCharacterBase* Body = Crawler.Get();
	if (Body && Body->Equipment && Body->Equipment->EquipmentTable)
	{
		for (const FName& Row : Body->Equipment->EquipmentTable->GetRowNames())
		{
			FDCCEquipmentDef Def;
			if (Body->Equipment->FindItem(Row, Def))
			{
				Entries.Add({ Row, Def.DisplayName.IsEmpty() ? Row.ToString() : Def.DisplayName.ToString(),
					UEnum::GetDisplayValueAsText(Def.Slot).ToString() });
			}
		}
	}
	for (const DCCCosmetics::FItem& Item : DCCCosmetics::Catalog())
	{
		Entries.Add({ FName(*(FString(TEXT("Cosmetic.")) + Item.Id.ToString())), Item.Name, TEXT("Hats & cosmetics") });
	}
	Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Label < B.Label; });

	ItemOptions.Reset();
	ItemIds.Reset();
	for (const FEntry& E : Entries)
	{
		if (ItemCategory != TEXT("All") && E.Category != ItemCategory)
		{
			continue;
		}
		if (!ItemFilter.IsEmpty() && !E.Label.Contains(ItemFilter) && !E.Id.ToString().Contains(ItemFilter))
		{
			continue;
		}
		ItemIds.Add(E.Id);
		ItemOptions.Add(MakeShared<FString>(FString::Printf(TEXT("%s  (%s)"), *E.Label, *E.Category)));
	}
	if (ItemCombo.IsValid())
	{
		ItemCombo->RefreshOptions();
	}
}

void UDCCGearBenchWidget::SelectItem(FName ItemId)
{
	ADCCCharacterBase* Body = Crawler.Get();
	if (!Body)
	{
		return;
	}
	SelectedItem = ItemId;

	// An arcade cosmetic: try it on (one per slot) and nudge it as a grip.
	if (IsCosmeticKey(ItemId))
	{
		const FName Id(*ItemId.ToString().RightChop(9));
		if (const DCCCosmetics::FItem* Item = DCCCosmetics::Find(Id))
		{
			if (Body->Wallet)
			{
				BenchCosmetics.Reset();
				for (const FDCCWornCosmetic& W : Body->Wallet->Worn)
				{
					BenchCosmetics.Add(W.Id);
				}
			}
			BenchCosmetics.RemoveAll([Item](FName Other)
			{
				const DCCCosmetics::FItem* O = DCCCosmetics::Find(Other);
				return !O || O->Slot == Item->Slot;
			});
			BenchCosmetics.Add(Id);
			if (Body->Wallet)
			{
				Body->Wallet->ApplyOutfit(BenchCosmetics);
			}
			Status = FString::Printf(TEXT("Wearing %s. Nudge it with Grip: all (every body) or Grip: this body."), Item->Name);
		}
		RebuildWornList();
		if (Target == ETarget::Socket || Target == ETarget::Bone)
		{
			Target = ETarget::GripAll;
		}
		LoadEdit();
		return;
	}
	if (!Body->Equipment)
	{
		return;
	}

	FDCCEquipmentDef Def;
	if (Body->Equipment->FindItem(ItemId, Def) && !Def.AttachSocket.IsNone())
	{
		SelectedSocket = Def.AttachSocket;
	}

	if (Body->HasAuthority())
	{
		Body->Equipment->Equip(ItemId);
		Status = FString::Printf(TEXT("Equipped %s in %s."), *ItemId.ToString(), *SelectedSocket.ToString());
	}
	else
	{
		Status = TEXT("Only the host (or a solo game) can equip from the bench. You can still nudge what you hold.");
	}

	if (Target == ETarget::Bone)
	{
		Target = ETarget::GripAll;
	}
	LoadEdit();
	RebuildWornList();
}

void UDCCGearBenchWidget::TryClickBody()
{
	APlayerController* PC = GetOwningPlayer();
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	float X = 0.0f, Y = 0.0f;
	FVector Origin, Direction;
	if (!PC || !Mesh || !PC->GetMousePosition(X, Y) || !PC->DeprojectScreenPositionToWorld(X, Y, Origin, Direction))
	{
		return;
	}
	FVector HitPoint, HitNormal;
	float Time = 0.0f;
	const FBox Box = Mesh->Bounds.GetBox();
	if (FMath::LineExtentBoxIntersection(Box, Origin, Origin + Direction * 5000.0f, FVector::ZeroVector, HitPoint, HitNormal, Time))
	{
		bShowWorn = true;
		RebuildWornList();
		Status = TEXT("Everything on this body is listed under ON THIS BODY. Nudge or Remove any of it.");
	}
}

void UDCCGearBenchWidget::RemoveWorn(FName Key)
{
	ADCCCharacterBase* Body = Crawler.Get();
	if (!Body)
	{
		return;
	}
	if (IsCosmeticKey(Key))
	{
		const FName Id(*Key.ToString().RightChop(9));
		if (Body->Wallet)
		{
			BenchCosmetics.Reset();
			for (const FDCCWornCosmetic& W : Body->Wallet->Worn)
			{
				if (W.Id != Id)
				{
					BenchCosmetics.Add(W.Id);
				}
			}
			Body->Wallet->ApplyOutfit(BenchCosmetics);
		}
	}
	else if (Body->Equipment)
	{
		for (int32 i = 0; i < int32(EDCCEquipmentSlot::MAX); ++i)
		{
			const EDCCEquipmentSlot GearSlot = EDCCEquipmentSlot(i);
			if (Body->Equipment->GetEquippedId(GearSlot) == Key)
			{
				Body->Equipment->Unequip(GearSlot);
			}
		}
	}
	if (SelectedItem == Key)
	{
		SelectedItem = NAME_None;
	}
	Status = FString::Printf(TEXT("Removed %s."), *Key.ToString());
	RebuildWornList();
}

void UDCCGearBenchWidget::RebuildWornList()
{
	if (!WornBox.IsValid())
	{
		return;
	}
	WornBox->ClearChildren();
	if (!bShowWorn)
	{
		return;
	}
	ADCCCharacterBase* Body = Crawler.Get();
	struct FRow { FName Key; FString Label; };
	TArray<FRow> Rows;
	if (Body && Body->Equipment)
	{
		for (int32 i = 0; i < int32(EDCCEquipmentSlot::MAX); ++i)
		{
			const EDCCEquipmentSlot GearSlot = EDCCEquipmentSlot(i);
			const FName Id = Body->Equipment->GetEquippedId(GearSlot);
			if (Id.IsNone())
			{
				continue;
			}
			FDCCEquipmentDef Def;
			const FString Name = Body->Equipment->FindItem(Id, Def) && !Def.DisplayName.IsEmpty() ? Def.DisplayName.ToString() : Id.ToString();
			Rows.Add({ Id, FString::Printf(TEXT("%s  (%s)"), *Name, *UEnum::GetDisplayValueAsText(GearSlot).ToString()) });
		}
	}
	if (Body && Body->Wallet)
	{
		for (const FDCCWornCosmetic& W : Body->Wallet->Worn)
		{
			const DCCCosmetics::FItem* Item = DCCCosmetics::Find(W.Id);
			Rows.Add({ FName(*(FString(TEXT("Cosmetic.")) + W.Id.ToString())),
				FString::Printf(TEXT("%s  (%s, cosmetic)"), Item ? Item->Name : *W.Id.ToString(), *DCCCosmetics::SlotLabel(W.Slot)) });
		}
	}
	if (Rows.IsEmpty())
	{
		WornBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Nothing on this body."))).Font(Font(10)).ColorAndOpacity(Ink)
		];
		return;
	}
	TWeakObjectPtr<UDCCGearBenchWidget> Self(this);
	for (const FRow& Row : Rows)
	{
		const FName Key = Row.Key;
		WornBox->AddSlot().AutoHeight().Padding(0, 1)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Row.Label)).Font(Font(10)).ColorAndOpacity(Ink)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 3, 0)
			[
				Button(FText::FromString(TEXT("Nudge")), [Self, Key]() { if (Self.IsValid()) { Self->SelectItem(Key); } })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ContentPadding(FMargin(8.0f, 2.0f))
				.ButtonColorAndOpacity(FLinearColor(0.85f, 0.3f, 0.3f))
				.OnClicked_Lambda([Self, Key]() { if (Self.IsValid()) { Self->RemoveWorn(Key); } return FReply::Handled(); })
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Remove"))).Font(Font(9, true))
				]
			]
		];
	}
}

void UDCCGearBenchWidget::SetTarget(ETarget NewTarget)
{
	Target = NewTarget;
	LoadEdit();
}

FText UDCCGearBenchWidget::GetTargetLabel() const
{
	switch (Target)
	{
	case ETarget::GripAll:  return FText::FromString(FString::Printf(TEXT("Grip of %s, every body"), *SelectedItem.ToString()));
	case ETarget::GripBody: return FText::FromString(FString::Printf(TEXT("Grip of %s, this body only"), *SelectedItem.ToString()));
	case ETarget::Socket:   return FText::FromString(FString::Printf(TEXT("Socket %s on this body"), *SelectedSocket.ToString()));
	default:                return FText::FromString(FString::Printf(TEXT("Bone %s on this body (added to the animation)"), *SelectedBone.ToString()));
	}
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

void UDCCGearBenchWidget::LoadEdit()
{
	Edit = DCCBench::FNudge();
	const FString BodyPath = GetBodyPath();
	ADCCCharacterBase* Body = Crawler.Get();

	switch (Target)
	{
	case ETarget::GripAll:
	case ETarget::GripBody:
	{
		if (SelectedItem.IsNone())
		{
			return;
		}
		if (DCCBench::FindGrip(Target == ETarget::GripBody ? BodyPath : FString(), SelectedItem, Edit))
		{
			return;
		}
		FDCCEquipmentDef Def;
		if (Body && Body->Equipment && Body->Equipment->FindItem(SelectedItem, Def))
		{
			Edit.Location = Def.GripOffsetLocation;
			Edit.Rotation = Def.GripOffsetRotation;
		}
		return;
	}
	case ETarget::Socket:
	{
		if (!DCCBench::FindSocket(BodyPath, SelectedSocket, Edit))
		{
			USkeletalMeshComponent* Mesh = GetBodyComponent();
			DCCBench::GetOriginalSocket(Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr, SelectedSocket, Edit);
		}
		return;
	}
	default:
		DCCBench::FindBone(BodyPath, SelectedBone, Edit);
		return;
	}
}

void UDCCGearBenchWidget::ApplyEdit()
{
	bDirty = true;
	UnsavedEntries.Add(EntryKey());
	bQuitArmed = false;
	ADCCCharacterBase* Body = Crawler.Get();
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	const FString BodyPath = GetBodyPath();

	switch (Target)
	{
	case ETarget::GripAll:
	case ETarget::GripBody:
		if (!SelectedItem.IsNone())
		{
			DCCBench::SetGrip(Target == ETarget::GripBody ? BodyPath : FString(), SelectedItem, Edit);
		}
		break;
	case ETarget::Socket:
		DCCBench::SetSocket(Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr, SelectedSocket, Edit);
		break;
	default:
		DCCBench::SetBone(BodyPath, SelectedBone, Edit);
		break;
	}

	// Rehang the gear so grip and socket changes show at once.
	if (Body && Body->Equipment && Target != ETarget::Bone)
	{
		Body->Equipment->RefreshAttachments();
	}
}

void UDCCGearBenchWidget::ResetEdit()
{
	bDirty = true;
	UnsavedEntries.Add(EntryKey());
	bQuitArmed = false;
	ADCCCharacterBase* Body = Crawler.Get();
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	const FString BodyPath = GetBodyPath();

	switch (Target)
	{
	case ETarget::GripAll:  DCCBench::ClearGrip(FString(), SelectedItem); break;
	case ETarget::GripBody: DCCBench::ClearGrip(BodyPath, SelectedItem); break;
	case ETarget::Socket:   DCCBench::ClearSocket(Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr, SelectedSocket); break;
	default:                DCCBench::ClearBone(BodyPath, SelectedBone); break;
	}
	if (Body && Body->Equipment)
	{
		Body->Equipment->RefreshAttachments();
	}
	LoadEdit();
	Status = TEXT("Reset to the authored value. Save to make that stick.");
}

void UDCCGearBenchWidget::Resize(const FVector& Factor)
{
	if (SelectedItem.IsNone() && (Target == ETarget::GripAll || Target == ETarget::GripBody))
	{
		Status = TEXT("Pick a piece first.");
		return;
	}
	Edit.Scale = (Edit.Scale * Factor).ComponentMax(FVector(0.05f));
	ApplyEdit();
	Status = FString::Printf(TEXT("Size %.2f x %.2f x %.2f. Save to keep it."), Edit.Scale.X, Edit.Scale.Y, Edit.Scale.Z);
}

void UDCCGearBenchWidget::AutoFit()
{
	ADCCCharacterBase* Body = Crawler.Get();
	USkeletalMeshComponent* Skin = GetBodyComponent();
	FDCCEquipmentDef Def;
	if (!Body || !Skin || !Body->Equipment || SelectedItem.IsNone() || IsCosmeticKey(SelectedItem) || !Body->Equipment->FindItem(SelectedItem, Def))
	{
		Status = TEXT("Auto-fit works on a head or body piece from the gear list.");
		return;
	}
	if (Def.Slot != EDCCEquipmentSlot::Head && Def.Slot != EDCCEquipmentSlot::Body)
	{
		Status = TEXT("Auto-fit is for head and body pieces. Use the size buttons for the rest.");
		return;
	}
	UStaticMesh* Mesh = Def.Mesh.LoadSynchronous();
	if (!Mesh)
	{
		Status = TEXT("That piece has no worn model.");
		return;
	}
	// Shoulder width: the first pair of arm bones this skeleton has (Meshy, the Mannequin, MakeHuman).
	static const TCHAR* Pairs[][2] = {
		{ TEXT("LeftArm"), TEXT("RightArm") }, { TEXT("upperarm_l"), TEXT("upperarm_r") },
		{ TEXT("LeftShoulder"), TEXT("RightShoulder") }, { TEXT("clavicle_l"), TEXT("clavicle_r") },
		{ TEXT("upperarm01.L"), TEXT("upperarm01.R") },
	};
	float Shoulders = 0.0f;
	for (const auto& Pair : Pairs)
	{
		if (Skin->GetBoneIndex(Pair[0]) != INDEX_NONE && Skin->GetBoneIndex(Pair[1]) != INDEX_NONE)
		{
			Shoulders = FVector::Dist(Skin->GetBoneLocation(Pair[0]), Skin->GetBoneLocation(Pair[1]));
			break;
		}
	}
	if (Shoulders < 1.0f)
	{
		Status = TEXT("Could not find this body's shoulders. Use the size buttons.");
		return;
	}
	// A helmet is a bit over half the shoulders wide; a vest or chestplate a bit wider than them.
	const FVector Size = Mesh->GetBounds().BoxExtent * 2.0;
	const float Wide = FMath::Max(1.0f, float(FMath::Max(Size.X, Size.Y)));
	const float Want = Def.Slot == EDCCEquipmentSlot::Head ? Shoulders * 0.62f : Shoulders * 1.3f;
	const float S = FMath::Clamp(Want / Wide, 0.2f, 5.0f);
	if (Target != ETarget::GripBody)
	{
		Target = ETarget::GripBody;
		LoadEdit();
	}
	Edit.Scale = FVector(S);
	ApplyEdit();
	Status = FString::Printf(TEXT("Fitted to this body's shoulders (%.0f cm): size %.2f. Nudge it, then Save."), Shoulders, S);
}

void UDCCGearBenchWidget::CurrentEntry(const TCHAR*& Kind, FString& MeshPath, FName& Name) const
{
	switch (Target)
	{
	case ETarget::GripAll:  Kind = TEXT("grips");   MeshPath = FString();     Name = SelectedItem;   break;
	case ETarget::GripBody: Kind = TEXT("grips");   MeshPath = GetBodyPath(); Name = SelectedItem;   break;
	case ETarget::Socket:   Kind = TEXT("sockets"); MeshPath = GetBodyPath(); Name = SelectedSocket; break;
	default:                Kind = TEXT("bones");   MeshPath = GetBodyPath(); Name = SelectedBone;   break;
	}
}

FString UDCCGearBenchWidget::EntryKey() const
{
	const TCHAR* Kind = nullptr; FString MeshPath; FName Name;
	CurrentEntry(Kind, MeshPath, Name);
	return FString::Printf(TEXT("%s|%s|%s"), Kind, *MeshPath, *Name.ToString());
}

// Save writes only the thing on screen (the selected item's grip, or the selected
// socket or bone), leaving everything else in the file exactly as it was.
void UDCCGearBenchWidget::SaveAll()
{
	const TCHAR* Kind = nullptr; FString MeshPath; FName Name;
	CurrentEntry(Kind, MeshPath, Name);
	if (Name.IsNone())
	{
		Status = TEXT("Pick an item, socket or bone first.");
		return;
	}
	FString Where;
	const bool bSaved = DCCBench::SaveEntry(Kind, MeshPath, Name, Where);
	if (!bSaved)
	{
		Status = FString::Printf(TEXT("Could not write %s"), *Where);
		return;
	}
	UnsavedEntries.Remove(EntryKey());
	bDirty = UnsavedEntries.Num() > 0;
	bQuitArmed = false;
	Status = UnsavedEntries.Num() > 0
		? FString::Printf(TEXT("Saved %s only. %d other change(s) not saved."), *Name.ToString(), UnsavedEntries.Num())
		: FString::Printf(TEXT("Saved %s only."), *Name.ToString());
}

bool UDCCGearBenchWidget::IsTool() const
{
	return DCCArcade::IsBenchTool(GetWorld());
}

void UDCCGearBenchWidget::RequestClose()
{
	if (!IsTool())
	{
		if (Hud.IsValid())
		{
			Hud->ToggleGearBench();
		}
		return;
	}
	if (bDirty && !bQuitArmed)
	{
		bQuitArmed = true;
		Status = TEXT("You have unsaved changes. Click Save, or click Quit again to throw them away.");
		return;
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void UDCCGearBenchWidget::TogglePlay()
{
	bPlaying = !bPlaying;
	if (USkeletalMeshComponent* Mesh = GetBodyComponent())
	{
		Mesh->SetPlayRate(bPlaying ? PlayRate : 0.0f);
	}
}

void UDCCGearBenchWidget::Step(float Seconds)
{
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	UAnimSingleNodeInstance* Single = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
	if (!Single || !CurrentAnim)
	{
		return;
	}
	bPlaying = false;
	Mesh->SetPlayRate(0.0f);
	const float Length = CurrentAnim->GetPlayLength();
	float Time = Single->GetCurrentTime() + Seconds;
	Time = Length > 0.0f ? FMath::Fmod(Time + Length, Length) : 0.0f;
	Single->SetPosition(Time, false);
}

float UDCCGearBenchWidget::GetScrub() const
{
	const USkeletalMeshComponent* Mesh = GetBodyComponent();
	const UAnimSingleNodeInstance* Single = Mesh ? const_cast<USkeletalMeshComponent*>(Mesh)->GetSingleNodeInstance() : nullptr;
	if (!Single || !CurrentAnim || CurrentAnim->GetPlayLength() <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(Single->GetCurrentTime() / CurrentAnim->GetPlayLength(), 0.0f, 1.0f);
}

void UDCCGearBenchWidget::SetScrub(float Fraction)
{
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	UAnimSingleNodeInstance* Single = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
	if (!Single || !CurrentAnim)
	{
		return;
	}
	bPlaying = false;
	Mesh->SetPlayRate(0.0f);
	Single->SetPosition(Fraction * CurrentAnim->GetPlayLength(), false);
}

// ---------------------------------------------------------------------------
// Camera and markers
// ---------------------------------------------------------------------------

void UDCCGearBenchWidget::TickBench(float DeltaSeconds)
{
	ADCCCharacterBase* Body = Crawler.Get();
	if (!Body || !Camera)
	{
		return;
	}
	Body->bBenchPosing = true;

	const FVector Focus = Body->GetActorLocation() + FVector(0.0f, 0.0f, FocusHeight);
	const FVector Direction = FRotator(Pitch, Yaw, 0.0f).Vector();
	Camera->SetActorLocationAndRotation(Focus - Direction * Distance, Direction.Rotation());

	// Mark what is being nudged: a small axis gizmo, drawn in front of the body.
	USkeletalMeshComponent* Mesh = GetBodyComponent();
	UWorld* World = GetWorld();
	if (!Mesh || !World)
	{
		return;
	}
	FTransform Marker;
	bool bHasMarker = false;
	if (Target == ETarget::Bone && !SelectedBone.IsNone() && Mesh->GetBoneIndex(SelectedBone) != INDEX_NONE)
	{
		Marker = Mesh->GetSocketTransform(SelectedBone, RTS_World);
		bHasMarker = true;
	}
	else if (!SelectedSocket.IsNone() && Mesh->DoesSocketExist(SelectedSocket))
	{
		Marker = Mesh->GetSocketTransform(SelectedSocket, RTS_World);
		bHasMarker = true;
	}
	if (bHasMarker)
	{
		DrawDebugCoordinateSystem(World, Marker.GetLocation(), Marker.Rotator(), 12.0f, false, -1.0f, SDPG_Foreground, 0.6f);
		DrawDebugSphere(World, Marker.GetLocation(), 2.5f, 8, FColor(255, 210, 90), false, -1.0f, SDPG_Foreground);
	}
}

FReply UDCCGearBenchWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey Button = InMouseEvent.GetEffectingButton();
	bOrbiting = Button == EKeys::RightMouseButton || Button == EKeys::LeftMouseButton;
	bPanning = Button == EKeys::MiddleMouseButton;
	DragDistance = 0.0f;
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UDCCGearBenchWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DragDistance < 6.0f)
	{
		TryClickBody();
	}
	bOrbiting = false;
	bPanning = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UDCCGearBenchWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Delta = InMouseEvent.GetCursorDelta();
	DragDistance += Delta.Size();
	if (bOrbiting)
	{
		Yaw += Delta.X * 0.4f;
		Pitch = FMath::Clamp(Pitch - Delta.Y * 0.3f, -80.0f, 80.0f);
		return FReply::Handled();
	}
	if (bPanning)
	{
		FocusHeight = FMath::Clamp(FocusHeight + Delta.Y * 0.5f, -120.0f, 160.0f);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UDCCGearBenchWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Distance = FMath::Clamp(Distance * (InMouseEvent.GetWheelDelta() > 0.0f ? 0.88f : 1.12f), 40.0f, 900.0f);
	return FReply::Handled();
}

FReply UDCCGearBenchWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::F3 || Key == EKeys::Escape || Key == EKeys::Tab)
	{
		RequestClose();
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar)
	{
		TogglePlay();
		return FReply::Handled();
	}
	if (Key == EKeys::Comma || Key == EKeys::Left)
	{
		Step(-1.0f / 30.0f);
		return FReply::Handled();
	}
	if (Key == EKeys::Period || Key == EKeys::Right)
	{
		Step(1.0f / 30.0f);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---------------------------------------------------------------------------
// The panel
// ---------------------------------------------------------------------------

TSharedRef<SWidget> UDCCGearBenchWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCGearBenchWidget> Self(this);
	auto Do = [Self](TFunction<void(UDCCGearBenchWidget&)> Fn)
	{
		return [Self, Fn]() { if (Self.IsValid()) { Fn(*Self); } };
	};
	auto Num = [Self](TFunction<float(const UDCCGearBenchWidget&)> Get)
	{
		return [Self, Get]() { return Self.IsValid() ? Get(*Self) : 0.0f; };
	};
	auto SetNum = [Self](TFunction<void(UDCCGearBenchWidget&, float)> Set)
	{
		return [Self, Set](float V) { if (Self.IsValid()) { Set(*Self, V); Self->ApplyEdit(); } };
	};
	auto TargetButton = [Self](const TCHAR* Label, ETarget Which)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(6.0f, 3.0f))
			.ButtonColorAndOpacity_Lambda([Self, Which]()
			{
				return Self.IsValid() && Self->Target == Which ? FLinearColor(1.0f, 0.75f, 0.25f) : FLinearColor(0.35f, 0.35f, 0.42f);
			})
			.OnClicked_Lambda([Self, Which]() { if (Self.IsValid()) { Self->SetTarget(Which); } return FReply::Handled(); })
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(9, true))
			];
	};

	TSharedRef<SWidget> BodyPicker = MakeCombo(BodyCombo, &BodyOptions,
		[Self]() -> FString
		{
			const ADCCCharacterBase* Body = Self.IsValid() ? Self->Crawler.Get() : nullptr;
			return Body && Body->Onboarding && !Body->Onboarding->CrawlerId.IsNone() ? Body->Onboarding->CrawlerId.ToString() : TEXT("(pick a body)");
		},
		[Self](int32 I) { if (Self.IsValid() && Self->BodyOptions.IsValidIndex(I)) { Self->SelectBody(FName(**Self->BodyOptions[I])); } });

	TSharedRef<SWidget> AnimPicker = MakeCombo(AnimCombo, &AnimOptions,
		[Self]() -> FString { return Self.IsValid() && Self->CurrentAnim ? Self->CurrentAnim->GetName() : TEXT("(pick an animation)"); },
		[Self](int32 I) { if (Self.IsValid()) { Self->SelectAnim(I); } });

	TSharedRef<SWidget> ItemPicker = MakeCombo(ItemCombo, &ItemOptions,
		[Self]() -> FString { return Self.IsValid() && !Self->SelectedItem.IsNone() ? Self->SelectedItem.ToString() : TEXT("(pick an item to equip)"); },
		[Self](int32 I) { if (Self.IsValid() && Self->ItemIds.IsValidIndex(I)) { Self->SelectItem(Self->ItemIds[I]); } });

	TSharedRef<SWrapBox> CategoryRow = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3.0f, 3.0f));
	for (const TCHAR* Category : { TEXT("All"), TEXT("Hats & cosmetics"), TEXT("Main Hand"), TEXT("Off Hand"), TEXT("Head"), TEXT("Body"), TEXT("Trinket") })
	{
		const FString Name = Category;
		CategoryRow->AddSlot()
		[
			SNew(SButton)
			.ContentPadding(FMargin(6.0f, 2.0f))
			.ButtonColorAndOpacity_Lambda([Self, Name]()
			{
				return Self.IsValid() && Self->ItemCategory == Name ? FLinearColor(1.0f, 0.75f, 0.25f) : FLinearColor(0.35f, 0.35f, 0.42f);
			})
			.OnClicked_Lambda([Self, Name]()
			{
				if (Self.IsValid())
				{
					Self->ItemCategory = Name;
					Self->RebuildItemOptions();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(9, true))
			]
		];
	}

	TSharedRef<SWidget> SocketPicker = MakeCombo(SocketCombo, &SocketOptions,
		[Self]() -> FString { return Self.IsValid() ? Self->SelectedSocket.ToString() : FString(); },
		[Self](int32 I)
		{
			if (Self.IsValid() && Self->SocketOptions.IsValidIndex(I))
			{
				Self->SelectedSocket = FName(**Self->SocketOptions[I]);
				Self->SetTarget(ETarget::Socket);
			}
		});

	TSharedRef<SWidget> BonePicker = MakeCombo(BoneCombo, &BoneOptions,
		[Self]() -> FString { return Self.IsValid() ? Self->SelectedBone.ToString() : FString(); },
		[Self](int32 I)
		{
			if (Self.IsValid() && Self->BoneOptions.IsValidIndex(I))
			{
				Self->SelectedBone = FName(**Self->BoneOptions[I]);
				Self->SetTarget(ETarget::Bone);
			}
		});

	TSharedRef<SWidget> Panel =
		SNew(SBox)
		.WidthOverride(400.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.035f, 0.03f, 0.06f, 0.94f))
			.Padding(FMargin(16.0f, 14.0f))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(NSLOCTEXT("DCC", "BenchTitle", "GEAR BENCH")).Font(Font(20, true)).ColorAndOpacity(Gold)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
					[
						SNew(STextBlock)
						.Text_Lambda([Self]() { return Self.IsValid() ? FText::FromString(Self->Status) : FText::GetEmpty(); })
						.Font(Font(10)).ColorAndOpacity(Ink).AutoWrapText(true)
					]

					+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "BenchBody", "BODY")) ]
					+ SVerticalBox::Slot().AutoHeight()[ BodyPicker ]

					+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "BenchAnim", "ANIMATION")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
					[
						SNew(SEditableTextBox)
						.Font(Font(10))
						.HintText(NSLOCTEXT("DCC", "BenchAnimFilter", "Filter clips (e.g. Walk, Slash)"))
						.OnTextChanged_Lambda([Self](const FText& Text)
						{
							if (Self.IsValid())
							{
								Self->AnimFilter = Text.ToString();
								Self->RefreshAnimOptions();
							}
						})
					]
					+ SVerticalBox::Slot().AutoHeight()[ AnimPicker ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.ContentPadding(FMargin(8.0f, 3.0f))
							.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->TogglePlay(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(Font(10, true))
								.Text_Lambda([Self]() { return FText::FromString(Self.IsValid() && Self->bPlaying ? TEXT("Pause") : TEXT("Play")); })
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[ Button(FText::FromString(TEXT("< frame")), Do([](UDCCGearBenchWidget& W) { W.Step(-1.0f / 30.0f); })) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[ Button(FText::FromString(TEXT("frame >")), Do([](UDCCGearBenchWidget& W) { W.Step(1.0f / 30.0f); })) ]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							Field(TEXT("x"), 0.05f,
								Num([](const UDCCGearBenchWidget& W) { return W.PlayRate; }),
								[Self](float V)
								{
									if (!Self.IsValid()) { return; }
									Self->PlayRate = FMath::Clamp(V, 0.05f, 4.0f);
									if (USkeletalMeshComponent* Mesh = Self->GetBodyComponent())
									{
										Mesh->SetPlayRate(Self->bPlaying ? Self->PlayRate : 0.0f);
									}
								})
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSlider)
						.Value_Lambda([Self]() { return Self.IsValid() ? Self->GetScrub() : 0.0f; })
						.OnValueChanged_Lambda([Self](float V) { if (Self.IsValid()) { Self->SetScrub(V); } })
					]

					+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "BenchGear", "GEAR")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[ CategoryRow ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
					[
						SNew(SEditableTextBox)
						.Font(Font(10))
						.HintText(NSLOCTEXT("DCC", "BenchItemFilter", "Search gear (e.g. hat, sword)"))
						.OnTextChanged_Lambda([Self](const FText& Text)
						{
							if (Self.IsValid())
							{
								Self->ItemFilter = Text.ToString();
								Self->RebuildItemOptions();
							}
						})
					]
					+ SVerticalBox::Slot().AutoHeight()[ ItemPicker ]

					+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "BenchWorn", "ON THIS BODY")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
					[
						Button(NSLOCTEXT("DCC", "BenchWornToggle", "Show / hide (or click the body)"), [Self]()
						{
							if (Self.IsValid())
							{
								Self->bShowWorn = !Self->bShowWorn;
								Self->RebuildWornList();
							}
						})
					]
					+ SVerticalBox::Slot().AutoHeight()[ SAssignNew(WornBox, SVerticalBox) ]

					+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "BenchNudge", "NUDGE")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)[ TargetButton(TEXT("Grip: all"), ETarget::GripAll) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)[ TargetButton(TEXT("Grip: this body"), ETarget::GripBody) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)[ TargetButton(TEXT("Socket"), ETarget::Socket) ]
						+ SHorizontalBox::Slot().AutoWidth()[ TargetButton(TEXT("Bone"), ETarget::Bone) ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
					[
						SNew(STextBlock)
						.Text_Lambda([Self]() { return Self.IsValid() ? Self->GetTargetLabel() : FText::GetEmpty(); })
						.Font(Font(10, true)).ColorAndOpacity(Ink).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)[ SocketPicker ]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[ BonePicker ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)[ Small(NSLOCTEXT("DCC", "BenchMove", "Move (cm)")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Row3(
							Field(TEXT("X"), 0.25f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Location.X); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Location.X = V; })),
							Field(TEXT("Y"), 0.25f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Location.Y); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Location.Y = V; })),
							Field(TEXT("Z"), 0.25f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Location.Z); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Location.Z = V; })))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)[ Small(NSLOCTEXT("DCC", "BenchTurn", "Turn (degrees): pitch, yaw, roll")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Row3(
							Field(TEXT("P"), 0.5f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Rotation.Pitch); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Rotation.Pitch = V; })),
							Field(TEXT("Y"), 0.5f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Rotation.Yaw); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Rotation.Yaw = V; })),
							Field(TEXT("R"), 0.5f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Rotation.Roll); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Rotation.Roll = V; })))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)[ Small(NSLOCTEXT("DCC", "BenchScale", "Scale: all three, or each axis")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
					[
						Field(TEXT("*"), 0.01f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Scale.X); }),
							SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Scale = FVector(FMath::Max(0.01f, V)); }))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Row3(
							Field(TEXT("X"), 0.01f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Scale.X); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Scale.X = FMath::Max(0.01f, V); })),
							Field(TEXT("Y"), 0.01f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Scale.Y); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Scale.Y = FMath::Max(0.01f, V); })),
							Field(TEXT("Z"), 0.01f, Num([](const UDCCGearBenchWidget& W) { return float(W.Edit.Scale.Z); }), SetNum([](UDCCGearBenchWidget& W, float V) { W.Edit.Scale.Z = FMath::Max(0.01f, V); })))
					]
					// FIT: armour that does not fit a body, fixed with buttons instead of typing scales.
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 2)[ Heading(NSLOCTEXT("DCC", "BenchFit", "FIT")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
					[
						Row3(
							Button(NSLOCTEXT("DCC", "BenchSmaller", "Smaller"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(0.95f)); })),
							Button(NSLOCTEXT("DCC", "BenchBigger", "Bigger"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(1.05f)); })),
							Button(NSLOCTEXT("DCC", "BenchAutoFit", "Auto-fit"), Do([](UDCCGearBenchWidget& W) { W.AutoFit(); })))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
					[
						Row3(
							Button(NSLOCTEXT("DCC", "BenchSlimmer", "Slimmer"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(0.95f, 0.95f, 1.0f)); })),
							Button(NSLOCTEXT("DCC", "BenchWider", "Wider"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(1.05f, 1.05f, 1.0f)); })),
							Button(NSLOCTEXT("DCC", "BenchTaller", "Taller"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(1.0f, 1.0f, 1.05f)); })))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Row3(
							Button(NSLOCTEXT("DCC", "BenchShorter", "Shorter"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(1.0f, 1.0f, 0.95f)); })),
							Button(NSLOCTEXT("DCC", "BenchThinner", "Thinner"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(1.0f, 0.95f, 1.0f)); })),
							Button(NSLOCTEXT("DCC", "BenchDeeper", "Deeper"), Do([](UDCCGearBenchWidget& W) { W.Resize(FVector(1.0f, 1.05f, 1.0f)); })))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[ Button(NSLOCTEXT("DCC", "BenchReset", "Reset this"), Do([](UDCCGearBenchWidget& W) { W.ResetEdit(); })) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
						[
							SNew(SButton)
							.ContentPadding(FMargin(14.0f, 3.0f))
							.ButtonColorAndOpacity(FLinearColor(0.75f, 0.35f, 0.35f))
							.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->RequestClose(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(Font(10, true))
								.Text_Lambda([Self]()
								{
									return Self.IsValid() && Self->IsTool()
										? NSLOCTEXT("DCC", "BenchQuit", "Quit")
										: NSLOCTEXT("DCC", "BenchClose", "Close");
								})
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.ContentPadding(FMargin(14.0f, 3.0f))
							.ButtonColorAndOpacity(FLinearColor(0.35f, 0.75f, 0.35f))
							.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->SaveAll(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(NSLOCTEXT("DCC", "BenchSave", "Save this one")).Font(Font(10, true))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						Small(NSLOCTEXT("DCC", "BenchHelp",
							"Drag on the view to orbit (left or right button), middle-drag to raise or lower, wheel to zoom. "
							"Space plays or pauses, comma and full stop step a frame. Drag a number box, or click it and type. "
							"Changes show at once and only stick when you Save."))
					]
				]
			]
		];

	// Left-hand panel over a transparent, full-screen area that takes the
	// orbit drags and wheel (the UUserWidget's own mouse handlers).
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Visibility(EVisibility::Visible)
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(FMargin(16.0f))
		[
			Panel
		];
}

void UDCCGearBenchWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	BodyCombo.Reset();
	AnimCombo.Reset();
	ItemCombo.Reset();
	WornBox.Reset();
	SocketCombo.Reset();
	BoneCombo.Reset();
}
