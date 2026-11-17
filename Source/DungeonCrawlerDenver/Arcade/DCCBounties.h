#pragma once

#include "CoreMinimal.h"

class APlayerState;

/**
 * Bounties (2026-11-16, Zoe: "quest tie ins"): standing goals that send you from one game into another, each paid once,
 * in tickets, from the BOUNTIES tab at the Prize Counter. Most are read straight off the profile (best height, best
 * views, the trophy vault); the rest are flags a game sets when the moment happens (Flag).
 *
 * Done and claimed both live in the profile, so a bounty finished at a friend's party is still yours at home.
 */
namespace DCCBounties
{
	struct FBounty
	{
		FName Id;
		const TCHAR* Name;
		const TCHAR* Blurb;
		const TCHAR* Where;     // which game, for the tile caption
		FLinearColor Colour;
		int32 Reward;           // tickets
	};

	const TArray<FBounty>& All();
	const FBounty* Find(FName Id);

	bool IsDone(const FString& Key, FName Id);
	bool IsPaid(const FString& Key, FName Id);

	/** Server: a game saw it happen (a tag on the cage, the shift boss going down). */
	void Flag(const APlayerState* PlayerState, FName Id);

	/** Server: pay a finished bounty once. False if it is not done or already paid. */
	bool Claim(APlayerState* PlayerState, FName Id);
}
