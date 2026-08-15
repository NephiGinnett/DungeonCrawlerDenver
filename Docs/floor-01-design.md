# Floor 1 — Cliff Diver Cantina

**The AI's name for it:** *"Floor One: A Celebration of Local Cuisine."*

Per the tone bible's naming convention, the floor's official name always undersells
the danger. Everything below is what it actually is.

---

## Why this one

The first draft of this document picked The Terminal. That was wrong, and the reason
is worth writing down: **it was chosen for how well it reconciled Room 01, and the
asset inventory was checked afterwards.** There are no airport models in any
installed pack, so the theme would have been carried entirely by signage and
willpower.

Cliff Diver Cantina is the tone bible's own entry for a cavernous fake-Mexican
restaurant with a waterfall and performers who dive to their deaths, nightly,
forever. Its real-world referent is a **cave-themed restaurant**. That single fact
inverts the whole asset problem:

| The floor needs | Installed pack |
|---|---|
| A cave the size of a building | `Cave_Camp`, `Alien_Cave_biome`, `Desert_cave` |
| A restaurant inside it | `Diner` — bar, bar top, kitchen modules, fryer, hood, chairs, signage |
| A fake-village dining tier | `Thai_Market` — beams, benches, stalls, awnings |
| A waterfall | `Abandonned_Brutalist/M_particle_waterfall` |
| Industrial kitchens and cold storage | `Ice_Station` |

**Nothing here is a substitute for the thing we wanted.** The cave *is* the theme.

It also does what the airport was picked for, and does it better. Room 01 is a lit
cave containing a campfire, a tent, a bedroll, a table and crates — as a squatter
camp that is odd, but as **the staff break area behind the falls** it is exactly
right, and needs no changes at all. Room 02's `Ice_Station` sci-fi becomes the
kitchens and walk-in.

---

## The floor at a glance

Seven rooms. The five `EDCCRoomType` templates each appear at least once, in an
order that teaches one mechanic at a time and never teaches two at once. The
structure survived the re-theme unchanged; only the fiction and the assets moved.

| # | Room | Type | Teaches | Status |
|---|---|---|---|---|
| 1 | The Queue | Narrative | Movement, the feed, the tracker | Spawn platform exists |
| 2 | The Grotto | Combat | Attacking, allies, fist bumps | **Room 01 exists** |
| 3 | The Falls | Puzzle | Climbing, stamina, sprint | **Climb point exists** |
| 4 | The Kitchens | Combat | Skills, mana, ranged enemies | **Room 02 exists** |
| 5 | The Bar | Safe Room | Rest, hints, inventory | New |
| 6 | The Pantry | Loot Vault | Carry limits, Strength, throwing | New |
| 7 | The Diving Pool | Combat (boss) | Everything at once | New |

Four exist in some form. **Three new rooms is the build cost**, and two are small.

---

## Room 1 — The Queue

**Type:** Narrative. **Where:** the existing spawn platform at the world origin.

A roped queue leading to a podium. There is no host. The queue is very long and the
restaurant is empty.

No enemies. The party walks and the System AI talks — the one place a wall of patter
is correct, because it is establishing the voice. Narrative rooms are normally where
the AI shuts up; Floor 1 inverts that deliberately, because the silence later only
works once the party knows what noise sounds like.

**Beats, in order:**
- On spawn: *"Welcome. Your table is not ready. Your table has never been ready."*
- On first movement: the tracker appears with `Orientation`.
- At the podium: the AI mentions the wait time is four hours, then that it has
  always been four hours, then changes the subject.

**Build:** `Diner/SM_entrance_module`, `SM_sign_Diner`, `SM_sign_open_24` and the
long wall modules, opening into the existing cave mouth. One `ADCCQuestMarker`
tagged `Orientation` at the podium.

**Twist:** a laminated menu on the podium. The prices are in a currency that does
not exist. The sopapillas are listed as *"complimentary, mandatory."*

---

## Room 2 — The Grotto

**Type:** Combat. **Where:** Room 01 as built (x 5200–6800, cave, `Cave_Camp`).

The main dining cavern. Tables between stalagmites, string lights, a waterfall
somewhere out of sight. The existing camp is the **staff break area** — where the
waitstaff sleep, because they are not allowed to leave.

**The enemies are waitstaff.** The tone bible's rat-swarm twist, reskinned: they are
not monsters, they are employees, and they respawn because the schedule says so.
Mechanically the existing `BP_DCCEnemy_Courier` in groups of three to five — low
level, fast, weak.

**This is where the allies are introduced.** CJ and Scott are already placed. The
fist-bump quest fires here, taught *during* a fight rather than before one, because
a greeting that costs a turn is a decision rather than a formality.

**Twist:** one server is on break, and a break is legally protected. It will not
fight, cannot be attacked, and will complain about the others at length. It is the
first hint the dungeon has an internal politics.

**Build:** exists. Additions: `Diner` chairs and tables scattered through the cave,
`Thai_Market` beams and awnings overhead for the fake-village tiers, and a marker
for the fist-bump objective.

---

## Room 3 — The Falls

**Type:** Puzzle. **Where:** the east wall of Room 01, already climbable.

The waterfall. The only way up to the diving platforms is beside it, on wet rock.
There is no staircase, because the divers do not use one.

**Everything physical the party has is tested here and nothing else is.** Climbing
costs stamina, stamina is Constitution, and sprinting to the wall first is a genuine
mistake because it shares the pool. A low-CON crawler will not make the ledge in one
go — that is the intended lesson, not a spike, and the ledge is low enough to retry
from the floor.

**Twist:** the AI's hints take the Puzzle posture — technically correct, actively
unhelpful. *"Statistically, most successful climbs involve upward motion."* Hint
quality scales inversely with AI Favor, so a party the crowd likes gets worse help.

**Build:** climb point and ledge exist. Additions: the waterfall itself
(`M_particle_waterfall`), a second route with a rest ledge partway so a
Constitution 2 crawler has a path a Constitution 6 one can skip, and the
`EastLedge` marker already specified.

---

## Room 4 — The Kitchens

**Type:** Combat. **Where:** Room 02 as built (y ≈ 5000, `Ice_Station`).

Stainless steel, walk-in freezers, extraction hoods, and lighting like an operating
theatre. The first fight with **range** in it.

This is where skills matter. CJ casts Fire Fingers here as an ally — the party watch
a spell work before being asked to use one. A `BP_DCCEnemy_Analyst` at the back does
the same to them, so the lesson lands twice, the second time painfully.

**Twist:** the ticket rail above the pass is a spawn timer. A new ticket printing
means a wave is coming, several seconds before anything appears. Nobody notices on
the first wave. Everybody notices on the second, and from then on the rail is the
most important object in the room.

**Build:** exists. Additions: `Diner/SM_kitchen_01..03`, `SM_fryer`, `SM_hood` and
the kitchen props over the `Ice_Station` shell; the ticket rail; and **linking the
`DCCRoom` to its `DCCEncounter`** — currently unlinked, which is why it logs
*"Combat room with no Encounter assigned"* every PIE start.

---

## Room 5 — The Bar

**Type:** Safe Room. **Where:** new, off the Kitchens.

Brightly lit, calm, hold music, and unmistakably sponsored. Safety is a *product*
and the AI wants you to know it is paying for it.

**The first breathing space:** stamina and health regenerate, no encounter can arm
here, and `AllowsAsyncLogin()` already returns true for `SafeRoom` — so this is
where a sixth player drops in mid-session. Not built yet, but the room is where it
goes, and placing it now gives the plumbing a home.

**The bartender** gives genuinely useful hints, eventually, if you are patient. He
is the tonal opposite of the System AI: slow, sincere, unhurried, completely
unbothered by the dungeon. The first evidence that not everything down here is a
performance.

**Build:** small, and almost entirely `Diner` — `SM_bar`, `SM_bar_top`, stools,
`SM_lamp_01/02`, glassware. One NPC, no encounter volume. Note the lighting units
gotcha: these are **Candelas**, default 8, not lumens.

---

## Room 6 — The Pantry

**Type:** Loot Vault. **Where:** new, between the Bar and the pool.

Dry storage. Shelves of unclaimed property and food nobody ordered. The AI reads the
terms of service for every item aloud, at speed, and does not stop when the party
walks away.

**This is the Strength room.** The pickup mass tiers already exist and teach without
a word of UI:

| Tier | Mass | Who can lift it |
|---|---|---|
| Light | 10 kg | anyone |
| Medium | 40 kg | STR 3+ |
| Heavy | 75 kg | STR 5+ |

Carry limit is `Strength score × 15kg`, so **Marcel (STR 6, 90kg) can pick up and
throw the crate Kyle (STR 2, 30kg) physically cannot move.** The party discovers
this by trying it, arguing about it, and then making Marcel do it. That argument is
the room working.

**Twist:** the heaviest item is the most valuable and cannot be carried and fought
with at the same time. Somebody has to be the one who carries it, and they are
useless for the boss. Nobody is told this in advance.

**Build:** small. `Ice_Station` crates and `Diner` kitchen props on shelving,
scattered `BP_Pickup_*` across the three tiers.

---

## Room 7 — The Diving Pool

**Type:** Combat (boss). **Where:** new, the floor's terminus.

The show. A pool at the base of the falls, tiered seating around it, and a platform
high above that someone dives from every twenty minutes. Forever.

**The boss is the Head Diver.** He has been performing since before the dungeon
took the building. He is very good at it and he cannot stop.

**Tier and reward.** The books put a Floor Boss at tier 6, but this is a *tutorial*
floor and the party will be around level 2. Treat him as a **Neighborhood boss,
tier 1–2** — one level's worth, not six. The floor-clear bonus already dwarfs the
kill, which is the right shape: finishing the floor should beat farming the room.

**Mechanically a check on everything above:** he dives (a leap attack telegraphed by
climbing, which the party now recognises), the pool is a hazard that drains stamina
(Room 3's lesson, weaponised), and he calls waitstaff when below half (waves, learned
in the Kitchens). The high platform is reachable by climbing — optional, rewarded.

**Twist:** he does not want to fight. He wants to finish his shift. If the party
works this out there is a non-combat resolution — and the System AI is contractually
unable to tell them, because a fight rates better.

**Build:** `Cave_Camp` / `Alien_Cave_biome` cavern shell, `Thai_Market` benches as
tiered seating, the waterfall, and a water plane. Boss is a re-levelled
`BP_DCCEnemy_Brute` for a first pass.

---

## What this needs that does not exist yet

Ordered by what blocks the most:

1. **A non-hostile NPC that talks.** The bartender and the server-on-break both need
   dialogue with no combat attached. Nothing in the codebase does this —
   `ADCCCharacterBase` assumes everything fights. **This is the largest genuinely
   new piece the floor needs.**
2. **A boss-tier enemy Blueprint.** A re-levelled Brute suffices for a first pass; a
   real boss needs a leap and a summon.
3. **Rooms 5, 6 and 7 as geometry.** All small, all using installed packs.
4. **Wave spawning** for the ticket rail. `ADCCEncounter` spawns once; waves need it
   to spawn in stages.
5. **Room-to-room connection.** Rooms 01 and 02 are still islands.

None of it is blocked on the ruleset, the stats layer, or anything already compiled.

---

## Quest chain for the floor

The existing `DT_DCCQuests.json` chain maps onto rooms 1–4 already. Extending it:

| Quest | Room | Objective |
|---|---|---|
| `Orientation` | 1 | Reach the marker |
| `MakeFriends` | 2 | Fist-bump an ally |
| `ClearTheRoom` | 2 | Clear the encounter, 3 kills |
| `GoUp` | 3 | Climb 10m, reach `EastLedge` |
| `Concourse` → rename `Kitchens` | 4 | Clear the encounter |
| `HeavyLifting` | 6 | Collect a heavy item |
| `FloorOne` | 7 | Clear every encounter |

`FloorOne` already exists and already pays the floor bonus, so the chain terminates
correctly with no new C++.
