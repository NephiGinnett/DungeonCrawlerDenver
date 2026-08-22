#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DCCGameMode.generated.h"

class ADCCRoom;

/**
 * The match rules, and specifically where a joining crawler appears.
 *
 * Exists because the project's premise is a five-player synchronous session and
 * there was, until now, no join path at all: one PlayerStart in the level, no
 * spawn selection, and `PostLogin` left entirely to the engine default. Five
 * people connecting meant five crawlers materialising inside one another.
 *
 * Two rules, in order:
 *
 *   1. A late joiner belongs in a Safe Room, not in the middle of whatever the
 *      party is currently losing to. `ADCCRoom::AllowsAsyncLogin()` already
 *      derives that from room type rather than authoring, so a floor layout
 *      cannot get it wrong - this just reads it.
 *   2. Failing that, the emptiest PlayerStart wins. "Emptiest" rather than
 *      "first free" because a start can be unoccupied and still be two metres
 *      from somebody, and spawning on top of a teammate is the failure this
 *      class exists to prevent.
 *
 * Deliberately a GameModeBase subclass with the project's classes as defaults,
 * so a fresh GameMode Blueprint needs no wiring to be correct. The existing
 * `BP_ThirdPersonGameMode` already points at ADCCGameState and ADCCHUD by hand;
 * reparenting it here makes those defaults rather than assumptions.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADCCGameMode();

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/**
	 * How close another crawler has to be for a PlayerStart to count as taken.
	 *
	 * Generous - a capsule is 42 units across, so this is roughly "three people
	 * could stand here without touching" rather than "this exact point is free".
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DCC|Spawning", meta = (ClampMin = "0.0"))
	float SpawnClearanceRadius = 250.0f;

	/**
	 * Whether a player joining a match already in progress is routed to a Safe
	 * Room rather than the default start.
	 *
	 * On, because the alternative is dropping somebody into a fight they have no
	 * context for, several rooms from their party.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DCC|Spawning")
	bool bRouteLateJoinersToSafeRoom = true;

	/**
	 * `DCCHost` - open this level as a listen server so friends can connect.
	 *
	 * The design doc puts hosting on Zoe's machine deliberately: the narrator
	 * talks to a model running locally, the dedicated server target has never
	 * been built, and five scheduled friends are not a matchmaking problem.
	 *
	 * On the GameMode rather than the character so it still works from the menu
	 * or before possessing a pawn.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "DCC|Session")
	void DCCHost();

	/** `DCCWhoAmI` - prints the local addresses to read out to the party. */
	UFUNCTION(Exec, BlueprintCallable, Category = "DCC|Session")
	void DCCWhoAmI();

protected:
	/** The Safe Room a late joiner should appear in, or null if the floor has none. */
	ADCCRoom* FindAsyncLoginRoom() const;

	/**
	 * Distance from this point to the nearest living crawler.
	 *
	 * Returned rather than a bool so callers can rank starts by how clear they
	 * are instead of taking the first one that passes a threshold.
	 */
	float DistanceToNearestCrawler(const FVector& Location) const;
};
