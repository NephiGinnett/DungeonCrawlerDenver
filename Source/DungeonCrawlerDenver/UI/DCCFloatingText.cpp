#include "UI/DCCFloatingText.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

ADCCFloatingText::ADCCFloatingText()
{
	PrimaryActorTick.bCanEverTick = true;

	// Never replicated: every machine spawns its own from a delegate that already
	// fires everywhere.
	bReplicates = false;

	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	SetRootComponent(Text);

	// Movable is load-bearing, not tidiness. Scene components default to Static,
	// and a Static root silently refuses both the spawn transform and the
	// per-frame rise - the number ends up stranded at the world origin, buried
	// under the level, spawning correctly and visible to nobody.
	Text->SetMobility(EComponentMobility::Movable);

	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetWorldSize(48.0f);

	// No collision and no shadow - this is a readout, not an object in the world.
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Text->SetCastShadow(false);
}

ADCCFloatingText* ADCCFloatingText::Spawn(
	const UObject* WorldContext,
	const FVector& Location,
	int32 Delta,
	bool bOnCrawler)
{
	if (Delta == 0)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADCCFloatingText* Ticker = World->SpawnActor<ADCCFloatingText>(
		ADCCFloatingText::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);

	if (!Ticker || !Ticker->Text)
	{
		return nullptr;
	}

	const bool bHealing = Delta > 0;
	const int32 Magnitude = FMath::Abs(Delta);

	// Colour carries who it happened to, which is the thing that is hard to read
	// in a five-player fight: red means one of us is being hurt, white means we
	// are hurting something, green is always good news.
	if (bHealing)
	{
		Ticker->BaseColor = FLinearColor(0.25f, 1.0f, 0.35f);
	}
	else if (bOnCrawler)
	{
		Ticker->BaseColor = FLinearColor(1.0f, 0.25f, 0.2f);
	}
	else
	{
		Ticker->BaseColor = FLinearColor(1.0f, 0.95f, 0.8f);
	}

	Ticker->Text->SetText(FText::AsNumber(Magnitude));
	Ticker->Text->SetTextRenderColor(Ticker->BaseColor.ToFColor(true));

	// Heals read smaller than damage so a big hit still dominates the screen.
	Ticker->Text->SetWorldSize(bHealing ? 40.0f : 52.0f);

	return Ticker;
}

void ADCCFloatingText::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSeconds);
}

void ADCCFloatingText::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedSeconds += DeltaSeconds;

	AddActorWorldOffset(FVector(0.0f, 0.0f, RiseSpeed * DeltaSeconds));

	// Face the local camera. TextRender draws on a flat quad, so without this the
	// number is edge-on and invisible from most angles.
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const APlayerCameraManager* Camera = PC->PlayerCameraManager)
		{
			const FVector ToCamera = Camera->GetCameraLocation() - GetActorLocation();
			if (!ToCamera.IsNearlyZero())
			{
				SetActorRotation(ToCamera.Rotation());
			}
		}
	}

	// Fade over the back half of the lifetime, so the number is fully readable
	// before it starts disappearing.
	if (Text && LifeSeconds > 0.0f)
	{
		const float Alpha = FMath::Clamp(1.0f - ((ElapsedSeconds / LifeSeconds) - 0.5f) / 0.5f, 0.0f, 1.0f);

		FLinearColor Faded = BaseColor;
		Faded.A = Alpha;
		Text->SetTextRenderColor(Faded.ToFColor(true));
	}
}
