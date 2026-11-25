#include "Arcade/DCCWallTV.h"

#include "Arcade/DCCArcadeArena.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"

ADCCWallTV::ADCCWallTV()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Bezel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bezel"));
	Bezel->SetupAttachment(RootComponent);
	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(RootComponent);
	Caption = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Caption"));
	Caption->SetupAttachment(RootComponent);
}

void ADCCWallTV::ApplyColour(UStaticMeshComponent* Mesh, const FLinearColor& Colour)
{
	if (!Mesh)
	{
		return;
	}
	UMaterialInterface* Base = Mesh->GetMaterial(0);
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Mesh))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Colour);
		Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
		Mesh->SetMaterial(0, Mid);
	}
}

void ADCCWallTV::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Bezel && Cube)
	{
		Bezel->SetStaticMesh(Cube);
		Bezel->SetRelativeScale3D(FVector(0.12f, 1.9f, 1.15f));
		Bezel->SetCollisionProfileName(TEXT("NoCollision"));
		ApplyColour(Bezel, FLinearColor(0.04f, 0.04f, 0.05f));
	}
	if (Screen && Cube)
	{
		Screen->SetStaticMesh(Cube);
		Screen->SetRelativeLocation(FVector(8.0f, 0.0f, 0.0f));
		Screen->SetRelativeScale3D(FVector(0.04f, 1.7f, 0.95f));
		Screen->SetCollisionProfileName(TEXT("NoCollision"));
		ApplyColour(Screen, FLinearColor(0.02f, 0.05f, 0.08f));
	}
	if (Caption)
	{
		Caption->SetRelativeLocation(FVector(14.0f, 0.0f, 0.0f));
		Caption->SetHorizontalAlignment(EHTA_Center);
		Caption->SetVerticalAlignment(EVRTA_TextCenter);
		Caption->SetWorldSize(22.0f);
		Caption->SetTextRenderColor(FColor(190, 230, 255));
		Caption->SetText(FText::FromString(TEXT("STAND BY")));
	}
	LoadLines();
	NextSwap = 1.0f;
}

void ADCCWallTV::LoadLines()
{
	Lines.Reset();
	const FString Path = FPaths::ProjectContentDir() / TEXT("DCC/TV/playlist.json");
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Text, *Path)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Root->TryGetArrayField(TEXT("lines"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Arr)
			{
				const FString Line = Value.IsValid() ? Value->AsString() : FString();
				if (!Line.IsEmpty())
				{
					Lines.Add(Line);
				}
			}
		}
	}
	if (Lines.IsEmpty())
	{
		Lines.Add(TEXT("The Safe Room is on the air."));
		Lines.Add(TEXT("Mile High Recovery Solutions thanks you for watching."));
	}
}

void ADCCWallTV::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	ADCCArcadeArena* Arena = nullptr;
	if (World)
	{
		for (TActorIterator<ADCCArcadeArena> It(World); It; ++It)
		{
			Arena = *It;
			break;
		}
	}
	if (Arena && Arena->HasAuthority() && !bPushedNews)
	{
		bPushedNews = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const FString Path = FPaths::ProjectSavedDir() / TEXT("TVNews.json");
		if (FFileHelper::LoadFileToString(Text, *Path)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Root->TryGetArrayField(TEXT("lines"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Arr)
				{
					if (Value.IsValid())
					{
						Arena->AddNews(Value->AsString());
					}
				}
			}
		}
	}

	NextSwap -= DeltaSeconds;
	if (NextSwap > 0.0f || !Caption)
	{
		return;
	}
	NextSwap = 8.0f;

	TArray<FString> Show = Lines;
	if (Arena)
	{
		for (const FString& News : Arena->NewsLines)
		{
			Show.Add(News);
		}
	}
	if (Show.IsEmpty())
	{
		return;
	}
	Index = (Index + 1) % Show.Num();
	FString Line = Show[Index];
	if (Line.Len() > 48)
	{
		Line = Line.Left(45) + TEXT("...");
	}
	Caption->SetText(FText::FromString(Line));
}

void DCCNews::Post(UWorld* World, const FString& Line)
{
	if (!World || Line.IsEmpty() || World->GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<ADCCArcadeArena> It(World); It; ++It)
	{
		It->AddNews(Line);
	}

	const FString Path = FPaths::ProjectSavedDir() / TEXT("TVNews.json");
	FString Text;
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Lines;
	if (FFileHelper::LoadFileToString(Text, *Path))
	{
		TSharedPtr<FJsonObject> Existing;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Existing) && Existing.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Existing->TryGetArrayField(TEXT("lines"), Arr))
			{
				Lines = *Arr;
			}
		}
	}
	Lines.Insert(MakeShared<FJsonValueString>(Line), 0);
	while (Lines.Num() > 10)
	{
		Lines.RemoveAt(Lines.Num() - 1);
	}
	Root->SetArrayField(TEXT("lines"), Lines);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	if (FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		FFileHelper::SaveStringToFile(Out, *Path);
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC News] %s"), *Line);
}

namespace
{
	TMap<FString, FString> GAnnouncerPaths;
	bool bAnnouncerLoaded = false;

	void LoadAnnouncers()
	{
		if (bAnnouncerLoaded)
		{
			return;
		}
		bAnnouncerLoaded = true;
		GAnnouncerPaths.Add(TEXT("Start"), TEXT("/Game/RadioSoldier_Announcer/wav/26_Entering_Hostile_Territory.26_Entering_Hostile_Territory"));
		GAnnouncerPaths.Add(TEXT("Wave"), TEXT("/Game/RadioSoldier_Announcer/wav/02_Hostiles_Inbound.02_Hostiles_Inbound"));
		GAnnouncerPaths.Add(TEXT("Boss"), TEXT("/Game/RadioSoldier_Announcer/wav/16_Enemy_On_the_Move.16_Enemy_On_the_Move"));
		GAnnouncerPaths.Add(TEXT("Wall"), TEXT("/Game/RadioSoldier_Announcer/wav/25_We_re_Under_Attack.25_We_re_Under_Attack"));
		GAnnouncerPaths.Add(TEXT("Good"), TEXT("/Game/RadioSoldier_Announcer/wav/13_Good_Work_Soldier.13_Good_Work_Soldier"));
		GAnnouncerPaths.Add(TEXT("Win"), TEXT("/Game/RadioSoldier_Announcer/wav/20_Mission_Accomplished.20_Mission_Accomplished"));
		GAnnouncerPaths.Add(TEXT("Lose"), TEXT("/Game/RadioSoldier_Announcer/wav/18_Objective_Lost.18_Objective_Lost"));
		GAnnouncerPaths.Add(TEXT("Fight"), TEXT("/Game/RadioSoldier_Announcer/wav/24_Fire_At_Will.24_Fire_At_Will"));

		FString Text;
		TSharedPtr<FJsonObject> Root;
		const FString Path = FPaths::ProjectContentDir() / TEXT("DCC/Announcers/announcers.json");
		if (!FFileHelper::LoadFileToString(Text, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return;
		}
		const TSharedPtr<FJsonObject>* Moments = nullptr;
		if (!Root->TryGetObjectField(TEXT("moments"), Moments) || !Moments || !Moments->IsValid())
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Moments)->Values)
		{
			const FString Sound = Pair.Value.IsValid() ? Pair.Value->AsString() : FString();
			if (!Sound.IsEmpty())
			{
				GAnnouncerPaths.Add(Pair.Key, Sound);
			}
		}
	}
}

FString DCCAnnouncer::PathFor(const FString& Moment)
{
	LoadAnnouncers();
	const FString* Found = GAnnouncerPaths.Find(Moment);
	return Found ? *Found : FString();
}

void DCCAnnouncer::PlayLocal(const UObject* WorldContext, const FString& Moment)
{
	const FString Path = PathFor(Moment);
	if (Path.IsEmpty() || !WorldContext)
	{
		return;
	}
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path))
	{
		UGameplayStatics::PlaySound2D(WorldContext, Sound, 0.55f);
	}
}
