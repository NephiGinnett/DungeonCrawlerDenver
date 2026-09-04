# Achievements — the loot-box tier list (Bronze → Celestial)

Content for the six-tier achievement→loot-box system the other session is building
mechanically. **This doc is content only** — no DataTable schema is assumed, since
that's the other session's call to make; every entry below is written so it can be
dropped into whatever shape that system ends up using.

## How this relates to the existing `Docs/DT_DCCAchievements.csv`

That table (20 rows, live, feeding `UDCCAchievementComponent`) is a different
*kind* of achievement, and this doc deliberately does not duplicate any of it:
it's **counters crossing a threshold** — kills, damage, containers looted, metres
climbed, fist bumps, skill ranks, levels, floors/rooms cleared, deaths — and it
pays flat XP/Favor the moment the threshold is crossed. That system stays exactly
as it is; nothing here replaces it.

The real Dungeon Crawler Carl achievement style the source material actually uses —
and what Zoe asked for — is different in kind: **specific, weird, one-off things
that happened**, not running totals. Entering a stairwell in the top ten. Being
first in with a cat. Wearing no pants. Inventing a stackable weapon. The reward is
a loot box, tiered to how hard or rare the trigger was, occasionally a permanent
**boon** instead of an item at the top end. That's what's below — 20 of them,
spread Bronze through Celestial, each tied to something that actually exists (or
is honestly flagged as not) in this game specifically, not generic fantasy
dressing.

**Every entry has a `Detection:` line.** Two states only:
- **EXISTING** — the signal this needs to fire on is already tracked somewhere in
  the codebase; wiring it up is (at most) one new `Record`-shaped call at an
  existing call site, no new system.
- **NEEDS NEW TRACKING** — nothing currently observes this condition at all; it
  needs a genuinely new hook, flagged with exactly what that hook would have to
  watch. Not a reason to cut the idea, just an honest cost estimate before anyone
  commits to building it.

Snarky System AI voice throughout, matching the existing table's Announcement
style — sponsor-aware, audience-aware, never actually on the crawler's side.

---

## Tier legend

| Tier | Rarity band | Reward shape |
|---|---|---|
| **Bronze** | Common — most parties trip over these without trying | Small gear, consumables, gold |
| **Silver** | Uncommon — needs a little luck or a little attention | Better gear, more gold |
| **Gold** | Rare — a real accomplishment, most parties miss it | Good gear, meaningful gold, occasional favor spike |
| **Platinum** | Very rare — needs the party to play *well*, not just survive | Strong gear, sometimes a boon |
| **Diamond** | Near-legendary — most sessions never see one of these trigger | Best gear tier, boon likely |
| **Celestial** | The ceiling. One or two per campaign, if that | Boon, almost always — the item pool at this tier is mostly symbolic |

---

## Bronze

### Still In Your Pajamas
*"The building fell down around you and you did not think to get dressed. Sponsors find this relatable."*

Reach the Queue with the Body equipment slot still empty — never equipped so much
as a blazer between waking up and arriving.

**Detection:** EXISTING. `UDCCEquipmentComponent` already knows whether the Body
slot is filled; check it once at Orientation start.

### Selfie At The End Of The World
*"Your building is rubble, the elevator is not an elevator, and your first instinct was a selfie. The audience respects the priorities."*

Take a Selfie Mode photo within the first five real minutes of arriving in the
dungeon.

**Detection:** EXISTING. `TakeSelfieScreenshot` already fires on a real event;
this only needs a timestamp compare against arrival time, no new tracking.

### House Pour
*"A drink has been purchased. Nobody asked what was in it. Nobody ever does."*

Buy your first drink from a vendor.

**Detection:** NEEDS NEW TRACKING, but cheaply — the purchase flow
(`Server_PurchaseDrink`) already exists and already runs server-side; this just
needs one `Record`-shaped call added at that existing call site. Also blocked in
practice on a vendor actually being placed in the level (see `DISPATCH.md`'s
still-open vendor-placement item) — the hook can be written today, but nobody can
trigger it until then.

### Didn't Even Flinch
*"Full exposure to a live radiation source and a dose reading of zero. Either extremely disciplined or extremely lucky. We are not paying out the difference."*

Fight (or pass through) a Nuclear Family encounter and take zero dose damage the
whole time — correct kiting from start to finish.

**Detection:** NEEDS NEW TRACKING. `UDCCRadiationAura` tracks live dose but
nothing currently records "this encounter's dose damage stayed at exactly zero"
as a completion flag.

---

## Silver

### Pause For Thought
*"The action has been paused. Somewhere, a producer is explaining to a sponsor why the fight stopped."*

Pause combat for the first time.

**Detection:** EXISTING. `ADCCGameState::OnCombatPauseChanged` already fires on
exactly this event — a one-line `Record` call on the rising edge.

### Untrained Genius
*"That skill has never been trained, ranked, or rehearsed, and it just cleared a Difficulty fifteen check anyway. We have no explanation and several unpaid interns looking for one."*

Succeed at an Untrained skill check (2d20 disadvantage, no Rank) against a
Difficulty of 15 or higher.

**Detection:** NEEDS NEW TRACKING. The ruleset resolves Untrained attempts
already, but nothing currently distinguishes "an Untrained roll succeeded against
a high DC" from any other resolved check — needs a flag added at the check
resolution site.

### Strongbox Escort
*"Two hundred and fifty gold, carried the entire length of a boss fight, by someone who could have just put it down. This is either loyalty or an inability to do math."*

Carry the Pantry's `ADCCPickup_StrongboxCrate` all the way through the
Voldemortgage fight without the carrier going down or dying.

**Detection:** NEEDS NEW TRACKING. Needs the boss encounter to check, at its own
resolution, whether the crate's carrier both (a) has been carrying it the whole
fight and (b) never went down — no existing hook ties carry-state to encounter
outcome.

### Pump The Brakes
*"Every debt collector on the floor, retired, before the big number finished counting up. Somewhere a briefcase is very confused."*

Clear every Debt Eater during Voldemortgage's Balloon Payment charge before the
three-turn telegraph completes, forcing it to fizzle into Minimum Payment.

**Detection:** NEEDS NEW TRACKING. `UDCCBossComponent` already knows whether the
charge fizzled or landed full-force; it just doesn't currently report that
outcome anywhere an achievement could listen for it.

---

## Gold

### Late Notice Survivor
*"The big number finished counting. You are still functionally a person. Legal is calling this a coincidence."*

Survive Voldemortgage's Balloon Payment landing at **full force** — the party let
the charge complete rather than clearing Debt Eaters first, and lived anyway.

**Detection:** NEEDS NEW TRACKING. Same missing hook as Pump The Brakes above,
opposite outcome — one new signal covers both.

### Beat The Collector
*"Retrieved with seconds to spare, moments before the janitorial shift. We will not be commenting on how close that was."*

Loot a party member's own corpse in the last few seconds before the body
collector would have arrived and swept it.

**Detection:** NEEDS NEW TRACKING. `CheckForBodyCollector`/`BodyCollectorTimer`
already exist and already know when a collector is about to spawn; this needs a
near-miss window check against `bLootHandled` flipping true right before that
timer fires.

### Five-For-Five
*"All five of you, through the same door, inside the same handful of seconds. Somewhere a continuity supervisor is thrilled."*

Get the whole party through the Collapse's exit door within a tight window of
each other — nobody stuck waiting for long.

**Detection:** EXISTING. `ReadyForCollapse` is already a replicated, timestamped
array; this only needs a spread check across its entries once it's full, not a
new tracking system.

### The Full Tour
*"Every room on this floor, physically entered, by the same crawler. We would call this thoroughness if we thought it was on purpose."*

Physically enter all seven of Floor 1's rooms in a single session.

**Detection:** NEEDS NEW TRACKING. `ADCCRoom`'s occupancy system (`AddOccupant`)
already fires per room entry and is the natural hook, but nothing currently
accumulates a per-character "rooms visited" set across a session — this is the
same replicated-room-tracking work behind `CurrentRoomDisplayName`, extended to
keep a running set instead of just the latest one.

---

## Platinum

### Gate Crasher
*"You stepped through a gate that should not exist, into a place this building's blueprints do not mention, and came back. HR has no form for this."*

Use the Oathgate to cross to its paired arena and survive whatever's waiting on
the other side.

**Detection:** NEEDS NEW TRACKING, and blocked on more than tracking —
`ADCCOathgate` has no placed instance and no "someone just used me" event at all
yet (see `DISPATCH.md`'s Oathgate-placement item). Both the placement and a
usage-fired event need to exist before this is triggerable.

### Lien Free
*"The debt transferred to you at his death has been paid off, in full, in gold, before the floor ended. Financially sound. Emotionally unclear why you bothered."*

Pay off Voldemortgage's post-death lien tax completely before the floor ends,
rather than letting it keep ticking into future fights.

**Detection:** NEEDS NEW TRACKING. The lien's gold-repayment mechanic exists but
doesn't currently report "fully paid off" as a distinct, listenable event.

### Zero Casualties
*"A cleared floor with nobody downed and nobody dead, start to finish. We checked twice. We are still checking."*

Clear an entire floor with no crawler ever going down or dying, from arrival to
the floor exit.

**Detection:** NEEDS NEW TRACKING. `bIsDowned`/`bHasDied` are both already
tracked per character, but nothing aggregates "never happened, for anyone, the
whole floor" into a single pass/fail flag.

---

## Diamond

### The Cliff Diver's Understudy
*"Rode the lift up. Did not use the platform. Went in anyway. The actual divers have been informed and are, apparently, unbothered."*

Take the cliff lift up to the top of the Falls, then jump off into the pool
below instead of using it for its actual purpose.

**Detection:** NEEDS NEW TRACKING, and depends on two things that don't exist
yet in combination: `ADCCCliffLift` (built, not yet placed) reaching the top, and
a fall/water-entry check afterward — no existing system currently connects "rode
the lift" to "then fell from height into the pool."

### Sponsor Of The Week
*"Peak audience approval, officially logged. Try not to let it go to your head. It will not last, and we will enjoy telling you so."*

Reach the maximum AI Favor tier at least once.

**Detection:** EXISTING. `UDCCFavorComponent` already tracks and replicates its
current tier; this only needs an edge-trigger the first time it hits the top of
`EDCCFavorTier`, not a new tracking system.

### No Notes
*"An entire boss encounter, every Skill Check, every party member, zero failures. Production considered this unbroadcastable. It has been broadcast anyway."*

Complete the Voldemortgage fight without a single failed Skill Check anywhere in
the party for the whole encounter.

**Detection:** NEEDS NEW TRACKING. Individual check resolution already exists;
nothing currently aggregates pass/fail across every party member for the
duration of one specific encounter.

---

## Celestial

### The Whole Building
*"Every crawler still standing, every crawler established, the floor's worst tenant evicted, and nobody went down once, all in the same sitting. We do not have a script for this. We are making one up."*

Finish a session where every surviving crawler is Level 5+, the floor boss is
dead, and nobody has been downed at any point — the floor's best possible
outcome, all at once.

**Detection:** NEEDS NEW TRACKING. Every individual piece is already tracked
(levels, boss death, downed state) — this is a composite check across all of
them at session end, not a new signal, but it is new aggregation logic.

**Reward:** boon, not an item — this tier is mostly symbolic by design (see tier
legend above).

### Off The Air
*"Last Call came, the lights came up, and everything still inside turned on you — including the things that weren't hostile five minutes ago. You are all still here. We genuinely didn't expect that."*

Trigger Last Call (the floor-wide "everything still in the building goes
hostile" timer) and survive it as a full, five-person party.

**Detection:** NEEDS NEW TRACKING, and this one is honest about starting from
zero — **Last Call itself does not exist yet.** It's still listed as a "Still
idea" in `Docs/enemy-roster.md`'s Denver layer section, not built in any form.
This achievement can't be wired up until that system is designed and built first;
flagging it here anyway because it's a strong Celestial-tier hook once Last Call
is real, not because it's buildable today.

**Reward:** boon, not an item, same reasoning as The Whole Building.

---

## Summary for whoever wires this in

**Wire up now, cheapest first (all EXISTING, no new tracking):**
Still In Your Pajamas, Selfie At The End Of The World, Pause For Thought,
Five-For-Five, Sponsor Of The Week.

**Needs one small new hook at an existing call site (NEW TRACKING, low effort):**
House Pour (blocked on vendor placement too), Untrained Genius, Pump The
Brakes / Late Notice Survivor (share one hook), Beat The Collector, The Full
Tour, Sponsor tier edge-trigger.

**Needs a real new system or a currently-unplaced feature first:**
Gate Crasher (Oathgate placement + usage event), The Cliff Diver's Understudy
(cliff-lift placement + fall/water check), Off The Air (Last Call doesn't exist
at all yet).

**Composite/aggregate checks, not new signals, but new logic:**
Zero Casualties, No Notes, The Whole Building.
