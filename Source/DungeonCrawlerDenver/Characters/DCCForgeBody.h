#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;
class USkeletalMesh;

/**
 * Dresses an enemy as an NPC Forge person (2026-11-15, Zoe: "have enemies just use the models you made and not our
 * crawlers"). Enemies that used to wear a crawler's body (the Interim Manager wore CJ; the Nuclear Family wore three
 * others) now wear one of the Tools/npc_forge.py people, with the Forge's own clips: idle, walk, jog, a punch and a
 * flinch. Every Forge body shares one skeleton (NPC_casual_300's), so any clip plays on any of them.
 *
 * Call it from a constructor: it uses ConstructorHelpers, which also makes the cooker package the body and clips.
 */
namespace DCCForgeBody
{
	/** Forge ids: casual_300..309, office_302..311, worker_301..310 (the folders under /Game/DCC/Characters/NPCs). */
	void Apply(ADCCCharacterBase& Character, USkeletalMesh* Body, float Scale = 1.0f);

	/** Every Forge body, found once. Index wraps. Constructor only. */
	USkeletalMesh* Body(int32 Index);
}
