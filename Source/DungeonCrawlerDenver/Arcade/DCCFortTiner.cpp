#include "Arcade/DCCFortTiner.h"

#include "Arcade/DCCWalletComponent.h"
#include "Session/DCCProfile.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FFortTinerSpeaker
	{
		const TCHAR* Id;
		const TCHAR* Name;
		const TCHAR* Body;
		const TCHAR* Hat;
	};

	struct FFortTinerLine
	{
		const TCHAR* Speaker;
		const TCHAR* Row;
		const TCHAR* Text;
	};

#include "Arcade/DCCFortTinerLines.inl"

	/** The cast and their lines as the game uses them: from the JSON if present, else the built-in copy. */
	struct FCastSpeaker { FString Id, Name, Body, Hat; };
	struct FCastLine { FString Speaker, Row, Text; };

	/**
	 * SourceArt/Voice/NPC/fort_tiner_lines.json (or Voice/NPC/ next to a packaged game), read fresh each
	 * call so a line edited in the Voice Studio is heard next round. Falls back to the compiled copy.
	 */
	void LoadCast(TArray<FCastSpeaker>& OutSpeakers, TArray<FCastLine>& OutLines)
	{
		const FString Candidates[] = {
			FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Voice/NPC/fort_tiner_lines.json")),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Voice/NPC/fort_tiner_lines.json")),
		};
		for (const FString& Path : Candidates)
		{
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
			{
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("speakers")))
			{
				const TSharedPtr<FJsonObject> O = Value->AsObject();
				OutSpeakers.Add({ O->GetStringField(TEXT("id")), O->GetStringField(TEXT("name")), O->GetStringField(TEXT("body")),
					O->HasField(TEXT("hat")) ? O->GetStringField(TEXT("hat")) : FString() });
			}
			for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("lines")))
			{
				const TSharedPtr<FJsonObject> O = Value->AsObject();
				OutLines.Add({ O->GetStringField(TEXT("speaker")), O->GetStringField(TEXT("row")), O->GetStringField(TEXT("text")) });
			}
			if (!OutSpeakers.IsEmpty())
			{
				return;
			}
		}
		OutSpeakers.Reset();
		OutLines.Reset();
		for (const FFortTinerSpeaker& Speaker : GFortTinerSpeakers)
		{
			OutSpeakers.Add({ Speaker.Id, Speaker.Name, Speaker.Body, Speaker.Hat });
		}
		for (const FFortTinerLine& Line : GFortTinerLines)
		{
			OutLines.Add({ Line.Speaker, Line.Row, Line.Text });
		}
	}
}

ADCCFortTinerNPC::ADCCFortTinerNPC()
{
	DialogueTable = nullptr; // lines come from gen_fort_tiner_lines.py, not DT_DCCDialogue
	Outfit = CreateDefaultSubobject<UDCCWalletComponent>(TEXT("Outfit"));
	Outfit->SetIsReplicated(false);

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	IdleAnimation = Idle.Object;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (Mesh && Manny.Succeeded())
	{
		Mesh->SetSkeletalMeshAsset(Manny.Object);
	}
	SpeechColour = FLinearColor(1.0f, 0.86f, 0.6f);
	TalkRadius = 450.0f;
}

void ADCCFortTinerNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCFortTinerNPC, Who);
}

void ADCCFortTinerNPC::BeginPlay()
{
	// Who arrives with the spawn, so everything about them can be set up here,
	// before the base class builds the script and shows the nameplate.
	SpeakerId = Who;
	TArray<FCastSpeaker> Speakers;
	TArray<FCastLine> Lines;
	LoadCast(Speakers, Lines);
	if (const FCastSpeaker* Speaker = Speakers.FindByPredicate([this](const FCastSpeaker& S) { return Who == FName(*S.Id); }))
	{
		DisplayName = FText::FromString(Speaker->Name);
		if (Speaker->Body.Equals(TEXT("Quinn"), ESearchCase::IgnoreCase) && Mesh)
		{
			if (USkeletalMesh* Quinn = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")))
			{
				Mesh->SetSkeletalMeshAsset(Quinn);
			}
		}
		if (Outfit && !Speaker->Hat.IsEmpty())
		{
			Outfit->ApplyOutfit({ FName(*Speaker->Hat) });
		}
	}
	Super::BeginPlay();
}

void ADCCFortTinerNPC::BuildScript()
{
	Script.Reset();
	NextLineIndex = 0;
	TArray<FCastSpeaker> Speakers;
	TArray<FCastLine> Lines;
	LoadCast(Speakers, Lines);
	for (const FCastLine& Line : Lines)
	{
		if (Who != FName(*Line.Speaker))
		{
			continue;
		}
		FDCCDialogueLine Entry;
		Entry.LineId = FName(*Line.Row);
		Entry.SpeakerId = Who;
		Entry.Text = FText::FromString(Line.Text);
		// About three words a second to read, never under five seconds.
		TArray<FString> Words;
		Line.Text.ParseIntoArrayWS(Words);
		Entry.DisplaySeconds = FMath::Max(5.0f, Words.Num() / 3.0f);
		Script.Add(Entry);
	}
}

void ADCCFortTinerNPC::SkipSummitLines()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World && GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	const APlayerState* State = PC ? PC->PlayerState : nullptr;
	if (State && DCCProfile::Get(State, TEXT("Climb"), TEXT("Summits")) >= 1.0f)
	{
		return;
	}
	int32 Guard = Script.Num();
	while (Guard-- > 0 && Script.IsValidIndex(NextLineIndex)
		&& Script[NextLineIndex].LineId.ToString().Contains(TEXT("Summit")))
	{
		++NextLineIndex;
	}
}

void ADCCFortTinerNPC::SayNextLine()
{
	// The mine lines stay in the script. They are not said until this crawler has been to the top.
	SkipSummitLines();
	Super::SayNextLine();
}
