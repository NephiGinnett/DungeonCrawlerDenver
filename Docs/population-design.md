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

### Tier 2 — The Crowd ✅ BUILT 2026-08-18

`Progression/DCCCrowdCrawler.*` and `Progression/DCCCrowdSpawner.*` — compiled.

**`ADCCCrowdCrawler` is an `AActor`, not a character.** No stats component, no
skill component, no AI controller, no health bar, no combat, no interaction, and
`bCanEverTick = false`. It cannot be fought, talked to, looted or killed, and it
never appears in any of the systems that iterate `ADCCCharacterBase` — which is
most of them. That restraint is the whole point.

**Variety from six meshes.** There will not soon be sixty, so identity is
manufactured per-instance from a seed:

| Varied | Range | Why |
|---|---|---|
| Mesh | 4 shared-skeleton bodies | A room of identical bodies reads worse than an empty one |
| Height / build | 0.88-1.12, varied separately | Two crowd members sharing a mesh still differ in silhouette |
| Facing | full 360° | A crowd all pointing one way reads as a chorus line |
| Idle clip | 4 clips | — |
| **Animation phase and rate** | 0.85-1.15x, random start | **The big one.** Same loop in unison makes twenty people read as one object |
| Colour tint | ±0.22 via dynamic material | Different clothing under the same bad lighting |
| Name | `Dale the Punctual`, `Priya of Accounts Payable` | Generated, because a crowd is texture — only Tier 1 is authored |

**The seed is replicated**, so every machine builds the same crowd rather than
five different ones. Hand-placed members seed from their own location, so a bar's
regulars are the same people every session instead of re-rolling each launch.

**`ADCCCrowdSpawner`** fills a box so a room is populated by placing one actor
instead of twenty. Placement is rejection-sampled against `MinSpacing` rather
than uniform random — random points cluster, and three people standing inside
each other beside a bare patch reads as broken, not busy. Attempts are bounded,
so an undersized box places what fits and stops rather than hanging the load.

Twenty reads as a busy bar; sixty as a concourse. This is the first number to
turn down if a room gets heavy.

**The rule still stands:** the moment a crowd member needs a stat, promote it to
Tier 1 or cut it.

### Tier 2 — original note

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

1. ~~**Hook attrition to the floor exit.**~~ ✅ **Done 2026-08-18.** `ADCCFloorExit`
   applies `FloorAttritionFraction` (0.18) once, when the first crawler leaves, and
   logs a population line. 18% of four million is roughly 750,000 people — the sort
   of number the System AI reports without changing its tone at all. Fires only at
   the floor boundary, because a number that drips down continuously becomes
   wallpaper.
2. **Surface the count.** A line in the Tab menu's Party page, or an occasional feed
   entry. It is currently invisible.
3. **Tier 2 crowd actor**, once the Bar (Room 5) exists as geometry.
4. **Promote one rival to a real character** — The Appraiser is the natural first,
   because "follows parties at a polite distance and takes what they leave behind"
   is already a mechanic.
