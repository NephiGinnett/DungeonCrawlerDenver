# Room-purpose audit — Floor 1

**Written 2026-08-26**, at Zoe's request: an honest room-by-room pass against a specific test, not a
general design review. Not fixed here on purpose - this is the list of where the gaps actually are, so
Zoe can decide what to do about them.

## The test

For each room: does it have a clear **player purpose** (combat, loot, puzzle, story beat), a clear
**NPC/economy purpose** (why this room or character makes sense from the in-world survivors'/AI's
economic logic - not just as level-design filler), or neither?

The economic logic that has to be satisfied, per `Docs/story-premise.md` and `Docs/dungeon-tone-bible.md`:
Borant runs Dungeon Crawler World as two businesses at once - entertainment (the show must go on) and
extraction (the floors are real excavation, the repossession premise is Denver's dramatization of a real
planet-wide default) - wearing a "reclaim the planet" cover story. Five-plus million people were pulled
into this suddenly and did not consent to the terms they're now living under (`story-premise.md`'s "the
Collapse"). A room passes the test when it reads as something that logic would actually produce - a
business still "operating" under repossession, staff who cannot leave, a show that continues regardless -
rather than a combat arena or loot pinata that happens to have cave rocks around it.

**Scope.** Only `Docs/floor-01-design.md` has room-by-room content - it's the only room-level design doc
that exists. Deliberately excluded, not overlooked:

- **The Oathgate** (`Rooms/DCCOathgate.h`) - explicitly a DM testing arena, not a place in the fiction.
  Applying a "why does this room exist economically" test to a dev tool would be a category error.
- **The procedural dungeon generator** (`Rooms/DCCDungeonGenerator`) - confirmed dormant per `CLAUDE.md`,
  contradicts the hand-authored floor premise, and Zoe's own call is to leave it alone for now. Generated
  rooms have no fictional justification by construction; auditing them isn't meaningful until/unless the
  generator is turned on.
- **`enemy-roster.md`'s unplaced content** (the Nuclear Family, Altitude Sickness, The HOA, Last Call, The
  Brunch Rush, and the whole "Raw Ideas" tier) - built or pitched, but not assigned to a room yet. Can't
  audit a room assignment that doesn't exist. Flagged at the bottom so this doesn't silently drop them.

---

## Room by room

### 1 — The Queue · Narrative

**Player purpose: clear.** Orientation beat, establishes the System AI's voice, delivers the first
Voldemortgage clue (the stapled foreclosure notice) without comment.

**NPC/economy purpose: clear, and the absence of an NPC IS the joke, not a gap.** "There is no host. The
queue is very long and the restaurant is empty" is already the bit - a business performing the ritual of
being open (a queue, a podium, a sign) with nobody left to actually serve. Adding a host here would break
it, not complete it. No note needed - this room passes on both counts, and the second count is satisfied
by a deliberate NPC vacuum.

### 2 — The Grotto · Combat

**Player purpose: clear.** First real fight, ally recruitment (the fist-bump quest fires mid-combat on
purpose), teaches the room's own twist (a legally-untouchable break).

**NPC/economy purpose: clear, and this is the floor's strongest room on this test.** Enemies are staff,
not monsters - reskinned, respawning "because the schedule says so," a direct restatement of "the staff
are still working, they cannot leave" from the floor's own thesis. The server on break is a labor-law joke
with real teeth (per the tone bible's rule 3, bureaucracy is the horror) and is also the first grievance in
the escalation ladder (see cross-cutting note below). Nothing to flag.

### 3 — The Falls · Puzzle

**Player purpose: clear.** The floor's whole physical-skills test - climbing, stamina/Constitution,
Puzzle-posture hints that get worse the more popular you are.

**NPC/economy purpose: not stated - flagged.** Nothing here explains why this specific stretch of wet
rock is the load-bearing path it's treated as. "No staircase - the divers do not use one" implies this is
the performers' own access route to the platforms above, which WOULD be a real economic/production reason
(infrastructure the show depends on) - but the room's own text never says so. As written, a player
experiences this as a level-design obstacle course dropped next to a waterfall, not as "the private access
route the divers use because building a proper staircase was never in anyone's budget." One line would
close this - something in the AI's patter or a diver-adjacent detail (a rope, a sign, a bucket) tying the
climb explicitly to Room 7's performers rather than leaving the connection to be inferred.

### 4 — The Kitchens · Combat

**Player purpose: clear.** First ranged fight, first Debt Eaters, the ticket-rail spawn-telegraph.

**NPC/economy purpose: clear.** "They came through the tills first. Not the safe. The tills. Petty." -
Debt Eaters characterized by what they bothered to steal, which is both a good joke and a precise piece of
economic logic (collectors doing an actual audit, not a monster raid). The kitchen itself is a real
restaurant kitchen still nominally "operating," which fits the show-must-go-on logic directly. The ticket
rail is the best single mechanic on the floor for satisfying player purpose and world logic
simultaneously - a real kitchen object repurposed as a threat-telegraph, not a UI element bolted onto a
generic room. Nothing to flag.

### 5 — The Bar · Safe Room

**Player purpose: clear.** Regen, the async-login point, hint delivery.

**NPC/economy purpose: clear.** "Safety is a *product* and the AI wants you to know it's paying for it" is
the sponsor-economy logic stated outright, matching the tone bible's own sponsor-tier rules directly. The
bartender is the escalation ladder's payoff (the only NPC who has actually met Voldemortgage) and the
tonal counterweight to the System AI - unhurried, sincere, unbothered, per the room's own text. Nothing to
flag.

### 6 — The Pantry · Loot Vault

**Player purpose: clear, and strong.** The Strength/carry-limit teaching room - the mass tiers, the
Strongbox twist, "the argument is the room working."

**NPC/economy purpose: thin - flagged.** "Shelves of unclaimed property and food nobody ordered" gestures
at bureaucratic horror (matches tone bible rule 3) but never says *whose* property this is or why it
accumulated here. Whose stuff is this? Confiscated from crawlers who didn't make it? Overflow from
evicted tenants during the repossession itself? Genuine restaurant dry storage that got reclassified as
collateral? Any of these would work and would cost one sentence - right now the room reads as "a loot
vault reskinned with cave-restaurant dressing" rather than "a specific bureaucratic artifact of this
particular repossession." No NPC lives or works here at all, which is fine if the room's own logic is
explicit, but currently isn't. Worth the same one-line treatment as Room 3.

### 7 — The Diving Pool · Boss

**Player purpose: clear, and it's the floor's climax.** The full Voldemortgage fight, the pause-loop's
best showcase per the room's own text.

**NPC/economy purpose: clear, and explicitly the best-articulated line in the whole document.** "The
divers are still diving. Unpaid, during the boss fight, in the background. Nobody acknowledges it. That
image... is the floor's thesis in one shot." This is the model every other room's economy-logic gap should
be measured against - it's not implied, it's stated as the point. The Head Diver's own dialogue closes the
escalation ladder at resignation ("I have gone at twenty past for longer than you have been alive"),
and killing him instead of the boss costing AI Favor ("he was the entertainment") is a precise piece of
the sponsor-economy logic applied to a single NPC's life. Nothing to flag.

---

## Cross-cutting observations

**The grievance escalation ladder works exactly as designed, verified against the actual dialogue data,
not just the design doc's claim.** Checked `Docs/DT_DCCDialogue.json` directly: Tier 1 (Grotto,
irritation, "he" unnamed) → Tier 2 (Kitchens, the name-drop delivered as an annoyance rather than a
reveal, first real fear) → Tier 3 (Bar, the only NPC who's met him, widens the world, the genuine hint) →
Tier 4 (Diving Pool, resignation). This spans exactly the four rooms that also pass the NPC-purpose test
above (2, 4, 5, 7) - **the three rooms flagged as thin (1, 3, 6) are also the three rooms with no
grievance dialogue at all.** That's not a coincidence worth ignoring: the escalation ladder is doing double
duty as this floor's actual mechanism for giving a room in-world purpose, and the rooms missing from it are
the same rooms missing an economic reason to exist. Extending the ladder (or something like it) into Rooms
3 and 6 specifically would likely close both gaps at once. Room 1 doesn't need this - see above, its
NPC-lessness is the joke on purpose.

**Worth reinforcing rather than fixing:** Room 2's enemies being distinct waitstaff bodies (not the
players' own crawler bodies) matters more than a cosmetic complaint once you apply this audit's own
lens - "5 million anxious, recently-abducted humans" reads very differently if the mobs visibly look like
scared trapped restaurant staff versus looking like palette-swapped versions of the party's own allies.
This is the same asset gap already tracked in `Docs/asset-needs-meshy-pipeline.md` (Waitstaff/Collections/
Line Cook/Server On Break all still placeholder), not a new finding - just worth flagging that fixing it
serves this exact audit's premise, not only the earlier "stop reusing my allies' bodies" complaint.

**Not yet assignable, flagged rather than skipped silently:** `Docs/enemy-roster.md`'s Nuclear Family
(built, unplaced), Altitude Sickness/The HOA/Last Call/The Brunch Rush (pitched, unbuilt), and the whole
"Raw Ideas" tier all lack a room assignment on Floor 1, so this audit's test can't be run against them yet.
Worth applying the same test at design time when any of them gets placed, rather than after - The HOA in
particular ("violations for things you did several rooms ago... annihilation costs AI Favor because the
audience finds it gauche") is already written with a strong economy-logic hook and would likely pass
immediately; Altitude Sickness's "it's genuinely sorry, it just lives here and you do not" line is also
already doing real economic-purpose work as written.

---

## Summary for Zoe

Two real gaps, both cheap to close, both already have the missing logic implied nearby:

1. **Room 3 (The Falls)** - tie the climb explicitly to the divers' own access route.
2. **Room 6 (The Pantry)** - one sentence on whose "unclaimed property" this actually is.

Everything else on the floor passes the test as written, and Room 7's diver line is the standard the rest
of the floor is already mostly living up to.
