#include "UI/DCCFloatingText.h"

#include "Arcade/DCCCosmetics.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCAllyCharacters.h"
#include "Characters/DCCCharacterBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "UI/DCCDamageSkins.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
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

	Glow = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Text);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetHorizontalAlignment(EHTA_Center);
	Glow->SetVerticalAlignment(EVRTA_TextCenter);
	Glow->SetRelativeLocation(FVector(-3.0f, 0.0f, 0.0f)); // just behind the number, seen from the camera
	Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow->SetCastShadow(false);
	Glow->SetVisibility(false);
}

void ADCCFloatingText::SpawnDamage(AActor* Target, const FVector& Location, int32 Amount, bool bOnCrawler, const ADCCCharacterBase* Dealer)
{
	if (Amount <= 0 || !Target)
	{
		return;
	}
	struct FRecent
	{
		TWeakObjectPtr<AActor> Target;
		int32 Amount = 0;
		double Time = 0.0;
		TWeakObjectPtr<ADCCFloatingText> Ticker;
		bool bTagged = false;
	};
	static TArray<FRecent> Recent;
	const double Now = FPlatformTime::Seconds();
	Recent.RemoveAll([Now](const FRecent& R) { return Now - R.Time > 0.6 || !R.Target.IsValid(); });
	const bool bTagged = Dealer != nullptr;
	for (FRecent& R : Recent)
	{
		if (R.Target.Get() != Target || R.Amount != Amount)
		{
			continue;
		}
		if (bTagged && !R.bTagged)
		{
			// The plain number came first: give it its dealer's look.
			R.bTagged = true;
			if (ADCCFloatingText* Ticker = R.Ticker.Get())
			{
				Ticker->ApplyStyle(Dealer, Amount);
			}
			return;
		}
		if (!bTagged && R.bTagged)
		{
			return; // already shown, in colour
		}
	}
	ADCCFloatingText* Ticker = Spawn(Target, Location, -Amount, bOnCrawler);
	if (Ticker && Dealer)
	{
		Ticker->ApplyStyle(Dealer, Amount);
	}
	FRecent Entry;
	Entry.Target = Target;
	Entry.Amount = Amount;
	Entry.Time = Now;
	Entry.Ticker = Ticker;
	Entry.bTagged = bTagged;
	Recent.Add(Entry);
}

void ADCCFloatingText::ApplyStyle(const ADCCCharacterBase* Dealer, int32 Amount)
{
	if (!Text || !Dealer)
	{
		return;
	}
	// Who it belongs to: a player, or the player whose summoned ally dealt it. Enemies keep the plain look.
	FString OwnerName;
	const ADCCCharacterBase* Player = Dealer->IsPlayerControlled() ? Dealer : nullptr;
	if (Player && Player->GetPlayerState())
	{
		OwnerName = Player->GetPlayerState()->GetPlayerName();
	}
	else if (const ADCCSummonedAlly* Ally = Cast<ADCCSummonedAlly>(Dealer))
	{
		OwnerName = Ally->SummonerName;
	}
	if (OwnerName.IsEmpty())
	{
		return;
	}
	const APlayerController* Local = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const bool bMine = Local && Player && Local->GetPawn() == Player;
	const bool bAlly = Player == nullptr;
	BaseColor = DCCDamageSkins::PlayerColour(OwnerName) * (bAlly ? 0.75f : 1.0f);
	BaseColor.A = 1.0f;
	BaseSize = (bMine ? 62.0f : 52.0f) * (bAlly ? 0.85f : 1.0f);
	FString Shown = FText::AsNumber(Amount).ToString();

	// The skin, worn in the Numbers slot. A summoned ally's numbers stay plain, in its summoner's colour.
	const DCCDamageSkins::FSkin* Skin = Player && Player->Wallet ? DCCDamageSkins::Find(Player->Wallet->WornIn(DCCCosmetics::Numbers)) : nullptr;
	if (Skin)
	{
		BaseColor = Skin->Main;
		GlowColor = Skin->Glow;
		BaseSize *= Skin->Scale;
		Motion = uint8(Skin->Motion);
		Shown = FString(Skin->Prefix) + Shown + Skin->Suffix;
		if (Glow)
		{
			Glow->SetVisibility(true);
			Glow->SetText(FText::FromString(Shown));
			Glow->SetWorldSize(BaseSize * 1.12f);
			Glow->SetTextRenderColor(GlowColor.ToFColor(true));
		}
	}
	Text->SetText(FText::FromString(Shown));
	Text->SetWorldSize(BaseSize);
	Text->SetTextRenderColor(BaseColor.ToFColor(true));
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
	Ticker->BaseSize = bHealing ? 40.0f : 52.0f;
	Ticker->Text->SetWorldSize(Ticker->BaseSize);

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

	if (Origin.IsZero())
	{
		Origin = GetActorLocation();
		Seed = FMath::FRand() * 10.0f;
	}
	const float T = ElapsedSeconds;
	FVector Offset(0.0f, 0.0f, RiseSpeed * T);
	float Size = BaseSize;
	FVector Side = FVector::ZeroVector;
	if (const APlayerController* SidePC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr)
	{
		if (SidePC->PlayerCameraManager)
		{
			Side = SidePC->PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();
		}
	}
	switch (static_cast<DCCDamageSkins::EMotion>(Motion))
	{
	case DCCDamageSkins::EMotion::Flicker:
		Offset.Z = RiseSpeed * 1.5f * T;
		Size *= 1.0f + 0.08f * FMath::Sin((T + Seed) * 40.0f);
		break;
	case DCCDamageSkins::EMotion::Drift:
		Offset.Z = RiseSpeed * 0.5f * T - 30.0f * FMath::Max(0.0f, T - 0.8f);
		Size *= 1.0f - 0.15f * FMath::Clamp(T / LifeSeconds, 0.0f, 1.0f);
		break;
	case DCCDamageSkins::EMotion::Wobble:
		Offset += Side * 18.0f * FMath::Sin((T + Seed) * 9.0f);
		break;
	case DCCDamageSkins::EMotion::Pop:
		Size *= T < 0.12f ? FMath::Lerp(1.7f, 1.0f, T / 0.12f) : 1.0f;
		Offset.Z = RiseSpeed * 0.8f * T;
		break;
	case DCCDamageSkins::EMotion::Glitch:
		if (FMath::Fmod((T + Seed) * 15.0f, 1.0f) < 0.3f)
		{
			Offset += Side * FMath::FRandRange(-14.0f, 14.0f) + FVector(0, 0, FMath::FRandRange(-6.0f, 6.0f));
		}
		break;
	case DCCDamageSkins::EMotion::Stamp:
		Size *= T < 0.1f ? FMath::Lerp(2.2f, 1.0f, T / 0.1f) : 1.0f;
		Offset.Z = T < 0.6f ? 0.0f : RiseSpeed * 0.6f * (T - 0.6f);
		break;
	default:
		break;
	}
	SetActorLocation(Origin + Offset);
	if (Text)
	{
		Text->SetWorldSize(Size);
	}
	if (Glow && Glow->IsVisible())
	{
		Glow->SetWorldSize(Size * 1.12f);
	}

	// Face the local camera. TextRender draws on a flat quad, so without this the
	// number is edge-on and invisible from most angles.
	//
	// Local, not first: GetFirstPlayerController returns the host on a listen
	// server, which would turn every client's damage numbers edge-on — the exact
	// failure this billboarding exists to prevent.
	if (const APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr)
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
		if (Glow && Glow->IsVisible())
		{
			FLinearColor GlowFaded = GlowColor;
			GlowFaded.A = Alpha;
			Glow->SetTextRenderColor(GlowFaded.ToFColor(true));
		}
	}
}
