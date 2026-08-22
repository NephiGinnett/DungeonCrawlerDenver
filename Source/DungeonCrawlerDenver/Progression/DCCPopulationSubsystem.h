#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DCCPopulationSubsystem.generated.h"

// ---------------------------------------------------------------------------
// THE OTHER FOUR MILLION
//
// The fiction has millions of crawlers in the dungeon. The party will ever meet
// about fifteen. Both of those things need to be true at once, and the way to
// get there is NOT to spawn more actors - it is to notice that the audience for
// scale is the player's imagination, and the delivery mechanism is a broadcaster
// who reports numbers.
//
// So the population lives here, as data, and is never an actor. Nothing in this
// file can be walked up to, fought, or rendered. It ticks down, it ranks the
// party against people who do not exist, and it hands the System AI something to
// say. That costs a few hundred bytes and produces the entire sense of a
// continent-sized death game happening off-screen.
//
// The failure mode this deliberately avoids: making the millionth crawler
// *slightly* real. An actor pool, a crowd simulation, even a lightweight
// representative sample - each buys the full cost of the population and almost
// none of the drama, because the drama was always the number going down.
//
// A GameInstanceSubsystem rather than something on the GameState, because the
// count has to survive travelling between floors. A floor is a level load; the
// death toll is not.
// ---------------------------------------------------------------------------

/**
 * One crawler the party has heard of but will probably never meet.
 *
 * Rivals are authored, not generated. A procedural name and a random stat block
 * produces someone nobody can care about - the reason "Prepotente" or "Quasar
 * Fanatic" land in the books is that they have a *deal*, not that they have
 * numbers. These exist to be named in announcements and to occasionally arrive
 * as a real actor, at which point they are promoted to a proper character class.
 */
USTRUCT(BlueprintType)
struct FDCCKnownCrawler
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Population")
	FText Name;

	/** What the audience knows them for. Used verbatim in announcements. */
	UPROPERTY(BlueprintReadOnly, Category = "Population")
	FText Reputation;

	/** Leaderboard position. Lower is better; 1 is the whole dungeon's favourite. */
	UPROPERTY(BlueprintReadOnly, Category = "Population")
	int32 Rank = 0;

	/** Cleared once they die, so the feed stops referring to them in the present. */
	UPROPERTY(BlueprintReadOnly, Category = "Population")
	bool bAlive = true;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCPopulationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * How many crawlers are still alive in the whole dungeon.
	 *
	 * int64 because the number genuinely exceeds two billion in later seasons of
	 * the fiction, and because a silent overflow in a number the AI reads aloud
	 * every few minutes would be a very stupid bug to hunt.
	 */
	UFUNCTION(BlueprintPure, Category = "Population")
	int64 GetCrawlersRemaining() const { return CrawlersRemaining; }

	/** The party's leaderboard position out of the surviving population. */
	UFUNCTION(BlueprintPure, Category = "Population")
	int32 GetPartyRank() const { return PartyRank; }

	UFUNCTION(BlueprintPure, Category = "Population")
	const TArray<FDCCKnownCrawler>& GetKnownCrawlers() const { return KnownCrawlers; }

	/**
	 * Kills off part of the population and, if Cause is non-empty, announces it.
	 *
	 * Called at floor transitions and by the DM. Attrition is a *beat*, not a
	 * simulation - it fires when it means something rather than continuously,
	 * because a number that drips down constantly stops being read. Cause is
	 * left empty by the automatic floor-transition call site (ADCCFloorExit),
	 * which already logs its own generic line via BuildPopulationAnnouncement -
	 * a named cause is for the DM invoking this deliberately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Population")
	void ApplyAttrition(int64 Deaths, const FText& Cause);

	/**
	 * Kills a fraction of everyone left. The floor-transition shape.
	 *
	 * Fractional rather than absolute so it stays meaningful as the population
	 * collapses - "300,000 died" means nothing once only 90,000 remain.
	 */
	UFUNCTION(BlueprintCallable, Category = "Population")
	void ApplyAttritionFraction(float Fraction, const FText& Cause);

	/** Improves the party's standing. Lower rank is better. */
	UFUNCTION(BlueprintCallable, Category = "Population")
	void AdjustPartyRank(int32 Delta);

	/**
	 * A line for the System AI about the wider dungeon, or empty.
	 *
	 * Deliberately returns empty most of the time. The AI mentioning the death
	 * toll every ninety seconds turns the most affecting number in the game into
	 * wallpaper; mentioning it rarely, and always attached to something that just
	 * happened, keeps it landing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Population")
	FText BuildPopulationAnnouncement();

	/** Marks a named crawler dead and returns a line about it. */
	UFUNCTION(BlueprintCallable, Category = "Population")
	FText KillKnownCrawler(FName CrawlerId);

	/** Starting population. Authored so a session can begin mid-season. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population")
	int64 StartingPopulation = 4182900;

private:
	int64 CrawlersRemaining = 0;

	/** Starts deep in the pack. The party are nobody until they are somebody. */
	int32 PartyRank = 318442;

	UPROPERTY()
	TArray<FDCCKnownCrawler> KnownCrawlers;

	/** Rotates announcement shapes so the AI does not repeat itself verbatim. */
	int32 AnnouncementIndex = 0;

	void SeedKnownCrawlers();
};
