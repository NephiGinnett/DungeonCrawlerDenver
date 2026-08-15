# Floor 1 — The Terminal

**The AI's name for it:** *"Floor One: A Warm Welcome to Our Newest Arrivals."*

Per the tone bible's naming convention, the floor's official name always undersells
the danger. Everything below is what it actually is.

---

## Why the airport

The tone bible offers ten Denver landmarks. The Terminal is the right one for Floor 1
for three reasons that have nothing to do with the joke being good:

1. **An airport is where you arrive.** The tutorial fiction writes itself — the party
   are new arrivals being processed. No contrivance needed to explain why a voice is
   explaining things to them.
2. **It reconciles what is already built.** Room 01 is committed to a cave look with
   `Cave_Camp` placed and lit. Re-read as *an excavated service tunnel under the
   terminal*, that cave becomes correct rather than something to re-theme. Room 02's
   `Ice_Station` sci-fi reads as airport plant and machinery with no changes at all.
3. **Everyone already believes there is something under that airport.** The joke is
   that the AI is offended you called it a conspiracy rather than a venue — and that
   joke only lands on the floor where the party has no context yet.

The two rooms that exist are therefore **kept**, not rebuilt. Everything new is
added around them.

---

## The floor at a glance

Seven rooms. The five `EDCCRoomType` templates each appear at least once, in an
order that teaches one mechanic at a time and never teaches two at once.

| # | Room | Type | Teaches | Status |
|---|---|---|---|---|
| 1 | Jetway | Narrative | Movement, the feed, the tracker | Spawn platform exists |
| 2 | Baggage Claim | Combat | Attacking, allies, fist bumps | **Room 01 exists** |
| 3 | Service Shaft | Puzzle | Climbing, stamina, sprint | **Climb point exists** |
| 4 | Concourse B | Combat | Skills, mana, ranged enemies | **Room 02 exists** |
| 5 | The Dispensary Kiosk | Safe Room | Rest, hints, inventory | New |
| 6 | Lost and Found | Loot Vault | Carry limits, Strength, throwing | New |
| 7 | Gate B-33 | Combat (boss) | Everything at once | New |

Rooms 1–4 exist in some form. **Three new rooms is the actual build cost of this
floor**, and two of them are small.

---

## Room 1 — Jetway

**Type:** Narrative. **Where:** the existing spawn platform at the world origin.

A corridor with no enemies in it. The party walks and the System AI talks — this is
the one place a wall of patter is correct, because it is establishing the voice.

Per the tone bible, Narrative rooms are normally where the AI *shuts up*. Floor 1
inverts that deliberately: the silence later only works if the party first learns
what noise sounds like.

**Beats, in order:**
- On spawn: *"Welcome. You have been selected. Selection was not competitive."*
- On first movement: the tracker appears with `Orientation`.
- At the far end: the AI mentions, in passing, that the return jetway has been
  removed for maintenance. It does not dwell on this.

**Build:** `Megastructure_Scifi_World` grand hallway pieces, narrowing into the
existing cave mouth. One `ADCCQuestMarker` tagged `Orientation` at the far end.

**Twist:** the arrivals board lists flights that have already landed. All of them
landed years ago. Nobody comments on this and the AI changes the subject if the
party lingers.

---

## Room 2 — Baggage Claim

**Type:** Combat. **Where:** Room 01 as built (x 5200–6800, cave, `Cave_Camp`).

The excavated remains of baggage handling. The carousel still turns. It has been
turning the whole time.

**The enemies are interns.** Straight from the tone bible's rat-swarm twist: they
are not monsters, they are staff, and they respawn because HR insists. Mechanically
they are the existing `BP_DCCEnemy_Courier` — low level, fast, weak — in a group of
three to five.

**This is where the allies are introduced.** CJ and Scott are already placed. The
fist-bump quest fires here, and it is deliberately taught *during* a fight rather
than before one, because a greeting that costs you a turn is a decision rather than
a formality.

**Twist:** one intern is not hostile. It is on a break. It will not fight, cannot be
attacked, and if approached will complain about the others at length. It is the
first hint that the dungeon has an internal politics.

**Build:** already built. Additions: the carousel (a `Modular_Sewer_Kit` circular
piece, rotating), plus a marker for the fist-bump objective.

---

## Room 3 — Service Shaft

**Type:** Puzzle. **Where:** the east wall of Room 01, already climbable.

The only way on is up. There is no door.

**Everything physical the party has is tested here** and nothing else is: climbing
costs stamina, stamina is Constitution, and sprinting to the wall first is a real
mistake because it shares the pool. A low-CON crawler will not make the ledge on
one attempt, and that is the intended lesson rather than a difficulty spike — the
ledge is low enough to retry from the floor.

**Twist:** the AI provides hints, and the hints are the tone bible's Puzzle posture —
technically correct and actively unhelpful. *"Statistically, most successful climbs
involve upward motion."* Hint quality scales inversely with AI Favor, so a party the
crowd likes gets worse help.

**Build:** the climb point and ledge exist. Additions: a second climb route with a
rest ledge partway, so a Constitution 2 crawler has a path that a Constitution 6 one
can skip. A `ADCCQuestMarker` tagged `EastLedge` is already specified.

---

## Room 4 — Concourse B

**Type:** Combat. **Where:** Room 02 as built (y ≈ 5000, `Ice_Station`).

Airport plant rooms, lit like a hospital. The first fight with **range** in it.

This is where skills matter. CJ casts Fire Fingers here as an ally, which is the
demonstration — the party watch a spell work before they are asked to use one. A
`BP_DCCEnemy_Analyst` at the back does the same thing to them, so the lesson lands
twice, the second time painfully.

**Twist:** the departure board on the far wall is a spawn timer. A boarding group
flashing red means a wave is coming, and it flashes several seconds before anything
appears. The party will not notice on the first wave. They will on the second, and
from then on the board is the most important object in the room.

**Build:** exists. Additions: the board (a `Ice_Station` hologram panel), and
linking the `DCCRoom` to its `DCCEncounter` — currently unlinked, which is why it
logs *"Combat room with no Encounter assigned"* every PIE start.

---

## Room 5 — The Dispensary Kiosk

**Type:** Safe Room. **Where:** new, off Concourse B.

Brightly lit, calm, hold music, and unmistakably sponsored. Per the tone bible,
safety is a *product* and the AI wants you to know it is paying for it.

**Mechanically the first breathing space:** stamina and health regenerate, no
encounter can arm here, and `AllowsAsyncLogin()` already returns true for
`SafeRoom` — so this is the room a sixth player would drop into mid-session. That
is not built yet, but the room is where it goes, and placing it now means the
plumbing has a home.

**The budtender** is an NPC who gives genuinely useful hints, eventually, if you are
patient. He is the tonal opposite of the System AI: slow, sincere, unhurried, and
completely unbothered by the dungeon. He is the first evidence that not everything
down here is a performance.

**Build:** small. `Ice_Station` interior kit, over-lit — and note the lighting units
gotcha, these are **Candelas**, default 8. One NPC, no encounter volume.

---

## Room 6 — Lost and Found

**Type:** Loot Vault. **Where:** new, between the Kiosk and the gate.

Shelves of unclaimed property. The AI reads the terms of service for every item
aloud, at speed, and does not stop when the party walks away.

**This is the Strength room.** The pickup mass tiers already exist and do the
teaching without a word of UI:

| Tier | Mass | Who can lift it |
|---|---|---|
| Light | 10 kg | anyone |
| Medium | 40 kg | STR 3+ |
| Heavy | 75 kg | STR 5+ |

Carry limit is `Strength score × 15kg`, so **Marcel (STR 6, 90kg) can pick up and
throw the crate that Kyle (STR 2, 30kg) physically cannot move.** The party will
discover this by trying, arguing about it, and then making Marcel do it. That
argument is the room working.

**Twist:** the heaviest item is the most valuable and there is no way to carry it
and fight at the same time. Somebody has to be the one who carries it, and they are
useless for the boss. Nobody is told this in advance.

**Build:** small. Shelving from `Ice_Station` crates, scattered `BP_Pickup_*` in the
three tiers.

---

## Room 7 — Gate B-33

**Type:** Combat (boss). **Where:** new, the floor's terminus.

A departure gate that has never departed.

**The boss is The Baggage Handler** — a `BP_DCCEnemy_Brute` variant with a job title
and a department. Per the tone bible, killing it triggers an automated out-of-office
reply, which the AI reads out in full, including the alternative contact.

**Tier and reward.** The books put a Floor Boss at tier 6, but this is a *tutorial*
floor and the party will be around level 2. Treat it as a **Neighborhood boss, tier
1–2**: one level's worth, not six. The floor-clear XP bonus already dwarfs the kill,
which is the correct shape — finishing the floor should beat farming the room.

**Mechanically it is a check on everything above:** it throws baggage (the carry and
throw system, used against the party for the first time), it summons interns when
below half (waves, learned in Concourse B), and the arena has a ledge that only a
climber reaches (Room 3, now optional but rewarded).

**Twist:** it does not want to fight. It is doing overtime. If the party can be made
to realise this there is a non-combat resolution — and per the AI-layer rules, the
System AI is contractually unable to tell them so, because a fight rates better.

---

## What this needs that does not exist yet

Ordered by what blocks the most:

1. **A boss-tier enemy Blueprint.** `BP_DCCEnemy_Brute` at a higher level is enough
   for a first pass; a real boss needs a summon behaviour and a throw.
2. **A non-hostile NPC that talks.** The budtender and the intern-on-a-break both
   need dialogue with no combat attached. Nothing in the codebase does this yet —
   `ADCCCharacterBase` assumes everything fights.
3. **Rooms 5, 6 and 7 as geometry.** All three are small and all three use packs
   already installed.
4. **The wave/spawn-timer hook** for the departure board. `ADCCEncounter` spawns once
   at present; waves need it to spawn in stages.
5. **Room-to-room connection.** Rooms 01 and 02 are still islands — the Approach
   walkway from the earlier plan is what joins them, and it is not built.

None of it is blocked on the ruleset, the stats layer, or anything already compiled.

---

## Quest chain for the floor

The existing `DT_DCCQuests.json` chain maps onto rooms 1–4 already. Extending it to
the full floor:

| Quest | Room | Objective |
|---|---|---|
| `Orientation` | 1 | Reach the marker |
| `MakeFriends` | 2 | Fist-bump an ally |
| `ClearTheRoom` | 2 | Clear the encounter, 3 kills |
| `GoUp` | 3 | Climb 10m, reach `EastLedge` |
| `Concourse` | 4 | Clear the encounter |
| `HeavyLifting` | 6 | Collect a heavy item |
| `FloorOne` | 7 | Clear every encounter |

`FloorOne` already exists and already pays the floor bonus, so the chain terminates
correctly with no new C++.
