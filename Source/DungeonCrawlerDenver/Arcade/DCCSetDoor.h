#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCCSetDoor.generated.h"

class UStaticMeshComponent;
class USoundBase;

// A working door inside a streamed set (Arcade/DCCSets.h), 2026-11-25.
//
// Fab sample maps place their doors as plain, closed static meshes. DCCSets::Load finds them (FOptions::bWorkingDoors)
// and hangs one of these on each. The door opens by itself when any character (player, ally, enemy) comes near and
// closes again a moment after the doorway is clear, so no new key is needed and marchers can still walk a route.
//   * A narrow leaf (up to 1.8 m) swings 100 degrees on its hinge edge, away from whoever opened it.
//   * A wide door (a double door cast as one mesh, a sci-fi slab) slides sideways along the wall.
// It runs on every machine from replicated character positions, so everyone sees the same door without replicating it.
UCLASS(ClassGroup = (DCC))
class DUNGEONCRAWLERDENVER_API UDCCSetDoorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCSetDoorComponent();

	// Takes over this mesh as the door leaf. Its current place is "closed".
	void Setup(UStaticMeshComponent* Leaf);

	// Keep it shut (and stop it opening) or let it work again. For games that lock a door (a puzzle, a lockdown).
	void SetLocked(bool bLock) { bLocked = bLock; }
	bool IsLocked() const { return bLocked; }

	// World position of the leaf's middle when it is shut. LockDoorsNear aims at this.
	FVector ClosedMiddle() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Is this mesh a door leaf (not a frame, a wall with a hole, a handle or a pane)? Name and size both count.
	static bool LooksLikeDoor(const UStaticMeshComponent* Mesh);

private:
	void Apply();
	void PlayMoveSound();

	TWeakObjectPtr<UStaticMeshComponent> LeafMesh;
	FTransform Closed;
	FVector Hinge = FVector::ZeroVector;   // world, swing doors: the vertical edge it turns on
	FVector Along = FVector::ForwardVector; // world, the door's width direction
	FVector Normal = FVector::RightVector;  // world, out of the door's face
	float Width = 100.0f;
	float Thickness = 10.0f;
	bool bSlide = false;
	bool bLocked = false;

	float Open = 0.0f;       // 0 shut, 1 fully open
	float Target = 0.0f;
	float Side = 1.0f;       // swing away from the opener: +1 or -1
	float NextLook = 0.0f;
	float ClearFor = 0.0f;
	float LastSound = -10.0f;

	// Swing leaves use the heavier door, sliding leaves the whoosh. The packs have those two and no creak.
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SwingSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SlideSound;
};
