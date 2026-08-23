# Races and Classes — when they happen, and what that means for us

Reference doc. The race list is canon vocabulary from the books; everything under
"What the rulebooks actually say" is quoted or paraphrased from the preview PDFs
(`Crawlers Preview`, `Rules for Survival`, `Game Masters Toolkit`) rather than from
the wiki, because the wiki does not carry the mechanics.

---

## The headline: race and class are NOT character creation

This is the thing that changes our roadmap, and it is easy to get wrong because
every other RPG does it the other way round.

> *"Oh, and you're human. That's your species, or, as the interface calls it, your
> race. You've always been human."* — Crawlers Preview

**Crawlers start as plain humans with no class.** Race and class are *selected on
the Third Floor*, as a mid-game event, and they are a big deal when they happen:

- Race and class benefits — including Passive Skills, which are otherwise only
  obtainable via potion, scroll or gear — begin **on the Third Floor**
  (Crawlers Preview).
- *"there are drawbacks to choosing certain races and classes — looking at you,
  Hobgoblins."* (Rules for Survival). So the selection is a real decision with
  real downside, not a stat-bonus picker.
- Stat increases also unlock at the Third Floor: *"Once you reach the Third Floor,
  you'll get to increase your Stats and more!"*

### Floors 1–2 are "Tutorial Floors"

Which is what our Floor 1 is, whether or not we were calling it that:

- Skill Rank **Upgrades** cap at the Rank 5 tier on Tutorial Floors. A crawler who
  grinds a skill to Rank 10+ still waits until Floor 3 for those benefits to
  activate (Crawlers Preview).
- Rank damage dice scale by Rank (Table 14): Rank 1 = +1, 2–3 = +1d2, 4–5 = +1d4,
  6–7 = +1d6, 8–9 = +1d8, 10–11 = +1d10. Higher tiers are Third Floor material.

**Implication for us:** Floors 1–2 are the introduction to the game, and they are
supposed to be. Nothing about identity — race, class, stat increases, Rank 10+
benefits — is available to the party yet, so what those floors teach is *systems*:
the HUD, the menus, inventory, what a Skill is, what the World Dungeon is. See
Game Guides below, which is the books' own answer to who does that teaching.

**The Earth Skills classbook is a placeholder and is not load-bearing.** Twelve
mundane-profession "classes" were written before the Third Floor rule was known;
they are shelved, not adopted. Revisit only once the real class list and its
benefits are in hand — at which point the question is whether any of that flavour
is worth folding into properly-built classes, not whether to ship it as-is. Do not
wire it into anything in the meantime.

---

## Game Guides — we have already half-built one

Also from the Game Masters Toolkit, and directly actionable:

- **Tutorial Guilds were installed on the First Floor**, each with a guildmaster.
  These are *"generally the first NPCs, apart from Mobs, that the players encounter."*
- Once the party enters a Tutorial Guild, that guildmaster becomes their **Game
  Guide for the first three floors**.
- Game Guides explain the HUD, the crawler menus, the Inventory system, what Skills
  do, and what the World Dungeon is. They answer basics; their hands are tied on
  big-picture questions.
- **They can be permanently killed and have no protections, so they stay in
  saferooms.**
- Around halfway through Floor 2, a Game Guide starts briefing the party on the
  Third Floor race-and-class selection. On Floor 3 they may become a *manager*,
  trading Dungeon Codex access for the freedom to say what they actually know.

**The Bar (Room 5) is already a SafeRoom with a bartender NPC in it who gives
genuinely useful hints, eventually, if you are patient.** That is a Game Guide with
a different job title. Promoting the bartender to the floor's Game Guide costs
almost nothing and makes the Bar mechanically load-bearing instead of just a rest
stop — and the "stays in saferooms because he can be permanently killed" rule
explains why he never leaves it.

---

## The race list (Third Floor content)

Player-selectable races, for when Floor 3 exists. Recorded as reference only —
nothing in the codebase implements races yet, and nothing should until the Floor 3
selection event is designed.

| | |
|---|---|
| **A** | Amazonian · Arachnid |
| **B** | Bune · Bush Elf |
| **C** | Caprid · Cat · Cat Girl/Cat Boy · Changbi Demon · Changeling · Chee · Crest · Crocodilian · Curupira |
| **D** | Dog Soldier · Doppelgänger · Dwarf (Classic, Fathom) |
| **E** | Elf (City, High, Night) |
| **F** | Fairy · Frost Maiden |
| **G** | Grulke |
| **H** | Half Elf · Half-Djinn · Hobgoblin · Human |
| **I** | Igneous · Incubus · Infiltrator · Intellect Hunter |
| **L** | Lajabless · Lutin |
| **N** | Naiad · Nodling |
| **O** | Obsidian Butterfly |
| **P** | Pocket Kuma · Porsuk · Primal · Pterolykos |
| **R** | Rat-kin (aka Rat Hooligan — same source lists both; treat as one entry until confirmed otherwise) |
| **S** | Sasquatch · Setonix · Skyfowl |
| **T** | Tetrakai · Tigran |
| **V** | Vampire |
| **Z** | Zebani |

**Reconciled 2026-08-23** against new screenshots of the same sourcebook: Dwarf and
Elf both have named subtypes we hadn't captured (Classic/Fathom; City/High/Night),
and Obsidian Butterfly and Tetrakai are new entries the earlier pass missed. The
screenshots cut off mid-alphabet, so this list is a merge, not a full replacement —
treat it as still-growing rather than closed.

Two notes worth keeping:

- **Human is on the list.** Staying human is a legitimate Third Floor choice, not a
  failure to choose — which is a good beat for a party who spent two floors being
  told their Earth job was worthless.
- **Changeling reassigns you.** Mordecai was originally Skyfowl and chose Changeling;
  the dungeon then *"assigns a new race, with the race always being that of a Mob on
  the current floor."* That is a mechanic with real narrative teeth if we ever want it.

---

## What is NOT in the previews

Flagged so nobody builds on a guess:

- Per-race stat modifiers, benefits and drawbacks. The Hobgoblin line implies they
  exist and are asymmetric; the numbers are in the full Core Rulebook, which we do
  not have.
- The mechanics of the Manager Benefit.

~~The class list and class benefits. Same book.~~ **Resolved 2026-08-23** — not from
the Core Rulebook, but from a different licensed sourcebook Zoe photographed
(*Anarchists' All the Races* or similar — a build-your-own Race/Class point-buy
supplement, not the base game's authored lists). See below.

---

## The Race & Class Build System (2026-08-23)

Straight from the sourcebook, and now the **canonical tool for every class we
generate ourselves** — not just custom ones. The old Earth Skills classbook
(twelve mundane-profession "classes," shelved above) is superseded by this: it
predates knowing this system existed, and everything it was trying to do belongs
here instead.

**The pitch, verbatim in spirit:** *"Didn't see an option you liked? Players can
simulate the thousands of options offered to their crawler by creating an entirely
new Race or Class."* **25 Race Build Points, 30 Class Build Points.** Spend them on
benefits below; optionally take Detriments for more points to spend. Leftover
points are lost, so budgets should land at or near zero.

**Alien vs. Earth Races** (from the sourcebook, worth keeping straight): Alien
Races get unique popularity-gain hooks — they've got planets of fans who want to
see their crawler represent them, but no access to Earth Classes. Earth Races get
a Silver Earth Box (a mostly-useless Earth Hobby Potion granting 3 Ranks in a
Skill) and access to Earth Classes — which is exactly the gap our own custom
career classes below are filling.

### Benefit tiers (cost each)

| Cost | Examples |
|---|---|
| **Minor (1)** | +1 to a Stat · Skill Advancement Check bonus for one Skill · allow a Skill to Rank 20 · double Stat Mod in a common circumstance · darkvision/light production · double Mana regen in a specific environment · see 2x as far · tier-1 crafting table · free saferoom room · store discount/sales bonus/interest (+1 each) · club membership · once-per-session non-combat benefit under specific conditions |
| **Moderate (2)** | +1 to a Skill or Spell (max 5 Passive Skill points per build) · heal 1 HP bar on damage/kill with a specific Skill (up to 5/combat) · advantage on Skill Advancement Checks · +1 DR (max +3) · guild-type access · unlimited double Mana regen · resistance to an uncommon damage type or to environmental damage · stat substitution in an ability · use one weapon type's Skills with another · healing spells heal +1 extra bar · terrain traversal without checks · burrow · breathe underwater · a natural attack scaling 1d6+StatMod (+1d6 at Rank 5/10/15) |
| **Major (3)** | significant assistance access · friendly Pet or Mount (+ buff it up to 4 pts) · alter an encounter once/day · negate one attack's damage once/day · buff the whole party once/day · limited flight · survive without breathing + toxin immunity · Rage (+1 melee dmg per lost HP bar) · Size 2 or less · +5ft Move · +5 Step · +1d4 to Evade/Stat Checks for one Stat · natural attack scaling 1d8+StatMod (+1d8 at Rank 5/10/15) · advantage on a non-combat Skill/Spell · x2 damage for one round, situational · swap CON for another Stat when computing HP · auto-climb (Climb Movement) |
| **Extreme (4)** | +1 to a Skill you couldn't normally learn · +1 to a thematic group of 3-5 Skills · advantage on all Stat Checks for one Stat · thematic group of up to 6 Skills to Rank 20 · immunity to Poison · resistance to a common damage type · double duration on Rank ≤5 Spells · advantage on a limited non-combat situation · an additional functional arm · Doppelgänger shapeshifting |
| **Epic (6)** | +1 to a thematic group of 6+ Skills · all Skills to Rank 20 · immunity to a common damage type, or halve the first 9 attacks/day · Limb Regeneration · advantage on a wide-ranging set of Skills · immunity to Poison and disease · advantage on Evade Checks · a powerful stat/skill-altering ability (Changeling-style) · Changeling shapeshifting · advantage on all Skill Checks for one Stat · unlimited flight |

*Thematically linked* = same Stat, same movement type, same weapon type, Passives
in general, or another grouping that makes sense. Same benefit twice never
double-stacks where that would be silly (two Resistances to the same damage type
just caps at half damage).

### Detriments (optional, max +5 extra points each for Race and Class — +10 total)

- **Stat detriments:** +1 Build Point per -2 total spread across one or more Stats.
- **Skill detriments:** +1 Build Point per -2 total spread across current Skills
  (never below 0, never exceeding current Rank) — or -1 to *every* Skill under one
  Stat for +1 Build Point per point of that penalty.
- **Minor (+1):** locked out of the Class via club membership · per -2 Stat/Skill
  detriment (see above) · per -1 to all Skills under a Stat · +1 Mana cost for
  off-theme Spells · disadvantage on a thematic Skill group under a condition ·
  must/cannot worship a deity · -1 to Advancement Checks for a Race/Class Skill at
  Rank 5+ · vulnerability (2x damage) to an uncommon damage type · halve a broad
  damage category not your Class's focus · a two-condition weakness · delay a
  bonus to Level 50 or Floor 6.
- **Moderate (+2):** a weakness with one broad condition · a Skill only Ranks up on
  floor descent · limited weapon choice · no Stat Mod on your Race/Class weapon
  Skill · +3 Mana cost to a Spell Class · disadvantage on a thematic Skill group ·
  banned from a wide item category.

---

## Custom classes we've built with this system (2026-08-23)

**Two design tracks, both feeding the same eventual Third Floor menu:** taking the
Race list above and *generating* a full class list by running real, book-flavoured
concepts through this point-buy tool — vs. **hand-authored custom classes**, which
is what this section is. Zoe's own two seeds:

- **Jumpscare Mage** — a CC (crowd control) caster, themed around internet
  creepypasta instead of traditional fantasy horror.
- **Lord of Dance** — locked into dancing any time you're in combat, in exchange
  for real mechanical upside.

Both stated below, plus a first batch of **10 career classes** — Zoe's ask for "a
mix of human/alien careers, competent to ridiculous," each costed with benefits
matched to the job and detriments matched to how dangerous or criminal it
naturally is. This is a first pass to gauge the results, not a locked list — retune
freely.

Convention used throughout: **bold** cost in parentheses, benefit tier noted, HP
"bar" language matches the segmented health system already in `DCCHUD`.

### Jumpscare Mage

*Concept:* A crowd-control caster whose whole kit is startling the enemy rather
than overpowering them — jump-scares, static, the feeling of being watched. Fits
the System AI's own "internet as a haunted house" register perfectly.

**Benefits (30 pts):**
- Natural attack, "Static Burst," 1d6 + INT Mod, scaling per Rank 5/10/15 **(Moderate, 2)**
- +1 to a thematic group of Fear/Startle-type Spells (4 Skills) **(Moderate, 2)**
- Advantage on Skill Advancement Checks for the same group **(Moderate, 2)**
- Once per combat, force one enemy to flinch and skip their next action ("A
  Presence Behind You") — costed as the Major "alter an encounter once/day" applied
  per-combat instead of per-day **(Major, 3)**
- Resistance to Psychic damage **(Moderate, 2)**
- +1 CHA **(Minor, 1)**
- +1 INT **(Minor, 1)**
- Darkvision **(Minor, 1)**
- Double Mana regen while in an unlit or "creepy" room **(Minor, 1)**
- Advantage on all Stat Checks for INT **(Extreme, 4)**
- All Fear/Startle Spells raisable to Rank 20 **(Extreme, 4)**
- Immunity to Fear-type effects (you invented the genre) **(Extreme, 4)**
- Store discount at any Spell-goods vendor **(Minor, 1)**
- +1 Skill Advancement Check bonus, "Lore: Creepypasta" **(Minor, 1)**
- Once-per-session: correctly predict a jump-scare-shaped encounter before it
  triggers **(Minor, 1)**

Subtotal: 30.

**Detriments (+5, funding a Skill/Stat mix, matching "unsettling but harmless"):**
- -2 STR, -2 DEX (spread Stat detriment) **(+2)**
- Disadvantage in all social situations that aren't about horror media **(+2,
  Moderate)**
- Cannot worship a deity — the algorithm is the closest thing they have **(+1,
  Minor)**

Total spent: 35 / 35.

### Lord of Dance

*Concept:* Locked into dancing the instant combat starts — the class **is** the
bit, and the mechanical upside has to be real enough that nobody picks it ironically
only to regret it.

**Benefits (30 pts):**
- +5ft Move **(Major, 3)**
- +5 Step **(Major, 3)**
- Auto-climb (Climb Movement) — "the floor is also a dance floor" **(Major, 3)**
- +1d4 to Evade Checks — you are extremely hard to hit while dancing **(Major, 3)**
- Once per day, buff the whole party's Move Speed for one encounter ("Everybody
  Dance Now") **(Major, 3)**
- Resistance to Bludgeoning damage — years of falling off tables **(Extreme, 4)**
- Advantage on all CHA Checks **(Extreme, 4)**
- +1 to a thematic group of Performance/Mobility Skills (4 Skills) **(Moderate, 2)**
- Double Mana regen while music is audibly playing **(Minor, 1)**
- Club membership, Dungeon Book of the Floor **(Minor, 1)**
- +1 DEX **(Minor, 1)**

Subtotal: 28.

**Detriments (+2, small — the drawback IS the concept, so mechanical
detriments stay light):**
- Must be dancing (constant minor movement) any time in combat — flavour-locked,
  no Build Point value on its own, per the rulebook's own note that free
  characteristics with no mechanical effect cost nothing
- -1 to all Skills under Constitution, representing exhaustion from never
  standing still **(+2, Minor: "-1 to all Skills under a single Stat")**

Total spent: 30 / 30.

### Career classes (10, Earth and alien, competent to ridiculous)

1. **Underwater Welder** *(Earth — Zoe's own example)*
   Benefits: +1 Swimming, +1 Diving, +1 Mechanics, +1 Crafting (four separate
   Minor Skill bonuses, 1 pt each — **4**); double breath-hold time **(Moderate,
   2)**; resistance to Cold damage **(Moderate, 2)**; +2 gold from any job/kill
   ("union rate") **(Minor, 1)**; breathe underwater **(Moderate, 2)**; burrow
   through soft/sandy terrain **(Moderate, 2)**; +1 CON **(Minor, 1)**; advantage
   on Mechanics Checks **(Extreme, 4)**; can raise Swimming/Diving to Rank 20
   **(Minor, 1)**; +1 to all Skills under Constitution for a duration <1hr, once
   per floor **(Moderate, 2)**. Subtotal 21.
   Detriments: vulnerability to Electric damage (2x) — an actual welding-adjacent
   hazard **(+1)**; -2 CON... no, keep CON as the whole point — instead, -2 DEX
   (bulky gear) **(+1)**; disadvantage on Skill Checks against anything
   fast-moving **(+2, Moderate)**. **Total spent: 25 / 25** (used only the +4
   detriment budget of the +5 max, on purpose — this job is dangerous but not
   *criminal*, so the detriment load stays lighter than the classes below).

2. **Storm Chaser** *(Earth)*
   Benefits: +1 Perception **(1)**; +1 Weather-reading (new Skill) **(1)**;
   resistance to Lightning **(2)**; resistance to Wind/Force-adjacent effects
   **(2)**; +5ft Move outdoors only, situational Major re-costed as Moderate for
   the restriction **(2)**; advantage on Perception Checks for incoming danger
   **(4, Extreme)**; +1 DEX **(1)**; see 2x as far **(1)**; a natural attack,
   "Ball Lightning," 1d6+DEX scaling **(2)**; double Mana regen outdoors in a
   storm **(1)**; +1 to a thematic group of Nature/Weather Skills, 4 Skills **(2)**.
   Subtotal 19.
   Detriments: vulnerability to Lightning (2x) — the irony is the point **(+1)**;
   disadvantage on all Skills while indoors, a broad condition **(+2, Moderate)**;
   -2 CHA (lives out of a truck, bad at people) **(+1)**; +1 Mana cost to
   non-Weather Spells **(+1)**. **Total spent: 24 / 25**, 1 point left on the
   table on purpose — chaotic packing.

3. **Crime Scene Cleaner** *(Earth)*
   Benefits: resistance to Poison **(2)**; resistance to Necrotic **(2)**;
   immunity to Fear from corpses/gore specifically **(1, Minor — narrow
   condition)**; +1 Perception, "knows where bodies are buried," literally **(1)**;
   advantage on Checks to find hidden loot on a corpse **(4, Extreme)**; +1
   Crafting (chemical/cleaning-adjacent) **(1)**; double Mana regen while alone
   **(1)**; +1 CON **(1)**; a tier-1 crafting table **(1)**; store discount at any
   Alchemy vendor **(1)**; +1 to Advancement Checks for Perception **(1)**; can
   raise Perception to Rank 20 **(1)**. Subtotal 17.
   Detriments: disadvantage in all social situations — people know what you do for
   a living **(+2, Moderate)**; vulnerability to Necrotic (2x) — overexposure
   **(+1)**; -1 to all Skills under Charisma **(+2, Minor category, "-1 to all
   Skills under a Stat" scaled)**. **Total spent: 22 / 25.**

4. **Bounty Hunter, Syndicate-Licensed** *(Alien)*
   Benefits: advantage on Intimidate Checks **(4, Extreme)**; +1 to a thematic
   group of Tracking/Combat Skills, 4 Skills **(2)**; a natural attack, "Mark and
   Collect," 1d8+STR scaling, only vs. a previously-Intimidated target **(3,
   Major)**; +1 DR **(2)**; +2 gold per bounty-flagged kill **(1)**; guild access,
   any Combat Training Guild **(2)**; +1 STR **(1)**; +1d4 to Evade Checks **(3,
   Major)**; club membership, Club Vanquisher **(1)**. Subtotal 19.
   Detriments: disadvantage in social situations with any authority figure — the
   Syndicate license is real, the reputation is worse **(+2, Moderate)**;
   vulnerability to being bounty-flagged themselves, reflavoured as vulnerability
   to Psychic damage (paranoia has a cost) **(+1)**; -2 CHA **(+1)**; prohibited
   from using Ranged weapons — license is melee-only **(+2, Moderate)**. **Total
   spent: 25 / 25.**

5. **Xenobiology Grad Student** *(Alien)*
   Benefits: +1 INT **(1)**; +1 to a thematic group of Intelligence-based Skills,
   4 Skills **(2)**; advantage on Checks to identify a creature's type/weaknesses
   **(4, Extreme)**; a tier-1 crafting table (field lab) **(1)**; double Mana
   regen while resting **(2, unlimited version)**; +1 Skill Advancement Check
   bonus, "Lore: Xenobiology" **(1)**; darkvision (night labs) **(1)**; can raise
   Lore: Xenobiology to Rank 20 **(1)**; a once-per-session non-combat benefit:
   correctly identify one enemy's damage type before engaging **(1)**. Subtotal 14.
   Detriments: -3 STR, -3 CON (never left the lab) **(+3)**; disadvantage on all
   Skills under Strength **(+2, Moderate re-flavoured as the broad-group
   version)**; can't afford good gear — treat as "no Stat Mod on your Class weapon
   Skill," since they were never issued a real one **(+2, Moderate)**. **Total
   spent: 21 / 25**, banked the rest — still saving up for a thesis defense.

6. **Rideshare Driver** *(Earth)*
   Benefits: a Pet/Mount — a beat-up sedan that counts as a Mount for movement
   purposes only **(3, Major)**; +5ft Move while the "Mount" is active **(3,
   Major)**; +1 Navigation (new Skill) **(1)**; advantage on Navigation Checks
   **(4, Extreme)**; +2 gold per encounter cleared ("5 stars, big tip") **(1)**;
   +1 CHA **(1)**; double Mana regen while the Mount is active **(1)**; store
   discount, any consumables vendor (snacks for passengers) **(1)**. Subtotal 15.
   Detriments: disadvantage on all Skills in combat without the Mount active — out
   of their element on foot **(+2, Moderate)**; no Stat Mod on any weapon Skill —
   never carried a weapon on shift **(+2, Moderate)**; -2 STR **(+1)**. **Total
   spent: 20 / 25.**

7. **Kua-Tin Noodle Stand Line Cook** *(Alien)*
   Benefits: resistance to Fire **(2)**; +1 Crafting (thematically linked to
   Cooking) **(1)**; healing Spells/items you craft heal +1 extra bar **(2)**; a
   tier-1 crafting table **(1)**; +1 CON **(1)**; double Mana regen near an open
   flame **(1)**; +2 gold from selling crafted food items **(1)**; can raise
   Crafting to Rank 20 **(1)**; +1 to Advancement Checks for Crafting **(1)**.
   Subtotal 11.
   Detriments: limited weapon choice — kitchen tools only (knives, ladles,
   cast-iron pans as improvised weapons) **(+2, Moderate)**; vulnerability to
   Fire (2x) — occupational hazard, ironically **(+1)**; -2 DEX **(+1)**;
   prohibited from Heavy weapons entirely **(+2, Moderate)**. **Total spent:
   17 / 25**, deliberately underspent — cheap and cheerful, most of the fun of
   this one is flavour, not power.

8. **Repo Man** *(Alien-adjacent, Denver's own — a Mile High Recovery Solutions
   franchise employee)*
   Benefits: advantage on Intimidate Checks **(4, Extreme)**; +2 gold from any
   kill or "repossession" **(1)**; a natural attack, "Lien Enforcement," 1d6+STR
   scaling **(2)**; +1 DR **(2)**; club membership, Club Desperado **(1)**; +1
   STR **(1)**; once per day, negate all damage from a single attack — "that's
   not covered under warranty" **(3, Major)**; resistance to damage dealt by
   environment/traps **(2)**. Subtotal 16.
   Detriments: disadvantage in all social situations — everyone hates a repo man,
   including other repo men **(+2, Moderate)**; vulnerability to being targeted
   first in any encounter involving debt-themed enemies (reflavoured as
   vulnerability to a common damage type, Bludgeoning) **(+1)**; -2 CHA **(+1)**;
   +1 Mana cost to any Healing Spell — nobody wants to help them either **(+1)**.
   **Total spent: 21 / 25.**

9. **Wildlife Rehabber / Zookeeper** *(Earth)*
   Benefits: a friendly Pet or Mount — an animal they rehabilitated **(3,
   Major)**; ability to buff that Pet/Mount up to 4 points **(3, Major)**;
   advantage on Checks involving Beast-type creatures **(4, Extreme)**; +1 Animal
   Handling (new Skill) **(1)**; can raise Animal Handling to Rank 20 **(1)**;
   double Mana regen while the Pet is nearby **(1)**; +1 CON **(1)**. Subtotal 14.
   Detriments: vulnerability to Piercing damage (2x) — old bite/scratch scars
   never fully healed **(+1)**; prohibited from loud weapons (Firearms) — spooks
   the Pet **(+2, Moderate)**; disadvantage on Skills under Charisma around
   people, advantage around animals nets out to a wash, so this is costed as the
   plain "-1 to all Skills under a Stat" instead **(+2, Minor scaled)**. **Total
   spent: 19 / 25.**

10. **Dentist** *(Earth — the ridiculous end of the spectrum, on purpose)*
    Benefits: advantage on Checks requiring fine motor precision (lockpicking,
    trap disarming, surgery-adjacent) **(4, Extreme)**; resistance to Poison
    (numbing agents) **(2)**; a tier-1 crafting table (dental tools double as
    lockpicks) **(1)**; +1 Crafting **(1)**; +1 INT **(1)**; double Mana regen
    while a patient — i.e. an ally — is nearby and willing **(1)**; healing
    Spells you cast heal +1 extra bar, but only on willing targets who "sit
    still" **(2)**; store discount, any Alchemy vendor (numbing agents) **(1)**;
    +1 to Advancement Checks for Crafting **(1)**. Subtotal 14.
    Detriments: disadvantage in all social situations — universally, viscerally
    feared **(+2, Moderate)**; -3 STR, -2 DEX (soft hands, soft everything)
    **(+3)**; prohibited from Heavy weapons **(+2, Moderate)**. **Total spent:
    21 / 25.**

---

When we're ready to run the real book Race list through this same system (rather
than hand-authoring), the process is: pick a Race, decide Earth vs. Alien, spend
its 25 points the same way, and name the drawbacks to match its flavour — same
tables, same math, just applied to Amazonian/Arachnid/etc. instead of a career.
Nothing here commits us to doing every Race that way; it is simply now possible.

---

## What it takes to actually build these (2026-08-23, corrected same day)

Checked against the current code, not guessed — and one of the original findings
below turned out to be **wrong**, caught while actually starting the build rather
than left to rot in the doc. Worth recording the correction in the open rather
than quietly editing it away: **`ResolveSkillCheck` (as opposed to the plainer
`ResolveCheck`) already existed** with a Skill Rank parameter and Disadvantage
(2d20-take-lower for untrained attempts) fully implemented — `DCCSkillComponent`
was already using it for every cast Skill. The real, narrower gap was that two
of this project's own social checks (Persuade, Intimidate) were calling the
plainer `ResolveCheck` instead, so Skill Rank and Disadvantage existed in the
engine but weren't reaching those two checks. **Fixed same session, see the TODO
tracker below.** The still-real gap is **there is no damage-type system at
all** — damage is a flat `int32`, so "resistance to Fire" has nothing to attach
to yet.

### Foundational primitives (build once, unlock most of the list)

1. ~~Skill Rank + a real non-combat Skill Check.~~ **Done 2026-08-23** — turned
   out to mostly already exist (`ResolveSkillCheck`, `FDCCKnownSkill`'s
   `{ SkillId, Rank, bAdvancementMark }`, `MarkSkillUsed`). What was missing:
   Persuade/Intimidate weren't routed through it, and there was no Skill
   Archetype for a pure non-combat check with no activation. Added
   `EDCCSkillArchetype::Check` for exactly that, rewired `ResolveSocialCheck` to
   pull the caster's real Rank and mark/record the attempt, and added a public
   `RecordUntrainedAttempt` so checks resolved outside `DCCSkillComponent` (a
   social check today; a future trap or lock) can still make an untrained
   attempt count toward being learned. New Skill rows staged in
   `Docs/DT_DCCSkills_ChecksBatch1.csv` (Intimidate, Persuade, Perception,
   Navigation, Weather-Reading, Animal Handling, Lore: Creepypasta, Lore:
   Xenobiology, Mechanics, Swimming, Diving) — queued for import alongside the
   Chatter table next time MCP is available.
2. ~~Advantage/Disadvantage as a `ResolveCheck` parameter.~~ **Done 2026-08-23**
   — `ResolveSkillCheck` now takes `bAdvantage` alongside the existing
   `bUntrained`, and the two cancel out to a single ordinary roll when both are
   true, same as they would on paper. Nothing yet *grants* Advantage from a
   Class benefit — that's what #3 is for — but the mechanism is real and
   build-verified.
3. **A damage-type + resistance/vulnerability/immunity system.** The bigger
   lift. Needs an `EDCCDamageType` enum, every damage source (weapons, Skills,
   environment) tagged with one, and a resistance/vulnerability/immunity lookup
   the Stats component consults on `ApplyDamage`. Don't build the full taxonomy
   at once — the twelve classes above only actually use Fire, Cold, Electric,
   Poison, Necrotic, Psychic, Bludgeoning, and Piercing. Start with those eight.
4. **A Class Benefit component — the real piece this has all been leading to.**
   Something has to actually *apply* a selected Class's benefit list at runtime:
   stat bonuses, Skill bonuses, resistances, movement changes, once-per-X
   triggers. Shape-wise this is `UDCCEquipmentComponent`'s pattern one level up —
   a DataTable row per Class holding a list of typed benefit structs, folded
   into the character the same way Equipment already folds gear bonuses into the
   Enhanced stat layer. This is also, not incidentally, the actual Third Floor
   race/class selection system the design doc has been waiting on — worth
   treating as its own project phase rather than something bolted on reactively.
   **Requirement, added 2026-08-23 per Zoe:** picking a Class must also grant a
   themed starter equipment kit through the existing Equipment system — a
   Dentist who never once sees a dental tool isn't fun, regardless of how
   correct their Build Point math is. Every class below gets a short starter-kit
   list as part of this component's data, authored as ordinary `DT_DCCEquipment`
   rows (mostly Trinket-slot props, sometimes a thematic MainHand item that
   doubles as the class's signature improvised weapon) and auto-equipped the
   moment the Class is selected. See "Starter kits" below for the per-class list.
5. **Pets/Mounts.** Only two of twelve classes need it (Rideshare Driver,
   Wildlife Rehabber), so lower priority. Rideshare Driver's "movement-only
   sedan" is the cheap version; a real combat pet is a bigger build — do the
   cheap version first if only one is needed soon.
6. **New movement modes: burrow, breathe underwater, flight.** Each is a custom
   `CharacterMovementComponent` mode — genuinely new engineering, no existing
   hook. **"+5 Step" doesn't map onto anything current** — this game runs
   real-time-with-pause, not a turn/step economy, so either reinterpret Step as
   something else (bonus dash distance?) or shelve it until/unless a turn-based
   mode ever exists.

### Already cheap — good news, existing hooks to reuse

- **Natural attacks** (Static Burst, Ball Lightning, Mark and Collect, Lien
  Enforcement) — these can likely just *be* `DCCSkillComponent` Damage-archetype
  rows with near-zero mana cost and a short cooldown. The Rank-based damage-dice
  scaling table (Rank 1 = +1, 2–3 = +1d2, etc.) already exists in the ruleset
  notes. This is a DataTable row, not new code.
- **Flat Stat bonuses** (+1 STR, +1 INT, etc.) — the Unenhanced stat layer
  already takes writes from potions and level-ups; a Class benefit is the same
  write from a different source.
- **Move Speed bonuses** — `ADCCCharacterBase::MovementSpeedOverride` already
  exists and is already read by movement prediction on every machine.
- **Doubled Mana regen under a condition** — Mana regen already ticks on the
  Stats component; this is a conditional multiplier on an existing tick, not a
  new system.
- **Gold bonuses on kill** — `AwardKillExperience` and the loot-roll functions
  are the existing hook points.
- **Club memberships** (Club Desperado, Club Vanquisher, Dungeon Book of the
  Floor) — pure tags for now. No mechanical system needs to exist before these
  can be recorded on a character; they only need to *do* something once a
  system reads them, which can come later.

### Blocked on systems that don't exist yet at all

- **Store discounts, sales bonus, interest earned** — there is no shop or
  vendor of any kind in the codebase yet. These benefits are unbuildable in
  isolation; they're a line item for whenever an economy/vendor system gets
  built, not something to chase per-class now.
- **Once-per-day triggers** ("negate one attack, once per day," "buff the party
  once per day") — there's no day/rest cycle tracked anywhere. Suggest
  reinterpreting these as **once per floor** or **once per encounter** instead —
  both already have real boundaries in the code (`ADCCRoom`/`ADCCEncounter`),
  so the benefit becomes buildable today instead of waiting on a system nobody
  has designed yet.

### New Skill IDs the twelve classes need authored

**Done 2026-08-23** — staged in `Docs/DT_DCCSkills_ChecksBatch1.csv`, queued for
import: Intimidate, Persuade, Perception, Navigation, Weather-Reading, Animal
Handling, Lore: Creepypasta, Lore: Xenobiology, Mechanics, Swimming, Diving.
`Intimidate` and `Persuade` are now also actually wired into
`ResolveSocialCheck` — Rank and Advantage both do something, not just exist.

### Starter kits (part of primitive #4, specified now so it's never lost)

One themed prop set per class, mostly Trinket-slot, occasionally a MainHand
item that doubles as the class's signature weapon (and, not coincidentally,
usually explains its "limited weapon choice" detriment). Not yet authored as
`DT_DCCEquipment` rows — most need real meshes sourced or reused from an
installed Fab pack the same way `ADCCPickup_Jukebox` reuses a kitchen
appliance; flagged `[needs mesh]` where nothing obvious exists yet.

| Class | Starter kit |
|---|---|
| Underwater Welder | Welding Torch (MainHand) · Diving Mask (Head) · Wetsuit (Body) `[needs mesh]` |
| Storm Chaser | Lightning-Rod Staff (MainHand) `[needs mesh]` · Anemometer (Trinket) `[needs mesh]` |
| Crime Scene Cleaner | Industrial Mop (MainHand) · Hazmat Suit (Body) `[needs mesh]` · Bio-hazard Kit (Trinket) |
| Bounty Hunter | Restraint Cuffs (Trinket) `[needs mesh]` · Syndicate Badge (Trinket) |
| Xenobiology Grad Student | Field Microscope (Trinket) `[needs mesh]` · Lab Coat (Body) `[needs mesh]` |
| Rideshare Driver | Phone Mount (Trinket) `[needs mesh]` — the sedan itself is the Pet/Mount grant, not equipment |
| Kua-Tin Noodle Stand Cook | Cast-Iron Pan (MainHand) · Apron (Body) `[needs mesh]` |
| Repo Man | Tow Chain (MainHand) `[needs mesh]` · Clipboard (Trinket) `[needs mesh]` |
| Wildlife Rehabber | Pet Carrier (Trinket) `[needs mesh]` — separate from the Pet/Mount grant itself |
| Dentist | Dental Tool Kit (MainHand) `[needs mesh]` · Numbing Kit (Trinket) |
| Jumpscare Mage | Creepy Mask (Head) `[needs mesh]` · Flickering Lantern (OffHand) |
| Lord of Dance | Boombox (Trinket) `[needs mesh]` — also the literal source of "music audibly playing" for its own Mana-regen benefit |

### Suggested build order

1. ~~Skill Rank + Check~~ **Done 2026-08-23**
2. ~~Advantage/Disadvantage~~ **Done 2026-08-23**
3. The Class Benefit component, including starter-kit grants (the actual Third
   Floor unlock — biggest remaining piece)
4. Damage types, scoped to the eight actually in use above
5. Natural attacks via existing Skill archetype rows — no dependency, buildable
   in parallel with any of the above, any time
6. Pets/Mounts — only blocks 2 of 12 classes
7. New movement modes — lowest priority, fewest classes depend on them

---

## Notes / TODO — engineering build tracker

Living checklist. Update in place as each piece lands; don't create a dated
"progress report" file next to this one.

- [x] **Skill Rank + non-combat Skill Check** — 2026-08-23. Turned out to
  mostly already exist (`ResolveSkillCheck`); fixed the real gap
  (Persuade/Intimidate weren't using it) and added `EDCCSkillArchetype::Check`.
  Build-verified, editor closed, real link. Not yet seen in PIE.
- [x] **Advantage/Disadvantage** — 2026-08-23. `ResolveSkillCheck` takes
  `bAdvantage` now, cancels correctly against `bUntrained`. Nothing grants it
  from a benefit yet — that's #3.
- [ ] **New Skill rows imported** — `Docs/DT_DCCSkills_ChecksBatch1.csv` written,
  not yet imported (needs MCP).
- [ ] **The Class Benefit component** — not started. The big one: applies a
  selected Class's full benefit list at runtime, including starter equipment
  kits (see above).
- [ ] **Damage types + resistance/vulnerability/immunity** — not started.
  Scoped to Fire/Cold/Electric/Poison/Necrotic/Psychic/Bludgeoning/Piercing.
- [ ] **Pets/Mounts** — not started. Rideshare Driver's movement-only sedan is
  the cheap version to build first.
- [ ] **New movement modes** (burrow, breathe underwater, flight) — not
  started. "+5 Step" still needs a decision — reinterpret or shelve.
- [ ] **Starter-kit equipment rows** — designed (table above), not yet
  authored as `DT_DCCEquipment` CSV rows or imported.
