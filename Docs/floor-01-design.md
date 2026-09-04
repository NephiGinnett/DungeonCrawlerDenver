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

**Updated 2026-08-19.** Items 1–4 of the original list have since been built and compiled —
`Dialogue/`, staged waves in `Rooms/`, `Combat/DCCBossComponent`, and gold in `Stats/`. What
is genuinely still missing:

1. **Rooms 5, 6 and 7 are dressed blockouts, not final art.** `DCCRoom_Bar` (SafeRoom) has
   walls, a ceiling, `SM_bar`/`SM_bar_top`, three stools and two lamps. `DCCRoom_DivingPool`
   (Combat) has a water plane, an 8-bench seating ring, and an `ADCCFloorExit`. The bartender NPC
   and the full `DCCEncounter_Voldemortgage` (both waves) are placed, wired, and PIE-verified to
   actually spawn and fight. **`DCCRoom_Pantry` (LootVault) was built 2026-08-19** — x8700-9800,
   y5800-6600, walls/ceiling/navmesh, wall shelving (`Diner/SM_store_large|short`),
   `Ice_Station` crate stacks, and the full mass-tier pickup set (see item 3). **All seven rooms
   of the floor now exist as geometry.**
2. **Room-to-room connection — done.** As of 2026-08-19 the floor is walkable
   Queue → Grotto → (climb) → Falls → Kitchens → Bar → Pantry, and Bar → Diving Pool, end to end:
   - **Queue → Grotto**: an existing but undocumented "Approach" plaza/ramp system (labelled
     actors `Approach_Ramp_Up/Down`, `Approach_Plaza_01/2`, spanning x987→4214) already carried
     the spawn area most of the way. Only the last stretch was missing — a floor slab now
     bridges x4214→5150 into Room01's west doorway (confirmed by the existing
     `Room01_Wall_South_A` geometry, which leaves the doorway gap exactly at x5150-5200,
     y-350..350 as this doc says).
   - **Falls → Kitchens**: built as a single long graded ramp (not individual
     `SciFi_Cliff/SM_bridge` segments — the run is ~4650 units, which would need 8+ segments;
     went with a blockout plank instead, kit swap is still a valid later art pass) from the climb
     ledge (`Room01_ClimbLedge`, x6600-6850, z360-400) down to Room02's actual west doorway gap.
     **Room02 is built larger than this doc states** — its real bounds are roughly
     x6660-8660, y4310-6360 (walls named `Room02_Wall_West_A/B/B2`, `_East`, `_North`, `_South`),
     not the y4700-5300 written above. The doorway gap is the space between Wall_West_A (ends
     y5050) and Wall_West_B (starts y5320).
   - **Room02 → Bar → Diving Pool**: done 2026-08-19, see above.
   - **Bar → Pantry**: done 2026-08-19. The Bar's north wall was rebuilt as two segments
     (`SM_Bar_Wall_North_A`/`_B`) to cut a 300-unit doorway at x9150-9450, with a corridor floor
     and nav volume running y5300→5800 into the Pantry's matching south-wall gap.
   - **Left alone on purpose**: a separate, undocumented ground-level path (`Approach_North_01/
     02/03`, `Approach_East_01/02`) already runs from the spawn area north then east, reaching
     all the way to x6803 — right at Room02's west wall. Did not extend or connect this into
     Room02, because the design doc is explicit that climbing is "the only way on" to the Falls/
     Kitchens side, and finishing that ground path would create an unintended bypass. It's
     unclear what this path was originally for; worth asking Zoe before touching it further.
3. **Pickups — placed 2026-08-19, no longer blocking.** There are still no `BP_Pickup_*`
   Blueprint assets and there do not need to be: the pickups are C++ subclasses
   (`Items/DCCPickupVariants.h`) and were placed directly as class instances. The Pantry now
   holds the full Strength lesson — Bottle (3kg) and EggBox (8kg) on the west side, GreaseBarrel
   (35kg) / Crate (40kg) / Jukebox (50kg) through the middle, Keg (70kg) and the Strongbox (75kg)
   on the east, so the mass tiers read left-to-right across the room.
   **The Strongbox was MOVED, not duplicated** — it was sitting in Room 01's throw sandbox at
   (5800, 100, 20) and is now at (9620, 6300, 60). Deliberate: it is `bBanksOnPickup = false`
   with `GoldValue` 250, i.e. *the floor's single payday*, and a second copy would have broken
   the Pantry twist ("somebody has to carry it, and they are useless for the boss"). The generic
   tier props were duplicated instead of moved, since both rooms want things to lift and those
   carry no unique value. Room 01 keeps Bottle/EggBox/Crate/GreaseBarrel/Keg/Coins as its throw
   sandbox.
   `HeavyLifting` uses `TargetId: "None"`, so any `Collect` of the right kind satisfies it —
   the objective can now fire. **Not yet playtested.**
4. **A Voldemortgage model.** Meshy from the roster description. `ADCCEnemyCharacter_Voldemortgage`
   currently reskins onto `SK_DiceWinged_Shared` at 1.5x pending the real art.
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

## 8 — The Claim · The Rocky Flats Extraction Shaft

**Added 2026-09-01, at Zoe's ask: give Floor 1 an Earthcore turn, and make the way down a
mine.** This is the floor's new terminus. Not built yet - this section is the spec.

### Why it exists

Floor 1 has been, correctly, a comedy about debt collection. Everything in it is a set: the
cantina, the pool, the bar. **The Claim is where the set ends and the actual hole in the
ground begins**, which is the point of it - per `story-premise.md`, the floors are excavation
shafts and "descending" is literal. This is the first place the game says so out loud without
explaining it.

The register shifts accordingly. Upstairs the horror is bureaucratic and verbal. Down the
shaft it gets quieter, older, and less interested in talking - the first faint step toward the
Undermind's end of the dial. Nothing down here should crack a joke about paperwork.

### ⚠️ REVISED 2026-09-01 — read this before the sections below

Zoe reworked the sequencing. The corporations are **not** standing at the shaft head when the
party first arrives, and the Core Sample no longer reveals impossible depth. The order is now:

1. **Only the Surveyor is around.** He talks about **lights on the mountain at night** — steady,
   blue, moving in straight lines — and is convinced somebody is **jumping the claim**. He also
   warns, in character, that whatever is holding those lights is *working, not hunting*, and
   will ignore you unless you take a swing at it.
2. **The party goes in unopposed.** The lights are **corporate survey drones**, deep inside,
   circling patrol routes. They are real enemies but `bPassiveUntilProvoked` — they will not
   approach or attack unless attacked. Killing them is possible, cheap, and a decision.
3. **The Core Sample is a platinum-iridium alloy.** The Surveyor makes a show of testing it —
   streak, density, a scratch — then admits the theatre: he knew the moment he picked it up.
   Alloyed all through, which does not happen in open ground. He tells the party to **hide it**,
   because if the suits see it they stop arguing about paperwork and start arguing about who
   owns *you*.
4. **Coming back out, the corporations are there.** They flew in the instant the party surfaced,
   which means they were watching the hole, which means the drones were theirs. They put up a
   **barricade**, bar re-entry, and **demand to see what was brought out**. Showing a faction the
   sample grants that faction's Favor — a real reward for doing the thing you were just warned
   against.
5. **Only after the party picks a side** does the System AI start addressing the *alien* audience
   about the material — that the acquisition was partly about this deposit, and that fifty-one
   years of surveying happened before any claim was filed. Per `story-premise.md`, this is the
   mining half of the cover story turning out to be the true half.

The claim-dispute puzzle (two of three signatures) is unchanged and still gates the door; it
just happens on the way **back in**, not the way in.

### The premise: everybody already knew about the platinum

The **Front Range Platinum Reef** runs north under the whole range. Platinum group metals,
known before the Collapse, mapped and then abandoned in the 1970s for reasons the official
record gives as "cost." The shaft head sits in the old **Rocky Flats** works - which usefully
puts it next door to the Nuclear Family enemies, who are already Rocky Flats satire and have
never had a home.

This is what makes the corporate bickering land: nobody is fighting over a rumour. It is a
confirmed deposit, and three parties have arrived with three incompatible claims.

### The Compliance Door — `ADCCComplianceDoor`

The shaft is gated. Entry needs **two authorisations**; **three parties** each insist they are
the only one with standing:

| Signatory Id | Who | Claim | The ask |
|---|---|---|---|
| `ClaimsAdjuster` | Pell, Mile High Recovery Solutions | Foreclosure — the city defaulted, the reef came with it | Back their filing |
| `SiteForeman` | Dunbar, Titan Conglomerate | An extraction contract with a municipality that no longer exists | Back Titan |
| `UnionSteward` | Local 4-C, Amalgamated Subsurface | Labour — the people who cut the shaft are owed | Put the crews' names on whatever comes out |

**The party only ever needs two of three.** They never have to resolve the dispute, only
notice the arithmetic - and `Steward_Claim_03` says so out loud once the door is open, which
is the joke landing rather than the joke being explained.

**Three routes in, by design:**

1. **Back two corporations.** The obvious road. Has consequences later; which two should
   matter beyond this floor.
2. **The surveyor's bypass** (`Surveyor_Access_01`). Thirty-year-expired credentials that were
   never formally revoked, and the door checks revocation rather than expiry. For a party that
   refuses to pick a side. **This is the route the Core Sample line quietly earns.**
3. **The crowbar.** It is a door. Forcing costs 75 Favor and needs Strength 5, and unites
   three legal departments in agreement for the first time in weeks.

### The level quest — *The Withdrawal*

**This is the spine, and the thing Floor 1 has been missing.** Voldemortgage does not simply
wait in the Diving Pool to be killed: once the party has met him, the account is "closed and
transferred to a lower servicing tier" and **he withdraws down the shaft toward Floor 2.**

That single change does three jobs at once:
- It gives the boss a reason to exist beyond standing at the end of a corridor.
- It gives the mine urgency - the door is not a curiosity, it is the thing between the party
  and someone getting away.
- It makes descending the floor's climax rather than its epilogue.

Quest chain (`DT_DCCQuests_Floor01Mine.json`): `TheWithdrawal` → `ChainOfCustody` →
`FirstDescent`, with `CoreSample` hanging off the Bar independently.

### What lives down there

Two enemies, both of which refuse the verb the party has been using all floor
(`Characters/DCCEnemyCharacter_Mine.h`):

- **Legacy System** — immune to being struck. Weapons and fists do nothing; it is not armoured
  so much as uninterested. Answer: hit it from range - the new `TargetedThrow` skill, a thrown
  prop, or any skill at all. The Game Guide teaches this before the party ever meets it.
- **Reconciliation** — two bodies, one ledger. Damage to one is reversed by the other unless
  both are struck within ~2.5s. The project's first enemy actually designed for five players
  rather than merely tolerating them.

### ✅ Blocked out 2026-09-01

Built over MCP and saved. Outliner folders `DCC/Room10_TheClaim` and `DCC/Room11_TheShaft`.

- **Shaft head** (`Room10_TheClaim`) — chamber centred **(16000, 3000)**, ~2000×1600, floor
  z=0, ceiling z=620. Doorway gaps west (to the corridor) and east (the shaft mouth). Two
  point lights, navmesh, and a corridor at y3000 running back west toward the Diving Pool.
- **`ADCCComplianceDoor`** at (16980, 3000), sitting in the east wall gap.
- **The three claimants**, each with its own mesh so they read apart at a glance: Pell
  (`SK_DonMinion`), Dunbar (`SK_BuffMinion`), the Steward (`SK_ArmsOut_Shared`). SpeakerIds and
  `DialogueTable` verified by read-back; Pell offers `ChainOfCustody`.
- **The Surveyor** in the Bar at (8250, 4980), offering `CoreSample`.
- **`QuestMarker_MineShaftHead`** at (15200, 3000) for *The Withdrawal*.
- **The shaft** (`Room11_TheShaft`) — gallery centred (18600, 3000), with `DCCRoom` +
  `DCCEncounter` **linked** (the Room02 trap: a Combat room whose `Encounter` is None fails
  silently). Waves: two Break Room Mold, then the Legacy System alone.
- **A twinned Reconciliation pair** placed directly at (18300, ±400) with `Twin` cross-wired
  and confirmed both ways.

**Two things caught by looking at it rather than trusting the coordinates**, both fixed: the
door was rendering with the engine checker material (its C++ default is
`/Engine/BasicShapes/Cube`, not the project's `LevelPrototyping/SM_Cube`), and the east wall
was a single solid slab, so the door would have opened onto masonry.

**Why the Reconciliation pair is placed rather than spawned:** `Twin` is `EditInstanceOnly` and
cannot be authored on something an encounter creates at runtime, so a wave-spawned pair would
have quietly behaved like ordinary enemies. `ADCCEnemyCharacter_Reconciliation::BeginPlay` now
auto-pairs to the nearest unpaired sibling, which fixes the spawned case — **written and
compile-checked, but NOT yet deployed.**

### Relocated 2026-09-01 — and why, because the mistake is worth not repeating

The complex was first built at **(16000, 3000)** and that was **wrong**: it landed inside 37
existing city actors. The emptiness check that cleared the site only probed `z -800..1500`,
and the buildings there are tall with high bounding-box centres, so they simply were not
returned. **Probe `z -4000..12000` when checking whether ground is free.**

Everything moved **+13000 in Y**. The complex now sits at **(16000, 16000)**, on ground verified
empty across the full height range. The old approach corridor was deleted rather than moved —
it pointed back at the Diving Pool from the old siting.

**The mine is therefore not yet connected to the rest of Floor 1.** That is a deliberate stopping
point rather than an oversight: an `ADCCOathgate` pair, a lift, or a long approach are all
reasonable answers and it is Zoe's call which. It is reachable now only by flying there.

### Dressed with real geometry, not cubes

- **Mountain:** `SoulCave` `SM_EntranceRockCliff2/3/5` at 1.3–1.7 scale wrap the chamber north,
  south and rear, with `Desert_cave/SM_rock_01` boulders and `SciFi_Cliff/SM_rock_02` scree
  framing the mouth. The grey shell now reads as a cut into rock rather than a box in a field.
- **Props:** `Desert_cave` mine carts, cases and rope at the shaft head and deeper in.
- **Light:** three cold point lights over the approach — the Surveyor's "too steady, too blue".

The interior shell is still `SM_Cube`. Rock outside, blockout inside; a full cave-interior pass
using `SoulCave`'s pillars and floor planes is the obvious next art job.

### ✅ Placement complete 2026-09-02 — the sequence works end to end

Deployed and placed. Everything below verified by read-back, not assumed.

- **Three `ADCCDialogueNPC_Signatory`** replace the plain dialogue NPCs (a placed C++ class
  instance cannot be reparented, so they were deleted and re-placed with properties restored).
  Each has `ComplianceDoor` wired and `SignatoryId` matching its `speakerId`. **Talking to a
  claimant now actually signs.**
- **Five `ADCCEnemyCharacter_SurveyDrone`** in the shaft, each on its own four-point circuit,
  all `bPassiveUntilProvoked = true` with a 1100-unit `ViolenceWatchRadius`. They circle and
  ignore the party until attacked.
- **A nine-piece barricade** at the mouth — five `Ice_Station` fence panels plus crates.
- **Two `ADCCArrivalTrigger`s**: a deep one at (19500, 16000) with no prerequisite, and one at
  the mouth (15100, 16000) gated on it, carrying all twelve arriving actors (3 claimants +
  9 barricade pieces). The corporations cannot appear until the party has actually been inside.

**Three placement bugs found by tracing and looking rather than trusting coordinates:**

1. **The mouth boulders sealed the entrance.** `Desert_cave/SM_rock_01` is 2612 units wide in Y;
   at 0.55 scale that is ±718, so boulders centred at y15300/16700 overlapped the entire
   y15700–16300 approach. Moved to y14750/17250. **Measure a mesh's Y extent before flanking a
   doorway with it.**
2. **The mountain lights were blowing the whole approach out** — 40000 intensity over a 6000
   radius. Dropped to 4000–9000 over 3500–5000 and tinted cold, which is what the Surveyor's
   "too steady, too blue" wants anyway.
3. **`SM_fence` is only 100 units tall**, so the barricade blocked at knee height and a
   head-height trace passed straight over it. Scaled to 2.4× vertical; now blocks at z=180
   across the full doorway.

`SceneTools.trace_world` is the right tool for all three — it returns a **distance along the
ray**, not a hit struct, and tracing at several heights is what exposed the fence problem.

### Still owed

- **A cave-interior art pass.** The shell is still `SM_Cube` inside; `SoulCave`'s pillars and
  floor planes are the obvious material. Rock outside, blockout inside.
- **The mine is still not connected to the rest of Floor 1** (see the relocation note above).
  An `ADCCOathgate` pair, a lift, or a long approach are all reasonable - Zoe's call.
- **Model swaps** - the Steward and the Surveyor are wearing Kyle's and Marcel's crawler meshes.
  See `Docs/model-assignments.md`; this is a readability bug, not a cosmetic one.
- **Lighting is unbuilt** in this area and the NPCs stand in T-pose (no idle animation assigned),
  same as the other dialogue NPCs on the floor.
- **Nothing here has been seen in PIE.** The whole chain - drones ignoring you, the sample
  reveal, the corporations arriving, signatures opening the door - needs one real playtest.

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
