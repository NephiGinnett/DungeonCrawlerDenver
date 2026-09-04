# The Next Evolution — Challenges, Enemies, Questlines, Puzzles

**Status: draft v1, 2026-09-01.** Written at Zoe's ask ("plan out a few more steps of
progress... challenges, enemies, intriguing questlines, fun puzzles"). Living document —
edit in place, don't fork a dated correction file next to it.

Read `story-premise.md` first; everything here is in service of it.

---

## The honest diagnosis first

The base is genuinely good. Stats, skills, ranks, loot, equipment, quests, dialogue,
vendors, bosses with phases, pause-and-queue combat, climbing, carry/throw, AI Favor,
achievements, a population subsystem tracking four million crawlers. That is a lot of
working machinery.

**But every single interaction in the game right now is one of two verbs: walk up and
press E, or click to attack.** That is the actual gap — not content volume. A player
never has to *think* about anything except which enemy to click, and never feels the one
thing the premise is entirely built on: **being watched.**

So the next evolution is two new layers, not more of the same layer:

1. **The watched layer** — the audience becomes a present, reacting, mechanically real
   thing instead of a premise the docs assert.
2. **The thinking layer** — locks that aren't fights, enemies that demand a different
   verb than "click harder."

Everything below serves one of those two.

---

## The design rule this whole plan hangs on

> **Every lock has a key and a crowbar. The crowbar always works. The crowbar is always
> more expensive.**

No puzzle in this game may ever hard-block a party. A confused or impatient party can
always smash, bribe, or brute-force its way through — and doing so costs AI Favor, spawns
an alarm encounter, or destroys the loot inside.

This matters for three reasons:

- **It is the correct call for a five-player synchronous table.** Nothing kills a session
  like four people watching one person fail a riddle. There is always a way to move on.
- **It is thematically perfect.** "You may comply, or you may be non-compliant. Both are
  processed." The System AI does not care which you pick — it cares that it is watchable.
  A party that crowbars everything is a *different show*, not a failed one.
- **It converts puzzles into economy.** The question stops being "can we solve this" and
  becomes "is solving this worth the time," which is a far better question and one this
  project already has the currency for.

---

## Layer 1 — Make the audience *present* (the watched layer)

**Correction to this document's own first draft, made before anything was built:** the
first pass of this plan proposed building Viewer Chatter from scratch and called it the
largest gap in the project. **That was wrong, and checking the code rather than trusting
the docs is what caught it.** Chatter already exists and is well-built:

- `UI/DCCChatterTypes.h` — eight-bucket `EDCCChatterBucket` (Sentiment, Equipment, Boss
  Battle, Appearance, Emote, Alien Language, Moderated, Benign), a `FDCCChatterLineDef`
  DataTable row with `{Name}`/`{Item}`/`{Event}` token substitution, in-universe handles.
- `ADCCGameState::RollChatterBatch` + `ADCCCharacterBase::Server_RequestChatterBatch`.
- A full Chatter tab in the Tab menu (`ADCCHUD::DrawChatterTab`), with starter content in
  `Docs/DT_DCCChatterLines.csv`.

So the audience is **written**. What it is not, is **present.**

### The actual gap

Chatter today is **pull-based**: a batch rolls when a player opens the Chatter tab, by
deliberate design ("a burst of comments when you check the channel, not a constant trickle
nobody asked for" — Zoe's own framing, and correct as far as it goes).

The consequence is that **the audience only exists when you go looking for it.** A player
mid-fight, which is precisely when being watched should feel like something, sees nothing.
The premise's central sensation is behind a menu.

That framing should be kept, not overturned — a constant trickle really would be noise.
The fix is not "make it always on." It is to give the audience **two ways to intrude on
their own terms**, while leaving the tab exactly as it is for anyone who wants to browse.

### What to build

**1. A spectacle meter — `SpectacleScore` and `ViewerCount` on `ADCCGameState`.**

Decaying, server-authoritative, driven by *spectacle rather than narrative*: crits, kills,
deaths, long falls, long climbs, near-death survivals, friendly fire, throwing something
enormous, dying stupidly. Deliberately **not** the things AI Favor rewards.

This is the piece that matters most and it is genuinely absent. Per `story-premise.md`, the
eventual reveal is that Favor is the System AI's private scorecard rather than crowd
approval, and **the tell is the two meters diverging.** That divergence has to be
*structural* — falling out of the two being driven by different inputs — or it reads as
arbitrary when someone finally notices. Building the second meter now means the gap
accumulates quietly for dozens of hours before it is ever pointed at, which is exactly how
that reveal should land.

**2. Reaction bursts.** When SpectacleScore spikes hard, three or four Chatter lines
surface briefly over the existing Feed area, then fade. Not a permanent stream — a *room
reacting*. Reuses `RollChatterBatch` wholesale; the only new part is the surfacing.

**3. Viewer gifts.** When a spike clears a threshold, a viewer gifts the party a
consumable — dropped at the crawler who earned it, announced by the System AI in full
sponsor voice. Small, random, occasionally useless.

This is what stops the whole layer being flavour. It makes spectacle a *resource*, so
players start deliberately performing — the flashier option, the riskier jump, the stupider
throw — because it pays. That is DCC's actual thesis expressed as a mechanic instead of as
narration, and on top of the Chatter content that already exists it costs very little.

### Lesson worth keeping

This section was rewritten because reading `UI/` contradicted what this document had
confidently asserted one draft earlier. CLAUDE.md's own standing warning applies to the
design docs too, this one included: **verify against the code before building on any claim
here.** The docs describe intent, and intent drifts from the tree.

---

## Layer 2 — Puzzles (the thinking layer)

Four primitives, each reusing systems that already exist and are currently underused.
Each ships with its crowbar.

### 1. The Payroll Floor — pressure plates and mass

**Primitive:** `ADCCPressurePlate` (reads total mass of overlapping actors) →
`ADCCPuzzleDoor` (opens when its plates are all satisfied).

The carry system already sorts props into mass tiers and the Pantry already holds the full
set. This makes that authoring load-bearing instead of decorative.

**The puzzle:** three plates, a combined mass requirement no one crawler can meet alone.
Solvable by standing on them — but a body on a plate is a body not in the fight, and the
door only stays open while the plates stay weighted. Or drag props in from elsewhere. Or
put the **Strongbox Crate** on a plate, which means setting down the floor's single 250-gold
payday and trusting it will still be there.

That last option is why this puzzle is good: it reuses a tension the Pantry already
built, and asks the party to *spend* it.

**Crowbar:** break the door. It is loud, it is on camera, and the room's contents are
scattered and partly destroyed.

### 2. Load Balancing — the science lab breaker puzzle

**Primitive:** `ADCCBreakerSwitch` in a group, flipping one flips its neighbours.
Classic "Lights Out," themed as balancing power draw across a grid that is very obviously
not rated for this.

Natural home is the science lab, which now has the portal console — this becomes the thing
you do *before* the console will accept input, giving that room a second beat.

**Crowbar:** overload it. The portal comes up anyway, browns out the whole area, and
something unpleasant comes through with it.

### 3. The Compliance Door — bureaucracy as a lock

**Primitive:** `ADCCComplianceDoor` requiring N "stamps," each granted by a different NPC
or terminal on the floor.

The joke writes itself and is exactly on-voice: each stamp-giver has a requirement that
contradicts another's, the final submission is rejected on a technicality the first time,
and the System AI congratulates the party on their engagement with the process.

**This is the flagship puzzle of the whole plan** — it is the one that could not exist in
any other game. It is not a fantasy riddle wearing a corporate hat; the bureaucracy *is*
the mechanic.

**Crowbar:** the door is, structurally, a door. Break it and take the Favor hit for
non-compliance — the AI is audibly delighted, because unprofessional conduct rates well.

### 4. Escrow — deposit to pass

**Primitive:** `ADCCEscrowLock`. Surrender an item to open the way; reclaim it on room
completion. Fail or flee, and it is gone.

Cheap to build (the loot/container plumbing is all there), and it introduces the idea that
the dungeon takes collateral — which is the floor's entire theme stated as a mechanic.

---

## Layer 3 — Enemies that are verbs, not stat blocks

The roster is now well-populated but nearly monolithic in behaviour: everything closes to
melee and swings, with The Auditor as the sole ranged exception. **The next five enemies
should each demand a different action from the party**, not a different number.

| Enemy | The verb it demands | Reuses |
|---|---|---|
| **The Intern Pool** | *Stop killing the adds.* Endless weak spawns from a Manager that is quietly standing at the back. Kill the Manager or fight forever. | `SpawnChargeMinion` + the `RegisterSpawnedEnemy` fix from the reconciliation pass |
| **The Notary** | *Hit the weak point.* Immune to all damage until its Seal — a separate, fragile actor — is destroyed. | Boss-component phase gating |
| **Escalation Specialist** | *Stop casting.* Gains a permanent stacking buff every time any crawler uses a skill. Beatable with basic attacks and discipline. | Skill activation hooks |
| **Legacy System** | *Throw something.* Enormous, slow, immune to equipped weapons; only damaged by thrown objects. | Carry/throw — currently a toy, becomes essential |
| **Reconciliation** (mid-boss) | *Coordinate.* Two bodies, one HP pool; damaging one heals the other unless both are struck within a few seconds. | Damage routing; built for five players |

**Legacy System and Reconciliation are the two worth building first.** Each takes a system
that already exists and is barely used — throwing, and genuine party coordination — and
makes it the only way through. That is far better value than a sixth melee mob, and
Reconciliation in particular is the first enemy in this project that is actually designed
for a five-person table rather than tolerating one.

---

## Layer 4 — Questlines

Three tiers, deliberately different in scale.

### Tier 1 — Errands (the Bar, the Guildmaster)

Small, flavourful, each one teaching a mechanic the player will need later. These exist to
make the safe room worth returning to.

- **"Shrinkage"** — inventory has gone missing from the Pantry. Collect objective; teaches
  carry and mass tiers before the Payroll Floor demands them.
- **"Team Building"** — mandatory, per policy. Befriend objective; teaches the fist-bump
  system, which almost nobody discovers unprompted.
- **"Elevator Pitch"** — the lift attendant needs a passenger for a ride-quality survey.
  Teaches the lift, and is the natural first use of the new RideLift menu option.

### Tier 2 — The floor throughline: *The Denver Account*

The Guildmaster's quest, and **the thing Floor 1 is currently missing most**.

Voldemortgage is a well-built boss with no reason to exist beyond standing at the end. This
gives him one: the Guildmaster wants the floor's **ledger** — the record of what Denver
actually owes and to whom — recovered before the floor closes. Fragments of it are held
across the floor's rooms; assembling them is what leads to the Diving Pool, and to him.

The payoff is not the ledger's contents. It is that the ledger is **short**. The amount
Denver owes is trivial — a number a moderately successful crawler could clear in an
afternoon. It was never about the debt.

That reframes the entire floor retroactively, costs nothing to implement beyond a quest
chain and one piece of dialogue, and does not spend a single secret from the story doc.

### Tier 3 — The first crack: *Core Sample*

**One quest. Two lines of dialogue. Do not expand it.**

Someone in the Bar — a crawler who used to do geological survey work, and is handling the
apocalypse by continuing to do geological survey work — asks the party to bring back a rock
from the deepest point they reach. Trivial Collect objective. Small, slightly sad reward.

When they hand it over, the surveyor goes quiet, then says the sample is wrong. Not
dangerous, not cursed. Just from a depth that Floor 1 does not go to, and from strata that
should be several kilometres further down than anywhere the party has walked.

Then the quest closes and nothing happens.

This is exactly what `story-premise.md` asks for in the "early-mid floors" beat — the first
hint that "floor" and "depth" are not metaphors — and it plants the excavation-shaft reveal
without spending any of it. The party will either forget it entirely or never stop thinking
about it, and both outcomes are correct.

---

## How to build this without burning open/close cycles

The constraint that actually shapes sequencing: reflection-level C++ needs the editor
closed, editor content work needs it open. So the work sorts into two piles, and the pile
boundaries are what the schedule should follow — not the layer boundaries above.

### ✅ Built and deployed 2026-09-01 (same session this plan was written)

Written and Development-Editor-built with the editor closed throughout. **None of it has
been seen in PIE** — that needs Zoe at the editor.

| Shipped | Where |
|---|---|
| Chatter runs continuously while its tab is open (burst on entry, jittered trickle after, nothing while closed) | `DCCHUD.cpp`, `ADCCGameState::TryRollChatterBatch` |
| Running jokes — 7 gags that go active, crowd out chat for a while, then age out | `EDCCChatterBucket::RunningGag`, `FDCCChatterLineDef::GagId`, `ActiveGags` |
| Chatter content 38 → 140 lines | `Docs/DT_DCCChatterLines.csv` |
| `ADCCPuzzleDoor` + the crowbar rule (Favor cost, Strength gate) | `Puzzles/` |
| `ADCCPressurePlate` — reads real prop mass | `Puzzles/` |
| `ADCCComplianceDoor` — signatures, two-of-three | `Puzzles/` |
| `ADCCBreakerSwitch` — Lights Out panel | `Puzzles/` |
| `TargetedThrow` skill with range that grows by Rank (`RangePerRank`) | `DCCSkillTypes.h`, `GetEffectiveRange` |
| `ADCCEnemyCharacter_LegacySystem`, `_Reconciliation` | `Characters/DCCEnemyCharacter_Mine.h` |
| `ModifyIncomingDamage` hook on the attack path only | `DCCCharacterBase.h` |
| The mine questline + corp dispute + Core Sample, as data | `Docs/DT_DCC{Quests,Dialogue}_Floor01Mine.json` |

### ✅ Batch A completed 2026-09-02 — the rest of the list

Written and DebugGame-compile-checked. **Needs one deploy build; not yet in the editor.**

| Shipped | Where |
|---|---|
| `ADCCEscrowLock` — deposit a carried prop to pass; redeemed when the guarded room clears | `Puzzles/DCCEscrowLock.h` |
| `ADCCEnemyCharacter_InternManager` + `_Intern` — endless adds, kill the manager | `Characters/DCCEnemyCharacter_Corporate.h` |
| `ADCCEnemyCharacter_Notary` — immune until its `Seal` actor dies | same |
| `ADCCEnemyCharacter_EscalationSpecialist` — stacks damage on every skill cast nearby | same |
| Admin panel registration for all four (resolver **and** buttons) | `UI/DCCHUD.cpp` |

**That completes every puzzle primitive and every enemy on this plan.**

Three implementation notes worth keeping:

- **The escrow lock uses `UDCCCarryComponent::ReleaseCarried`**, not a normal drop — the prop is
  about to be hidden rather than left on the floor, so waking its physics first would be wasted
  work and a visible frame of falling. It also never destroys the deposit: a failed party has
  lost it in every way that matters, and a returning one should not find a hole where their
  crate was.
- **The Intern Manager registers spawns via a direct `Cast<ADCCEncounter>(GetOwner())`**, copying
  `UDCCBossComponent::GetOwningEncounter` — the encounter sets itself as spawn Owner, so no
  search is needed. Without registration the encounter can report itself clear with interns
  still standing, which is exactly the Balloon Payment bug from the 2026-08-22 pass.
- **The Escalation Specialist counts failed casts too.** The lesson is "stop reaching for
  skills", not "stop landing them" — a fizzle that escalated nothing would teach the wrong rule
  while still costing mana.

**One design change made mid-build, worth recording:** Legacy System was going to be
vulnerable *only* to thrown props, which would have meant new plumbing to distinguish throw
damage from everything else. Zoe's call was to add a **Targeted Throw skill** instead and skip
the extra system. That turned out to need almost no code at all — skill damage already goes
straight to `ApplyDamage` while attack damage goes through `PerformAttack`, so putting the
immunity hook on the attack path made "immune to being hit, vulnerable to anything thrown or
cast" fall out of two paths the project already had. The only new C++ was one range field.
Worth remembering as a pattern: **the cheaper feature was also the better-integrated one.**

### Batch A — C++, all in one closed-editor build

Write it all, compile-check with the DebugGame target while the editor stays open, then
deploy the whole batch in a single close.

1. `SpectacleScore`/`ViewerCount` on `ADCCGameState`, reaction bursts, viewer gifts —
   **extending** the existing Chatter system, not replacing it
2. `ADCCPressurePlate` / `ADCCPuzzleDoor`
3. `ADCCBreakerSwitch`
4. `ADCCComplianceDoor`
5. `ADCCEscrowLock`
6. `ADCCEnemyCharacter_LegacySystem`, `_Reconciliation`, `_InternPool` (+ Manager)
7. Admin panel registration for all of the above — resolver **and** visible buttons, since
   these are two separate lists and missing the second one makes the first untestable

### Batch B — MCP only, no build needed

All of this can happen with the editor open, in any order, across as many sessions as it
takes:

- `DT_DCCChatter`, `DT_DCCQuests` rows, `DT_DCCDialogue` rows for the Guildmaster, the
  surveyor, and the stamp-givers
- Placing the puzzle actors and authoring the Compliance Door's stamp chain
- Placing the new enemies into real encounters
- The towns/shops/farms build-out still outstanding from the earlier ask

### Suggested order

**The spectacle meter first, alone, before any of the puzzle work.** It is the smallest
piece of C++ on the list, it sits on top of a Chatter system that is already written, it
changes how every existing minute of the game feels, and unlike everything else here it
gets *better* the longer it runs before the reveal that depends on it. Everything else can
follow in one batch afterwards.

---

## Deliberately not decided here

- Whether Viewer Chatter ever becomes *interactive* (viewers voting on hazards, paying to
  interfere). Powerful, and a large enough design in its own right that it should not be
  smuggled in as a footnote.
- Whether the Compliance Door's stamp chain is floor-local or becomes a recurring
  structure across floors.
- Whether the surveyor from *Core Sample* survives Floor 1. Strong argument either way.
- Exact Favor costs for every crowbar. Needs play, not a table written in advance.
