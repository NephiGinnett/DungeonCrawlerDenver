#pragma once

#include "CoreMinimal.h"

/**
 * The notes on the bodies in Dead Air's mine (2026-11-15). The stories and why they are told this way are in
 * Docs/dead-air-mine-plan.md section 2. Edit the wording here; the game reads it straight from these tables.
 *
 * Notes are in a miner's 1893 voice. The System never writes a note; it only speaks when a story is complete.
 */
namespace DCCMineNotes
{
	struct FStory
	{
		const TCHAR* Title;
		/** The System's line when the crew has read every note in the story. */
		const TCHAR* Complete;
		/** Where the payoff ghost appears: 0 the deepest stope, 1 the cage, 2 a middle stope. */
		int32 PayoffAt;
		/** The payoff is a small warm light (the canary), not a miner. */
		bool bBird;
	};

	struct FNote
	{
		/** Index into Stories(), or -1 for a loose note. */
		int32 Story;
		/** What is written on the body's tag or scrap. */
		const TCHAR* Text;
	};

	inline const TArray<FStory>& Stories()
	{
		static const TArray<FStory> List = {
			{ TEXT("The Platinum Man"),
				TEXT("Story complete: The Platinum Man. Viewers have voted the white metal 'definitely platinum'. The assay office has been notified and does not care."), 0, false },
			{ TEXT("The Knife in the Lunch Pail"),
				TEXT("Story complete: The Knife in the Lunch Pail. Chat predicted a murder. Chat is upset. Please rate this twist."), 1, false },
			{ TEXT("The Tommyknockers' Due"),
				TEXT("Story complete: The Tommyknockers' Due. Legal would like to clarify that the tommyknockers are not union. Please stop asking."), 2, false },
			{ TEXT("The Canary"),
				TEXT("Story complete: The Canary. We are not crying. This is a heritage experience."), 2, true },
			{ TEXT("The Bell at 4:15"),
				TEXT("Story complete: The Bell at 4:15. The bank that sold the Lucky Tuesday is now a subsidiary of this programme's parent company. We have nothing further to add."), 1, false },
		};
		return List;
	}

	inline const TArray<FNote>& Notes()
	{
		static const TArray<FNote> List = {
			// The Platinum Man
			{ 0, TEXT("Pay docked from 3.50 to 2.75 a day. \"The white metal will make it up to you,\" says Rourke. Make it up to my wife.") },
			{ 0, TEXT("Rourke had us chase the white streak forty feet past the timbering. Said platinum doesn't wait for timber.") },
			{ 0, TEXT("ASSAY, Lucky Tuesday, deep vein, sample 6: Galena. Pyrite. Trace silver. PLATINUM: 0.0 oz. Do not tell the men.") },
			{ 0, TEXT("Rourke tore up the assay in front of us and said it was a forgery by the bank.") },
			{ 0, TEXT("It is in here. I can hear it. Tell Margaret I was right.  - E. Rourke, Foreman") },
			// The Knife in the Lunch Pail
			{ 1, TEXT("Found a knife in my pail this morning. Not mine. Halvard says not his either. He did not look at me when he said it.  - T. Ruiz") },
			{ 1, TEXT("Knife is gone again. Halvard was down the drift alone at noon.") },
			{ 1, TEXT("Teo will not hold the steel for me now. He holds it for the new boy instead. Fine.  - H. Lund") },
			{ 1, TEXT("Knife is back in my pail, wrapped in a rag this time. I am not sleeping in the bunkhouse.") },
			{ 1, TEXT("It's for the hoist rope, Teo. If the cage does not come, you cut it and you climb. I could not say it in front of Rourke. Happy birthday. H.") },
			// The Tommyknockers' Due
			{ 2, TEXT("Left the crust on the ledge by No. 4 like Gran said. Rourke kicked it into the sump.") },
			{ 2, TEXT("Three knocks last night in the east drift. Pengelly pulled us out. The roof came down an hour later.") },
			{ 2, TEXT("Rourke has banned the crusts. Rats, he says. Pengelly says the knockers do not forget.") },
			{ 2, TEXT("Knocking all day now. Everywhere. No one will tell Rourke what it means.") },
			// The Canary
			{ 3, TEXT("Here is Pip. He is the loud one. If he goes quiet, you go up. Love, Maisie.") },
			{ 3, TEXT("Mr. Rourke says Pip is costing the company 4 cents a week. I am paying it myself.") },
			{ 3, TEXT("Please send Pip up with the shift tonight. Please.") },
			{ 3, TEXT("(on an empty canary cage, the door open) Pip came up on his own on Wednesday. I waited at the cage all night. I am still waiting.  - M.") },
			// The Bell at 4:15
			{ 4, TEXT("Bell at 4:15. Three rings. We ran. Cage was already going up empty.") },
			{ 4, TEXT("We rang back. Nine rings, the signal for men on the bottom. Nothing came down.") },
			{ 4, TEXT("Pike was a good man. Pike had a note from the bank. Those are not the same thing.") },
			{ 4, TEXT("(scratched into a cage bar) A. PIKE. 1893. I HEARD THEM. I HEARD THEM EVERY NIGHT.") },
			// Loose notes
			{ -1, TEXT("If you find this, I owe Duffy 40 cents. Do not pay him. He cheats at cribbage.") },
			{ -1, TEXT("Candle stub, matches, one boot. Where is the other boot. Who takes ONE boot.") },
			{ -1, TEXT("Day four. Water's sweet in the north sump. Rourke still says the cage is coming.") },
			{ -1, TEXT("Somebody keeps moving my pick. I put it down HERE. I am writing this down so I know I am not mad.") },
			{ -1, TEXT("Mother, the work is steady and the air is good. (That is not true. Do not show her this one.)") },
			// Mount Fourteener has never been left alone. Later visitors, each sure the mountain owed them something.
			{ -1, TEXT("U.S. Geological Survey, 1931. Third crew this decade sent to measure Fourteener from the inside. Third crew to find the levels do not add up. The mountain is taller inside than out.") },
			{ -1, TEXT("1958. Geiger counter clicking since the No. 2 level. Not the rock, the rock is clean. It is the cage.  - Uranium Prospecting Co-op of Golden") },
			{ -1, TEXT("Field trip, Colorado School of Mines, 1967. Prof says Mount Fourteener has been claimed, sold, lost and claimed again eleven times since 1871. Nobody keeps it. Nobody stops wanting it.") },
			{ -1, TEXT("Alpine Rescue tag, 1988: SEARCH ENDED. Two hikers reported knocking from inside the peak. Found: one lunch pail, very old, still warm.") },
			{ -1, TEXT("SPOOKSEEKERS, episode 13, production note, 2004: do NOT air the bell footage. Network says it is \"too real for basic cable\".") },
			{ -1, TEXT("Fourteener Resort & Spa, Phase II survey, 2009. The old mine becomes the \"Heritage Experience\". Remove the bodies before the soft opening. (Who keeps putting the bodies back?)") },
			{ -1, TEXT("Note to self: the summit is 14,115 ft. The shaft goes down 2,000. My altimeter says I am at 15,600. Do not tell Fort Tiner.") },
		};
		return List;
	}
}
