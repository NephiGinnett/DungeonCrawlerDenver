#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCTalkShowHost.generated.h"

/**
 * Hiss Cranson, host of PRIMETIME, at his desk in the Den (2026-11-23, Zoe: "a talk show NPC with many voice
 * lines and they can introduce the players and show them the fame meter and everything like that").
 *
 * Walk up to the stage and he runs a segment: an opener, each crawler in the room introduced by name with a reaction
 * to their Fame (or their Integrity strikes), two explainers about the LIVE corner and the Fame track, a tip about
 * something new, and a sign-off. Between segments an idle quip, then a fresh segment if anyone is still there.
 * Lines and tags come from SourceArt/Voice/NPC/talkshow_lines.json (Tools/gen_talkshow_lines.py), voices from
 * SourceArt/Voice/NPC/Hiss/VO_<row>.wav. The script is built on each machine for its own player, like the impound
 * staff. Spawned by ADCCArcadeLobby::ServerBegin; the set and the FAME BOARD are ADCCArcadeLobby::BuildTalkShow.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCTalkShowHost : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	ADCCTalkShowHost();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BuildScript() override;

private:
	/** 0 waiting, 1 a full segment is playing, 2 an idle quip is playing. */
	uint8 Phase = 0;
	double NextSegmentAt = 0.0;
	double NextIdleAt = 0.0;
	/** Build an idle quip instead of a segment on the next BuildScript. */
	bool bIdleNext = false;
};
