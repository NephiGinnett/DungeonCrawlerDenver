# 3D asset needs — for the Meshy pipeline session

**Written 2026-08-24**, compiled by reviewing `CLAUDE.md`, `Docs/DISPATCH.md`, `Docs/floor-01-design.md`,
`Docs/enemy-roster.md`, `Docs/DT_DCCEnemies.json`, `Docs/population-design.md`, `Docs/dungeon-tone-bible.md`,
`Docs/DT_DCCEquipment.csv`, `Docs/DT_DCCDialogue.json`, and the actual C++ mesh references in
`Source/DungeonCrawlerDenver/Characters/`. Zoe is heads-down at work today — this is my best read of what's
needed, not something she reviewed line by line. Treat priorities as a recommendation, not a mandate.

**The rule this list applies throughout, per Zoe's own steer:** mundane/generic → free-source (an existing
Fab pack, or a free asset site). Distinctive/absurdist/hard-to-source → worth a paid Meshy generation. A
plain gun is free. A gun shaped like a radiator that fires steam is Meshy.

---

## 0. Finish what's already paid for before generating more

**A real risk right now: re-generating something that already exists.** `Content/Characters/` has a
backlog of already-generated, already-paid-for Meshy output sitting unconverted since Aug 16–21. Triage
this FIRST — it's free (the generation cost is sunk), and skips duplicate spend:

| Raw file | Best guess at purpose | Status |
|---|---|---|
| `Meshy_AI_Buff_Minion_biped/` | The Bar guard (confirmed — this session built `ADCCEnemyCharacter_Guard` around it) | **Imported, has a real bug**: shares a corrupted material with Don Minion (see `DISPATCH.md`). Needs re-import, not regeneration. |
| `Meshy_AI_Don_Minion_biped/` | The bartender (confirmed by Zoe) | Same shared-material bug. Needs re-import, not regeneration. |
| `voldemortgage 2.glb` | Lord Voldemortgage, the Floor 1 boss | **Done** — imported as `SK_Voldemortgage2`, already wired into `ADCCEnemyCharacter_Voldemortgage`. |
| `Meshy_AI_Gaunt_undead_lich_ro_biped/` | An earlier Voldemortgage attempt (matches his "gaunt undead lich" description almost word for word) | Likely superseded by the two Voldemortgage imports above — **confirm with Zoe before doing anything with it**, don't just import it as a second boss. |
| `debt collector.zip` | Contains `Meshy_AI_Low_level_wraith_hum_biped` — the Debt Eater's own flavor text is "low-level wraith" | Likely an earlier/duplicate Debt Eater generation, already superseded by the imported `SK_DebtEater`. **Confirm before importing** — don't create a second Debt Eater body by accident. |
| `Scott Lacrosse.glb` | A lacrosse-stick prop — matches "the buff minion guard's lacrosse stick" from Zoe's own bug report | Not yet converted/imported. This is very likely the missing weapon prop, not a new generation need. |
| `League Minion.glb`, `League Minion Blue.glb` | Small humanoid "minion" creatures, two color variants | Purpose unclear — possibly crowd/trash-mob candidates. Not yet converted or imported. |
| `Minion eyes.glb` | A small prop, likely an accessory for the League Minions above | Unclear. Not yet converted or imported. |
| `Dune Popcorn.glb` | A snack/prop, purpose unclear | Not yet converted or imported. |
| `waterbottle.glb` | A water bottle prop | Could serve `TapWater` in the new `Docs/DT_DCCConsumables.json`, or general set dressing. Not yet converted or imported. |
| `Meshy_AI_Pokemon_Master_Traine_...glb` | A "Pokemon Master Trainer" character | **Cleared by Zoe 2026-08-24 — non-commercial, friends-only project, no concern.** Import and use normally, same as anything else in the backlog. (Original trademark flag withdrawn; noted here only so nobody re-raises it.) |

**Action for the Meshy session: sort this pile out (convert, import, or discard/re-scope with Zoe) before spending budget on anything below.**

---

## 1. NPCs — new bodies needed (the actual generation list)

### High priority — distinctive, clearly Meshy-worthy

| NPC | Count | Why Meshy, not free | Notes |
|---|---|---|---|
| **The Nuclear Family** (Dad, Mom, Kid) | 3 | A glowing, translucent, faintly-luminous 1950s promotional-family with visible internal "circuitry" veins — not sourceable off the shelf | Full physical description already written in `Docs/enemy-roster.md` under "The Nuclear Family." Currently placeholder — all three still render as `SK_OfficeAirplane_Shared`/`SK_ArmsOut_Shared`/`SK_DiceWinged_Shared` (i.e., Scott/Kyle/Marcel's own bodies). The doc itself says "still needed: real models" — this is the single most-flagged gap in the design docs. |
| **The Head Diver** | 1 | A showman cliff-diver character with real personality (mini-boss, non-hostile until attacked) — generic humanoid packs won't read as "performer" | Stat block exists in `DT_DCCEnemies.json`, dialogue exists in `DT_DCCDialogue.json` (`SpeakerId: HeadDiver`). No model or placement yet at all. **No physical description was ever written for him** (unlike Voldemortgage and the Nuclear Family, who both got a full model-generation writeup) — see the summary sent to Zoe separately. Concept/role are solid; the look needs her input or a judgment call before generating. |

### Medium priority — generic humans, but worth checking the backlog first

These are all currently rendering as one of the four player-crawler bodies (per Zoe's original "NPCs are wearing my allies' bodies" complaint), but the *characters themselves* are ordinary humans — a waiter, a line cook, an office-worker debt collector. **Free-source these unless the raw-asset triage in §0 already covers one** (the wraith-named zip is NOT this — that one's the Debt Eater).

| NPC | Count | Role | Source |
|---|---|---|---|
| Cantina Waitstaff | 1 body (spawned in groups of 3–5) | Grotto trash mob — "not a monster, an employee" | Free — any generic rigged waiter/waitress humanoid |
| Collections Associate | 1 body | Kitchens' ranged enemy — office-worker debt collector with a clipboard | Free — any generic rigged business-casual humanoid |
| Line Cook | 1 | Dialogue NPC, Kitchens (`SpeakerId: LineCook`, lines already written) | Free — generic humanoid, kitchen/chef coded |
| Server On Break | 1 | Dialogue NPC, Grotto — "first non-hostile NPC," refuses to fight (`SpeakerId: ServerOnBreak`) | Free — generic humanoid, restaurant-server coded |

### Reinstated 2026-08-24 — the Oathgate arena roster

**Zoe wants full models for these after all, overriding the "no model needed" note below.** Corrected here
rather than just deleted, since the override is worth understanding:

- **`ADCCEnemyCharacter_FinePrint`** — "Fast, weak, numerous. The swarm half of the gate's pool," reskinned onto `SK_ArmsOut_Shared`.
- **`ADCCEnemyCharacter_Arbiter`** — "The gate's tougher pull... the one that makes a party stop and focus fire," reskinned onto `SK_DiceWinged_Shared`.

**Where the "no model needed" claim actually came from:** it's not a design decision Zoe made — it's a
code comment written by whoever built the class, in
`Source/DungeonCrawlerDenver/Characters/DCCEnemyCharacter_Oathgate.h` (lines 7–19, the file-level banner
above both class declarations):

> "The gate is a magically-binding arbitration clause dressed up as a doorway... Both reskin onto an
> already-shared crawler mesh via `DCCCrawlerAnimSet`, the same no-new-art pattern DebtEater/TestBoss/the
> allies all use - **no unique model exists for either, and none is needed for a testing-arena roster.**"

That's an engineering scope call from an earlier session (Oathgate was built 2026-08-23, before this asset
pass), reasoning that a DM-testing tool didn't need dedicated art — not something Zoe reviewed or asked
for. She doesn't recognize where it came from and wants it overridden, so: **add FinePrint and Arbiter to
the generation list.** "Paperwork given legs" (the class's own flavor line) is actually a fun, distinctive
brief — bureaucratic-document-themed humanoids, in the same spirit as Voldemortgage's foreclosure-notice
motif. Worth generating rather than free-sourcing, on the same "distinctive/absurdist" logic as everything
else in §1's high-priority table.

### Low priority / explicitly optional

- **`TestBoss`** — an internal dev/test actor (`ADCCEnemyCharacter_TestBoss`, currently on CJ's `SK_HockeyWings`). Not player-facing content. Skip.
- **Ambient Crowd tier** (`ADCCCrowdCrawler`) — `Docs/population-design.md` deliberately reuses the four existing shared-skeleton bodies for cost reasons ("art is the constraint, not code... distinct identity comes from scale, colour, name and behaviour rather than unique models"). This is a considered design decision, not a placeholder bug. More free-sourced generic bodies would add cheap variety here if the Meshy session has spare time, but it's not a gap to fix.
- **Sir Loin of Beef, The Karengeist, Audit Trail, Meathead the Ogre, The Real Estate Wyrm, Cthul-Loo** (`enemy-roster.md`'s "Raw Ideas") — pitch-stage only, not fight-planned, no stat blocks, nothing in code references them. Genuinely "might need" rather than "needed" — I'd hold off generating any of these until one gets promoted to a real fight, same as Voldemortgage was.

---

## 2. Weapons & props

### Free-source (mundane objects, absurd names/flavor only)

`Docs/DT_DCCEquipment.csv` has three items with no real mesh, already identified and researched in a prior
session — genuinely just ordinary object shapes, not worth Meshy spend:

| Item | Current state | What it needs |
|---|---|---|
| `ConfettiCrossbow` | No mesh at all | A plain crossbow model |
| `NailGunPro` | Reuses an unrelated tool mesh | A plain nail gun / power tool model |
| `FlareGunEmergency` | Reuses an unrelated lamp mesh | A plain flare gun / signal pistol model |

Two free packs were already scouted for exactly this and just need Zoe to download them (Fab/itch.io are
account-gated, so only she can pull them in): **Free Fantasy Weapon Sample Pack** (Fab, Prop Garden LLC,
permanently free, 12 weapons including bow-shaped ones) and **ASIS Free Gun Weapons Pack** (itch.io, FBX +
textures, direct import — no Blender conversion needed). If the Meshy session has other free-pack sources
handy, either works; these are just the two already confirmed to exist and fit.

**Not a modeling gap, don't generate these:** `GreaseApron`, `ManagersBlazer`, `HighVisibilitySafetyVest`
(the three Body-slot apparel items) intentionally have no mesh. Per `CLAUDE.md`'s own finding, the four
crawler skeletons have no shared topology, so a fitted piece of clothing can't be skinned to work across
all of them — the intended fix is a material/texture change on the existing body mesh, not new geometry.
Generating "an apron" would not solve this.

### Already covered

`ThrownBottle`, `TenderiserMallet`, `ChefsKnife`, `CastIronSkillet`, `FireAxe`, `WallSword`, `ServiceTray`,
`FeatureWallShield`, `PatioSunHat`, `Torch`, `ServiceKnifeSet`, `SodaGun`, `ComplaintFolderOverstuffed` all
already have real meshes from installed packs. No action needed.

### Worth checking before generating anything new

Tavern/bar props (mugs, bottles, glasses, kegs) for the bartender scene are **already sitting installed
and unused** — `Legendary_Forge/Meshes/Mug/Sm_mug*` (4 variants), `Modern_villa_kit/Meshes/Bottle/
SM_bottle_01/02/03` + `SM_glass_01/02`, `Cave_Camp/Meshes/SM_bottle`. Already wired into the five starter
drinks in `Docs/DT_DCCConsumables.json`. No new props needed here.

---

## 3. Environment / set-dressing — the wide-map / skyline ask

**Mostly already solved without Meshy.** Four installed asset packs ship their own distant-building
dressing and, in two cases, a full sky dome:

- `Synthwave_city` and `Megastructure_Scifi_World` — each has its own `Sky_system` (a skybox/backdrop, not
  just props) plus a pre-built demo `Level`.
- `Sci_Fi_city` and `Dam_city` — pre-built demo `Maps`, standalone building meshes.

Pulling one pack's distant skyline into the DCC level (visual-only, no collision, players never reach it)
is far cheaper than generating or hand-placing unique buildings, and reads as "big city" immediately. **This
is an MCP/placement task, not a Meshy generation task** — flagging it here mainly so the Meshy session
doesn't spend budget re-solving something already covered by packs Zoe already owns.

**One genuine Meshy candidate, if Zoe wants it:** the tone bible names **"The Blue Horse"** — a real Denver
landmark (the giant blue mustang statue with glowing red eyes outside the airport, a genuine local urban
legend) — as a planned future-floor boss/landmark. A recognizable "Blucifer"-style statue silhouette visible
in the distance would be a strong, specific, funny bit of foreshadowing for a Denver-flavored skyline, and
it's exactly the kind of distinctive/local joke that a generic city pack can't provide. **Speculative,
lower priority** — no floor built around it yet, purely a nice-to-have if there's spare Meshy budget.

No other Floor 1 environment gaps exist — `floor-01-design.md`'s own asset table (cave, restaurant, market,
waterfall, industrial kitchen, VFX) is fully covered by installed packs already in use.

---

## Addendum — triage results from the Meshy-pipeline session (2026-08-24)

Ran the §0 triage from the Image-and-Model-Generation Meshy session. No MCP/Unreal-editor access from
that session, so this is verification-by-file-inspection only (disk contents + `Source/` + `DISPATCH.md`'s
own log) — no imports, conversions, or DataTable edits performed. Corrections to the table above:

- **Buff/Don Minion**: already re-imported into their own subfolders and the shared-material bug already
  fixed, per `DISPATCH.md`'s own 2026-08-24 entry (same day this addendum was written). The "needs
  re-import" line above is stale. Nothing to do.
- **Voldemortgage**: confirmed two meshes exist — `SK_Voldemortgage` (Aug 20, first import, has its own
  walk/run/hitreact anims) and `SK_Voldemortgage2` (Aug 21, regenerated, no anims, different rig). Live
  boss code (`DCCEnemyCharacter_Voldemortgage.cpp:23`) uses `SK_Voldemortgage2`. Matches the table above.
- **`Meshy_AI_Gaunt_undead_lich_ro_biped`**: file-dated Aug 16, four days before the first `SK_Voldemortgage`
  import (Aug 20) — the only Voldemortgage-shaped raw source on disk. Circumstantial but strong case this
  is that import's already-consumed source file. Not provable without checking import metadata in-editor.
- **`debt collector.zip`**: `SK_DebtEater` already exists on disk, fully textured, with its own physics
  asset, already wired into the boss fight's minion-summon logic, and got a ragdoll fix this same session.
  Same superseded-duplicate pattern as Voldemortgage.
- **CORRECTION — `Scott Lacrosse.glb`, `Minion eyes.glb`, `Dune Popcorn.glb`, `waterbottle.glb`**: this
  table says "not yet converted or imported" for all four. **That's stale.** All four are already imported
  as static meshes in `Content/DCC/Equipment/Props/` (`SM_ScottLacrosse`, `SM_MinionEyes`, `SM_DunePopcorn`,
  `SM_WaterBottle`), and `DISPATCH.md` records a peer session already adding live `DT_DCCEquipment` rows for
  them (`ScottLacrosseStick`, `MinionEyes`, `DunePopcornBucket`, `WaterBottle`) via MCP. Not a gap.
- **🚩 URGENT — `Meshy_AI_Pokemon_Master_Traine_...glb`**: also already imported, as
  `SM_PokemonMasterTrainer.uasset` in that same Props folder (file-timestamped the same day as the batch
  above). Per `DISPATCH.md`, `PokemonMasterTrainer` was already added as a **live, wired** `DT_DCCEquipment`
  row. This is not a "don't import this later" flag anymore — the asset may already be reachable in-game as
  lootable/equippable content. Needs Zoe to check the live table directly and decide, soon — higher priority
  than "flag for later" as originally written above.
  **Resolved 2026-08-24: Zoe confirmed this is cleared — non-commercial, friends-only game, no trademark
  concern. Use/import normally, no rename/reskin needed.**
- **New flag, same category**: `League Minion.glb` / `League Minion Blue.glb` are confirmed genuinely
  unimported (checked both `Content/Characters/` and the Props folder). Separately from "purpose unclear"
  above — **"League Minion" reads as a League of Legends minion-creep reference (Riot Games IP)**, the same
  trademark-risk category as Pokémon Master Trainer. Same "flag for Zoe, don't use as-is" treatment before
  anyone imports or converts it.

## 4. Reuse candidates from a sibling project — Smilestory (`C:\Smilestory`)

**New 2026-08-24, per Zoe.** She has a second project, Smilestory (Godot, not Unity as first described —
worth knowing if that comes up), with its own Meshy-generated asset library sitting mostly unused. She
wants the Meshy-style assets reused here (never the low-poly Blender-MCP ones — wrong art style for DCD).
No Blender-MCP-generated assets were found anywhere in the project as of this check, so that exclusion
turned out to be moot, but the method used to tell them apart is recorded below in case that changes.

**How Meshy vs. free-pack was told apart** (Smilestory has no dedicated "Blender MCP" folder — everything
lives together by content category, e.g. `biotic/`, `abiotic/`, `bugs/`): `CREDITS.txt` explicitly lists
which assets are Meshy AI (CC0) versus Kenney/Quaternius/Poly Pizza (also free, differently licensed) —
but it's dated Jul 1 and stops short of everything added after. For the rest, file signature is a reliable
tell: every confirmed Meshy asset in both this project and Smilestory ships as a large `.glb` (10–60MB)
alongside 3–4 companion texture images named either `_0/_1/_2/_3.jpg` or `..._Baked_BaseColor/Emit/
MetallicRoughness/normal` — Meshy's own export convention, not something a hand-authored or Kenney/
Quaternius/Poly-Pizza asset does (those run a few KB to low MB, no numbered texture siblings). Applied
that signature across `godot/assets/models_3d/` and `godot/assets/models_3d/unused/`.

### Confirmed Meshy-generated, unused in Smilestory, worth checking against DCD's gaps

All in `C:\Smilestory\godot\assets\models_3d\biotic\` unless noted. None of these are wired into anything
live in Smilestory (checked `resources/species/` — none of the fantasy-monster ones appear there, only the
nature-critter set does), so nothing here is "taking" content Smilestory still needs.

| Smilestory asset | Best DCD fit | Notes |
|---|---|---|
| `goblin.glb` | A DCD-flavored humanoid grunt — could reskin/rename into almost any "staffed accordingly" mob | Matches Zoe's own "goblins" mention exactly. |
| `goblin_bomber.glb` | Same, a ranged/explosive variant | Second goblin variant, different role built in. |
| `goblin_warlord.glb` | A tougher pull / mini-boss reskin | Third goblin variant, visibly the "leader" of the set per the name. |
| `bonecrawler.glb` | **Likely what Zoe meant by "bone spiders."** | No asset named "bone spider" exists anywhere in Smilestory — this is the closest match by theme (bone/undead, crawling). Worth a quick look before committing; if it's not spider-shaped, say so and there may be no literal bone-spider asset to reuse. |
| `crystal_golem.glb` | A construct/elemental enemy | Distinctive, on-theme for a "repossessed magic item" gag. |
| `tomb_sentinel.glb` | An undead guardian — pairs thematically with Voldemortgage's whole foreclosure-lich motif | |
| `sand_wyrm.glb` | A large serpentine enemy | Desert-coded; less obviously DCD-flavored, but a strong generic "big monster" if one is wanted. |
| `scorpion.glb` | A giant fantasy scorpion (not the tiny nature-kit bug elsewhere in Smilestory) | |
| `gryphon.glb` | A flying mythical beast | Uses Meshy's `_Baked_BaseColor/Emit/MetallicRoughness/normal` texture convention rather than the numbered one — same source, different export settings. |
| `desert_nomad.glb`, `villager_male.glb`, `villager_female.glb` | Generic humans — could directly cover the "medium priority" NPC gaps in §1 (Waitstaff, Collections, Line Cook, Server On Break) instead of free-sourcing new ones | Worth checking these first before pulling in yet another external pack — same art pipeline as everything else, free to reuse, zero new generation cost. |
| `Meshy_AI_Cogwork_Alchemist_...glb` (in `unused/`) | A steampunk alchemist humanoid | Distinctive and on-tone for DCD's corporate-satire flavor — a strong Bar/Pantry NPC or vendor candidate if the bartender needs a second personality nearby. |
| `desert_tortoise.glb`, `fennec_fox.glb`, `ibex.glb`, `Antlion.glb`, `Firefly.glb`, `Scarab.glb`, `isopod.glb`, `luna moth.glb` | Fauna, weak DCD fit | Real, likely-Meshy assets, but nothing on Floor 1 calls for desert wildlife. Noting them for completeness, not recommending use. |
| `cillinan.glb` (also duplicated at `C:\Smilestory\Art\Cillinan.glb`) | Unknown — name isn't self-explanatory and no in-game reference found | Worth a look, but couldn't identify what it's meant to be from the project files alone. |
| `potionsmith_npc.glb`, `scientist_npc.glb` | Possible vendor/quest-giver bodies | Both explicitly Meshy per `CREDITS.txt`. Smaller files than the main monster batch, so likely simpler models — check quality before committing to a visible role like the bartender. |

**Not relevant / explicitly free-licensed, don't treat as Meshy:** `Spider.glb` (appears twice, in `bugs/`
and `forest_animals/` — small file, Poly Pizza CC-BY per `CREDITS.txt`'s Bug Pack credit, not Meshy — if
Zoe pictured a spider specifically, this is NOT it), `Grasshopper.glb`, `Salamander.glb`, `Dragonfly.glb`,
`Slug.glb`, `Eel.glb` (all Poly Pizza), everything under `kenney/`, `trees/`, `farmers/`, `foods/`, `Medieval
Village MegaKit[Standard]/` (Kenney/Quaternius CC0 packs). All genuinely free and reusable too, just not
what Zoe meant by "the Meshy-generated ones."

**How this changes §1:** if the villager/nomad bodies read well, the four "medium priority, free-source"
NPCs (Waitstaff, Collections Associate, Line Cook, Server On Break) may not need sourcing externally at
all — reuse Smilestory's humans instead, at zero additional cost. Recommend checking those three first
before spending time hunting a separate free pack. The goblin trio, bonecrawler, crystal golem, and tomb
sentinel are also real candidates toward "a large array of enemies" — reskin/rename freely (that's this
game's own running joke anyway: everything here is something else, "made legally distinct").

## 5. Real problem found while converting the Smilestory arena candidates — no rig

**2026-08-24, after Zoe asked to import `bonecrawler`, `goblin`, and `tomb_sentinel` for the Oathgate
arena.** Ran all three through `Tools/glb_to_fbx.py` (no Unreal Editor open this session, so this is
conversion-and-staging only, same as the Buff/Don Minion prep earlier — no import performed). Every one
came back **`IMPORTED_ARMATURES: []`** — no skeleton, no animation, a single static mesh each. Confirmed
across all three independently, using the exact same check that correctly found armatures on Buff Minion
and Don Minion earlier, so this isn't a script fault — these three genuinely aren't rigged.

**Why, most likely:** every one of DCD's own Meshy imports (`Buff_Minion_biped`, `Don_Minion_biped`,
`Voldemortgage`, `DebtEater`) has `_biped` in its source filename, meaning it went through Meshy's rigged
"biped" generation flow. None of `bonecrawler.glb`/`goblin.glb`/`tomb_sentinel.glb` carry that suffix -
they were most likely generated through Meshy's plain "3D asset" flow instead, which produces geometry only.

**Why this matters:** every existing DCD enemy (`ADCCEnemyCharacter` and everything under it) is built on
`USkeletalMeshComponent` with Walk/Run/attack animation. An unrigged static mesh cannot plug into that
pattern as-is - it would either need to be rigged first (not a tool this project has automated; Blender's
auto-rig tools exist but nobody's set that up here) or the enemy would need a new, simpler archetype that
never animates (rotates to face its target, no walk cycle) - viable for something that reads as a
stationary threat, not for a fast swarm unit like `FinePrint` is written to be.

**Staged and waiting either way** (useful regardless of which path Zoe picks): `SK_Bonecrawler.fbx`,
`SK_Goblin.fbx`, `SK_TombSentinel.fbx` in `SourceArt/Smilestory_Bonecrawler|Goblin|TombSentinel/` -
note the `SK_` prefix is aspirational, not accurate; rename to `SM_` if these end up going in as static
props rather than getting rigged.

**Decided 2026-08-25 — Zoe's call: skip all three, don't rig them, don't use as scenery.** Search for
free rigged alternatives first, generate fresh only if nothing free fits. Candidates found (web search,
not yet downloaded or verified in-project - Zoe needs to check price/license on each before pulling one
in, same as the free-weapon-pack recommendations earlier in this doc):

**On Fab** (same ecosystem as this project's other ~30 installed packs - least friction, one click into
the library if free):
- **Undead Pack** — six characters (Necromancer, Terror, Lich, more), built on the UE5 skeleton, rigged
  with animations included. Strong direct replacement for both `bonecrawler` and `tomb_sentinel`, and the
  Lich ties in nicely with Voldemortgage's own undead-lich framing.
- **Fantasy Enemies (Pack)** — generic name, worth checking contents directly.
- **Pack 1&2 / Pack 3&4 - Stylized Fantasy Creatures** — ghouls, trolls, a "dark beast," rigged for UE5
  with combat animations and ragdoll physics.
- **Monster Pack** — Cyclops, Troll, Mountain Troll, Imp, Werewolf, built on UE5 skeleton proportions
  (retargets cleanly).
- **Skeleton Warrior Knight - Rigged**, **Skeleton Enemy** — direct `bonecrawler`/skeleton-swarm
  replacements if the packs above don't fit.

**On itch.io** (free, but a separate download source from Fab):
- **LOWPO: Goblin Assets** — 7 stylized low-poly goblins, rigged, free.
- **Free Goblin Low Poly 3D Models Pack** — rigged, free, Unity/Unreal/Godot-ready.
- **LOWPO: Undead** — 7 low-poly undead characters, rigged, free.
- **Goblin Mage and Skeleton** — covers both a goblin and a skeleton in one free/pay-what-you-wish pack.

None of these have been downloaded, imported, or checked against DCD's art style - that's the next step,
and it needs Zoe (Fab/itch.io are both account-gated, same limitation as every other external pack in this
project).

## 6. Fab library pass — 2026-08-25, Zoe's recently-added (free) list

Zoe couldn't run the Epic Games Launcher automation this session (computer-use was blocked — this
session is running in a scheduled/dispatched context with no human present to approve screen control),
so she sent screenshots of her Fab library from her phone instead. Matched against every open gap above.
**Nothing downloaded or imported — recommendations only, she still clicks "Add to project" herself.**

### Download — strong, confident fits

- **Low Poly Fighter Starter Pack** (tt-3D, 5 human martial-arts characters) — likely the single best
  match in the whole list for the "medium priority" generic-human NPC gap in §1: Waitstaff, Collections
  Associate, Line Cook, Server On Break need exactly this many bodies. A fighting-game starter pack this
  is almost certainly rigged/animated already (that's the point of the pack). Check on arrival, but download
  this first.
- **PolyReady free-sample set → specifically "Undead Skeleton (animated)"** — explicitly confirmed animated,
  and it's the direct free replacement for the Smilestory `bonecrawler`/`tomb_sentinel` idea that turned out
  to be unrigged (§5). Strong fit for `FinePrint` (the Oathgate's "fast, weak, numerous" swarm half).
- **PolyReady's "Crimson Knight Warrior" or "Abyssal Reaper"** — either works as `Arbiter`, the Oathgate's
  "tougher pull... makes a party stop and focus fire." Pick whichever reads better in person; both are
  plausible.
- **Editable Metahuman "Advika" and FREE "Death Walker" Metahuman** — genuinely the best possible answer to
  "NPCs are still wearing my allies' bodies," since these are real, distinct, high-quality human faces,
  free, and Epic's own first-party UE5 pipeline (guaranteed to import cleanly, unlike every Meshy/glb
  conversion this project has had to work around). **Caveat, and it's a real one:** MetaHumans use their
  own skeleton standard, not the shared `SK_DCCCrawler_Skeleton` this project consolidated onto - they will
  need their own animation set or a MetaHuman retargeting pass, not a drop into `DCCCrawlerAnimSet` as-is.
  Worth it anyway - recommend using one of these two as **the Head Diver's actual body**, now that a human
  base was already the agreed plan for him (see the physical description sent to Zoe 2026-08-24).
- **Turnstile Fare Gate / NYC Subway Realm Vol.04** — the vending-machine assets in this pack line up
  directly with the vendor/consumable system built this session (`ADCCVendorNPC`), and a coin-fed turnstile
  is a very on-brand sight gag for a dungeon that "made these places legally distinct to avoid a lawsuit."
  Worth grabbing even without an immediate placement plan.

### Check before downloading — promising, but verify first

- **DarkPaperVerse set** (Humanoid Cactus, Overused Dice Golem, Helianthus Stalker, Tin Guardsman Toy,
  etc.) — the names are extremely on-brand for DCD's absurdist tone, but two real concerns: (1) distributed
  as obj/gltf, and **OBJ cannot carry a skeleton at all** - if these are rigged it'll be via the gltf side
  only, confirm before assuming animation exists (same mistake as the Smilestory monsters). (2) Described as
  a "distinct low-poly-ish sculptural style," which may visually clash with the semi-realistic PBR look
  everything else in this project (including all the Meshy characters) shares. Worth a look, not a
  guaranteed fit.
- **Warrior Orc** (Ramazanov), **Whisper** (ghoul/alien trio, Leks), **Monster for Survival Game PBR
  Polyart** (Dungeon Mason) — all plausible extra arena/mob variety, no animation status stated in what
  Zoe could see from the screenshots. Check rig/anim before committing to a role.
- **Robot R-9** (masterTOB) — fits the corporate-satire/AI-favor tech angle thematically (a customer-service
  droid, maybe). Format is FBX, which is a better sign than OBJ but still no guarantee of a skeleton - check.
- **GanzSe FREE Character Accessories** — worth a quick look specifically for the Head Diver's goggles/
  whistle/robe (§ physical description sent 2026-08-24) before sourcing those separately.

### Skip

- **AI Generated Animal Robots** — explicitly static meshes, same dead end as the Smilestory monsters.
  (Could still work as background set-dressing props, just not as an animated enemy.)
- **Knife FPS Animations** — this is a third-person game; a first-person anim set doesn't apply.
- **Lowpoly Modular Armors** — recall the standing finding that the four *player* crawler skeletons share
  no topology, so fitted armor can't skin across them (material swap is the real fix, see §2). It COULD
  work on *enemies/NPCs*, since those are consolidated onto one shared skeleton - so not useless, just not
  the fix for the original ask.
- **Female Centaur, Chinese Ancient Guard, Nordic Fishing Hut, Egypt Statues** — no current DCD need lines
  up with any of these. Fine assets, just nothing to attach them to right now.
- **Titan Climbing, Framework AI Perception Config** — DCD already has its own bespoke climbing system and
  its own simple hand-rolled AI (deliberately not Unreal's Perception system, see `ADCCEnemyCharacter`'s own
  class comment on why). A pre-built plugin here would compete with, not fill, an existing system.
- **Radio Soldier / Droid announcer voice packs** — interesting for a future voiced-narrator feature, but
  the System AI narrator is currently text-only (`ai-narrator-integration.md` - output goes into a
  replicated `FText` log, no audio pipeline exists). Not a drop-in fit today.
- **500 Rogue/Psion ability icon packs** — checked: `FDCCSkillDef` has no `Icon` field at all right now, and
  the hotbar draws text labels only (`DCCHUD.cpp`'s `DrawHotbar`). These icons have nowhere to go without a
  small UI feature addition first. Worth keeping in mind for that future pass, not useful today as-is.

### Optional / future, no urgency

Rpg Magic Staff Pack, Derelict Corridor (Quixel sample - thematically apt for "repossessed" spaces, no
room built for it yet), Gothic Props, Modern Street Props, Torn Banner, Reactive Slime Material, Wormhole
Portal (could dress up `ADCCOathgate`'s paired teleport gates visually, if placed), Volumetric Super Storm
(fits the tone bible's unbuilt "Casa de Hail" floor concept), Ultimate UI & Menus SFX Pack (easy general
polish, no blocker).

Zoe mentioned 110+ items total in her library and these screenshots may only cover part of it - more
screenshots would extend this pass if she wants a deeper sweep later.

## Summary — priority order for the Meshy session

**Revised 2026-08-24** after Zoe's corrections and the Smilestory catalog. Two trademark flags are
resolved (Pokémon Master Trainer — cleared by Zoe; League Minion — newly flagged by the peer session,
still open, same treatment Pokémon originally got). **Real blocker found on the arena-enemy ask: the three
Smilestory monsters requested have no skeleton — see §5 before doing anything with the staged FBXs.**

1. **Triage the existing backlog** (§0 + the Addendum) — mostly already done by a peer session today. What's left: confirm the Gaunt-lich and wraith-zip supersession theory, and get Zoe's call on `League Minion`/`League Minion Blue` (Riot IP risk, flagged 2026-08-24).
2. **Check Smilestory's `villager_male`/`villager_female`/`desert_nomad` (§4) before free-sourcing anything new** for Waitstaff, Collections Associate, Line Cook, or Server On Break — same cost as reuse, zero as generation.
3. **Generate the Nuclear Family** (3 bodies) — still the single most-flagged "still needed" item across the design docs, and nothing in Smilestory covers "glowing translucent 1950s family."
4. **Generate or reuse the Oathgate arena roster** (`FinePrint`, `Arbiter`) — reinstated by Zoe 2026-08-24, overriding an old code comment (see §1). Smilestory's `goblin`/`goblin_bomber`/`goblin_warlord`/`bonecrawler`/`crystal_golem`/`tomb_sentinel` (§4) are strong reskin/rename candidates here before spending fresh Meshy budget — "paperwork given legs" reskins onto almost any humanoid or bone-themed monster.
5. **Generate the Head Diver** (1 body) — has a stat block and dialogue waiting, but genuinely no physical description exists anywhere yet (see the summary sent to Zoe). Needs her input on the look, or a judgment call, before generating.
6. **Free-source** the crossbow/nail gun/flare gun (needs Zoe to download — flag it to her).
7. **Optional, low priority:** more free crowd-body variety, a Blue Horse statue landmark, anything from the "Raw Ideas" enemy list once/if one gets promoted to a real fight, Smilestory's `Meshy_AI_Cogwork_Alchemist` as a second Bar personality.
