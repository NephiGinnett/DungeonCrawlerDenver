# Crafting benches

Written 2026-11-26 for the next session. Zoe asked for crafting in the shape of Dungeon Crawler Carl's professions (Alchemy, Mechanics, and a few more), and said the game-specific prize menus could become workbenches. This is the brief. It is not built yet.

Do not copy the book's recipes, skill names, or item names. The tone is Denver and the company: a still in a break room, a bay at the yard, a kitchen that used to feed a shift.

## Rules

- No new player key. A bench is a pad you stand on and press E, the same way the Prize Counter and the Chop Shop already open. The belt stays the belt.
- The host rolls and pays. A craft RPC gets `DCCIntegrity::Allow`. A client does not invent the result.
- A crafted thing is never worth more tickets than it cost to make, if it can be sold at all. Most results are gear or a tonic you use, not a ticket printer.
- Salvage is the material. It already drops in tower defense (`Siege.Salvage` on the profile) and currently buys nothing. Scrap stays the truck currency. Tickets stay the shop currency. Do not fold them into one pile.
- Recipes are a static table in a new `Arcade/DCCCrafting.cpp`, the same shape as `DCCBounties`. One row is an id, a profession, a name, a line of description, the salvage (and anything else) it costs, and the existing item id it puts in the bag. No new meshes for the first pass.

## What becomes a bench, and what stays a shop

The Prize Counter stays a shop. It is the ticket sink (hats, armour, guns, the summon tubes are the other sink). Turning it into a bench would remove the place tickets go.

| Menu today | Becomes | Pays with |
|---|---|---|
| Chop Shop tab | Mechanics bench | Scrap for the truck, as now. Salvage for the new recipes. |
| Mine Outfitters | Stays a shop | Credits. It is kit for one mine, not a profession. |
| Trading Post | Stays a shop | Tickets, for this climb only. |
| Prize Counter | Stays a shop | Tickets. |
| New Alchemy tab, beside Chop Shop | Alchemy bench | Salvage, and a bottle if the bag has one. |
| New Kitchen tab, later | Cook bench | Food already in the bag. Not the first pass. |

The first build does not need a new mesh or a new `UCLASS`. Add the Alchemy tab to `UDCCCosmeticShopWidget` and teach the Chop Shop tab the salvage recipes. A workbench mesh in the Safe Room can come after, and it only opens that same screen.

## Professions

### Mechanics

The yard and the annex. You already upgrade the truck here. Salvage recipes sit under those upgrades.

First recipes, all existing gear:

- **Shop Nail.** 6 salvage. Puts `NailGunPro` in the bag if it is not already there. The cordless drill the renovation left behind.
- **Patch Kit.** 3 salvage. A one-shot heal tonic, whichever consumable the kitchen already uses for a small heal. If no tonic class is an easy grant, skip this row rather than inventing a potion class.
- **Brace.** 8 salvage. Sets a profile flag `Craft.Brace`. The next tower-defense round starts with the side door's first lockdown 5 seconds shorter. One flag, read where `DriveLockdown` sets `LockdownUntil`. Do not build a tower editor.

### Alchemy

The break-room still. Inputs are salvage. Outputs are tonics you drink off the belt's food slot, not new keys.

First recipes:

- **House Tonic.** 4 salvage. A heal, stronger than Patch Kit, same drink path as the bar.
- **Shift Coffee.** 2 salvage. A short stamina drink. Use an existing Fourteener boost item id if one is a drink. If none is, this row waits.
- **Blue Lamp.** 5 salvage. A light tonic, or the existing `Torch` in the bag if a drink does not exist yet. Light is the thing Dead Air is about. Do not add a new VFX.

Alchemy does not teach spells. The Ring already teaches four spells. A still that grants `FireFingers` would step on that.

### Later, not this pass

- **Cook.** The cantina kitchen. Combine two foods into the better one (the burrito, the jerky). Same belt slot.
- **Tailor.** Looks. Easy to confuse with the Prize Counter. Leave it until Alchemy and Mechanics have been used in a session.
- **Scrivener.** Complaint forms into the folder, or a one-use contract. Only if Gate Run wants it.

## What not to do

- Do not add a profession level, a rank, and a minigame of its own in the first pass. The bench is a list. You can make it or you cannot.
- Do not spend tickets on the bench. Tickets have shops. Salvage did not.
- Do not move Cillinan, and do not guess the giraffe.
- Do not commit unless Zoe asks.

## Done when

One session can open the Chop Shop, see Shop Nail, spend salvage from a tower-defense round, and find the nail gun in the bag. Alchemy's House Tonic does the same with a drink. `Profile_Zoe.sav` is restored if the test spends her salvage.
