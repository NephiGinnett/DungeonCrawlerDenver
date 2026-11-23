#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;
class ADCCGameState;

/**
 * The live audience (2026-11-23, Zoe: "I don't feel like the 'audience' is really a part of the game yet ... can you
 * borrow from what we already have like the chatter system for the main DCD game?").
 *
 * Every game is on the air. ADCCGameState carries a viewer count and a hype level (0-1) that everyone sees in the
 * corner, with the chat scrolling above it. The chat is the existing Chatter channel and its line table
 * (DT_DCCChatterLines), plus reaction lines written here for the moments that matter.
 *
 *  - Moment():  something watchable happened. Viewers jump, hype rises, chat reacts (often naming who did it), and
 *               the crawler earns a little AI Favor and some Fame.
 *  - Fame (2026-11-23, Zoe: "a progress / exp bar that awards prizes or lootboxes at various levels progressing in
 *               rarity", in place of sponsors paying tickets): every moment earns Fame, doubled at full hype. Each Fame
 *               level pays a loot box, rarer as the levels climb (FameReward). Kept on the player's arcade profile
 *               ("Audience.Fame"), at most 150 a minute so it cannot be farmed. The box goes to the crawler's own
 *               loot boxes, like an achievement's.
 *  - Tick():    hype cools, viewers drift toward what the hype deserves, and the chat keeps talking: slowly when
 *               nothing is happening, fast when the room is on fire.
 * Server only. The HUD reads ADCCGameState::LiveViewers / Hype and the Chatter messages.
 */
namespace DCCAudience
{
	enum class EMoment : uint8
	{
		Kill,
		BossDown,
		Downed,
		Revive,
		Trick,
		PerfectTrick,
		Jackpot,
		Achievement,
		LevelUp,
		RoomClear,
		Wipeout,
	};

	void Moment(const UObject* WorldContext, ADCCCharacterBase* Who, EMoment Kind);

	/** Fame needed to go from Level to Level + 1. */
	int32 FameToNext(int32 Level);
	/** The loot box tier (EDCCLootBoxTier as a number) for reaching Level. */
	uint8 FameReward(int32 Level);

	/** Server, about twice a second (ADCCGameState's audience timer). */
	void Tick(ADCCGameState* State, float DeltaSeconds);
}
