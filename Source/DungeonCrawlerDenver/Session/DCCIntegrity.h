#pragma once

#include "CoreMinimal.h"

class AActor;
class ADCCCharacterBase;
class UWorld;

/**
 * The Integrity Office (2026-11-22, after the first night online. Zoe: Vaughn "was able to find the values representing
 * stats by equipping and unequipping items ... and then edit it and give himself max stats", and "play the games 1000
 * times a second". "I think that applying punishments for this type of abuse rather than strictly banning the player
 * would be the most fun.")
 *
 * The host is the only machine whose numbers count. A guest's game can show whatever it likes on its own screen. This
 * watches for a guest trying to make the host believe it:
 *
 *  - Allow():  a rate limit on the buttons that pay out (casino, summons, loot boxes, shops). A person can press a
 *              button about eight times a second; a script can press it a thousand. Over the limit, the press is
 *              refused and it is a strike.
 *  - Report:   every few seconds a guest's game says what it shows (its stats, level, gold, tickets). Numbers the host
 *              never gave out, still there fifteen seconds later, are a strike. That is what editing memory looks like.
 *  - The DM's buttons (the F1 panel, gold, levels, invulnerability) work for the host's own crawler only. A guest
 *              pressing one is a strike.
 *  - A visiting profile (DCCProfile::MergeFromClient) that claims a mountain of tickets earned away from this PC is
 *              capped, and a strike.
 *
 * Strikes are punished in public, never with a ban. The System is delighted: the audience loves a cheater.
 *   1st  announced, the Tampering With The Evidence achievement, and a 10% ticket fine.
 *   2nd  announced, 25% fine, and five minutes of garnished winnings (tickets earned go to the house).
 *   3rd+ the full audit: half the tickets, ten minutes garnished, and CHEATER on the nameplate for the rest of the visit.
 * Strikes are saved on the crawler's profile ("Integrity.Strikes"), so the ladder remembers.
 */
namespace DCCIntegrity
{
	/** Server. False (and a strike) when this player presses Bucket faster than PerSecond, after a burst of Burst. */
	bool Allow(AActor* WhoActor, const TCHAR* Bucket, float PerSecond, float Burst);

	/** Server. A strike, with the reason said out loud ("pressing the scratch-card button faster than a hand can"). */
	void Flag(ADCCCharacterBase* Who, const FString& Reason);

	/** Server. A strike against a profile key with no pawn handy (the profile merge). Applied when they next appear. */
	void FlagKey(UWorld* World, const FString& Key, const FString& Reason);

	/** Server. A guest's report of what their screen shows, in ReportOrder. */
	void CheckReport(ADCCCharacterBase* Who, const TArray<int32>& Seen);
	/** Both sides build the report the same way: 5 enhanced scores, level, gold, tickets. */
	void BuildReport(const ADCCCharacterBase* Who, TArray<int32>& Out);

	/** True while Key's winnings are garnished. DCCProfile::AddTickets checks this. */
	bool IsGarnished(const FString& Key);

	/** How many strikes Key has on record. */
	int32 Strikes(const FString& Key);

	/**
	 * Host: the DM's audit button. Resets a crawler to the bottom: level 1, every score 2, no gold, no XP, no stat
	 * points spent, and the tickets to 10. Works on a crawler who is here (their pawn) and on their saved profile.
	 * Says so in public. False if there is nobody by that name and no profile.
	 */
	bool Audit(UWorld* World, const FString& Key, FString& OutReport);
}
