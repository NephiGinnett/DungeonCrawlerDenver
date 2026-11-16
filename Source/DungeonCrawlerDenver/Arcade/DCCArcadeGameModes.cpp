#include "Arcade/DCCArcadeGameModes.h"

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCArcadeHaunt.h"
#include "Arcade/DCCArcadeAudit.h"
#include "Arcade/DCCArcadeGate.h"
#include "Arcade/DCCArcadeFrontDoor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ADCCArcadeGameMode::ADCCArcadeGameMode()
{
	bUseSeamlessTravel = true;
}

void ADCCArcadeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// Same crawler, input and controller as the main game.
	if (UClass* MainMode = LoadClass<AGameModeBase>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C")))
	{
		const AGameModeBase* Defaults = MainMode->GetDefaultObject<AGameModeBase>();
		DefaultPawnClass = Defaults->DefaultPawnClass;
		PlayerControllerClass = Defaults->PlayerControllerClass;
	}

	Super::InitGame(MapName, Options, ErrorMessage);

	// The arena, and its PlayerStarts, before anyone logs in.
	if (UWorld* World = GetWorld())
	{
		if (const TSubclassOf<ADCCArcadeArena> ArenaClass = GetArenaClass())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADCCArcadeArena* Arena = World->SpawnActor<ADCCArcadeArena>(ArenaClass, FTransform::Identity, Params))
			{
				Arena->SpawnStarts();
			}
		}
	}
}

TSubclassOf<ADCCArcadeArena> ADCCLobbyGameMode::GetArenaClass() const { return ADCCArcadeLobby::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCClimbGameMode::GetArenaClass() const { return ADCCArcadeClimb::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCKartGameMode::GetArenaClass() const { return ADCCArcadeKart::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCNeonGameMode::GetArenaClass() const { return ADCCArcadeNeon::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCHauntGameMode::GetArenaClass() const { return ADCCArcadeHaunt::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCAuditGameMode::GetArenaClass() const { return ADCCArcadeAudit::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCGateGameMode::GetArenaClass() const { return ADCCArcadeGate::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCFrontDoorGameMode::GetArenaClass() const { return ADCCArcadeFrontDoor::StaticClass(); }
TSubclassOf<ADCCArcadeArena> ADCCBenchGameMode::GetArenaClass() const { return ADCCArcadeFrontDoor::StaticClass(); }
