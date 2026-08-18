# Floor 1 — Under New Management

**The AI's name for it:** *"Floor One: A Celebration of Local Cuisine."*

Per the tone bible's naming convention the official name always undersells the
danger. Everything below is what it actually is.

---

## The theme, and why it holds together

The tone bible already states the dungeon's premise: *"the dungeon didn't build
these places. It **repossessed** them, then made them legally distinct enough to
avoid a lawsuit."*

**Lord Voldemortgage is that premise with a face on it.** He is not a boss bolted
onto a floor theme — he is the character the conceit was already implying and never
named. Once he exists, three things that were previously unconnected become one
idea:

- **Why the dungeon owns a Denver landmark.** He foreclosed on it.
- **Why the staff are still working.** They are paying off debt. They cannot leave.
- **Why the enemies are employees rather than monsters.** Because they are.

That gives Floor 1 a single sentence: **a beloved local restaurant has been
repossessed, and the collections department is still on site.**

### The floor is the Cliff Diver Cantina

A cavernous fake-Mexican restaurant with a waterfall and performers who dive,
nightly, forever. Chosen because its real-world referent is a **cave-themed
restaurant**, which means the cave assets are the theme rather than a substitute
for it:

| The floor needs | Installed pack |
|---|---|
| A cave the size of a building | `Cave_Camp`, `Alien_Cave_biome`, `Desert_cave` |
| A restaurant inside it | `Diner` — bar, bar top, kitchens, fryer, hood, chairs, signage |
| A fake-village dining tier | `Thai_Market` — beams, benches, stalls, awnings |
| A waterfall | `Abandonned_Brutalist/M_particle_waterfall` |
| Industrial kitchens, cold storage | `Ice_Station` |
| Repossession paperwork, debt haze | `Variant_Combat/NS_Damage`, `Legendary_Forge/NS_sparks` |

Nothing on that list is a compromise, and Room 01's existing campfire and bedroll —
odd as a squatter camp — are exactly right as **the staff break area behind the
falls**, unchanged.

### The ambient texture: everyone is furious about him

This is the part that makes the theme land rather than just exist. **Every
non-hostile NPC on the floor complains about Voldemortgage**, and none of them
explain who he is. The party assembles him from grievances before they ever meet
him.

The complaints escalate room by room — irritation, then fear, then resignation —
so the party's sense of him sharpens without a single exposition dump. Lines are in
`Docs/DT_DCCDialogue.json`.

---

## The map

World coordinates, based on what is already placed. **Every new bounding box below
must be checked with `find_actors` before anything is built in it** — Room 02 was
originally constructed inside cave rock because that step was skipped.

```
                                                    N (+Y)
                                                      ^
                                                      |
   [6] PANTRY                    [5] BAR              |
   x  8600..9800                 x  8600..9800        |
   y  5800..6600                 y  4700..5300        |
        |                             |               |
        +------------- corridor ------+               |
                                      |               |
   [4] KITCHENS  ------- corridor ----+   [7] THE DIVING POOL
   x  6800..8400                          x 10400..13000
   y  4700..5300                          y  4400..6400
   (Room 02, EXISTS)                      (boss arena)
        |
        | ramp down / south
        |
   [3] THE FALLS  (east wall of Room 01, climb)
   x  6800..6850, z 0..400
   ledge x 6600..6850, z 360..400   (EXISTS)
        |
   [2] THE GROTTO                          [1] THE QUEUE
   x  5200..6800  <--- west doorway ---    x  0..4600
   y  -800..800                            y  -400..400
   z  0..400   (Room 01, EXISTS)           z  210 descending to 0
```

**Connections, in play order:**

1. **Queue → Grotto.** East along y≈0, descending from the spawn platform (z≈210)
   to the cave floor (z=0), entering Room 01's west doorway at x≈5175, y −350..350.
2. **Grotto → Falls.** The east wall. There is no door; climbing is the only way on.
3. **Falls → Kitchens.** From the ledge (x 6600..6850, z 400) north along a service
   catwalk to y≈4700, arriving at Room 02's west doorway (x=6800).
4. **Kitchens → Bar → Pantry.** Short corridors east and north.
5. **Pantry → Diving Pool.** East, opening onto the arena.

The Approach walkway from the earlier terrain plan is what builds connection 1, and
`SciFi_Cliff/SM_bridge_01..03` is still the right kit for the catwalk in 3.

---

## Rooms

### 1 — The Queue · Narrative

A roped queue leading to a podium. There is no host. The queue is very long and the
restaurant is empty.

No enemies. The party walks and the System AI talks — the one place a wall of patter
is correct, because it establishes the voice. Narrative rooms are normally where the
AI shuts up; Floor 1 inverts that deliberately, because the silence later only works
once the party knows what noise sounds like.

- On spawn: *"Welcome. Your table is not ready. Your table has never been ready."*
- On movement: the tracker appears with `Orientation`.
- At the podium: a **foreclosure notice** is stapled over the menu. It is the first
  time the party sees his name, and the AI does not comment on it.

**Build:** `Diner/SM_entrance_module`, `SM_sign_Diner`, `SM_sign_open_24`, long wall
modules, opening into the cave mouth. `ADCCQuestMarker` tagged `Orientation`.

### 2 — The Grotto · Combat

The main dining cavern. Tables between stalagmites, string lights, a waterfall out
of sight. The existing camp is the staff break area — where the waitstaff sleep,
because they are not permitted to leave.

**Enemies are waitstaff**, not monsters: the tone bible's rat-swarm twist, reskinned.
They respawn because the schedule says so. Mechanically `BP_DCCEnemy_Courier` in
groups of three to five.

Allies are introduced here — CJ and Scott are already placed — and the fist-bump
quest fires *during* a fight rather than before one, because a greeting that costs a
turn is a decision rather than a formality.

**Twist:** one server is on break, and a break is legally protected. It will not
fight, cannot be attacked, and complains about Voldemortgage at length. First
non-hostile NPC, and the first grievance.

**Build:** exists. Add `Diner` chairs and tables through the cave, `Thai_Market`
beams and awnings overhead, and the fist-bump marker.

### 3 — The Falls · Puzzle

The waterfall. The only way to the diving platforms is beside it, on wet rock. No
staircase — the divers do not use one.

Everything physical is tested here and nothing else is. Climbing costs stamina,
stamina is Constitution, and sprinting to the wall first is a real mistake because
it shares the pool. A low-CON crawler will not make the ledge in one go; the ledge
is low enough to retry from the floor, so that is a lesson rather than a wall.

**Twist:** hints take the Puzzle posture — technically correct, actively unhelpful.
*"Statistically, most successful climbs involve upward motion."* Hint quality scales
inversely with AI Favor, so a popular party gets worse help.

**Build:** climb point and ledge exist. Add the waterfall particle, a second route
with a rest ledge partway, and the `EastLedge` marker.

### 4 — The Kitchens · Combat

Stainless steel, walk-ins, extraction hoods, lighting like an operating theatre. The
first fight with **range** in it, and the first appearance of **Debt Eaters** —
Voldemortgage's collectors, not staff. They have gone through the tills.

CJ casts Fire Fingers here as an ally, so the party watch a spell work before being
asked to use one. A `BP_DCCEnemy_Analyst` reskinned as a Debt Eater does the same to
them, so the lesson lands twice, the second time painfully.

**Twist:** the ticket rail above the pass is a spawn timer. A ticket printing means a
wave is coming, several seconds before anything appears. Nobody notices the first
wave. Everybody notices the second, and from then on the rail is the most important
object in the room.

**Build:** exists. Add `Diner/SM_kitchen_01..03`, `SM_fryer`, `SM_hood`, kitchen
props over the `Ice_Station` shell, the ticket rail, and **link the `DCCRoom` to its
`DCCEncounter`** — currently unlinked, which is why it logs *"Combat room with no
Encounter assigned"* every PIE start.

### 5 — The Bar · Safe Room

Brightly lit, calm, hold music, unmistakably sponsored. Safety is a *product* and
the AI wants you to know it is paying for it.

The first breathing space: stamina and health regenerate, no encounter can arm, and
`AllowsAsyncLogin()` already returns true for `SafeRoom` — so this is where a sixth
player drops in mid-session. The plumbing is not built, but the room is where it
goes.

**The bartender** is the tonal opposite of the System AI: slow, sincere, unhurried,
completely unbothered. He gives genuinely useful hints, eventually, if you are
patient. He is also the angriest person on the floor about Voldemortgage, and the
only one who has met him.

**Build:** small, almost entirely `Diner` — `SM_bar`, `SM_bar_top`, stools,
`SM_lamp_01/02`, glassware. One NPC, no encounter volume. Lights are in **Candelas**,
default 8 — not lumens.

### 6 — The Pantry · Loot Vault

Dry storage. Shelves of unclaimed property and food nobody ordered. The AI reads the
terms of service for every item aloud, at speed, and does not stop when you leave.

**The Strength room.** The mass tiers already exist and teach without UI:

| Tier | Mass | Who can lift it |
|---|---|---|
| Light | 10 kg | anyone |
| Medium | 40 kg | STR 3+ |
| Heavy | 75 kg | STR 5+ |

Carry limit is `Strength score × 15kg`, so **Marcel (STR 6, 90kg) can throw the crate
Kyle (STR 2, 30kg) cannot lift.** The party finds this out by trying it and arguing
about it. The argument is the room working.

**Twist:** the heaviest item is the most valuable and cannot be carried into a fight.
Somebody has to be the one who carries it, and they are useless for the boss. Nobody
is told this in advance.

**Build:** small. `Ice_Station` crates and `Diner` kitchen props on shelving,
`BP_Pickup_*` across the three tiers.

### 7 — The Diving Pool · Boss

The show. A pool at the base of the falls, tiered seating, and a platform high above
that someone dives from every twenty minutes. Forever.

**The divers are still diving.** Unpaid, during the boss fight, in the background.
Nobody acknowledges it. That image — a performance continuing through a battle
because the contract says it must — is the floor's thesis in one shot.

**Lord Voldemortgage** floats above the pool with his briefcase, doing the fight
described in the roster. Full structure in `Docs/DT_DCCEnemies.json`; the short
version:

| Phase | What happens |
|---|---|
| Opening Statement | Summons 3 Debt Eaters, hangs back throwing paperwork, talks constantly |
| Late Notice (50% HP) | Stops, straightens tie, monologues, summons 5 more |
| Balloon Payment | 3-turn telegraph. Every living Debt Eater adds damage to the hit. Clear them and it fizzles to a Minimum Payment |
| Aftermath | Two turns of reduced defence and no summoning — the real window |
| Death — the lien | Transfers to a random crawler: a small recurring debt tax at the start of future fights until paid off in gold |

**Why this fight suits what we have built.** The Balloon Payment telegraph is a
*timer that rewards target priority* — exactly what the real-time-with-pause loop is
for. Pausing to assign five players onto five Debt Eaters, then resuming to watch
them resolve in initiative order, is the system's best possible showcase. It is also
the first fight where pausing is genuinely the correct play rather than a convenience.

**Tier and reward.** The books put a Floor Boss at tier 6, but this is a tutorial
floor and the party will be around level 2. Treat him as a **Neighborhood boss,
tier 1–2** — one level's worth, not six. The floor-clear bonus already dwarfs the
kill, which is the right shape.

**Build:** `Cave_Camp` / `Alien_Cave_biome` cavern shell, `Thai_Market` benches as
tiered seating, waterfall, water plane. Voldemortgage needs a model — the roster's
physical description is written for exactly that, and the Meshy → Blender → FBX
pipeline in `Tools/` is the route.

---

## What this needs that does not exist yet

**Updated 2026-08-15.** Items 1–4 of the original list have since been built and compiled —
`Dialogue/`, staged waves in `Rooms/`, `Combat/DCCBossComponent`, and gold in `Stats/`. What
is genuinely still missing:

1. **Rooms 5, 6, 7 as geometry.** All small, all installed packs, except the arena.
2. **Room-to-room connection.** Rooms 01 and 02 are still islands. The two joins that matter
   are the Queue→Grotto walkway and the Falls→Kitchens catwalk
   (`SciFi_Cliff/SM_bridge_01..03`); together they make the floor walkable end to end for the
   first time, which is worth more for playtesting than any single room.
3. **Pickup Blueprints.** `Items/` compiles and `HeavyLifting` is authored against it, but there
   are **no `BP_Pickup_*` assets in the project**, so the Pantry's whole Strength lesson — the
   mass tiers, the argument about who can lift what — has nothing to act on.
4. **A Voldemortgage model.** Meshy from the roster description.
5. **A cast animation for anyone at all.** No skeleton in the project has one; `SK_HockeyWings`
   (CJ) has only Walking, Running, AgreeGesture and BoomDance. The Room 4 beat where CJ casts
   Fire Fingers currently plays in an idle pose. The nearest usable poses — `A_DW_CallGesture`,
   `A_AO_ArcheryAim` — are on **other skeletons** and need IK Rig retargeting, not a copy.
   There is also no montage system: `Montage` appears nowhere in `Source/`.

Casting *does* now root the caster — `FDCCSkillDef::CastTimeSeconds` defaults to 0 (instant, as
before) and any positive value locks movement for that long. So the positional cost exists as
soon as a skill row asks for it; only the animation is missing.

None of it is blocked on the ruleset, the stats layer, or anything already compiled.

---

## The way out — `ADCCFloorExit`

**Added 2026-08-15.** In DCC the real problem on a floor is finding the stairs, so the exit
is a first-class actor rather than a trigger bolted to the boss arena.

**It locks itself.** The exit watches for living actors carrying a `UDCCBossComponent` and
opens once none remain. Two consequences worth knowing:

- **Voldemortgage needs no wiring to the exit.** Give him a `UDCCBossComponent` — which the
  fight already requires — and the exit locks behind him automatically. There is no reference
  to forget, which matters because a forgotten reference here fails *silently*: the exit would
  simply never open.
- It is **latched on "a boss has been seen"**, so it stays shut before he spawns rather than
  reading an empty world as victory. That matters because he arrives as an encounter wave, not
  at BeginPlay.

The sign above it reads `EXIT — CLOSED FOR MAINTENANCE` in red, then `EXIT` in green. Set
`bRequiresBossDefeat = false` for a tutorial floor that should just let people through.

Walking out fires `ClearFloor` for the quest chain, and **banks any carried loot that deferred
its value** — see below.

## Gold, loot and why the Strongbox matters

Three pieces, all added 2026-08-15:

- **`ADCCPickup::GoldValue`** — what a prop is worth.
- **`bBanksOnPickup`** — `true` pays out the moment it is lifted and vanishes (coins; making
  the party walk pocket change to the exit is tedium, not tension). `false` keeps its value
  until somebody carries it **out through the floor exit**.
- **`LootTable` on `ADCCCharacterBase`** — `FDCCLootDrop` entries with an independent chance
  roll each, spawning real pickups at the corpse. Hostiles default to a 55% chance of 3–12
  gold; crawlers never drop, and that covers allies, since `IsCrawler()` returns `bIsAlly`.

Drops are **actors on the floor, not credit to the killer**. That costs a few actors per
fight and buys the thing that makes gold interesting at a table: loot has to be walked to,
which pulls the party out of position, and whoever grabs it is a conversation rather than an
automatic payout to whoever landed the hit.

**`ADCCPickup_StrongboxCrate` is where all of this converges.** 75kg — the exact STR 5 line —
worth 250 gold, and `bBanksOnPickup = false`. It is worth nothing unless a crawler carries it
through the exit, and carrying it means both hands are full for the boss fight. The Pantry's
"somebody has to be the one who carries it, and they are useless for the boss" twist is now
mechanical rather than aspirational, and nobody has to be told the rule.

## Quest chain

| Quest | Room | Objective |
|---|---|---|
| `Orientation` | 1 | Reach the marker |
| `MakeFriends` | 2 | Fist-bump an ally |
| `ClearTheRoom` | 2 | Clear the encounter, 3 kills |
| `GoUp` | 3 | Climb 10m, reach `EastLedge` |
| `Kitchens` | 4 | Clear the encounter |
| `HeavyLifting` | 6 | Collect a heavy item |
| `FloorOne` | 7 | Clear every encounter |

`FloorOne` already exists and already pays the floor bonus, so the chain terminates
correctly with no new C++.

**All seven are authored and imported as of 2026-08-15.** `Kitchens` and `HeavyLifting` were
the two missing rows; the chain now links end to end,
`Orientation → MakeFriends → ClearTheRoom → GoUp → Kitchens → HeavyLifting → FloorOne`, verified
by walking `NextQuestId` out of the live DataTable rather than the source file.

Both new rows use `TargetId: "None"`, matching `ClearTheRoom`'s existing convention — the
notify sites report the **encounter actor's name** for `ClearEncounter` and the **pickup's class
name** for `Collect`, so an empty `TargetId` counts any of the right kind. Tighten them to a
specific room or prop once those actors exist and have stable names.
