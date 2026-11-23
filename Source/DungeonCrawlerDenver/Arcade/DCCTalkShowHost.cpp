#include "Arcade/DCCTalkShowHost.h"

#include "Arcade/DCCArcade.h"
#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCAudience.h"
#include "Arcade/DCCWalletComponent.h"
#include "Animation/AnimationAsset.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FShowLine
	{
		FString Row, Tag, Text;
	};

	/** talkshow_lines.json, read once per run (it is small). */
	const TArray<FShowLine>& ShowLines()
	{
		static TArray<FShowLine> Lines;
		if (!Lines.IsEmpty())
		{
			return Lines;
		}
		const FString Candidates[] = {
			FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Voice/NPC/talkshow_lines.json")),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Voice/NPC/talkshow_lines.json")),
		};
		for (const FString& Path : Candidates)
		{
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
			{
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("lines")))
			{
				const TSharedPtr<FJsonObject> O = Value->AsObject();
				Lines.Add({ O->GetStringField(TEXT("row")), O->GetStringField(TEXT("tag")), O->GetStringField(TEXT("text")) });
			}
			if (!Lines.IsEmpty())
			{
				break;
			}
		}
		if (Lines.IsEmpty())
		{
			// No file: he still opens and closes the show, in text.
			Lines.Add({ TEXT("TS_Open_01"), TEXT("OPEN"), TEXT("Good evening, Denver, and welcome to Primetime!") });
			Lines.Add({ TEXT("TS_Fame_02"), TEXT("FAME"), TEXT("Under the viewer count is your Fame bar. Fill it up and a loot box shows up.") });
			Lines.Add({ TEXT("TS_Close_01"), TEXT("CLOSE"), TEXT("That's our show! Stand on a pad to vote for the next game.") });
		}
		return Lines;
	}

	/** A random line with this tag that is not already in Used, or null. */
	const FShowLine* Pick(const FString& Tag, TSet<FString>& Used)
	{
		TArray<const FShowLine*> Pool;
		for (const FShowLine& Line : ShowLines())
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
		const FShowLine* Chosen = Pool[FMath::RandRange(0, Pool.Num() - 1)];
		Used.Add(Chosen->Row);
		return Chosen;
	}
}

ADCCTalkShowHost::ADCCTalkShowHost()
{
	DialogueTable = nullptr; // lines from talkshow_lines.json
	SpeakerId = TEXT("Hiss");
	DisplayName = FText::FromString(TEXT("HISS CRANSON"));
	TalkRadius = 1300.0f;
	PauseBetweenLinesSeconds = 0.6f;
	bLoopWhenExhausted = false;
	SpeechColour = FLinearColor(1.0f, 0.85f, 0.45f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/DCC/Characters/NPCs/NPC_office_311/SK_NPC_office_311.SK_NPC_office_311"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_Idle.NPC_MM_Idle"));
	if (Body.Succeeded() && Mesh)
	{
		Mesh->SetSkeletalMeshAsset(Body.Object);
	}
	if (Idle.Succeeded())
	{
		IdleAnimation = Idle.Object;
	}
}

void ADCCTalkShowHost::BeginPlay()
{
	Super::BeginPlay();
	Script.Reset(); // nothing until somebody walks up
	NextLineIndex = 0;
}

void ADCCTalkShowHost::Tick(float DeltaSeconds)
{
	// Each machine listens with its own player, like the impound staff.
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

	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const bool bQuiet = !IsBusyTalking() && !HasMoreToSay();
	if (bQuiet && Phase != 0)
	{
		// A segment ended: a quip in half a minute, the next full segment in a minute and a quarter.
		if (Phase == 1)
		{
			NextIdleAt = Now + 30.0;
			NextSegmentAt = Now + 75.0;
		}
		Phase = 0;
	}
	if (bQuiet && Phase == 0 && Near > 0)
	{
		if (Now >= NextSegmentAt)
		{
			bIdleNext = false;
			BuildScript();
			Phase = 1;
		}
		else if (NextIdleAt > 0.0 && Now >= NextIdleAt)
		{
			NextIdleAt = 0.0;
			bIdleNext = true;
			BuildScript();
			Phase = 2;
		}
	}
	Super::Tick(DeltaSeconds);
}

void ADCCTalkShowHost::BuildScript()
{
	Script.Reset();
	NextLineIndex = 0;
	TSet<FString> Used;
	auto Say = [this](const FShowLine* Line, const FString& Shown)
	{
		if (!Line)
		{
			return;
		}
		FDCCDialogueLine Entry;
		Entry.LineId = FName(*Line->Row);
		Entry.SpeakerId = SpeakerId;
		Entry.Text = FText::FromString(Shown.IsEmpty() ? Line->Text : Shown);
		TArray<FString> Words;
		Line->Text.ParseIntoArrayWS(Words);
		Entry.DisplaySeconds = FMath::Max(3.5f, Words.Num() / 2.6f);
		Script.Add(Entry);
	};

	if (bIdleNext)
	{
		Say(Pick(TEXT("IDLE"), Used), FString());
		return;
	}

	Say(Pick(TEXT("OPEN"), Used), FString());

	// The crawlers in the room, introduced one by one, with what the Fame board says about them.
	int32 Introduced = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADCCCharacterBase> It(World); It && Introduced < 5; ++It)
		{
			ADCCCharacterBase* Crawler = *It;
			if (!Crawler || !Crawler->IsPlayerControlled() || !Crawler->GetPlayerState())
			{
				continue;
			}
			const FString Name = Crawler->GetPlayerState()->GetPlayerName();
			const FShowLine* Intro = Pick(FString(TEXT("INTRO_")) + Name, Used);
			if (!Intro)
			{
				Intro = Pick(TEXT("INTRO_ANY"), Used);
			}
			Say(Intro, FString());
			const int32 Fame = Crawler->Wallet ? Crawler->Wallet->FameLevel : 0;
			const TCHAR* Tag = Crawler->CheatStrikes > 0 ? TEXT("CHEAT") : Fame < 2 ? TEXT("FAME_LOW") : Fame < 6 ? TEXT("FAME_MID") : TEXT("FAME_HIGH");
			if (const FShowLine* Reaction = Pick(Tag, Used))
			{
				// The words he says, with the number on the card under them.
				Say(Reaction, FString::Printf(TEXT("%s  [%s: FAME %d]"), *Reaction->Text, *Name, Fame));
			}
			++Introduced;
		}
	}

	Say(Pick(TEXT("FAME"), Used), FString());
	Say(Pick(TEXT("FAME"), Used), FString());
	Say(Pick(TEXT("TIP"), Used), FString());
	Say(Pick(TEXT("CLOSE"), Used), FString());
}

// ---- The set, in the Den (built on every machine) and the FAME BOARD behind the desk -------------------------------

FVector ADCCArcadeLobby::TalkShowCenter() const
{
	const float L = FMath::Max(2600.0f, (DCCArcade::Games().Num() - 1) * 500.0f + 1100.0f);
	return GetActorLocation() + FVector(-L + 520.0f, -350.0f, 0.0f);
}

void ADCCArcadeLobby::BuildTalkShow()
{
	const FVector C = TalkShowCenter();
	const FLinearColor Plum(0.18f, 0.06f, 0.22f);
	const FLinearColor Gold(1.0f, 0.75f, 0.25f);
	// A low stage, a back wall, the desk, the guest couch.
	Block(C + FVector(0, 0, 15), FVector(900, 1100, 30), Plum);
	Block(C + FVector(-430, 0, 260), FVector(40, 1100, 520), FLinearColor(0.07f, 0.03f, 0.1f));
	Block(C + FVector(-405, 0, 6), FVector(20, 1100, 12), Gold, 0.0f, false);
	Prop(TEXT("/Game/Dam_city/Meshes/Desk/SM_desk.SM_desk"), C + FVector(-120, -230, 30), 90.0f, FVector(220, 110, 0), true);
	Prop(TEXT("/Game/Beach_house/Meshes/Furniture/Couch/SM_couch.SM_couch"), C + FVector(-80, 250, 30), -90.0f, FVector(260, 110, 0), true);
	// The show's name and the ON AIR light, facing the room (+X).
	Sign(C + FVector(-405, 0, 470), 0.0f, TEXT("PRIMETIME"), FColor(255, 200, 80), 70.0f);
	Sign(C + FVector(-405, 380, 400), 0.0f, TEXT("ON AIR"), FColor(255, 60, 70), 30.0f);
	// The board: everyone's Fame, kept current in TickTalkShow.
	Block(C + FVector(-408, 0, 250), FVector(6, 620, 200), FLinearColor(0.02f, 0.02f, 0.04f), 0.0f, false);
	Sign(C + FVector(-402, 0, 330), 0.0f, TEXT("FAME BOARD"), FColor(255, 214, 120), 26.0f, false);
	FameBoard = Sign(C + FVector(-402, 0, 240), 0.0f, TEXT(" "), FColor(235, 230, 255), 20.0f, false);
	Light(C + FVector(200, 0, 520), FLinearColor(1.0f, 0.85f, 0.6f), 9000.0f, 1400.0f);
	Light(C + FVector(-300, -400, 300), FLinearColor(1.0f, 0.3f, 0.6f), 3000.0f, 700.0f);
	Light(C + FVector(-300, 400, 300), FLinearColor(0.3f, 0.6f, 1.0f), 3000.0f, 700.0f);
}

void ADCCArcadeLobby::TickTalkShow()
{
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	ATextRenderActor* Board = FameBoard.Get();
	if (!Board || Now < NextBoardAt)
	{
		return;
	}
	NextBoardAt = Now + 1.0;
	TArray<TPair<int32, FString>> Rows;
	for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerControlled() && It->GetPlayerState() && It->Wallet)
		{
			Rows.Add({ It->Wallet->FameLevel, It->GetPlayerState()->GetPlayerName() });
		}
	}
	Rows.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B) { return A.Key > B.Key; });
	FString Text;
	for (int32 i = 0; i < Rows.Num() && i < 6; ++i)
	{
		Text += FString::Printf(TEXT("%d. %s   FAME %d\n"), i + 1, *Rows[i].Value, Rows[i].Key);
	}
	if (Text.IsEmpty())
	{
		Text = TEXT("nobody famous yet");
	}
	if (UTextRenderComponent* Render = Board->GetTextRender())
	{
		Render->SetText(FText::FromString(Text.TrimEnd()));
	}
}
