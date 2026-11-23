#include "Arcade/DCCShopkeeperNPC.h"

#include "Arcade/DCCArcadeGames.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FShopLine
	{
		FString Row, Tag, Text;
	};

	const TArray<FShopLine>& ShopLines()
	{
		static TArray<FShopLine> Lines;
		if (!Lines.IsEmpty())
		{
			return Lines;
		}
		const FString Candidates[] = {
			FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Voice/NPC/cillinan_lines.json")),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Voice/NPC/cillinan_lines.json")),
		};
		for (const FString& Path : Candidates)
		{
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
			{
				for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("lines")))
				{
					const TSharedPtr<FJsonObject> O = Value->AsObject();
					Lines.Add({ O->GetStringField(TEXT("row")), O->GetStringField(TEXT("tag")), O->GetStringField(TEXT("text")) });
				}
				break;
			}
		}
		if (Lines.IsEmpty())
		{
			Lines.Add({ TEXT("CL_Greet_02"), TEXT("GREET"), TEXT("Welcome to the prize counter. I am Cillinan.") });
		}
		return Lines;
	}

	const FShopLine* PickShop(const TCHAR* Tag, TSet<FString>& Used)
	{
		TArray<const FShopLine*> Pool;
		for (const FShopLine& Line : ShopLines())
		{
			if (Line.Tag == Tag && !Used.Contains(Line.Row))
			{
				Pool.Add(&Line);
			}
		}
		if (Pool.IsEmpty())
		{
			return nullptr;
		}
		const FShopLine* Chosen = Pool[FMath::RandRange(0, Pool.Num() - 1)];
		Used.Add(Chosen->Row);
		return Chosen;
	}
}

ADCCShopkeeperNPC::ADCCShopkeeperNPC()
{
	DialogueTable = nullptr;
	SpeakerId = TEXT("Cillinan");
	DisplayName = FText::FromString(TEXT("CILLINAN"));
	TalkRadius = 750.0f;
	PauseBetweenLinesSeconds = 0.9f;
	bLoopWhenExhausted = false;
	SpeechColour = FLinearColor(0.6f, 1.0f, 0.7f); // the green of the vial he warns you about

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(
		TEXT("/Game/DCC/Characters/Cillinan/Meshy_AI_Character_output/SkeletalMeshes/Meshy_AI_Character_output.Meshy_AI_Character_output"));
	if (Body.Succeeded() && Mesh)
	{
		Mesh->SetSkeletalMeshAsset(Body.Object);
	}
	IdleAnimation = nullptr; // his own sculpted pose; he has no Mannequin rig yet
	if (Speech)
	{
		Speech->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	}
}

void ADCCShopkeeperNPC::BeginPlay()
{
	Super::BeginPlay();
	Script.Reset();
	NextLineIndex = 0;
	if (Mesh)
	{
		MeshBase = Mesh->GetRelativeLocation();
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Shopkeeper] %s at %s, mesh %s, %d lines"), *GetName(), *GetActorLocation().ToCompactString(),
		Mesh && Mesh->GetSkeletalMeshAsset() ? *Mesh->GetSkeletalMeshAsset()->GetName() : TEXT("none"), ShopLines().Num());
}

void ADCCShopkeeperNPC::Tick(float DeltaSeconds)
{
	int32 Near = 0;
	const UWorld* World = GetWorld();
	if (World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (Pawn && Pawn->IsLocallyControlled() && FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation()) <= TalkRadius)
			{
				++Near;
			}
		}
	}
	ListenerCount = Near;

	// Breathing: a slow rise and settle, so a statue reads as a person.
	Breath += DeltaSeconds;
	if (Mesh)
	{
		Mesh->SetRelativeLocation(MeshBase + FVector(0.0f, 0.0f, 0.8f * FMath::Sin(Breath * 1.6f)));
		Mesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f + 0.006f * FMath::Sin(Breath * 1.6f)));
	}

	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const bool bQuiet = !IsBusyTalking() && !HasMoreToSay();
	if (bQuiet && Phase != 0)
	{
		if (Phase == 1)
		{
			NextPitchAt = Now + 45.0;
			NextMutterAt = Now + 18.0;
		}
		else
		{
			NextMutterAt = Now + 25.0;
		}
		Phase = 0;
	}
	if (bQuiet && Phase == 0 && Near > 0)
	{
		if (Now >= NextPitchAt)
		{
			bMutterNext = false;
			BuildScript();
			Phase = 1;
		}
		else if (Now >= NextMutterAt)
		{
			bMutterNext = true;
			BuildScript();
			Phase = 2;
		}
	}
	Super::Tick(DeltaSeconds);
}

void ADCCShopkeeperNPC::BuildScript()
{
	Script.Reset();
	NextLineIndex = 0;
	TSet<FString> Used;
	auto Say = [this](const FShopLine* Line)
	{
		if (!Line)
		{
			return;
		}
		FDCCDialogueLine Entry;
		Entry.LineId = FName(*Line->Row);
		Entry.SpeakerId = SpeakerId;
		Entry.Text = FText::FromString(Line->Text);
		TArray<FString> Words;
		Line->Text.ParseIntoArrayWS(Words);
		Entry.DisplaySeconds = FMath::Max(3.5f, Words.Num() / 2.5f);
		Script.Add(Entry);
	};
	if (bMutterNext)
	{
		Say(PickShop(TEXT("QUIP"), Used));
		return;
	}
	Say(PickShop(TEXT("GREET"), Used));
	Say(PickShop(TEXT("PITCH"), Used));
	Say(PickShop(TEXT("PITCH"), Used));
	Say(PickShop(TEXT("QUIP"), Used));
}
