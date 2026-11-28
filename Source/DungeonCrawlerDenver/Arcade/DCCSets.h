#pragma once

#include "CoreMinimal.h"

class UWorld;
class ULevelStreamingDynamic;

// Sets: a Fab pack's finished sample map, streamed into a game as one ready-made room (2026-11-25).
//
// Most place packs ship a composed level (the Old Mine, the Power Plant, the Broadcast Studio...). An ordinary
// (non World Partition) level can be loaded at runtime at any spot with LoadLevelInstance, so a game gets the whole
// dressed room for one call and builds its own marks around it. The list of sets and what each is good for is in
// Docs/kits.md; the raw mesh catalogue is SourceArt/Kits/<Pack>.json (Tools/kit_catalogue.py).
//
// Every machine loads its own copy (call it from BeginPlay or the arena's local build, not server-only). Use the same
// InstanceName on every machine so the level's actors line up across the network. The pack's own actors are not
// replicated; keep gameplay actors (marks, enemies) in our code, never in the set.
namespace DCCSets
{
	struct FOptions
	{
		// Keep the map's own directional light (its sun). Off by default: the arena lights the world.
		bool bKeepSun = false;
		// Keep its sky, sky light, fog, clouds and post process volumes. Off by default, same reason.
		bool bKeepSky = false;
		// Keep its point, spot and rect lights. On by default: they are the room's look.
		bool bKeepLocalLights = true;
		// Remove any actor with a static mesh whose name starts with one of these.
		TArray<FString> RemoveMeshes;
		// Make the set's doors work: they open by themselves when anyone comes near (UDCCSetDoorComponent). On by
		// default, so every set's doors behave like doors without anyone listing them.
		bool bWorkingDoors = true;
	};

	// A short name ("OldMine") to the map's package path, or the input as is if it already is a path.
	FString Resolve(const FString& NameOrPath);

	// Every alias Resolve knows, for the dev command's help.
	TArray<FString> Aliases();

	// Streams the map in at At/Rot. OnReady runs on this machine once it is visible, with the bounds of its geometry
	// (world space, after the move). Returns null if the map does not exist.
	ULevelStreamingDynamic* Load(UWorld* World, const FString& NameOrPath, const FVector& At, const FRotator& Rot,
		const FString& InstanceName, const FOptions& Options, TFunction<void(const FBox&)> OnReady = nullptr);

	// Unloads a set loaded with Load.
	void Unload(ULevelStreamingDynamic* Set);

	// Lock or unlock every working door whose shut middle sits within Radius (cm, flat) of At.
	// Does not change how a door opens. Returns how many it touched.
	int32 LockDoorsNear(UWorld* World, const FVector& At, float Radius, bool bLock);
}
