#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * The gear bench's numbers: what the bench (F3) saves and the game applies.
 * Modelled on The Familiar's pose bench, which writes placement.json for the
 * game to read the next time a pose plays.
 *
 * Three kinds of nudge, all in one file (Content/DCC/Bench/DCCBench.json):
 *
 *   grips    How an item sits in its socket. Keyed by item id (every body), or
 *            "<mesh path>|<item id>" for one body only, which wins over the
 *            every-body value. Replaces the DataTable's GripOffset when present,
 *            and adds a scale the table never had.
 *   sockets  Where a socket sits on one body ("hand_rSocket" on CJ's mesh).
 *            Moves everything held in that socket, on that body.
 *   bones    An extra rotation, offset and scale on one bone of one body,
 *            layered on top of whatever animation is playing.
 *
 * Bones are applied in USkeletalMeshComponent's "bone transforms finalized"
 * callback: after the pose is evaluated, before attached gear is moved to its
 * sockets and before the frame is drawn. So a nudged hand carries its sword.
 *
 * Editor builds read and write the Content file (it ships with the game: see
 * DirectoriesToAlwaysStageAsUFS in DefaultGame.ini). A packaged build cannot
 * write into its own pak, so it saves to Saved/DCCBench.json and reads that
 * first; copy it over the Content one to keep it.
 */
namespace DCCBench
{
	struct FNudge
	{
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector Scale = FVector::OneVector;

		FTransform ToTransform() const { return FTransform(Rotation, Location, Scale); }
		bool IsIdentity() const
		{
			return Location.IsNearlyZero(0.01) && Rotation.IsNearlyZero(0.01f) && Scale.Equals(FVector::OneVector, 0.001);
		}
	};

	/** Reads the file once. Every accessor below calls it. */
	void EnsureLoaded();
	bool Save(FString& OutWhere);

	/**
	 * Writes ONE entry to the file and leaves every other entry in the file as it
	 * was on disk (2026-11-13: Zoe wanted Save to keep only what she is working on,
	 * not every nudge she tried in the session). Removes the entry from the file
	 * if it has been reset. Kind is "grips", "sockets" or "bones"; MeshPath is
	 * empty for an every-body grip.
	 */
	bool SaveEntry(const TCHAR* Kind, const FString& MeshPath, FName Name, FString& OutWhere);
	FString GetSavePath();

	// --- grips --------------------------------------------------------------
	/** Body-specific first, then every-body. False if neither is set. */
	bool FindGrip(const FString& MeshPath, FName ItemId, FNudge& Out);
	void SetGrip(const FString& MeshPathOrEmpty, FName ItemId, const FNudge& Grip);
	void ClearGrip(const FString& MeshPathOrEmpty, FName ItemId);

	// --- sockets ------------------------------------------------------------
	/** The socket's transform as authored, before any bench override. */
	bool GetOriginalSocket(USkeletalMesh* Mesh, FName Socket, FNudge& Out);
	bool FindSocket(const FString& MeshPath, FName Socket, FNudge& Out);
	void SetSocket(USkeletalMesh* Mesh, FName Socket, const FNudge& Value);
	void ClearSocket(USkeletalMesh* Mesh, FName Socket);
	/** Pushes saved socket overrides onto a mesh asset. Called on load and when a body is swapped in. */
	void ApplySockets(USkeletalMesh* Mesh);

	// --- bones --------------------------------------------------------------
	bool FindBone(const FString& MeshPath, FName Bone, FNudge& Out);
	void SetBone(const FString& MeshPath, FName Bone, const FNudge& Value);
	void ClearBone(const FString& MeshPath, FName Bone);

	/** Hooks a character's mesh so its bone nudges apply every frame. Safe to call more than once. */
	void BindBoneNudges(USkeletalMeshComponent* Mesh);
}
