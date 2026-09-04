# Towns — settlements between the set pieces

**Started 2026-09-02** at Zoe's ask for "a few different towns complete with basic shops and
farms, and connect them all," with quest-bearing NPCs.

Living document. One town exists; this is the pattern the rest should follow.

---

## Why towns at all

Floor 1 is a sequence of set pieces — cantina, pool, pantry, mine — with nothing between them.
Towns are the connective tissue, and they do three jobs no room does:

1. **Somewhere to spend money.** Gold currently accumulates with almost nowhere to go.
2. **A second, independent voice on the same events.** The most valuable thing a town gives
   the story is *corroboration from a different angle* — see Marguerite below.
3. **A reason the world reads as inhabited** rather than as a series of arenas with monsters
   parked in them.

---

## Tailings (Town 01) — the settlement below the workings

**Where:** centred **(26000, 16000)**, outliner folder `DCC/Town01_Tailings`.
**Built 2026-09-02.** Blockout, saved, not yet playtested.

People who live off the Front Range mine without going into it. The name is what a mining
town would actually be called by the people upstream of it, and nobody in it chose the name.

| Part | Built from | Notes |
|---|---|---|
| Six houses | `Viking_village/Packed_level/L_house_01..06` | **Packed levels — one actor per house.** Enormously cheaper than assembling buildings from modules; this is the way to build any future town. |
| Market row (the shops) | `Thai_Market` market covers, tables, fruit, signs | Four stalls down the town's spine |
| Farm | `Thai_Market` fences + `Viking_village` grass/flowers/plants, 26 clumps, barrels | Fenced plot to the west |
| Light + nav | 5 warm point lights, one `NavMeshBoundsVolume` | |

### The people

| NPC | Class | Role |
|---|---|---|
| **Quartermaster Odie** | `ADCCVendorNPC` | Sells salvage — Torch, Broken Chair Leg, Corporate Letter Opener, Grease Apron, Patio Sun Hat, plus water/ale/tonic. *"I sell what people leave behind, which since the Collapse is most things."* |
| **Sable, who farms** | `ADCCDialogueNPC` | Offers `TailingsHarvest`. Her field grows and she has decided not to interrogate why. |
| **Marguerite, who remembers** | `ADCCDialogueNPC` | Offers `TailingsLights`. Sixty-one years at the foot of the mountain. |

### Marguerite is the point of this town

She has been counting the drone lights for nine weeks — **"Two on a slow night. Eleven on the
worst one"** — and she is sharper about it than the Surveyor is:

> *"The surveyor down at the cantina thinks it is claim-jumpers. Bless him. Claim-jumpers do not
> run in a grid, dear, and they do not stop at the same hour every night as though something
> told them to."*

She has correctly identified automated survey patrols. She has no word for a drone, so she
cannot say it. **The party hears about the lights twice, from two towns and two unrelated
people, before they ever see one** — which is what turns the drones from a monster into a fact
about the world.

Her quest shares the `MineShaftHead` marker with *The Withdrawal*, so it completes on the way to
the mine rather than needing its own trip. She never learns what they were. That is deliberate.

---

## The pattern for the next town

1. **Trace-verify the ground first.** `find_actors(bounds)` is not trustworthy for this — see
   CLAUDE.md. Tailings was built once at (18500, 9500), which the bounds check called empty and
   which actually contained 5710-unit skyscrapers, and had to be moved wholesale.
2. **Packed levels first, modules second.** Six houses cost six actors.
3. **One vendor, one quest-giver, one witness.** The witness is the part that matters: somebody
   who saw something the party will later confirm from another direction.
4. Folder everything under `DCC/TownNN_Name/{Ground,Houses,Market,Farm,NPCs}` so it can be
   relocated as a unit — which, on current form, it will need to be.

## Not done

- **Nothing connects the towns to anything.** Tailings, like the mine, is reachable only by
  flying there. Roads or Oathgates are the open question.
- Only one town exists; the ask was for a few.
- No idle animations on the NPCs (T-pose), same as every other dialogue NPC on the floor.
- `TailingsHarvest` uses a bare `Collect` objective with no `TargetId`, so **any** four pickups
  satisfy it. Wiring it to real produce props needs pickups placed in the field.
