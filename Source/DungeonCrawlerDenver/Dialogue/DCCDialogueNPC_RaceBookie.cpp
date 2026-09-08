#include "Dialogue/DCCDialogueNPC_RaceBookie.h"

#include "Dialogue/DCCDialogueTypes.h"
#include "Vehicles/DCCRaceTrack.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"

ADCCDialogueNPC_RaceBookie::ADCCDialogueNPC_RaceBookie()
{
	SpeakerId = TEXT("Bookie");
}

void ADCCDialogueNPC_RaceBookie::PlaceBetAndStartRace(ADCCCharacterBase* Interactor)
{
	if (!Interactor || !LinkedTrack)
	{
		return;
	}

	FDCCDialogueLine Line;
	Line.SpeakerId = SpeakerId;

	if (LinkedTrack->IsRaceInProgress())
	{
		Line.Text = NSLOCTEXT("DCC", "BookieAlreadyRunning",
			"Book's closed, they're already away. Watch it like everyone else.");
		ShowLine(Line);
		return;
	}

	// Money moves on the server only, same as every other gold transaction in
	// this project - a client that could pay its own stake could also decline to.
	if (!HasAuthority())
	{
		return;
	}

	if (WagerAmount > 0)
	{
		UDCCStatsComponent* Stats = Interactor->Stats;
		if (!Stats || !Stats->SpendGold(WagerAmount))
		{
			Line.Text = FText::Format(NSLOCTEXT("DCC", "BookieNoGold",
				"That's {0} gold to play. Come back when you're solvent."),
				FText::AsNumber(WagerAmount));
			ShowLine(Line);
			return;
		}
	}

	// The bookie picks the car, not the punter. One menu row instead of a
	// second nested list, and it keeps the bet genuinely uncertain - a player
	// choosing for themselves would just learn which slot is fastest and take
	// it every time, which is not a bet, it is a withdrawal.
	const int32 Field = FMath::Max(1, LinkedTrack->RacerCount);
	BackedRacerIndex = FMath::RandRange(0, Field - 1);
	Bettor = Interactor;
	bAwaitingResult = true;

	LinkedTrack->StartRace();

	if (WagerAmount > 0)
	{
		Line.Text = FText::Format(NSLOCTEXT("DCC", "BookieBetPlaced",
			"{0} gold on {1}. No, you don't get to pick. That's what makes it gambling."),
			FText::AsNumber(WagerAmount), LinkedTrack->GetRacerName(BackedRacerIndex));
	}
	else
	{
		Line.Text = FText::Format(NSLOCTEXT("DCC", "BookieFreeBet",
			"House is backing {0} on your behalf. Enjoy the show."),
			LinkedTrack->GetRacerName(BackedRacerIndex));
	}
	ShowLine(Line);
}

void ADCCDialogueNPC_RaceBookie::SettleBet()
{
	bAwaitingResult = false;

	if (!LinkedTrack)
	{
		return;
	}

	ADCCCharacterBase* Punter = Bettor;
	const int32 Backed = BackedRacerIndex;
	BackedRacerIndex = INDEX_NONE;
	Bettor = nullptr;

	if (!IsValid(Punter) || Backed == INDEX_NONE)
	{
		return;
	}

	FDCCDialogueLine Line;
	Line.SpeakerId = SpeakerId;

	if (LinkedTrack->WinnerIndex == Backed)
	{
		const int32 Payout = FMath::Max(1, FMath::RoundToInt(WagerAmount * WinMultiplier));
		if (UDCCStatsComponent* Stats = Punter->Stats)
		{
			Stats->AddGold(Payout);
		}

		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "BookiePayout", "{0} came in. {1} gold, paid out under protest."),
			LinkedTrack->GetRacerName(Backed), FText::AsNumber(Payout)));

		Line.Text = NSLOCTEXT("DCC", "BookiePayoutLine", "Don't gloat. Go again?");
	}
	else
	{
		Line.Text = FText::Format(NSLOCTEXT("DCC", "BookieLostLine",
			"{0}. Unlucky. Double or nothing?"),
			LinkedTrack->GetRacerName(LinkedTrack->WinnerIndex));
	}

	ShowLine(Line);
}

void ADCCDialogueNPC_RaceBookie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Polled rather than delegate-driven for the same reason ADCCRaceTrack
	// polls lap counts: one bool compare on an actor that is already ticking
	// is cheaper than the binding it would replace, and it cannot go stale if
	// the track is swapped at runtime.
	if (HasAuthority() && bAwaitingResult && LinkedTrack
		&& LinkedTrack->State == EDCCRaceState::Finished)
	{
		SettleBet();
	}
}

void ADCCDialogueNPC_RaceBookie::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	// Same deliberate non-Super as ADCCDialogueNPC_LiftAttendant - see its
	// comment. A player's E press reaches the NPC menu now, not this; kept for
	// anything still calling TryPlayerInteract programmatically.
	PlaceBetAndStartRace(Interactor);
}
