#include "Combat/DCCGameMode.h"

#include "Combat/DCCGameState.h"
#include "Characters/DCCCharacterBase.h"
#include "Rooms/DCCRoom.h"
#include "UI/DCCHUD.h"
#include "UI/DCCEventLogTypes.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SocketSubsystem.h"

ADCCGameMode::ADCCGameMode()
{
	// Defaults rather than per-Blueprint wiring. GameStateClass in particular has
	// a silent failure mode - without ADCCGameState the pause/queue loop does
	// nothing at all, and nothing says so.
	GameStateClass = ADCCGameState::StaticClass();
	HUDClass = ADCCHUD::StaticClass();

	// Everyone arrives at once at the start of a session, so seamless travel and
	// mid-match joining both matter from the first playtest.
	bUseSeamlessTravel = true;
}

ADCCRoom* ADCCGameMode::FindAsyncLoginRoom() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ADCCRoom> It(World); It; ++It)
	{
		ADCCRoom* Room = *It;

		// Derived from room type rather than authored per level, so a floor
		// cannot accidentally declare a boss arena a safe place to appear.
		if (Room && Room->AllowsAsyncLogin())
		{
			return Room;
		}
	}

	return nullptr;
}

float ADCCGameMode::DistanceToNearestCrawler(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return TNumericLimits<float>::Max();
	}

	float Nearest = TNumericLimits<float>::Max();

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		const ADCCCharacterBase* Character = *It;

		// Corpses do not reserve a spawn point. Downed crawlers do - landing on
		// somebody who is bleeding out would shove them around the floor.
		if (!Character || !Character->IsCrawler() || Character->IsPendingKillPending())
		{
			continue;
		}

		if (!Character->IsAlive() && !Character->IsDowned())
		{
			continue;
		}

		Nearest = FMath::Min(Nearest, FVector::Dist(Location, Character->GetActorLocation()));
	}

	return Nearest;
}

AActor* ADCCGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// A match already underway means this is a late joiner, and the Safe Room is
	// where they belong - see ADCCRoom::AllowsAsyncLogin, which was built for
	// exactly this and has been waiting for a caller.
	const bool bMatchUnderway = GetWorld()->GetTimeSeconds() > 5.0f;

	if (bRouteLateJoinersToSafeRoom && bMatchUnderway)
	{
		if (ADCCRoom* SafeRoom = FindAsyncLoginRoom())
		{
			// Returned as the start actor itself; the engine spawns at its
			// transform, which for a room is its centre.
			return SafeRoom;
		}
	}

	// Otherwise: the PlayerStart furthest from anyone already standing around.
	AActor* Best = nullptr;
	float BestClearance = -1.0f;

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}

		const float Clearance = DistanceToNearestCrawler(Start->GetActorLocation());

		// An empty level returns FLT_MAX for every start, so the first one wins
		// and the behaviour matches the old single-start default exactly.
		if (Clearance > BestClearance)
		{
			BestClearance = Clearance;
			Best = Start;
		}
	}

	if (Best)
	{
		// Only worth saying when it was actually a problem - five people joining
		// a level with five starts should not print five warnings.
		if (BestClearance < SpawnClearanceRadius)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[DCC Spawn] Best PlayerStart is only %.0f units from another crawler. "
					"Add more PlayerStarts if players are landing on each other."),
				BestClearance);
		}

		return Best;
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void ADCCGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Announced through the same feed as everything else, so the table sees
	// arrivals in the running account of the session rather than only in a log.
	const int32 Count = GetNumPlayers();

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogPlayerJoined",
			"A crawler has entered the floor. {0} now competing. Please welcome them warmly and briefly."),
		FText::AsNumber(Count)));

	UE_LOG(LogTemp, Log, TEXT("[DCC Spawn] Player joined. %d total."), Count);
}

void ADCCGameMode::DCCHost()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ?listen is the whole of it. Everything else in the multiplayer plan -
	// direct IP, Tailscale, no matchmaking - follows from the host simply being
	// a running copy of the game with the flag set.
	const FString LevelName = World->GetMapName();
	const FString Command = FString::Printf(TEXT("open %s?listen"), *LevelName);

	UE_LOG(LogTemp, Warning, TEXT("[DCC Session] Hosting: %s"), *Command);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->ConsoleCommand(Command);
	}
}

void ADCCGameMode::DCCWhoAmI()
{
	// Printed rather than shown in the HUD because the person who needs it is
	// reading it aloud to four other people, and the console can be copied from.
	bool bCanBind = false;
	TSharedPtr<FInternetAddr> Addr;

	if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		Addr = Sockets->GetLocalHostAddr(*GLog, bCanBind);
	}

	const FString Local = Addr.IsValid() ? Addr->ToString(false) : TEXT("unknown");

	UE_LOG(LogTemp, Warning,
		TEXT("[DCC Session] This machine: %s. Others join with:  open %s"), *Local, *Local);

	UE_LOG(LogTemp, Warning,
		TEXT("[DCC Session] If you are not on the same LAN, use the Tailscale address instead - "
			"the LAN one will not route."));
}
