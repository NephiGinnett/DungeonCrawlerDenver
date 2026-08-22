#include "Progression/DCCPopulationSubsystem.h"

#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"

void UDCCPopulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CrawlersRemaining = StartingPopulation;
	SeedKnownCrawlers();
}

void UDCCPopulationSubsystem::SeedKnownCrawlers()
{
	KnownCrawlers.Reset();

	// Authored, not generated. A procedural name with a random stat block is
	// somebody nobody can care about; what makes a rival land is that they have a
	// deal. Each of these is one sentence long on purpose - enough for the AI to
	// reference them, little enough that promoting one to a real character later
	// contradicts nothing.
	auto Add = [this](FText Name, FText Reputation, int32 Rank)
	{
		FDCCKnownCrawler Crawler;
		Crawler.Name = MoveTemp(Name);
		Crawler.Reputation = MoveTemp(Reputation);
		Crawler.Rank = Rank;
		KnownCrawlers.Add(MoveTemp(Crawler));
	};

	Add(NSLOCTEXT("DCC", "CrawlerHollowPoint", "Hollow Point"),
		NSLOCTEXT("DCC", "CrawlerHollowPointRep",
			"has not spoken since Floor 1 and is four floors ahead of everyone who has"), 12);

	Add(NSLOCTEXT("DCC", "CrawlerMrsAlbright", "Mrs. Albright"),
		NSLOCTEXT("DCC", "CrawlerMrsAlbrightRep",
			"is a retired substitute teacher who has killed more crawlers than the dungeon has"), 47);

	Add(NSLOCTEXT("DCC", "CrawlerTheAppraiser", "The Appraiser"),
		NSLOCTEXT("DCC", "CrawlerTheAppraiserRep",
			"follows parties at a polite distance and takes what they leave behind"), 903);

	Add(NSLOCTEXT("DCC", "CrawlerSixteenSeconds", "Sixteen Seconds"),
		NSLOCTEXT("DCC", "CrawlerSixteenSecondsRep",
			"holds the record for the fastest floor clear and the record for the fastest death"), 88);

	Add(NSLOCTEXT("DCC", "CrawlerDenverDave", "Denver Dave"),
		NSLOCTEXT("DCC", "CrawlerDenverDaveRep",
			"is sponsored by nine competing brands and cannot legally endorse any of them"), 2214);

	Add(NSLOCTEXT("DCC", "CrawlerTheQuietFloor", "The Quiet Floor"),
		NSLOCTEXT("DCC", "CrawlerTheQuietFloorRep",
			"is a party of eleven who have stopped descending and started building"), 156);
}

void UDCCPopulationSubsystem::ApplyAttrition(int64 Deaths, const FText& Cause)
{
	if (Deaths <= 0)
	{
		return;
	}

	// Never below one - the party themselves are in this number, and a dungeon
	// reporting zero survivors while five people are standing in it is the kind
	// of detail that breaks the fiction on the one night it gets noticed.
	CrawlersRemaining = FMath::Max<int64>(1, CrawlersRemaining - Deaths);

	// The party climb as the field thins, without ever quite catching up - the
	// rank improves more slowly than the population falls, so progress is
	// visible but never free.
	const int32 RankGain = FMath::Max(1, static_cast<int32>(Deaths / 12));
	PartyRank = FMath::Max(1, PartyRank - RankGain);

	// Only when a cause is actually given. The floor-transition call site
	// (ADCCFloorExit) already builds and logs its own generic ticker line via
	// BuildPopulationAnnouncement, so announcing again here on every floor exit
	// would just be noise on top of noise - Cause was previously accepted and
	// never read at all, which is the gap this closes. A named cause (the DM
	// invoking this by hand, say) is specific enough to be worth its own line.
	if (!Cause.IsEmpty())
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "PopulationAttritionCause",
				"{0} crawlers lost to {1}. The number keeps its distance from you specifically."),
			FText::AsNumber(Deaths), Cause));
	}
}

void UDCCPopulationSubsystem::ApplyAttritionFraction(float Fraction, const FText& Cause)
{
	const float Clamped = FMath::Clamp(Fraction, 0.0f, 0.99f);
	ApplyAttrition(static_cast<int64>(CrawlersRemaining * Clamped), Cause);
}

void UDCCPopulationSubsystem::AdjustPartyRank(int32 Delta)
{
	PartyRank = FMath::Max(1, PartyRank + Delta);
}

FText UDCCPopulationSubsystem::KillKnownCrawler(FName CrawlerId)
{
	for (FDCCKnownCrawler& Crawler : KnownCrawlers)
	{
		// Matched on the display string rather than a separate id field: the list
		// is six entries authored in one place, and a parallel id column would be
		// one more thing to keep in sync for no benefit at this size.
		if (Crawler.bAlive && Crawler.Name.ToString() == CrawlerId.ToString())
		{
			Crawler.bAlive = false;

			return FText::Format(
				NSLOCTEXT("DCC", "KnownCrawlerDied",
					"{0} is out. Ranked {1}. The audience will be told they were always overrated."),
				Crawler.Name, FText::AsNumber(Crawler.Rank));
		}
	}

	return FText::GetEmpty();
}

FText UDCCPopulationSubsystem::BuildPopulationAnnouncement()
{
	// Four shapes, rotated. The point is not variety for its own sake - it is
	// that a broadcaster who says the same sentence twice stops sounding like a
	// broadcaster and starts sounding like a system.
	switch (AnnouncementIndex++ % 4)
	{
	case 0:
		return FText::Format(
			NSLOCTEXT("DCC", "PopRemaining",
				"{0} crawlers remain. Local time is 4:15 AM. Please enjoy the complimentary humidity."),
			FText::AsNumber(CrawlersRemaining));

	case 1:
		return FText::Format(
			NSLOCTEXT("DCC", "PopRank",
				"Your party is currently ranked {0}. This is not a compliment, it is a coordinate."),
			FText::AsNumber(PartyRank));

	case 2:
	{
		// Only ever cites someone still alive, so the feed does not refer to the
		// dead in the present tense.
		TArray<const FDCCKnownCrawler*> Living;
		for (const FDCCKnownCrawler& Crawler : KnownCrawlers)
		{
			if (Crawler.bAlive)
			{
				Living.Add(&Crawler);
			}
		}

		if (Living.IsEmpty())
		{
			break;
		}

		const FDCCKnownCrawler* Pick = Living[FMath::RandRange(0, Living.Num() - 1)];

		return FText::Format(
			NSLOCTEXT("DCC", "PopRival", "Elsewhere on the network: {0} {1}."),
			Pick->Name, Pick->Reputation);
	}

	default:
		return FText::Format(
			NSLOCTEXT("DCC", "PopViewership",
				"{0} crawlers are still competing for your slot. Several of them are better at this."),
			FText::AsNumber(CrawlersRemaining));
	}

	return FText::GetEmpty();
}
