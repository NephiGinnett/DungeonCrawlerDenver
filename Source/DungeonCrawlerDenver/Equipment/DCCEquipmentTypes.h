#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Stats/DCCAbilityTypes.h"
#include "Stats/DCCDamageTypes.h"
#include "Items/DCCLootBoxTypes.h"
#include "DCCEquipmentTypes.generated.h"

class UStaticMesh;
class ADCCPickup;
class UAnimationAsset;

// ---------------------------------------------------------------------------
// EQUIPMENT
//
// Data, not code, for the same reason skills are: the tabletop game invents loot
// at the table, and a new sword should cost a DataTable row rather than a
// recompile.
//
// Everything here is RIGID attachment - a mesh snapped to a socket. There is no
// body-conforming armor and there cannot be one while every crawler carries its
// own Meshy-generated skeleton: a chestplate skinned to CJ's rig has no
// correspondence to Marcel's, so there is nothing to deform through. Weapons,
// helmets, shoulders and packs all work; a fitted breastplate does not. If the
// project ever standardises on one skeleton, modular armor and cross-character
// animation retargeting both become possible in the same pass - which is the
// real argument for doing it.
//
// Until then, "armor" that must change the body's silhouette is a material
// change on the existing mesh, not new geometry.
// ---------------------------------------------------------------------------

/**
 * Where a piece of equipment goes. One item per slot.
 *
 * Deliberately short. Every extra slot is another thing a floor's loot has to
 * fill before the party feels dressed, and DCC's interest is in what an item
 * does rather than how many straps it has.
 */
UENUM(BlueprintType)
enum class EDCCEquipmentSlot : uint8
{
	MainHand UMETA(DisplayName = "Main Hand"),
	OffHand  UMETA(DisplayName = "Off Hand"),
	Head     UMETA(DisplayName = "Head"),
	Body     UMETA(DisplayName = "Body"),
	Trinket  UMETA(DisplayName = "Trinket"),

	MAX      UMETA(Hidden)
};

/**
 * One item, as a DataTable row.
 *
 * Ability bonuses are five flat fields rather than a map or a nested struct
 * because DataTable imports only accept CSV here - the MCP importer routes
 * through CSVImportFactory - and CSV cannot express a nested container without
 * hand-written UE struct literals. Five columns is uglier in the header and far
 * better at the authoring end.
 */
USTRUCT(BlueprintType)
struct FDCCEquipmentDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable identifier. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	EDCCEquipmentSlot Slot = EDCCEquipmentSlot::MainHand;

	/** Flavour, and what the AI narrator reads out when it is picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FText Description;

	// -- Enhanced-layer contributions ---------------------------------------
	// Added on top of the Unenhanced scores. Negative is legal and encouraged:
	// heavy armour that costs Dexterity is a more interesting item than one
	// that is simply better.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Bonuses")
	int32 BonusStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Bonuses")
	int32 BonusIntelligence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Bonuses")
	int32 BonusConstitution = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Bonuses")
	int32 BonusDexterity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Bonuses")
	int32 BonusCharisma = 0;

	/**
	 * Replaces the wielder's damage die while held. Zero leaves it alone, which
	 * is what everything that is not a weapon wants.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Combat", meta = (ClampMin = "0"))
	int32 WeaponDamageDie = 0;

	/** What type WeaponDamageDie deals. Ignored when WeaponDamageDie is 0 -
	 *  nothing to type on a non-weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Combat")
	EDCCDamageType WeaponDamageType = EDCCDamageType::Physical;

	/** Flat addition to Evade, DCC's name for the value an attack must beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Combat")
	int32 EvadeBonus = 0;

	/**
	 * The same six-tier ladder loot boxes use (Bronze..Celestial), reused here
	 * rather than a parallel "item rarity" enum - a box's tier and an item's
	 * rarity are the same concept looked at from two directions. None (the
	 * default) means "not a rarity-tiered item" - every hand-authored weapon
	 * on this table today (the frying pan, the fire axe, the found guns) is
	 * left at None and keeps exactly the damage its own WeaponDamageDie
	 * already says, unaffected by this field. Only items explicitly tagged a
	 * tier - loot-box rewards, going forward - get the bonus below. See
	 * UDCCEquipmentComponent::GetWeaponDamageDie for where it's added in.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Combat")
	EDCCLootBoxTier Rarity = EDCCLootBoxTier::None;

	/**
	 * Replaces the wielder's attack reach while held. Zero leaves it alone.
	 *
	 * This is the whole of "ranged weapon" as far as the rules are concerned:
	 * everything else about an attack - the die, the ability, the cooldown -
	 * already works at any distance, and the only thing stopping a crossbow from
	 * being a crossbow was that AttackRange was a fixed 200 on the character.
	 *
	 * No projectile is simulated. The shot resolves instantly like every other
	 * attack, which matches how the ranged *skills* already behave and avoids
	 * inventing a second, parallel combat path for one item type.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Combat", meta = (ClampMin = "0.0"))
	float AttackRangeOverride = 0.0f;

	/**
	 * How heavy this item is to wear/wield, in kilograms - feeds
	 * UDCCEquipmentComponent::GetTotalEquippedMass, which UDCCRuleset's stamina
	 * regen formula reads once the total crosses GetEncumbranceThresholdKg.
	 * Zero (the default) means "weightless" - a trinket or a light garment
	 * should not need every row touched just to opt out of encumbrance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (ClampMin = "0.0"))
	float MassKilograms = 0.0f;

	/** Normal (the default) means this item grants no resistance at all -
	 *  most items leave this alone. See EDCCResistanceLevel for how Resistant/
	 *  Vulnerable from different sources interact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Combat")
	EDCCDamageType ResistanceDamageType = EDCCDamageType::Physical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Combat")
	EDCCResistanceLevel ResistanceLevel = EDCCResistanceLevel::Normal;

	/**
	 * Free-form classification - Liquid, Alcohol, Flammable, Organic,
	 * Craftable, whatever fits (per Zoe's own steer that every item type
	 * should carry a wealth of these). Same idea as FDCCConsumableDef::Tags -
	 * not read by any code yet, so an empty container here is not a bug, just
	 * nothing downstream asking yet.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FGameplayTagContainer Tags;

	// -- Presentation -------------------------------------------------------

	/**
	 * Soft so a floor's worth of loot definitions does not drag every mesh into
	 * memory at load; resolved only when something actually equips it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/**
	 * Socket on the character's skeletal mesh to snap to.
	 *
	 * The socket NAME is the contract and the bone it hangs from is per-skeleton
	 * detail - which is what lets one item definition work across five
	 * differently-rigged crawlers. Each skeleton needs a socket of this name;
	 * what it is parented to is that skeleton's business.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Visual")
	FName AttachSocket = TEXT("hand_rSocket");

	/**
	 * Per-item nudge applied on top of the socket snap, in the socket's own
	 * local space - what actually fixes "the hand grips the pan by the plate,
	 * not the handle." The socket's transform is identical for every item
	 * that attaches to it, but different meshes were modelled with their
	 * pivot in different places (a pan's pivot sits near the plate; a knife's
	 * sits at the blade tip; a sword's is often already at the grip) - one
	 * socket-wide offset cannot fix all of them at once, so each item needs
	 * its own. Zero for both fields (the default) reproduces the exact old
	 * behaviour - snapped flush to the socket, no nudge - so every item that
	 * hasn't been tuned yet is unaffected by this existing at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Visual")
	FVector GripOffsetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Visual")
	FRotator GripOffsetRotation = FRotator::ZeroRotator;

	/** Worth this much if sold. Also what the corpse drops it as. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (ClampMin = "0"))
	int32 GoldValue = 0;

	// -- Light -----------------------------------------------------------------
	// A torch's entire reason to exist. Kept on the item definition rather than
	// as a property of the mesh, because the mesh is cosmetic-only elsewhere in
	// this system and a light needs gameplay-relevant tuning (colour, range) an
	// artist's static mesh import has no business carrying.

	/** A point light attaches at AttachSocket alongside the mesh when true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Light")
	bool bEmitsLight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Light", meta = (EditCondition = "bEmitsLight"))
	FLinearColor LightColor = FLinearColor(1.0f, 0.65f, 0.30f);

	/** Candelas, matching the project's other placed lights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Light", meta = (EditCondition = "bEmitsLight", ClampMin = "0.0"))
	float LightIntensity = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Light", meta = (EditCondition = "bEmitsLight", ClampMin = "0.0"))
	float LightAttenuationRadius = 700.0f;

	// -- Container ---------------------------------------------------------
	// A canteen or a pouch does not do anything to the wearer's numbers - it
	// makes a slot able to swallow *other* pickups instead. Kept as flat
	// fields on the same row rather than a separate table for the same
	// reason ability bonuses are: one row per item stays authorable from a
	// single MCP set_rows call.

	/** CJ's water bottle is the first item with this on. Everything else defaults it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Container")
	bool bIsContainer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Container", meta = (EditCondition = "bIsContainer", ClampMin = "0"))
	int32 ContainerSlots = 0;

	/**
	 * A candidate pickup qualifies by fitting under this mass - reusing
	 * ADCCPickup::MassKilograms as "size" rather than inventing a parallel
	 * stat, since mass is already this project's size signal (see the
	 * Pantry's mass tiers). Ignored for anything ItemTags marks as Liquid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Container", meta = (EditCondition = "bIsContainer", ClampMin = "0.0"))
	float ContainerMaxItemMassKg = 0.0f;

	/** A liquid-tagged pickup bypasses ContainerMaxItemMassKg entirely when true - the whole point of a bottle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Container", meta = (EditCondition = "bIsContainer"))
	bool bContainerAcceptsLiquids = false;

	// -- Curse -----------------------------------------------------------------
	// A curse is a forced side effect the wearer does not get to opt out of -
	// distinct in kind from the passive Bonus*/Resistance* fields above, which
	// the wearer barely notices. First use: Supercan's costume. Built as a
	// reusable shape (any item can flip bIsCursed on) rather than a special
	// case, since more of these are wanted later.

	/**
	 * Once equipped, this item cannot be voluntarily unequipped - see
	 * UDCCEquipmentComponent::Unequip/Equip, both of which refuse to remove a
	 * cursed item from its slot. There is no removal mechanism anywhere in this
	 * project yet, so equipping one is currently a one-way commitment for the
	 * rest of the run. Worth building "ask for it back" or a cleansing service
	 * before this reaches real players in numbers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Curse")
	bool bIsCursed = false;

	/** Whether wearing this forces an action the instant the wearer's room's encounter goes Active. Ignored unless bIsCursed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Curse", meta = (EditCondition = "bIsCursed"))
	bool bForcesActionOnCombatStart = false;

	/**
	 * Forward launch speed (cm/s, additive to existing velocity - same
	 * LaunchCharacter convention this project's own hit-stagger already uses)
	 * for the forced hop. No grid/space unit exists anywhere in this project
	 * to derive an exact "3 spaces" from, so this is a tuned approximation,
	 * not a derived value - adjust freely per item.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Curse", meta = (EditCondition = "bForcesActionOnCombatStart", ClampMin = "0.0"))
	float ForcedHopForwardSpeed = 900.0f;

	/** Upward component of the launch, so the hop reads as a bounce rather than a shove. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Curse", meta = (EditCondition = "bForcesActionOnCombatStart", ClampMin = "0.0"))
	float ForcedHopUpwardSpeed = 350.0f;

	/** Optional gesture clip played alongside the hop - the "taunt" half. Soft, same reasoning as Mesh: unauthored curses pay no load cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Curse", meta = (EditCondition = "bForcesActionOnCombatStart"))
	TSoftObjectPtr<UAnimationAsset> ForcedActionAnimation;

	/** Flavor line logged to the feed when the curse fires, e.g. "does a forced happy little bounce and a taunt." Falls back to a generic line when blank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Curse", meta = (EditCondition = "bForcesActionOnCombatStart"))
	FText ForcedActionFlavorText;
};

/**
 * A pickup's identity as remembered inside a container slot rather than as a
 * live actor in the world.
 *
 * Stored rather than keeping the actor itself, because the whole point of
 * putting something in a bottle is that the world actor goes away - the
 * same "pick it up and it stops existing as a prop" shape ADCCPickup's own
 * gold and equipment paths already use.
 */
USTRUCT(BlueprintType)
struct FDCCContainedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<ADCCPickup> PickupClass;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FName EquipmentItemId;

	/**
	 * Preserved from the original pickup so a round trip through a container is
	 * lossless. Without these, an item that fit a container because of its mass
	 * or its Liquid tag could come back out at the class default 25kg with no
	 * tags at all - CanStoreInContainer's own size/liquid check reads exactly
	 * these two fields, so a lossy retrieval could pass or fail differently than
	 * the original storage did.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	float MassKilograms = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<FName> ItemTags;
};

/** One equipped item. Replicated as an array because TMap does not replicate. */
USTRUCT(BlueprintType)
struct FDCCEquippedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	EDCCEquipmentSlot Slot = EDCCEquipmentSlot::MainHand;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FName ItemId;

	/** Only ever non-empty when the equipped item's row has bIsContainer set. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<FDCCContainedItem> Contents;
};
