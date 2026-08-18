# Populating the dungeon — the millions problem

**Written 2026-08-18.** The design question: the fiction has millions of crawlers,
the party will meet about fifteen, and both have to be true at once.

---

## The core move: scale is data, not actors

Carl never *meets* five million people. He knows they exist because the System tells
him — a number that drops, a leaderboard, a name that recurs in announcements.

That feeling costs nothing to produce. Spawning even two hundred crawlers would cost
everything, and would produce *less* of it, because two hundred is visibly not a
million and the illusion dies on contact.

**The trap to avoid: making the millionth crawler slightly real.** An actor pool, a
crowd sim, a "representative sample" — each buys the full cost of the population and
almost none of the drama, because the drama was always the number going down.

---

## Three tiers that never mix

| Tier | Count | What it is | Where it lives |
|---|---|---|---|
| **The Named** | 8–15 | Full `ADCCCharacterBase`. Stats, skills, memory, relationships, can be recruited or fought | `Characters/` classes |
| **The Crowd** | 20–60, occasional | Ambient bodies in safe rooms. No stats component, no AI, no combat, no nameplate | Not built yet |
| **The Population** | Millions | Pure numbers. Never rendered, never spawned | `UDCCPopulationSubsystem` ✅ built |

### Tier 1 — The Named

Author these. Do not generate them.

A procedural name with a random stat block is somebody nobody can care about. What
makes a rival land is that they have a **deal** — one sentence of premise that the
audience can hold. Prepotente is not memorable because of his numbers.

Budget: **around twelve**, total, across the whole game. Four to six party-side, four
to six rivals, a couple who switch. That is genuinely enough — the books run on about
that many.

Art is the constraint, not code. With four Meshy meshes plus mannequins, distinct
identity has to come from **scale, colour, name and behaviour** rather than unique
models — which is exactly what the Nuclear Family already does (0.72x Kid, 1.08x Dad,
same shared skeleton, three completely distinct threats).

### Tier 2 — The Crowd

Not built. When it is: a `ADCCCrowdCrawler` with a skeletal mesh, an idle loop, and
**nothing else** — no `UDCCStatsComponent`, no AI controller, no nameplate, no
health bar. They stand in the Bar, they do not fight, they cannot be talked to.

They exist so that a Safe Room feels like a place other people also survived to
reach. Twenty is plenty. Sixty if the room is big and they are instanced.

Rule: **the moment a crowd member needs a stat, promote it to Tier 1 or cut it.**
A half-simulated crowd is the worst of both.

### Tier 3 — The Population ✅

`Progression/DCCPopulationSubsystem.*` — compiled.

A `UGameInstanceSubsystem`, not a GameState component, because the death toll has to
survive travelling between floors. A floor is a level load; the body count is not.

- `CrawlersRemaining` — `int64`, starts at 4,182,900
- `PartyRank` — starts at 318,442. The party are nobody until they are somebody
- `ApplyAttritionFraction(0.18f, ...)` at each floor transition — fractional, because
  "300,000 died" stops meaning anything once 90,000 remain
- Rank improves **more slowly than the population falls**, so climbing is visible but
  never free
- Six authored `FDCCKnownCrawler` rivals — a name, a one-line reputation, a rank.
  Cited by the AI, and promotable to real characters later without contradiction

**`BuildPopulationAnnouncement()` returns empty most of the time, on purpose.** An AI
that reports the death toll every ninety seconds turns the most affecting number in
the game into wallpaper. Rarely, and always attached to something that just happened.

---

## What to build next, in order

1. **Hook attrition to the floor exit.** `ADCCFloorExit` already fires on floor
   completion — one call to `ApplyAttritionFraction` there and the number starts
   moving for the first time.
2. **Surface the count.** A line in the Tab menu's Party page, or an occasional feed
   entry. It is currently invisible.
3. **Tier 2 crowd actor**, once the Bar (Room 5) exists as geometry.
4. **Promote one rival to a real character** — The Appraiser is the natural first,
   because "follows parties at a polite distance and takes what they leave behind"
   is already a mechanic.
