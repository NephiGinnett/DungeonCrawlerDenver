#include "Narration/DCCNarratorComponent.h"

#include "Narration/DCCNarratorSettings.h"
#include "Combat/DCCGameState.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/World.h"

UDCCNarratorComponent::UDCCNarratorComponent()
{
	// Ticks only to notice silence for idle colour. Once a second is ample.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f;

	// Never replicated. The output travels as a normal feed entry; the component
	// itself is a server-side detail clients have no reason to know about.
	SetIsReplicatedByDefault(false);
}

void UDCCNarratorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UWorld* World = GetWorld())
	{
		LastEventTime = World->GetTimeSeconds();
		LastNarrationTime = World->GetTimeSeconds();
	}
}

bool UDCCNarratorComponent::ShouldNarrate(const FDCCEventLogEntry& Entry) const
{
	switch (Entry.Category)
	{
	// Beats worth a round trip: rare, significant, and each one is a moment the
	// table will already be reacting to.
	case EDCCLogCategory::Death:
	case EDCCLogCategory::Achievement:
	case EDCCLogCategory::Announcement:
	case EDCCLogCategory::Encounter:
		return true;

	// Rank-ups and levels are rare enough to be worth it.
	case EDCCLogCategory::Advancement:
	case EDCCLogCategory::Experience:
		return true;

	// Favor moving is the audience reacting, which is the narrator's own subject.
	case EDCCLogCategory::Favor:
		return true;

	// Never. Several a second, already have authored lines, and narrating each
	// would be both expensive and unbearable.
	case EDCCLogCategory::Combat:
	case EDCCLogCategory::Skill:
	default:
		return false;
	}
}

FString UDCCNarratorComponent::BuildEventContext() const
{
	const ADCCGameState* GameState = Cast<ADCCGameState>(GetOwner());
	const UDCCNarratorSettings* Settings = GetDefault<UDCCNarratorSettings>();
	if (!GameState || !Settings)
	{
		return FString();
	}

	const TArray<FDCCEventLogEntry>& Log = GameState->GetEventLog();

	const int32 Start = FMath::Max(0, Log.Num() - Settings->ContextLineCount);

	FString Context;
	for (int32 Index = Start; Index < Log.Num(); ++Index)
	{
		Context += Log[Index].Text.ToString();
		Context += TEXT("\n");
	}

	return Context;
}

void UDCCNarratorComponent::Consider(const FDCCEventLogEntry& Entry)
{
	const UWorld* World = GetWorld();
	const UDCCNarratorSettings* Settings = GetDefault<UDCCNarratorSettings>();
	if (!World || !Settings || !Settings->bEnabled)
	{
		return;
	}

	// Server only. Belt and braces - the feed is server-authored anyway, but this
	// component must never fire on a client that somehow gets one.
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	LastEventTime = World->GetTimeSeconds();

	if (!ShouldNarrate(Entry))
	{
		return;
	}

	// One request in flight, ever. A queue would only build a backlog of stale
	// narration, since local models serialise anyway - so a new beat simply wins
	// and the old one is forgotten.
	if (bRequestInFlight || World->GetTimeSeconds() < SuppressedUntilTime)
	{
		return;
	}

	if (World->GetTimeSeconds() - LastNarrationTime < Settings->MinSecondsBetweenLines)
	{
		return;
	}

	SendRequest(BuildEventContext());
}

void UDCCNarratorComponent::SendRequest(const FString& EventContext)
{
	const UDCCNarratorSettings* Settings = GetDefault<UDCCNarratorSettings>();
	if (!Settings || EventContext.IsEmpty())
	{
		return;
	}

	const FString FullPrompt = Settings->SystemPrompt
		+ TEXT("\n\nEvents:\n") + EventContext
		+ TEXT("\nNarrate the most recent event in one or two sentences.");

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("model"), Settings->Model);

	if (Settings->Api == EDCCNarratorApi::Ollama)
	{
		Writer->WriteValue(TEXT("prompt"), FullPrompt);
		Writer->WriteValue(TEXT("stream"), false);

		Writer->WriteObjectStart(TEXT("options"));
		Writer->WriteValue(TEXT("temperature"), Settings->Temperature);
		Writer->WriteValue(TEXT("num_predict"), Settings->MaxTokens);
		Writer->WriteObjectEnd();
	}
	else
	{
		// OpenAI-compatible runners want a message array rather than a flat prompt.
		Writer->WriteArrayStart(TEXT("messages"));

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("role"), TEXT("system"));
		Writer->WriteValue(TEXT("content"), Settings->SystemPrompt);
		Writer->WriteObjectEnd();

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("role"), TEXT("user"));
		Writer->WriteValue(TEXT("content"), TEXT("Events:\n") + EventContext
			+ TEXT("\nNarrate the most recent event in one or two sentences."));
		Writer->WriteObjectEnd();

		Writer->WriteArrayEnd();

		Writer->WriteValue(TEXT("temperature"), Settings->Temperature);
		Writer->WriteValue(TEXT("max_tokens"), Settings->MaxTokens);
		Writer->WriteValue(TEXT("stream"), false);
	}

	Writer->WriteObjectEnd();
	Writer->Close();

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Settings->Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetTimeout(Settings->TimeoutSeconds);
	Request->SetContentAsString(Body);

	bRequestInFlight = true;

	// Weak capture: the reply can arrive after a level change has destroyed this
	// component, and a raw 'this' would be a crash rather than a dropped line.
	TWeakObjectPtr<UDCCNarratorComponent> WeakThis(this);
	const FString ContextCopy = EventContext;

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, ContextCopy](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
		{
			UDCCNarratorComponent* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			Self->bRequestInFlight = false;

			const UDCCNarratorSettings* Settings = GetDefault<UDCCNarratorSettings>();
			const UWorld* World = Self->GetWorld();
			if (!Settings || !World)
			{
				return;
			}

			if (!bSucceeded || !Response.IsValid() || Response->GetResponseCode() != 200)
			{
				// Back off rather than retrying on every beat. A model that simply
				// is not running would otherwise fill the log with failed requests.
				Self->SuppressedUntilTime = World->GetTimeSeconds() + 60.0f;

				UE_LOG(LogTemp, Warning,
					TEXT("DCCNarrator: no reply from %s. Narration suppressed for 60s."),
					*Settings->Endpoint);
				return;
			}

			TSharedPtr<FJsonObject> Json;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
			{
				++Self->RepliesRejected;
				return;
			}

			FString Raw;

			// Ollama returns { "response": "..." }; OpenAI-compatible runners return
			// { "choices": [ { "message": { "content": "..." } } ] }.
			if (!Json->TryGetStringField(TEXT("response"), Raw))
			{
				const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
				if (Json->TryGetArrayField(TEXT("choices"), Choices) && Choices && Choices->Num() > 0)
				{
					const TSharedPtr<FJsonObject> First = (*Choices)[0]->AsObject();
					const TSharedPtr<FJsonObject> Message = First.IsValid()
						? First->GetObjectField(TEXT("message"))
						: nullptr;

					if (Message.IsValid())
					{
						Message->TryGetStringField(TEXT("content"), Raw);
					}
				}
			}

			FString Clean;
			if (!Self->SanitiseReply(Raw, ContextCopy, Clean))
			{
				++Self->RepliesRejected;
				return;
			}

			// Dropped rather than shown late. A narrator commenting on a fight that
			// ended a minute ago is worse than saying nothing.
			if (World->GetTimeSeconds() - Self->LastEventTime > 30.0f)
			{
				++Self->RepliesRejected;
				return;
			}

			Self->LastNarrationTime = World->GetTimeSeconds();
			++Self->LinesNarrated;

			ADCCGameState::Log(Self, EDCCLogCategory::Announcement, FText::FromString(Clean));
		});

	Request->ProcessRequest();
}

bool UDCCNarratorComponent::SanitiseReply(const FString& Raw, const FString& SourceContext, FString& OutClean) const
{
	const UDCCNarratorSettings* Settings = GetDefault<UDCCNarratorSettings>();
	if (!Settings)
	{
		return false;
	}

	OutClean = Raw.TrimStartAndEnd();

	if (OutClean.IsEmpty())
	{
		return false;
	}

	// Small models add these constantly despite being told not to.
	OutClean.ReplaceInline(TEXT("**"), TEXT(""));
	OutClean.ReplaceInline(TEXT("*"), TEXT(""));
	OutClean.ReplaceInline(TEXT("\""), TEXT(""));

	const TArray<FString> Prefixes = {
		TEXT("Narrator:"), TEXT("System AI:"), TEXT("AI:"), TEXT("System:")
	};

	for (const FString& Prefix : Prefixes)
	{
		if (OutClean.StartsWith(Prefix))
		{
			OutClean = OutClean.RightChop(Prefix.Len()).TrimStart();
		}
	}

	if (OutClean.Len() > Settings->MaxCharacters)
	{
		OutClean = OutClean.Left(Settings->MaxCharacters).TrimEnd() + TEXT("...");
	}

	// The invented-number guard, and the reason this function exists at all.
	//
	// Every digit in the reply must appear somewhere in the events that produced
	// it. Crude - it compares digit characters, not values - but it costs nothing
	// and it directly enforces the design doc's line that the AI never invents a
	// mechanical outcome. A narrator that says "6 damage" when the game rolled 6
	// is charming; one that says 14 is actively lying to the table.
	if (Settings->bRejectInventedNumbers)
	{
		for (const TCHAR Character : OutClean)
		{
			if (FChar::IsDigit(Character) && !SourceContext.Contains(FString::Chr(Character)))
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("DCCNarrator: rejected a reply containing an invented number."));
				return false;
			}
		}
	}

	return !OutClean.IsEmpty();
}

void UDCCNarratorComponent::TickComponent(float DeltaSeconds, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	const UDCCNarratorSettings* Settings = GetDefault<UDCCNarratorSettings>();

	if (!World || !Settings || !Settings->bEnabled || GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (Settings->IdleCommentAfterSeconds <= 0.0f || bRequestInFlight)
	{
		return;
	}

	if (World->GetTimeSeconds() < SuppressedUntilTime)
	{
		return;
	}

	const float Quiet = World->GetTimeSeconds() - LastEventTime;
	const float SinceLine = World->GetTimeSeconds() - LastNarrationTime;

	// Idle colour: nothing has happened for a while, so fill the silence. The
	// lowest-value mode, and the first to drop under any load.
	if (Quiet >= Settings->IdleCommentAfterSeconds && SinceLine >= Settings->IdleCommentAfterSeconds)
	{
		LastEventTime = World->GetTimeSeconds();
		SendRequest(BuildEventContext());
	}
}
