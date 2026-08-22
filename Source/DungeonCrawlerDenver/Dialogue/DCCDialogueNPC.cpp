#include "Dialogue/DCCDialogueNPC.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

ADCCDialogueNPC::ADCCDialogueNPC()
{
	// Ticks for the line timer and to billboard the bubble. Both cheap.
	PrimaryActorTick.bCanEverTick = true;

	// Replicated so the actor exists on clients, but the talking itself is driven
	// locally from the replicated script - see BeginPlay.
	bReplicates = true;

	// Defaulted so a placed NPC talks without anyone remembering to assign a
	// table. SpeakerId still selects which lines it says, so every NPC on the
	// floor can share this one script.
	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultTable(
		TEXT("/Game/DCC/Dialogue/DT_DCCDialogue.DT_DCCDialogue"));
	if (DefaultTable.Succeeded())
	{
		DialogueTable = DefaultTable.Object;
	}

	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetCapsuleSize(42.0f, 90.0f);

	// Blocks movement so the party cannot walk through them, but is invisible to
	// every combat query. Nothing that fights should ever find this actor.
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Matches the character template's offset so an imported Meshy model lines up
	// with the capsule without hand-tuning every time.
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	TalkTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("TalkTrigger"));
	TalkTrigger->SetupAttachment(Collision);
	TalkTrigger->SetSphereRadius(TalkRadius);
	TalkTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TalkTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	TalkTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TalkTrigger->SetGenerateOverlapEvents(true);

	Speech = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Speech"));
	Speech->SetupAttachment(Collision);
	Speech->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	Speech->SetHorizontalAlignment(EHTA_Center);
	Speech->SetVerticalAlignment(EVRTA_TextCenter);
	Speech->SetWorldSize(20.0f);
	Speech->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Movable for the same reason every other runtime text component here is: a
	// Static text component ignores transform changes and never moves again.
	Speech->SetMobility(EComponentMobility::Movable);
}

void ADCCDialogueNPC::BeginPlay()
{
	Super::BeginPlay();

	TalkTrigger->SetSphereRadius(TalkRadius);
	TalkTrigger->OnComponentBeginOverlap.AddDynamic(this, &ADCCDialogueNPC::HandleTalkOverlapBegin);
	TalkTrigger->OnComponentEndOverlap.AddDynamic(this, &ADCCDialogueNPC::HandleTalkOverlapEnd);

	// Built on every machine rather than replicated. The script is authored data
	// that every client already has on disk, and the lines are cosmetic - there is
	// nothing here worth spending bandwidth on.
	BuildScript();

	if (Mesh && IdleAnimation)
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->PlayAnimation(IdleAnimation, /*bLooping=*/true);
	}

	ShowIdleText();
}

void ADCCDialogueNPC::BuildScript()
{
	Script.Reset();
	NextLineIndex = 0;

	if (!DialogueTable || SpeakerId.IsNone())
	{
		return;
	}

	static const FString Context(TEXT("ADCCDialogueNPC::BuildScript"));

	TArray<FDCCDialogueLine*> Rows;
	DialogueTable->GetAllRows<FDCCDialogueLine>(Context, Rows);

	TArray<FDCCDialogueLine> Hints;

	for (const FDCCDialogueLine* Row : Rows)
	{
		if (!Row || Row->SpeakerId != SpeakerId)
		{
			continue;
		}

		// Hints are held back to the end regardless of where they sit in the table,
		// so the useful line is a reward for listening rather than the first thing
		// said. That is the tone bible's Safe Room posture, enforced here rather
		// than left to whoever orders the rows.
		if (Row->bIsHint)
		{
			Hints.Add(*Row);
		}
		else
		{
			Script.Add(*Row);
		}
	}

	Script.Append(Hints);
}

bool ADCCDialogueNPC::HasMoreToSay() const
{
	return Script.IsValidIndex(NextLineIndex);
}

void ADCCDialogueNPC::GetRemainingLines(TArray<FDCCDialogueLine>& OutLines) const
{
	OutLines.Reset();

	for (int32 Index = NextLineIndex; Index < Script.Num(); ++Index)
	{
		OutLines.Add(Script[Index]);
	}
}

void ADCCDialogueNPC::SayNextLine()
{
	if (!HasMoreToSay())
	{
		if (!bLoopWhenExhausted || Script.IsEmpty())
		{
			return;
		}

		NextLineIndex = 0;
	}

	ShowLine(Script[NextLineIndex++]);
}

void ADCCDialogueNPC::ShowLine(const FDCCDialogueLine& Line)
{
	bSpeaking = true;
	LineTimeRemaining = Line.DisplaySeconds > 0.0f ? Line.DisplaySeconds : DefaultLineSeconds;

	if (Speech)
	{
		Speech->SetText(Line.Text);
		Speech->SetTextRenderColor(SpeechColour.ToFColor(/*bSRGB=*/true));
		Speech->SetVisibility(true);
	}

	if (Mesh && TalkAnimation)
	{
		Mesh->PlayAnimation(TalkAnimation, /*bLooping=*/false);
	}
}

void ADCCDialogueNPC::ShowIdleText()
{
	bSpeaking = false;

	if (!Speech)
	{
		return;
	}

	if (DisplayName.IsEmpty())
	{
		Speech->SetVisibility(false);
		return;
	}

	// Dimmed, so a resting nameplate is visibly not someone talking to you.
	Speech->SetText(DisplayName);
	Speech->SetTextRenderColor((SpeechColour * 0.55f).ToFColor(/*bSRGB=*/true));
	Speech->SetVisibility(true);

	if (Mesh && IdleAnimation)
	{
		Mesh->PlayAnimation(IdleAnimation, /*bLooping=*/true);
	}
}

void ADCCDialogueNPC::HandleTalkOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	// Only someone a person is driving. An ally NPC wandering past should not use
	// up the bartender's script on nobody.
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	++ListenerCount;

	if (ListenerCount == 1 && !bSpeaking)
	{
		SayNextLine();
	}
}

void ADCCDialogueNPC::HandleTalkOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	ListenerCount = FMath::Max(0, ListenerCount - 1);
}

void ADCCDialogueNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (LineTimeRemaining > 0.0f)
	{
		LineTimeRemaining -= DeltaSeconds;

		if (LineTimeRemaining <= 0.0f)
		{
			if (bSpeaking)
			{
				// Line finished. Drop to the idle nameplate and start the gap - the
				// pause is what stops a monologue reading as a wall of text.
				ShowIdleText();
				LineTimeRemaining = PauseBetweenLinesSeconds;
			}
			else if (ListenerCount > 0)
			{
				// Gap finished and somebody is still standing here listening.
				SayNextLine();
			}
		}
	}
	else if (ListenerCount > 0 && !bSpeaking && HasMoreToSay())
	{
		SayNextLine();
	}

	// Billboarded per machine towards its own viewer.
	//
	// Deliberately NOT GetFirstPlayerController: on a listen server that returns
	// the host, so every remote client's text would turn to face Zoe's camera.
	// GEngine->GetFirstLocalPlayerController resolves per machine, which is the
	// whole point. The nameplates and quest markers have the same bug and are
	// listed in Docs/multiplayer-plan.md as certain-to-be-wrong.
	if (Speech && Speech->IsVisible())
	{
		if (const APlayerController* PC = GEngine
			? GEngine->GetFirstLocalPlayerController(GetWorld())
			: nullptr)
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

			// Forward must point AT the viewer - see the identical fix and comment
			// on ADCCCharacterBase's nameplate billboarding, same bug.
			Speech->SetWorldRotation((ViewLocation - Speech->GetComponentLocation()).Rotation());
		}
	}
}
