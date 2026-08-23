#pragma once

#include "CoreMinimal.h"
#include "DCCChatTypes.generated.h"

class ADCCCharacterBase;

// -----------------------------------------------------------------------------
// PLAYER-FACING CHAT
//
// Separate from the Feed (DCCEventLogTypes.h / EDCCLogCategory), which is a
// mechanical dice-and-damage log the System AI narrates and nobody types into.
// This is what the five people at the table actually type to each other, plus
// the simulated audience. See Docs/story-premise.md's "The System AI wants
// something" section for why Chatter (the audience) is deliberately a
// different concept from AI Favor.
// -----------------------------------------------------------------------------

/** Which channel a chat message belongs to. Feed and Chatter are not here -
 *  Feed is EDCCLogCategory's own system, Chatter's simulated lines are tagged
 *  Chatter below alongside the player channels so all four can share one
 *  replicated array and one set of draw/filter code. */
UENUM(BlueprintType)
enum class EDCCChatChannel : uint8
{
	/** Broadcast to everyone currently connected. */
	Party      UMETA(DisplayName = "Party"),

	/** Same delivery as Party - broadcast to everyone connected - but the tab
	 *  pairs it with a roster of who you've actually fist-bumped and is still
	 *  alive, rather than the full crawler roster Party's tab shows. */
	Allies     UMETA(DisplayName = "Allies"),

	/** The only channel with real 1:1 targeting. WhisperRecipientName on the
	 *  message names who it's for; the HUD filters client-side to messages
	 *  where the local player is sender or recipient. Not cryptographically
	 *  private - this is a trusted five-person table, not a competitive game -
	 *  just not shown outside the two participants' own UI. */
	Whispers   UMETA(DisplayName = "Whispers"),

	/** Simulated audience lines. Never sent by a player - see
	 *  ADCCGameState::RequestChatterBatch. */
	Chatter    UMETA(DisplayName = "Chatter"),

	MAX        UMETA(Hidden)
};

/**
 * One line in a chat channel.
 *
 * SenderCharacter is kept (not just a name snapshot) specifically so the dead-
 * crawler header can be evaluated live at render time - a message sent while
 * someone was alive still needs to grow the header the moment they die, and a
 * held reference makes that a status check rather than something that has to
 * be re-stamped retroactively across history.
 */
USTRUCT(BlueprintType)
struct FDCCChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	EDCCChatChannel Channel = EDCCChatChannel::Party;

	/** Display name at time of send. Kept even though SenderCharacter also
	 *  exists, so a message still reads sensibly if the actor is ever gone. */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString SenderName;

	/** Null for Chatter lines - the audience isn't a character in the world. */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	TObjectPtr<ADCCCharacterBase> SenderCharacter = nullptr;

	/** Only meaningful when Channel == Whispers. */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString WhisperRecipientName;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FText Text;

	/** Server world-seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	float TimeSeconds = 0.0f;

	/** Chatter-only: this line was rolled as "removed for violating community
	 *  guidelines" instead of real content - see EDCCChatterBucket::Moderated. */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	bool bModerated = false;
};
