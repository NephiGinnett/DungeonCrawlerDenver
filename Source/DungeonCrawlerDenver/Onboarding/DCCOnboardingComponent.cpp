#include "Onboarding/DCCOnboardingComponent.h"

#include "Onboarding/DCCOnboardingActors.h"
#include "Combat/DCCGameMode.h"
#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/AudioComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UDCCOnboardingComponent::UDCCOnboardingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Matches what each crawler's ally Blueprint wears (the rig-fitted _Fixed meshes where they exist).
	auto Look = [this](const TCHAR* Id, const TCHAR* Path) { CrawlerMeshes.Add(FName(Id), TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(Path))); };
	Look(TEXT("CJ"), TEXT("/Game/DCC/Characters/HockeyWings/SK_HockeyWings.SK_HockeyWings"));
	Look(TEXT("Scott"), TEXT("/Game/DCC/Characters/OfficeAirplane/SK_OfficeAirplane_Fixed.SK_OfficeAirplane_Fixed"));
	Look(TEXT("Kyle"), TEXT("/Game/DCC/Characters/ArmsOut/SK_ArmsOut_Fixed.SK_ArmsOut_Fixed"));
	Look(TEXT("Marcel"), TEXT("/Game/DCC/Characters/DiceWinged/SK_DiceWinged_Fixed.SK_DiceWinged_Fixed"));
	Look(TEXT("Vaughn"), TEXT("/Game/DCC/Characters/Shared/SK_Vaughn_Shared.SK_Vaughn_Shared"));
	Look(TEXT("Zoe"), TEXT("/Game/DCC/Characters/Zoe/SK_Zoe.SK_Zoe"));

	SystemWindowTable = TSoftObjectPtr<UDataTable>(
		FSoftObjectPath(TEXT("/Game/DCC/UI/DT_DCCSystemWindows.DT_DCCSystemWindows")));
}

void UDCCOnboardingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// No settings actor in the level means no onboarding: keep the defaults
	// (Complete, everything unlocked) so sandbox levels are untouched.
	for (TActorIterator<ADCCOnboardingSettings> It(GetWorld()); It; ++It)
	{
		Stage = It->StartingStage;
		UnlockedFeatures = It->StartingFeatures;
		break;
	}

	// A respawned pawn is a new component. The game mode remembers where its
	// controller had got to, so dying in the trial room does not put you back
	// in bed with no HUD. Deferred one tick because possession happens after
	// BeginPlay for a freshly spawned pawn.
	//
	// Retried until the pawn has a controller, not tried once: BeginPlay runs before the game mode possesses a
	// freshly spawned pawn, so a single next-tick attempt usually found no controller and silently skipped. The
	// crawler then stayed nameless (and in the default body) until something unrelated happened to restore it
	// (found in PIE, 2026-11-05: about 50 s late). Gives up after 10 s for pawns nobody ever possesses.
	TWeakObjectPtr<UDCCOnboardingComponent> WeakThis(this);
	TSharedRef<FTimerHandle> Retry = MakeShared<FTimerHandle>();
	TSharedRef<int32> Attempts = MakeShared<int32>(0);
	GetWorld()->GetTimerManager().SetTimer(*Retry, FTimerDelegate::CreateLambda([WeakThis, Retry, Attempts]()
	{
		UDCCOnboardingComponent* Self = WeakThis.Get();
		APawn* Pawn = Self ? Cast<APawn>(Self->GetOwner()) : nullptr;
		UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
		if (!World)
		{
			return;
		}
		ADCCGameMode* GameMode = World->GetAuthGameMode<ADCCGameMode>();
		const bool bDone = !GameMode || Pawn->GetController() || ++(*Attempts) > 40;
		if (GameMode && Pawn->GetController())
		{
			UE_LOG(LogTemp, Verbose, TEXT("[DCC Onboarding] %s: controller %s after %d retries, restoring."),
				*GetNameSafe(Pawn), *GetNameSafe(Pawn->GetController()), *Attempts);
			GameMode->RestoreOnboarding(Pawn->GetController(), Self);
		}
		else if (bDone && GameMode)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Onboarding] %s: never possessed; no crawler identity applied."), *GetNameSafe(Pawn));
		}
		if (bDone)
		{
			World->GetTimerManager().ClearTimer(*Retry);
		}
	}), 0.25f, /*bLoop=*/true, /*FirstDelay=*/0.0f);
}

void UDCCOnboardingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCOnboardingComponent, CrawlerId);
	DOREPLIFETIME(UDCCOnboardingComponent, Stage);
	DOREPLIFETIME(UDCCOnboardingComponent, UnlockedFeatures);
}

namespace
{
	void RememberOnGameMode(UDCCOnboardingComponent* Component)
	{
		APawn* Pawn = Component ? Cast<APawn>(Component->GetOwner()) : nullptr;
		UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
		ADCCGameMode* GameMode = World ? World->GetAuthGameMode<ADCCGameMode>() : nullptr;
		if (GameMode && Pawn->GetController())
		{
			GameMode->RememberOnboarding(Pawn->GetController(), Component);
		}
	}
}

void UDCCOnboardingComponent::AdvanceTo(EDCCOnboardingStage NewStage)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || NewStage <= Stage)
	{
		return;
	}

	Stage = NewStage;
	UE_LOG(LogTemp, Log, TEXT("[DCC Onboarding] %s (%s) -> stage %d"),
		*GetNameSafe(GetOwner()), *CrawlerId.ToString(), static_cast<int32>(Stage));

	// Finishing onboarding issues whatever was still withheld, so a crawler who
	// skipped a trigger is never left without a menu for the rest of the game.
	if (Stage == EDCCOnboardingStage::Complete)
	{
		UnlockedFeatures = DCCHud::AllFeatures;
	}

	RememberOnGameMode(this);
}

void UDCCOnboardingComponent::UnlockFeatures(int32 FeatureMask)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || FeatureMask == 0)
	{
		return;
	}

	UnlockedFeatures |= FeatureMask;
	RememberOnGameMode(this);
}

bool UDCCOnboardingComponent::BuildPayload(FName RowName, FDCCSystemWindowPayload& OutPayload, int32& OutUnlocks) const
{
	OutUnlocks = 0;

	UDataTable* Table = SystemWindowTable.LoadSynchronous();
	if (!Table || RowName.IsNone())
	{
		return false;
	}

	const FDCCSystemWindowDef* Row = Table->FindRow<FDCCSystemWindowDef>(RowName, TEXT("DCCOnboarding"), /*bWarnIfRowMissing=*/false);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Onboarding] No System window row '%s' in %s."),
			*RowName.ToString(), *GetNameSafe(Table));
		return false;
	}

	OutPayload.Header = Row->Header;
	OutPayload.Title = Row->Title;
	OutPayload.Body = Row->Body;
	OutPayload.Style = Row->Style;
	OutPayload.Voice = Row->Voice;
	OutUnlocks = Row->UnlockFeatures;
	return true;
}

bool UDCCOnboardingComponent::ShowWindowRow(FName RowName)
{
	FDCCSystemWindowPayload Payload;
	int32 Unlocks = 0;
	if (!BuildPayload(RowName, Payload, Unlocks))
	{
		return false;
	}

	UnlockFeatures(Unlocks);
	ShowWindow(Payload);
	return true;
}

void UDCCOnboardingComponent::ShowWindow(const FDCCSystemWindowPayload& Window)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Client_ShowSystemWindow(Window);
	}
}

void UDCCOnboardingComponent::AdvanceLater(EDCCOnboardingStage NewStage, FName WindowRow, float DelaySeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TWeakObjectPtr<UDCCOnboardingComponent> WeakThis(this);
	auto Fire = [WeakThis, NewStage, WindowRow]()
	{
		if (UDCCOnboardingComponent* Self = WeakThis.Get())
		{
			Self->AdvanceTo(NewStage);
			Self->ShowWindowRow(WindowRow);
		}
	};

	if (DelaySeconds <= 0.0f)
	{
		Fire();
		return;
	}

	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Fire), DelaySeconds, false);
}

void UDCCOnboardingComponent::NotifyOwner(const FText& Message)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Client_Notify(Message);
	}
}

void UDCCOnboardingComponent::ApplyRestoredState(FName InCrawlerId, bool bHasProgress, EDCCOnboardingStage InStage, int32 InFeatures)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!InCrawlerId.IsNone() && InCrawlerId != CrawlerId)
	{
		CrawlerId = InCrawlerId;
		ApplyCrawlerLook(); // the server's own copy; clients get it through OnRep_CrawlerId
	}

	// Only progress overrides the level's starting values. A first spawn has
	// none and keeps what ADCCOnboardingSettings gave it in BeginPlay.
	if (bHasProgress)
	{
		Stage = InStage;
		UnlockedFeatures = InFeatures;
	}
}

void UDCCOnboardingComponent::Client_Notify_Implementation(const FText& Message)
{
	if (const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner()))
	{
		Crawler->ShowLocalNotice(Message);
	}
}

void UDCCOnboardingComponent::OnRep_CrawlerId()
{
	ApplyCrawlerLook();
}

void UDCCOnboardingComponent::ApplyCrawlerLook()
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(GetOwner());
	if (!Crawler || CrawlerId.IsNone())
	{
		return;
	}

	// The nameplate reads DisplayName, which replicates, so only the server writes it.
	if (Crawler->HasAuthority())
	{
		Crawler->DisplayName = FText::FromName(CrawlerId);
	}

	const TSoftObjectPtr<USkeletalMesh>* Wanted = CrawlerMeshes.Find(CrawlerId);
	USkeletalMesh* Body = Wanted ? Wanted->LoadSynchronous() : nullptr;
	USkeletalMeshComponent* MeshComponent = Crawler->GetMesh();
	if (!Body || !MeshComponent || MeshComponent->GetSkeletalMeshAsset() == Body)
	{
		return;
	}

	MeshComponent->SetSkeletalMesh(Body, /*bReinitPose=*/true);
	Crawler->RestartLoopAnimation();

	// Sockets are per mesh (CLAUDE.md), so anything held or worn has to be re-hung on the new body.
	if (Crawler->Equipment)
	{
		Crawler->Equipment->RefreshAttachments();
	}

	UE_LOG(LogTemp, Log, TEXT("[DCC Onboarding] %s is wearing %s's body (%s)."),
		*GetNameSafe(Crawler), *CrawlerId.ToString(), *GetNameSafe(Body));
}

void UDCCOnboardingComponent::OnRep_Stage()
{
	// Nothing to do yet: the HUD and the sky beacon read the stage every frame.
	// Kept as a hook for stage-change stingers.
}

void UDCCOnboardingComponent::Client_ShowSystemWindow_Implementation(const FDCCSystemWindowPayload& Window)
{
	EnqueueLocal(Window);
}

void UDCCOnboardingComponent::EnqueueLocal(const FDCCSystemWindowPayload& Window)
{
	PendingWindows.Add(Window);
}

void UDCCOnboardingComponent::ShowLocalTutorialOnce(FName RowName)
{
	if (RowName.IsNone() || ShownLocalTutorials.Contains(RowName))
	{
		return;
	}
	ShownLocalTutorials.Add(RowName);

	FDCCSystemWindowPayload Payload;
	int32 IgnoredUnlocks = 0;
	if (BuildPayload(RowName, Payload, IgnoredUnlocks))
	{
		EnqueueLocal(Payload);
	}
}

const FDCCSystemWindowPayload* UDCCOnboardingComponent::GetActiveWindow()
{
	if (PendingWindows.Num() == 0)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (ActiveSince < 0.0f && World)
	{
		ActiveSince = World->GetRealTimeSeconds();

		if (USoundBase* Voice = PendingWindows[0].Voice.LoadSynchronous())
		{
			ActiveVoice = UGameplayStatics::SpawnSound2D(this, Voice);
		}
	}

	return &PendingWindows[0];
}

float UDCCOnboardingComponent::GetActiveWindowAge() const
{
	const UWorld* World = GetWorld();
	return (World && ActiveSince >= 0.0f) ? World->GetRealTimeSeconds() - ActiveSince : 0.0f;
}

bool UDCCOnboardingComponent::DismissActiveWindow()
{
	if (PendingWindows.Num() == 0)
	{
		return false;
	}

	// Still swallow the press: E during the unroll should not also open a corpse.
	if (GetActiveWindowAge() < MinimumWindowSeconds)
	{
		return true;
	}

	if (UAudioComponent* Voice = ActiveVoice.Get())
	{
		Voice->Stop();
	}
	ActiveVoice.Reset();

	PendingWindows.RemoveAt(0);
	ActiveSince = -1.0f;
	return true;
}
