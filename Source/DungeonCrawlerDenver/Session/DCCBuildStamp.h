#pragma once

#include "CoreMinimal.h"

/**
 * Which build of the game this is (2026-11-14). Everyone in a party has to run the same build, so the stamp is shown
 * on the main menu and the host refuses a join from a different one with a clear "update your game" message, rather
 * than letting mismatched code fail in strange ways later.
 *
 * Tools/package_game.ps1 writes the stamp into Config/DefaultGame.ini ([DCCBuild] Stamp=...) just before it cooks,
 * so every packaged build carries the time it was made. The editor (and -game runs of it) report "dev", and a "dev"
 * host lets anyone in, so testing from the editor is never blocked.
 */
namespace DCCBuildStamp
{
	DUNGEONCRAWLERDENVER_API FString Get();
	DUNGEONCRAWLERDENVER_API bool IsDev();
}
