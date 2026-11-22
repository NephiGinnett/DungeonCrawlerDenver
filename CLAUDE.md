# DungeonCrawlerDenver

Dungeon Crawler Carl × BG3 — 5-player synchronous networked dungeon crawler, UE 5.8.
Design doc: `C:\Users\kmett\Downloads\dcc-bg3-design-doc.md` (read it before planning work).

Zoe (`Nephi` on this machine) is the developer and the DM. Players: Scott, CJ, Marcel, Vaughn, Kyle.

## Environment

- **Engine:** `D:\UE\UE_5.8` (Epic launcher binary install). This is NOT the project — don't run Claude Code from there.
- **Project:** this directory. Run Claude Code from here so `.mcp.json` is picked up.
- **Toolchain:** VS 2022 Community, MSVC 14.44.35207, Windows SDK 10.0.26100. The "Game development with C++" workload is NOT installed, but the compiler and SDK are, which is all UnrealBuildTool needs.
- **Engine association:** the `.uproject` uses `"EngineAssociation": "5.8"`. It previously held a
  GUID, which is the form used for *source-built* engines resolved via
  `HKCU\Software\Epic Games\Unreal Engine\Builds` — that key is empty on this machine, so the GUID
  resolved to nothing and double-clicking the `.uproject` raised a **"Convert Project"** dialog.
  If that dialog ever reappears: **Cancel**. Never "Open a copy" — it forks the project into a new
  folder and strands the built binaries and source. Nothing needs converting; the engine is right.

### When Zoe has to close the editor (and when she doesn't)

Most work needs **no** restart. Only ask when the change is one of the bottom two rows.

| Change | Editor restart? |
|---|---|
| Level edits, actor placement, properties, Blueprints, UMG | No — all live over MCP |
| C++ edit to an existing function *body* | No — Live Coding (`Ctrl+Alt+F11`, or the LiveCodingToolset MCP tool) |
| New `UCLASS`/`UFUNCTION`/`UPROPERTY`, changed signatures | **Yes** — reflection data can't hot-patch |
| Enabling a plugin / new toolset | **Yes** — toolsets only register at startup |

So batch reflection-level C++ changes into one pass rather than trickling them out; each one costs a
close/build/reopen cycle. **Always `save_assets` before asking her to close** — unsaved level work is lost.

> **Turning Live Coding OFF does not make building-with-the-editor-open safe.** Confirmed
> 2026-08-15. With Live Coding disabled, UBT no longer refuses — it compiles to a numbered
> `UnrealEditor-DungeonCrawlerDenver-0001.dll` and the editor picks it up via **legacy Hot
> Reload** (`LogHotReload: Starting Hot-Reload from IDE`), reporting success in under a second.
> That is the mechanism Live Coding was built to *replace*, and it is worse at exactly the same
> thing: new `UFUNCTION`/`UPROPERTY`/`UCLASS` reflection data. So the trap did not go away, it
> only changed hats, and `Result: Succeeded` still is not evidence the editor is running your
> code. **Check `Binaries/Win64/` for numbered DLLs after any build — if one appeared, the build
> hot-reloaded rather than relinked, and a clean build with the editor closed is still owed.**

> **Do not use Live Coding to dodge that last rule — it lies.** Running `CompileLiveCoding` after
> adding a `UFUNCTION` returned `Result: Success` with only a mild warning about packaging, then
> crashed the editor 12 seconds later on PIE start with
> `Cast of Object /Script/CoreUObject.Default__Object to Actor failed`. It patches the binary but
> leaves reflection data inconsistent. **A "Success" from Live Coding is not evidence the change was
> safe.** If the change touches reflection, close the editor and do a full build — no exceptions.
> Recovery: delete `Binaries/Win64/*patch_*` and rebuild normally; on-disk assets are unaffected.

**Level edits survive a crash only if saved, and the `.umap` timestamp is not the signal.** This
project uses World Partition, so actors are stored one-file-per-actor under `Content/__ExternalActors__/`.
Check for recently-modified files there, not on `Lvl_ThirdPerson.umap`.

**Building requires the editor to be fully closed** — UnrealBuildTool refuses with "Unable to build
while Live Coding is active". Ask Zoe to close it rather than trying to work around it:

```
"D:/UE/UE_5.8/Engine/Build/BatchFiles/Build.bat" DungeonCrawlerDenverEditor Win64 Development \
  -Project="C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver/DungeonCrawlerDenver.uproject" -WaitMutex
```

## Checking on / closing the editor: `Tools/editor_ctl.py` (2026-11-13)

`status` (is it running, is MCP up, is PIE running), `shot` (PNG of the Unreal windows ONLY, by window handle;
never screenshot the whole desktop, which captured Zoe's private windows once), `save`, `close [--nosave]` (a
normal close; the editor asks about unsaved work itself and the script reports it), `kill` (only when asked).
Zoe asked for this so builds do not wait on her. Prefer `close --nosave` unless she said to save.

## UE5 MCP

The editor hosts the MCP server via the built-in `ModelContextProtocol` plugin at
`http://127.0.0.1:8000/mcp`, consumed through `.mcp.json` in this directory.

- `bAutoStartServer=True` is saved in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`,
  so it starts with the editor. **It only starts at editor startup** — toggling the setting on a
  running editor does nothing until restart.
- If tools are missing: check the editor is running, then check port 8000 is listening.
- Console fallback inside the editor: `ModelContextProtocol.StartServer`.

**The server is only the transport — the tools come from separate plugins.** `ModelContextProtocol`
being enabled and port 8000 answering proves nothing about what Claude can actually do. The tools
live in ~28 `*Toolset` plugins under `D:\UE\UE_5.8\Engine\Plugins\Experimental\Toolsets\`, all
disabled by default. Without `EditorToolset` there is no way to spawn an actor or touch a Blueprint,
and `list_toolsets` will return only `AgentSkillToolset` (which just reads/writes skill assets).

Enabled in the `.uproject`: `EditorToolset`, `GameplayTagsToolset`, `AIModuleToolset`, `UMGToolSet`,
`ConfigSettingsToolset`, `LiveCodingToolset`. All ship precompiled, so enabling them costs no build
time — but they only register at editor startup, so adding one means a restart.

Deliberately *not* using the `AllToolsets` aggregator: it pulls in 21 experimental plugins
(Niagara, PCG, Chaos Cloth) whose tools we don't need and whose load cost we do pay.

To confirm the setup is genuinely working, check the editor log for one line per toolset:
`LogToolsetRegistry: Display: Registering Toolset ...`

## Architecture decisions

**C++ hybrid.** Core systems in C++; Blueprints for animation, UI, and level scripting.
Chosen because the dedicated Linux server binary effectively requires it, replication is
far more reliable in C++, and C++ gives readable diffs (Blueprints are binary assets).

**Three build targets** already exist, including `DungeonCrawlerDenverServer.Target.cs`.
The server target exists from day one specifically so the project never quietly grows
client-only assumptions that are painful to unpick at deploy time.

**Server-authoritative everything mechanical.** Damage, healing, and dice rolls are refused
on clients and reach them by replication. Per the design doc, the AI/LLM layer narrates
outcomes but never decides them.

### `Source/DungeonCrawlerDenver/Stats/` — keep this ruleset-agnostic

The design doc requires the real DCC TTRPG ruleset to be swappable in later without
restructuring character data. So this folder must stay free of DCC-specific concepts
(sponsors, mutations, audience approval). Those live elsewhere and talk to this layer.

- `DCCAbilityTypes.h` — `EDCCAbility`, `FDCCAbilityScoreSet`, `FDCCCheckResult`
- `DCCRuleset.h/.cpp` — **the swap-in point.** All rules arithmetic routes through this
  UObject. The real ruleset becomes a subclass; callers don't change.
- `DCCStatsComponent.h/.cpp` — replicated per-character stats + HP. Goes on players and
  enemies alike, so the DM client can inspect both the same way.

**Two stat layers, as the books specify (added 2026-08-15).** `AbilityScores` is the
**Unenhanced** layer - authored, replicated, changed by potions and level-ups. `EnhancedScores`
is derived: Unenhanced plus equipment, and later spells and buffs. **All rules read
`GetEnhanced()`**; only advancement writes to the base. Recomputed from base on every change
rather than adjusted in place, so removing an item can never leave a residue. Floored at 1,
because cursed gear should make you bad at something, not delete the something.

The one deliberate write to the base layer is `DCCEncounter.cpp` applying a spawn's authored
stat block. Everything else was migrated.

**Level cap: raised to 20 on 2026-11-22** (arcade direction; at 5 everyone hit it and earned nothing). Gate Run pitches foes at the party level. The history: **Level was capped at 5 (added 2026-09-02).** `UDCCStatsComponent::MaxLevel`, `EditAnywhere` so
a later floor can raise it. XP is awarded from four places — kills, quest completion,
achievements and encounter clears — and the curve (`100*L + 50*L*(L-1)`) needs only **1500 XP
total** to reach level 5, while the quest table alone already offers **6700**. Uncapped, a party
would leave Floor 1 far past the tutorial band, which matters because
`Docs/races-and-classes.md` puts race and class selection on the **third** floor: arriving
over-levelled makes that choice matter less.

At the cap, surplus XP is discarded rather than banked, `ExperiencePoints` is parked at the
threshold so the HUD bar reads full instead of climbing behind a meaningless number, and the
bar greys out and reads `MAX (tutorial cap)`. `MaxLevel` gates **XP-driven levelling only** — an
enemy or boss whose `Level` is authored directly above 5 is unaffected.

Gotcha worth preserving: ability modifiers use `FMath::FloorToInt32((Score - 10) / 2.0f)`,
not integer division — C++ truncates toward zero, which gives the wrong answer for scores
below 10 (7 would give -1 instead of -2).

#### What the real DCC ruleset actually says (from the preview PDFs)

Source: `C:\Users\kmett\Downloads\{Rules for Survival, Crawlers Preview, Game Masters Toolkit}.pdf`.
These are **Sneak Preview excerpts** (7/11/5 real pages of selected spreads), not the full books —
the harness reports inflated page counts. Extract text with `pypdf` and grep; do not page through them.

Confirmed rules, and where our placeholder diverges:

- **Five core stats, not six: STR, INT, CON, DEX, CHA. There is no Wisdom.** **Swapped in
  2026-08-14** — `EDCCAbility` now has five, ordered Str/Int/Con/Dex/Cha to match the books and the
  party's own sheet.
- **The score scale is 2–6, not 3–18.** A typical human crawler starts with scores between 3 and 5.
  The **Standard Array** assigns exactly 2, 3, 4, 5, 6 — one per stat, totalling 20. The party's
  handwritten sheet uses a 20-point spread, confirming this. Scores climb into the hundreds later.
- **The modifier is a banded lookup, never negative** (Rules for Survival p.11, Table 1):

  | Score | Mod | Score | Mod |
  |---|---|---|---|
  | 1–2 | +1 | 20–49 | +5 |
  | 3–5 | +2 | 50–99 | +6 |
  | 6–9 | +3 | 100–149 | +7 |
  | 10–19 | +4 | 300+ | +10 |

  A dumped stat does not penalise you here; it just fails to help. The old
  `floor((Score − 10) / 2)` was d20-fantasy math and was **wrong** — it made the party's STR 6
  bruiser a −2. Fixed.
- **Scores and modifiers are not interchangeable, and the books signal which they mean by name.**
  The full word ("Strength") means the **score**, used for derived values — the rules' own example is
  *"a crawler's Strength score is used to determine what they can lift."* The abbreviation ("Str")
  means the **modifier**, added to d20 rolls. Carry limit, throw distance and move speed therefore
  read the **score**; checks and attack rolls read the **modifier**.
- **Skill Check = `1d20 + Skill Rank + Stat Mod`** vs Difficulty.
- Stats have two layers, **Unenhanced** (base + potions + level-ups) and **Enhanced** (plus gear,
  spells, buffs). Not yet modelled — we have a single layer.
- **Mana = Intelligence score at a 1:1 ratio** (the score itself, not the modifier; spells cost
  double-digit Mana). `GetMaxManaPoints` already implements this exactly.
- **Constitution determines Health.** Formula is on book p.41, which is *not* in the preview — our
  `10 + CONmod × Level` is still an invention.
- **Dexterity drives "Evade"**, which is DCC's name for the defence value an attack must beat.
- **"AI Favor"** is the real name for the design doc's audience-approval/sponsor meter. Build the
  sponsor system against that term.

##### Skill Ranks and advancement (Crawlers Preview p.42–44; Rules for Survival p.12)

Confirmed from the previews on 2026-08-14. Skills level **through use**, Runescape-style:

- **Skill Rank 0–20**, capped at **15** on Floors 1–5. The core check is
  `d20 + Stat Mod + Skill Rank` vs Difficulty — our `ResolveCheck` is missing the Rank term.
- **Using a skill marks it.** Attempting a Skill Check marks the skill whether it **succeeds or
  fails**. It is a *binary mark*, not a usage counter — twenty uses mark it once.
- **Skill Advancement Check:** roll `1d20` against **the skill's current Rank as the Difficulty**;
  meet or beat it to gain +1 Rank, max 15. Self-limiting by construction: trivial at low Rank,
  rare at high Rank. No XP table is needed or wanted.
- **Intervals:** Rank ≤4 → every 2 hours of play. Rank ≥5 → end of each floor. Then erase all marks.
- **Untrained use:** Attack and Utility skills may be attempted untrained at **Disadvantage**
  (2d20 take lower), Stat Mod but no Rank. On Tutorial Floors this is how new skills are *earned*
  (1 Rank in one skill tried untrained per 2-hour session; they cannot be banked).
  **Spell Skills cannot be attempted untrained.** Passive Skills never get marks.
- **No experience points anywhere.** Levels come from playtime (1 per 2 hours), quests, and boss
  tier (Neighborhood 1 → Floor Boss 6). Killing another crawler gives 1d6 modified by level gap.

Implication for `Skills/`: Rank is **per-character state, not library data**. `KnownSkills` holding
whole `FDCCSkillDef` copies is the wrong shape for it — the intended end state is
`{ SkillId, Rank, bMarked }` per character with definitions resolved from `SkillTable`.

**Swapped in on 2026-08-14**, once the party's own character sheet confirmed the five-stat, 20-point
layout and the previews yielded the modifier table. Only **Health remains an invention** — the real
formula is on Crawlers & Customization p.41, which is not in the excerpts. `GetMaxHitPoints` is
flagged in-code as the last placeholder. Evade's exact formula is likewise absent, so `10 + Dex`
remains ours, though it now uses the correct banded modifier underneath.

### `Source/DungeonCrawlerDenver/Equipment/` — and the skeleton problem

**Added 2026-08-15.** DataTable-driven gear: `EDCCEquipmentSlot` (MainHand, OffHand, Head,
Body, Trinket), `FDCCEquipmentDef` rows, and a replicated `UDCCEquipmentComponent` that
folds bonuses into the stats layer and hangs meshes off sockets.

**Ability bonuses are five flat `int32` columns, not a map or nested struct**, because the
MCP DataTable importer only accepts CSV and CSV cannot express a nested container without
hand-written UE struct literals. Uglier header, far better authoring.

#### Rigid attachment only — and why that is not a temporary limitation

**Every crawler has its own Meshy-generated skeleton ASSET**: `SK_HockeyWings_Skeleton`,
`SK_OfficeAirplane_Skeleton`, `SK_ArmsOut_Skeleton`, `SK_DiceWinged_Skeleton`, plus the
mannequin for the player.

> ### ✅ CONSOLIDATED 2026-08-15. All four crawlers now share one skeleton.
>
> Canonical skeleton: **`/Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton`** (renamed from
> `SK_HockeyWings_Skeleton`, which was misleading once everyone used it; CJ's mesh followed the
> redirector cleanly).
>
> - CJ's `SK_HockeyWings` was already on it and needed no re-import.
> - `SK_OfficeAirplane_Shared`, `SK_ArmsOut_Shared`, `SK_DiceWinged_Shared` were imported bound
>   to it, with sockets re-added and materials/physics assets restored. Vertex counts verified
>   **identical** to the originals, so nothing was mangled. Originals are still present as a
>   fallback and can be deleted once the Blueprints are repointed.
> - **All 24 animations** re-imported against it into `Shared/Anims` as `A_DCC_<src>_<clip>`.
>   Every crawler can now play every clip — which fills three gaps the docs listed as missing:
>   `A_DCC_DW_ChargedSlash` (an attack), `A_DCC_DW_CallGesture` (a cast, for CJ's Fire Fingers
>   beat) and `A_DCC_DW_ClimbStairs` (a climb, for anyone).
>
> **Re-import is the only rebinding path** — `SkeletalMeshTools` has `get_skeleton` but no
> `set_skeleton`, and `import_file` refuses to overwrite an existing asset. Hence new `_Shared`
> assets rather than in-place rebinding.
>
> **Sockets are MESH-level, not skeleton-level.** A mesh imported against a skeleton that has
> sockets inherits none of them, so every new mesh needs its own `add_socket` pass.
>
> **Animation imports return the junk mesh but NOT the AnimSequence.** The clip still lands, as
> `<AssetName>_Anim` next to it — find it by path rather than trusting the return value.
>
> Original finding that made this possible:
> **the four Meshy rigs are structurally IDENTICAL — verified 2026-08-15.** Byte-identical
> bone lists: 24 bones, root `Hips`, same names in the same order
> (`RightHand`, `LeftHand`, `Head`, `Spine02`, ...). They are separate *assets*, not separate
> *rigs*. This is much better news than "five different skeletons" implies:
>
> - Animation retargeting between the four is a 1:1 bone map, i.e. nearly free.
> - **Rebinding all four meshes to one shared Skeleton asset would make every animation
>   directly usable by every character with no retargeting at all** — which is the cheap fix
>   for CJ having no cast or attack animation, since `A_DW_ChargedSlash` and `A_DW_CallGesture`
>   already exist on a structurally identical rig.
> - Modular armour across the four becomes possible too, with clipping risk where Meshy gave
>   them different silhouettes.
>
> Untested risk: identical hierarchy does not guarantee identical proportions or bind pose, so
> shared animation may drift visually. Test on one character before committing.

The player's mannequin is the genuine outlier — UE naming (`hand_r`, `spine_03`) against
Meshy's (`RightHand`, `Spine02`).

That rules out body-conforming armour outright. A chestplate skinned to CJ's rig has **no
correspondence** to Marcel's — no shared skeleton to skin against, no shared topology to
morph through. What works is rigid props snapped to sockets: weapons, helmets, shoulders,
packs. What does not work is a fitted breastplate. Armour that must change the body's
silhouette has to be a **material change on the existing mesh**, not new geometry.

**The socket NAME is the contract; the bone it hangs from is per-skeleton detail.** That is
what lets one item definition serve five differently-rigged characters — each skeleton just
needs a socket of the agreed name, parented to whatever that rig calls its right hand.
Names in use: `hand_rSocket`, `hand_lSocket`, `headSocket`, `spine_03Socket`.

**Sockets are authored — done 2026-08-15.** All six skeletal meshes carry `hand_rSocket`,
`hand_lSocket`, `headSocket` and `spine_03Socket`: the four Meshy characters and both
mannequins (`SKM_Manny_Simple`, `SKM_Quinn_Simple`). Added with
`SkeletalMeshTools.add_socket` — **a dedicated write API that works, unlike the broken
`ObjectTools.set_properties`**, which is a useful pattern: when an ObjectTools write fails,
look for a purpose-built tool on the asset's own toolset before giving up.

> **Their transforms are identity and still need a visual pass.** A socket at the hand bone
> puts a weapon at the wrist pivot, not in the grip, so expect props to sit wrong until
> someone nudges them in the skeleton editor. Fallbacks still exist for safety
> (`DCCCarryComponent` → capsule offset, equipment → mesh root).

### `Source/DungeonCrawlerDenver/UI/DCCHUD.cpp` — enemy health bars moved off UMG entirely

**2026-08-15.** `WBP_EnemyHealthBar` (a Screen Space `UWidgetComponent`, added on `BP_DCCEnemy`
in the Blueprint editor, not C++) is now retired at runtime — `ADCCEnemyCharacter::BeginPlay`
finds it via `FindComponentByClass<UWidgetComponent>()` and calls `DestroyComponent()` on every
machine. It is not deleted from the Blueprint asset itself; disabling it in code was faster and
safer than editing a component shared across seven subclasses.

**Why it moved:** Screen Space widget components composite as a dedicated render pass *per
instance*, every frame, and do not taper off with distance the way World Space ones do. With
`LDMaxDrawDistance` unset (0 = unlimited) every enemy's bar rendered regardless of range — this
was the direct cause of "health bars everywhere" and a real contributor to frame stutter with
more than a handful of enemies on screen.

**Where it lives now:** `ADCCHUD::DrawSegmentedBar` — the exact block-drawing algorithm the
player's own vitals bar already used, extracted into a shared helper so both draw identically.
`ADCCHUD::DrawNearbyHealthBars` iterates live characters, distance-gates them (2500 units),
projects their head position to screen space, and draws a mini version of the same bar tinted
by `GetAllegianceColour()` — flat by allegiance, not health-lerped, so a bar reddening as it
empties can never be confused with "this is hostile." One bounded Canvas pass per frame, not N
persistent widget components.

**A second, unrelated finding while tracing an "allies won't fight" report:** the mechanics were
fine — `PerformAttack` was resolving, dealing damage, and logging every hit. Nothing was visibly
happening because `Multicast_PlayAttack` only calls `PlayOneShotAnimation(AttackAnimation)`, and
`AttackAnimation` was unset on every character Blueprint, so it silently no-ops. Worth checking
the combat log feed before assuming a mechanics bug when a fight looks static - it may just be
an unset animation slot.

**If the project ever standardises on one skeleton**, modular armour *and* cross-character
animation retargeting both become possible in the same pass. That is the real argument for
doing it: the fragmentation is already costing us — it is why CJ has no cast animation, why
there is no shared AnimBlueprint, and why `BP_DCCEnemy_HockeyWings` runs a single looping
clip with no blending. Armour would make the same tax hit twice.

### `Source/DungeonCrawlerDenver/Rooms/` and `Characters/`

- `DCCRoomTypes.h` — `EDCCRoomType` (the design doc's five templates), `EDCCEncounterState`.
- `DCCRoom.h/.cpp` — one floor node: bounds volume, type, server-side occupancy list, optional
  encounter armed on player entry. **Occupancy is exposed on purpose** — it's the hook both the
  instanced-pocket merge and safe-room session gating will read. Neither is built yet.
  `AllowsAsyncLogin()` is derived from room type, not authored, so a floor layout can't get it wrong.
- `DCCEncounter.h/.cpp` — an enemy set spawned on demand, cleared when none are left standing.
  Enemies exist only on the server and arrive on clients by replication.
- `DCCCharacterBase.h/.cpp` — shared by players and enemies so encounters don't care which side
  they're looking at. `AttackCooldownSeconds` is the design doc's per-action initiative value;
  the pause/queue loop will consume it rather than inventing its own.
- `DCCEnemyCharacter.h/.cpp` — intentionally dumb AI (nearest player → close → swing on cooldown),
  no behaviour tree, because a tree needs asset authoring before the first fight is playable at all.
  Replace with StateTree when encounters need real tactics.

Build gotcha, hit once already: `Cond ? TSubclassOf<T> : T::StaticClass()` won't compile under
MSVC — the ternary is ambiguous between `TSubclassOf<T>` and `UClass*`. Spell both branches
as `TSubclassOf`.

Second gotcha: a `UFUNCTION` parameter may not be named `Instigator`. `AActor` already declares
that property and UHT rejects the shadow. Use `Character`/`Actor` instead.

**This bites for any inherited property name, not just `Instigator`** — hit again 2026-09-02
with a parameter named `Skills` on a `ADCCEnemyCharacter` subclass, because `ADCCCharacterBase`
has a `Skills` component. Two separate compilers complain in two different ways and it is worth
knowing both, because fixing only the first leaves you with a second failed build:

- **UHT** rejects the *`UFUNCTION` parameter* outright: *"cannot be defined ... as it is already
  defined in scope 'ADCCCharacterBase' (shadowing is not allowed)"*.
- **MSVC** then rejects any *local variable* with the same name as `error C4458: declaration of
  'X' hides class member` — warnings-as-errors, so a `if (UDCCSkillComponent* Skills = ...)`
  inside the same file fails too.

Prefix the name (`CastingSkills`, `CrawlerSkills`) rather than trying to qualify it. Names
already known to collide on characters: `Instigator`, `Skills`, `Stats`, `Equipment`, `Mesh`.

### `Source/DungeonCrawlerDenver/Skills/`

The design doc's Skill Archetype System: player-invented skills are **data, not code**, so Zoe
can add one mid-session without a recompile — which is the whole point, since the tabletop game
invents skills at the table.

- `DCCSkillTypes.h` — `FDCCSkillDef` derives from `FTableRowBase`, so skills are DataTable rows.
  7 archetypes; parameters are mana cost, cooldown, range, cone angle, power die, DC, duration.
- `DCCSkillComponent.h/.cpp` — server-authoritative activation. Clients may *ask*
  (`TryActivateSkill`); cost, cooldown, targeting and the roll all resolve on the server.
  Mana is spent **before** the roll — a failed spell still burns it, which is what makes a bad
  roll hurt. Damage/Heal resolve in C++; the fuzzier archetypes fire `BP_OnSkillEffect` so the
  DM authors them in Blueprint. That split is deliberate: deterministic mechanics in C++,
  "anything is possible" in Zoe's hands.

### `Source/DungeonCrawlerDenver/Puzzles/` — added 2026-09-01

`ADCCPuzzleDoor` and its subclasses (`ADCCComplianceDoor`), plus `ADCCPressurePlate` and
`ADCCBreakerSwitch`. Design spec is `Docs/next-evolution-plan.md`; Floor 1's use of them is
`Docs/floor-01-design.md` §8 "The Claim".

**The one rule this folder exists to enforce:** *every lock has a key and a crowbar; the
crowbar always works and always costs more.* `ForceOpen` lives on the base class, not on
subclasses — a puzzle has to actively set `bAllowForcing = false` to remove it. Nothing here
may hard-block a five-player table, because four people watching a fifth fail a riddle is how
a session dies. It is also correct in fiction: the System AI does not care whether you comply,
only that it is watchable.

**Two damage paths, and why that matters.** `ADCCCharacterBase::ModifyIncomingDamage` is
consulted **only** on the attack path (`PerformAttack`). Skill damage (`UDCCSkillComponent`)
and thrown-prop impact damage (`ADCCPickup`) both call `Stats->ApplyDamage` directly and never
reach it. That asymmetry is load-bearing, not an oversight — it is what lets
`ADCCEnemyCharacter_LegacySystem` be immune to being *hit* while still dying to anything
thrown or cast, with no new system to tell those cases apart. Keep the hook on the attack path
only; moving it into `ApplyDamage` would break that and drag monster-specific rules into
`Stats/`, which is supposed to stay ruleset-agnostic.

**`FDCCSkillDef::RangePerRank`** (added the same day) is flat-added reach per Rank above the
first, zero for almost everything. It exists for skills where reach *is* the thing being
practised — `TargetedThrow` is the case it was added for. Flat rather than multiplicative
because Rank runs 0–20 and a multiplier gets absurd fast. Read it via
`UDCCSkillComponent::GetEffectiveRange`, never `Skill.Range` directly.

### `Source/DungeonCrawlerDenver/Favor/`

The sponsor/audience meter, named **AI Favor** to match the published ruleset.

- `DCCFavorTypes.h` — `EDCCFavorEvent` (why favor moved), `EDCCFavorTier`, `FDCCFavorChange` ledger.
- `DCCFavorComponent.h/.cpp` — replicated meter with authored tier thresholds and a capped
  ledger. Every change records a reason and a note, so the AI narrator can say *why* the crowd
  turned rather than inventing it. v1 is DM-driven (`AwardFavor` with `DirectorCall`); automatic
  detection of "style" is deliberately not attempted yet.

### `Source/DungeonCrawlerDenver/Combat/`

- `DCCGameState.h/.cpp` — the real-time-with-pause loop. Any player may pause; while paused each
  character queues one action (re-queueing replaces it); on resume actions resolve in
  **initiative order** using the action's own cooldown, which is what stops unpausing from
  becoming a fastest-clicker contest. Lives on GameState because that is match-wide state every
  client must agree on.
  - `P` toggles pause. Left-click means "attack" live and "plan an attack" while paused.
  - Enemy AI early-outs while paused, so the dungeon stops thinking during the planning phase.
  - **Requires the GameMode's `GameStateClass` to be set to `ADCCGameState`** — without that
    wiring the pause silently does nothing.

## Importing character models (Meshy AI / glTF)

Meshy exports a `.zip` with two `.glb` files: the character, and a *merged animations* file.
Getting them in has two gotchas that cost a while to find:

1. **The MCP `SkeletalMeshTools.import_file` cannot read `.glb`.** It routes through `FbxFactory`,
   which accepts only `fbx` and `obj` — even though the editor's own drag-and-drop glTF importer
   would handle them. Convert first with Blender (installed at `D:\Blender\blender.exe`):
   `blender --background --python <script> -- <src.glb> <dst.fbx> <bake_anim 0|1>`.
2. **One animation per FBX.** Unreal imports a single take per file, so a merged-animations FBX
   yields exactly one AnimSequence no matter how many actions it contains. Export each Blender
   action to its own FBX.
3. **An armature-only FBX imports as nothing at all** — no error, just "produced no assets". The
   skeletal-mesh import path needs geometry present, so every per-animation FBX must include the
   mesh. Each import therefore also spits out a throwaway SkeletalMesh; delete it afterwards.
   Naming: importing as `TMP_X` produces `TMP_X` (junk mesh) and `TMP_X_Anim` (the animation).

Conversion scripts live in `Tools/` (`glb_to_fbx.py`, `split_glb_anims.py`); extracted
source art lives in `SourceArt/`.

**Two import warnings are expected and harmless:**
- *"Found texture name clash, `texture_0_001` was renamed…"* — every Meshy character ships a
  texture called `texture_0`, so the second and later imports get suffixed. Cosmetic; each
  character's material still points at its own texture. Verify by capturing the material
  thumbnail rather than the mesh thumbnail, which caches before textures finish importing.
- *"No smoothing group information was found"* — the per-animation FBX files carry a throwaway
  mesh whose normals are never used. `mesh_smooth_type='FACE'` is now set in both scripts to
  silence it, because the noise obscures real import errors.

**⚠️ Meshy `.glb` props must not be imported into a shared folder (found 2026-11-13).** Their materials and images
are unnamed, so the glTF importer calls every one `Material_0` / `Image_0..2`; five props imported into
`DCC/Equipment/Props/` ended up sharing ONE material, and four of them wore another prop's texture (the "black
smears" on the goggles, popcorn bucket and trainer). Fixed with `Tools/fix_prop_materials.py` (Blender: named
material + extracted textures) and `Tools/fix_prop_materials_ue.py` (MCP: per-prop folder, texture wired to Base
Color, original mesh repointed). For a new prop, run both, or import into its own folder.

**Grey character thumbnails are usually a stale cache, not missing textures.** The mesh thumbnail
renders before the material and textures finish importing. Check `CaptureAssetImage` on the
*material* — if the sphere is textured, the character is fine in-world.

**Verifying an import without opening the editor UI:** `EditorAppToolset.CaptureAssetImage` renders
an asset thumbnail and `CaptureViewport` grabs the level view. Both return base64 PNG that blows the
tool-output token limit, so decode the saved tool-result file to a `.png` and read that instead.
`CaptureViewport` requires `captureTransform` and `annotations` to be passed explicitly.

### Imported so far

`/Game/DCC/Characters/HockeyWings/` — CJ's crawler model (`SK_HockeyWings`), a bearded man in a
flaming hockey jersey. 170cm, correct scale against the mannequin, textures intact. Animations in
`Anims/`: `A_HW_Walking`, `A_HW_Running`, `A_HW_AgreeGesture`, `A_HW_BoomDance`.
**There is no attack or death animation in the set** — `BP_DCCEnemy_HockeyWings` currently runs a
single looping clip via `AnimationSingleNode` rather than an AnimBlueprint, so it has no
idle/walk blending and no attack montage. A proper ABP for this skeleton is the next step.

## When the MCP tools are missing but the editor is up — `Tools/mcp_call.py`

**Added 2026-09-01, and it unblocked a whole session.** Claude Code's MCP client connects
**once, at session start**. A session that begins with the editor closed (which every session
starting with a build does) gets `ConnectionRefused` and then has **no `unreal-mcp` tools for
its entire life**, even after the editor comes back and port 8000 starts listening. There is no
in-session reconnect.

The server is a plain JSON-RPC-over-HTTP endpoint, so it can be driven directly:

```bash
python Tools/mcp_call.py toolsets
python Tools/mcp_call.py describe SceneTools
python Tools/mcp_call.py call SceneTools find_actors '{"name":"","tag":"","collision_channels":[]}'
python Tools/mcp_call.py batch calls.json     # one handshake, many calls - much faster
```

Import it as a module (`from mcp_call import McpSession, unwrap`) for anything non-trivial;
that is how the whole Floor 1 mine was built.

Three things that cost time and are handled inside the script now:

- **Toolset names must be fully qualified** (`editor_toolset.toolsets.scene.SceneTools`).
  Passing `SceneTools` fails with "Toolset not found", which reads like a missing plugin rather
  than a naming mistake. `TOOLSET_ALIASES` expands the short names.
- **Everything dispatches through one `call_tool`** — the server publishes only
  `list_toolsets` / `describe_toolset` / `call_tool` at top level.
- **Parameter names differ from the docs' habits**: `find_assets` wants `folder_path`+`name`
  (not `pattern`), `get_properties` wants `properties` (not `names`), `set_actor_folder` wants
  `folder_path` (not `folder`). When a call errors, the server **prints the full input schema** —
  read that rather than guessing.

## MCP level-editing gotchas

- **Shrinking a struct array with `set_properties` is refused**, with "elements changed
  alongside the size change; removed elements are ambiguous". Write `[]` first, then write the
  new array. Hit re-authoring `ADCCEncounter::Waves` from 3 spawns down to 2.
- **`add_to_scene_from_*`'s `name` sets the LABEL, not the object path.** The actor still gets a
  `StaticMeshActor_UAID_...` path, so a later `remove_from_scene` by the name you passed fails
  with "not a valid Actor". Keep the returned `refPath`, or find the actor again by transform.
- **`DataTableTools.import_file` refuses to overwrite an existing table** (same shape as the
  `.glb`/`SkeletalMeshTools` gotcha). To update one: `add_rows` for the new row names, then
  `set_rows` with the full payload.
- **`set_rows` rejects the whole call if any property is unknown**, naming the offending fields.
  The authoring JSON in `Docs/` may carry extra fields the runtime struct does not have —
  `DT_DCCDialogue.json` has `Room`/`Tier`, which the `FDCCDialogueLine` struct does not.

> ### ✅ CORRECTED 2026-08-16: `ObjectTools.set_properties` DOES work.
>
> The previous entry here said it was inert and that quest markers, `HUDWidgetClass` and
> DataTable-to-component assignment were permanently manual. **That was wrong**, and acting on
> it cost real time: several things were built as C++ subclasses purely to route around a tool
> that works.
>
> **The actual bug was the call, not the tool.** `set_properties` takes its payload in a
> parameter named **`values`**, and that parameter is a **JSON-encoded string**, not an object:
>
> ```
> set_properties(instance={"refPath": "..."}, values="{\"ObjectiveTag\": \"Orientation\"}")
> ```
>
> Passing an object literal, or naming the parameter `properties`, raises a schema error rather
> than writing anything. A call shaped that way returning "false" is what the old note recorded.
>
> Verified working 2026-08-16 by read-back on every property class the old note claimed was
> blocked:
>
> | Target | Property | Result |
> |---|---|---|
> | Placed level actor | `ObjectiveTag` (`FName`), `MarkerLabel` (`FText`) | ✅ written |
> | Placed actor's component | `StaticMesh` (object ref), `RelativeScale3D` (struct) | ✅ written |
> | Blueprint CDO component | `SkeletalMeshAsset` (object ref) | ✅ written |
> | Blueprint CDO, direct | `HUDWidgetClass` (`TSubclassOf`), set to null | ✅ cleared |
>
> **Still read the value back after writing.** Not because the tool lies, but because a wrong
> refPath or a soft-pointer format mistake fails quietly — see the soft-pointer note below.
>
> **Soft object pointers take a plain path string, not a `refPath` object.** A
> `TSoftObjectPtr` column written as `{"refPath": "..."}` silently stores `None`; written as
> `"/Game/Path/Asset.Asset"` it stores correctly. Hit on `FDCCEquipmentDef.Mesh`.
>
> `ObjectTools.reset_properties` is the genuinely unreliable one, and its semantics are not what
> the name suggests: it clears a property to the **type default** (null), *not* to the owning
> C++ class's constructor value. Clearing `SkeletalMeshAsset` on a Blueprint whose C++ parent
> sets a mesh leaves it `None`, and **compiling the Blueprint does not re-run the native
> constructor** — only an editor restart rebuilds the CDO. Prefer `set_properties` and say what
> you mean.
>
> **What this does not change:** setting defaults in C++ with
> `ConstructorHelpers::FObjectFinder` is still the better answer for anything that must never
> fail quiet, and the `ADCCAlly_*` / `DCCCrawlerAnimSet` classes remain the right shape — a
> C++ default reaches every Blueprint that has not overridden the property, which is one edit
> instead of four. Use `set_properties` for one-off instance wiring, C++ for defaults.

- **`ConstructorHelpers::FObjectFinder` resolves ONCE, at editor startup.** An asset created
  *during* a session is not picked up by a constructor default until the next editor restart —
  the CDO was already built. Confirmed 2026-08-15: `achievementTable` resolved (its table
  existed at startup) while `equipmentTable` read `None` (its table was imported mid-session).
  **Self-healing on the next restart, no rebuild required** — the DLL is unchanged, the finder
  just needs the asset to exist when the class is constructed. Do not conclude the system is
  broken; check whether the table post-dates the session.
- **`DataTableTools.import_file` cannot read `.json`** — it routes through `CSVImportFactory`,
  which accepts only `csv`. Same shape as the `.glb`/`FbxFactory` gotcha. Since `DT_DCCQuests.json`
  is JSON precisely *because* `Objectives` is an array of structs that CSV cannot express, the
  route is `create` → `add_rows` → `set_rows` instead. `AssetTools.read_file` can read the source
  file if it is staged under `Saved/` (its allowed roots are `/Game/`, plugin `Content/`, and
  `Saved/`, and **relative paths resolve against the engine binary directory**, so pass absolute
  ones) — which lets `ProgrammaticToolset` do the whole transform editor-side without the file
  passing through Claude's context.
- **DataTable property case is asymmetric.** `set_rows` takes **camelCase** at every level, but
  `get_rows` returns top-level fields camelCase and **nested struct fields PascalCase**. Reading
  back `objectives[0].type` gives null; the field is `Type`. This looks exactly like a failed
  write and is not one.
- **`ActorTools.set_actor_transform` resets any field you omit.** Passing only `location` silently
  sets rotation to zero and **scale to 1**. A 20×20 scaled floor cube becomes a 1m cube and the room
  vanishes. The "unset means don't change" note in its docs applies to blueprint defaults, not to
  placed actors. **Always pass location, rotation and scale together.**
- **`SceneTools.save_actor` fails on World Partition actors** ("Asset does not exist" for the
  `__ExternalActors__` path). Save the level instead: `save_assets(["/Game/ThirdPerson/Lvl_ThirdPerson"])`.
- **`CaptureViewport` requires `captureTransform` and `annotations` explicitly** — both error if
  omitted rather than defaulting. Its PNG always blows the tool-output limit; decode the spilled
  tool-result file with `Tools/decode_tool_image.py`.
- **`snap_to_ground` snaps to whatever is under the point**, including the Cave_Camp rocks that
  surround Room 01 — it put a test actor at z=551 rather than on the floor. Prefer explicit Z.
- **⚠️ `find_actors` with a `bounds` box is NOT a reliable emptiness check — it has now been
  wrong twice, and both times cost a full rebuild-and-relocate.** It reports actors whose
  pivot/bounds fall in the box, which silently misses large meshes whose origin sits somewhere
  else entirely. It cleared a site that turned out to contain 37 city actors (the mine,
  2026-09-01) and then cleared another containing 5710-unit skyscrapers (Tailings, 2026-09-02).
  Widening the z range helps but does not fix it.

  **Use `SceneTools.trace_world` instead — it tests real geometry.** Trace straight down from
  high altitude across a grid; anything that hits well above ground level is an obstruction:

  ```python
  d = trace_world(start={x,y,z:6000}, end={x,y,z:-500})   # returns DISTANCE, not a hit struct
  clear = d is None or (6000 - d) < 400                    # only ground-level hits are clear
  ```

  `trace_world` returns a **float distance along the ray** (or null for no hit), which is easy
  to mistake for a hit record. It is also what caught a barricade that blocked at knee height
  but not at head height - so trace at several heights when checking passability, not one.

- **Check for existing geometry before placing a room.** `find_actors` with a `bounds` box gives
  a rough first look (see the warning above about trusting it). Room 02 was first built at x≈7800,y=0 and landed inside cave rock; it now
  lives at **y≈5000**, which `find_actors` confirmed was empty apart from the sky sphere.
- **Ice_Station assets are station-scale, not room-scale.** `SM_large_floor_module` is 55×58 **metres**;
  `SM_pipe_group_01` is 20m wide. Measure with `StaticMeshTools.get_bounds` before placing. The
  crates (≈6m) and computers (≈3m) are the human-scale ones.

## Content / tone

`Docs/dungeon-tone-bible.md` — the System AI's voice, Denver-flavoured floor themes, room
clichés with twists, sponsor and AI Favor beats, loot/achievement naming. All original material
written in the books' spirit; deliberately no reuse of the books' actual floors or characters.
Read it before writing any player-facing text so announcements, loot and rooms sound like one
showrunner.

## ⭐ Start here: `Docs/ROADMAP.md` (added 2026-11-05)

The roadmap and progress tracker. It covers the three products (the game, the Crawler Companion, the
future live Table), what's owed in the editor, and a dated progress log. **Update its log at the end of
every session.** The "Current state" sections below are older history.

### `Source/DungeonCrawlerDenver/Onboarding/` (2026-11-05)

Covers the first ten minutes: wake up with no HUD, pick up your personal item, the System's announcement,
the sky arrow, a solo random-encounter trial, then the safe room issues the HUD.
- `UDCCOnboardingComponent` (on every `ADCCCharacterBase`) holds `CrawlerId`, the per-crawler stage, the
  HUD unlock bitmask, and a client-side queue of System windows.
- **Gating only switches on when an `ADCCOnboardingSettings` actor is in the level.** Without one,
  everything is unlocked and every old level behaves as before.
- System window text, encounter rows and voice lines all come from `Tools/gen_onboarding_content.py`.
  Edit the wording there, not in the JSON.
- `ADCCCharacterBase::DisplayName` is now **Replicated**. That lets a server-side rename (rolled encounter
  names) reach clients.
- Crawler identity: join with `?Crawler=CJ`, or get the first roster name that has a PlayerStart whose
  **Player Start Tag** matches.

### ⭐ Direction as of 2026-11-13: an arcade of minigames, not a long-term RPG
Zoe moved the project away from the long-term RPG. **Dungeon Crawler Denver is now gate-entered roguelike runs**
(Concourse Zero, then Departures, then the endless World Dungeon), and **other minigames are preferred**
(Dead Air, Fourteener, Repo Rally, Neon Mile, and more to come). Weigh new work by whether it makes a minigame
or a run more fun, not by long-term RPG progression. The older onboarding, bedroom, floor-design and RPG
material below is history unless she brings it back.

### The arcade (`Source/DungeonCrawlerDenver/Arcade/`, 2026-11-13)
The lobby ("The Den") is where players vote. Each game is a level load: `DCCArcade::Travel` server-travels to
`<map>?game=<GameMode>`. The non-DCD games run on `/Engine/Maps/Entry`, and their `ADCCArcadeArena` subclass
builds everything at runtime (sky included) and sets the replicated `Title`/`StatusLines` that the HUD draws.
To add a game: add a row to `DCCArcade::Games()`, a GameMode subclass, and an arena. Per-player records go
through `DCCProfile` (`Profile_<Name>.sav`). **Never write a game's records into the DCD character save.**
Build original games in a genre, never a recreation of a specific existing game.

**The front door (character select).** A launched game starts on `/Engine/Maps/Entry` with `ADCCFrontDoorGameMode`, set by
`GameDefaultMap` + `LocalMapOptions` in `DefaultEngine.ini` (the editor still opens `Lvl_ThirdPerson`). The player picks a crawler, and
then `DCCArcade::Travel` goes to the Den. Failed joins and Leave come back here too. Switching crawlers goes through
`DCCSession::SwitchCrawler`, which saves the old crawler and then changes the body **and the player name**. The name is the key
for every save, so changing only the body would revert on the next travel. In PIE, try it with `DCC.Arcade FrontDoor`.
The front door widget is the **main menu**: PLAY, HOST A PARTY, JOIN A PARTY (joins the invite on the clipboard), POSE BENCH, QUIT. Zoe wants every in-game action to be a button. Never make her type console commands for normal use.
Caveat for later: a dedicated server would also boot into the front door. Give the server its own map options when that target is built.

### Saves, the hub, and skill gear (2026-11-13)
- **Saves** (`Session/DCCSaveSystem.cpp`) export a fixed list of component properties by name. To keep
  something new across sessions, add its property name to `Parts()` there, not to the save class. Only the
  pawn that loaded a crawler may save it (`GOwners`).
- **Concourse Zero** (`Rooms/DCCHub`) is the hub. It is built at runtime at (120000, 150000, 5000), and every
  tale-floor run returns there. Vendor stock lives in `Dialogue/DCCHubVendors.cpp` constructors.
- **Item skills:** `FDCCEquipmentDef::WeaponSkillId` (an Attack Skill named after the weapon, per the books),
  plus `SplashRadius`/`HandlingSkillId`/`FumbleOnNatural` for explosives. Skill rows and the gear itself come
  from `Tools/gen_loot_content.py` -> `Tools/import_loot_content.py`.

### `Source/DungeonCrawlerDenver/Bench/` - the gear bench (F3), added 2026-11-13
**It is also a separate program:** `Pose bench.lnk` in the project root (via `Tools/pose_bench.bat`) runs `ADCCBenchGameMode`, the bench alone on a stage. This is what Zoe means by "the pose bench". In game it is F3, or the POSE BENCH button on the main menu.
Nudges for item grips (every body, or one body), sockets per body, and bones per body live in
`Content/DCC/Bench/DCCBench.json`. They are not in the DataTable or the mesh assets. Bone nudges are applied
in `RegisterOnBoneTransformsFinalizedDelegate`: the hook runs after the anim pose is final and before socket
children and rendering read it. **If a grip "will not change" from the DataTable, check DCCBench.json first,
because the bench value wins.**
**Save writes one entry only** (`DCCBench::SaveEntry`): it re-reads the file from disk and patches just the grip, socket or bone on screen, so nudges tried and abandoned elsewhere in the session never reach the file. Zoe asked for this.

### Stretched crawlers: `DCC.FixCrawlerRetargeting`
Meshy clips key translation on every bone, so shared clips force the source body's bone lengths onto every
crawler. Run the console command once, then save `SK_DCCCrawler_Skeleton`. Also: **the bone transforms in
Meshy `.glb` files are the first animation frame, not the bind pose.** Read the rest pose from the skin's
inverse-bind matrices. Details are in ROADMAP §7.

### `Companion/` - Electron + three.js character sheet and desktop buddy
A fresh codebase, not a fork of The Familiar (`Downloads\TheFamiliar`), whose renderer is a single 345 KB
file tied to its own systems. Patterns were borrowed (Kokoro voices, the transparent buddy window). See
`Companion/README.md`.

## Current state / next up

**Superseded by the reconciliation pass below (2026-08-22/23) for anything the two disagree on.**
The table immediately under this note is the old (2026-08-15) snapshot, kept for history. The
one after it is current.

### ✅ Reconciliation pass complete (2026-08-22/23, no-MCP dispatch session, Zoe away from the keyboard)

The ~4 days of uncommitted work flagged earlier in this file has been read in full, fixed where
reading it surfaced a real bug, committed in ~13 logical chunks (each individually described in
its own commit message — `git log` from here back to `d28d937` is the actual changelog, more
detailed than this summary), and Development-Editor-build-verified after every chunk with the
editor closed throughout. **Content/ (the .uasset binary assets, including Zoe's own recent
manual map edits) is deliberately NOT committed** — that is hers to review, not a call to make
unsupervised. Everything below is source-code state only.

**Real bugs found and fixed, not just documented:**
- Corpse loot tables never forwarded `EquipmentItemId` from a roll to the spawned pickup —
  equipment entries on any character's `LootTable` were silently inert. (The newer
  `DCCLootTableRoller`, used by the dungeon generator, already did this correctly.)
- `UDCCBossComponent::SpawnChargeMinion` spawned its Debt Eaters with no `Owner` and never
  registered them with the encounter — everything the Balloon Payment summoned mid-charge was
  invisible to `ChargeDamagePerMinion` and to Repossession Nip's heal-the-boss lookup. Added
  `ADCCEncounter::RegisterSpawnedEnemy`.
- `UDCCEquipmentComponent::Equip`/`Unequip` silently destroyed a container's contents instead of
  spilling them into the world the way `RetrieveFromContainer` always has.
- `ADCCIntroDirector`'s cold-open only ever teleported ONE crawler (the server's own
  `GetFirstPlayerController`) to the dungeon arrival point — in this project's five-player model,
  four players' pawns were simply never moved. Added `TeleportAllCrawlersToDungeon`, spread on a
  ring rather than stacked on one point.
- `ADCCDungeonGenerator`'s `BuildRoomShell` spawned each room with a plain `SpawnActor`, running
  `BeginPlay` before `RoomType`/`Encounter` were ever set — every generated Combat room's
  clear-tracking delegate never bound. Switched to `SpawnActorDeferred` + `FinishSpawning` after
  population.
- `UDCCStatsComponent::RecalculateEnhancedScores` had no authority guard (unlike every sibling
  mutator), and a character with stat-boosting starting gear spawned permanently pinned below its
  true max HP/Mana (the initial refill happens before equipment can be read; the later
  equipment-driven recalc only clamped down, never refilled up). Both fixed.
- `ADCCClimbPoint::FindOrCreateForSurfaceHit` forced a synthesized climb point to be at least 300
  units tall even when the first upward probe found nothing there at all — clicking near the top
  of a short wall could climb into open air above the real geometry. Removed the erroneous floor.
- `UDCCPopulationSubsystem::ApplyAttrition` took a `Cause` parameter that was accepted and never
  read anywhere, despite its own doc comment promising an announcement. Now used when non-empty.

**A real architectural question, not a bug — flagged for Zoe, not decided unilaterally:**
`Rooms/DCCDungeonGenerator.h/.cpp` + `DCCDungeonFrontierTrigger` are a complete, working,
**entirely orphaned** procedural floor generator — nothing spawns an instance anywhere, and it
appears in no roadmap/design doc despite `floor-01-design.md`'s whole premise being that Floor 1
is deliberately hand-authored. Committed so the work isn't lost, explicitly not as an endorsement
of the direction. Worth asking whether it's wanted before investing further.

**Also found, not yet acted on (documented, not fixed — lower value without a decision to build on
first, or too invasive for a no-PIE session):**
- Missing `NavMeshBoundsVolume` per generated dungeon cell — enemy pathing there likely has
  nothing to path on.
- Two dungeon-generator frontier triggers can legitimately target the same unclaimed cell from
  different sides; whichever is reached first can leave the other's corridor stub facing a wall.
- Reaching the generator's room budget makes a frontier trigger silently vanish with an open
  doorway gap and no "nothing further generated" notice.
- `ADCCClimbPoint::FindOrCreateForSurfaceHit`'s probe traces carry no ignored-actor list (a stray
  pawn could corrupt the estimated span), and two players clicking the same never-before-seen
  wall in the same tick can spawn two overlapping synthetic climb points.
- `ADCCCrowdSpawner` (the Tier 2 ambient-crowd system) is code-complete and reachable via the
  admin debug panel but has never been placed anywhere — its prerequisite (Bar/Room 05 geometry)
  has existed since 2026-08-19, so this is purely an editor placement task now.
- The Nuclear Family (`ADCCEnemy_NuclearDad/Mom/Kid`, Rocky Flats satire, dose-not-damage hazard)
  is built and spawnable via the admin panel but not placed in any room/encounter.
- `UDCCAchievementComponent`'s new claim-reward flow (`RequestClaimReward`, already called by the
  HUD) has never been exercised in PIE.
- **The `[DCC Talk]` ally-interaction diagnostic from earlier this session is still the open
  question on "can't interact with allies"** — nothing in this pass's reading turned up an
  obvious cause; it still needs one PIE run with that log filter.

**Discovered mid-pass and fixed opportunistically:** several of this pile's own files depend on
symbols that only exist in OTHER, still-more-uncommitted files from the same pile (Equipment
called `Stats->RecalculateEnhancedScores()` before that function existed in committed history;
the HUD's achievement-claim UI, itself already committed this session, called
`RequestClaimReward` before `DCCAchievementComponent` had it; `DCCGameMode` needed a `Sockets`
module dependency that was never added to `Build.cs`; `DCCCharacterBase.cpp` called
`ADCCClimbPoint::FindOrCreateForSurfaceHit` before that function existed anywhere in git). None of
this broke the actual working tree — every file was already present on disk together and this
session's builds cover the true combined state — but it means **several individual commits in
this pile's history do not independently compile if checked out alone**. Not worth rewriting
history to fix; noted here so nobody burns an hour bisecting.

### What exists and works — snapshot at 2026-08-15, superseded above

Kept for history only. The vertical slice was playable and had been played, including a
**two-player PIE session**. Room 01 (`DCC/Room01`, x 5200–6800) and Room 02 (`DCC/Room02`,
y≈5000) both exist, with a `DCCRoom`+`DCCEncounter`, navmesh, enemies that path and swing,
ragdoll deaths, and a working clear condition.

Systems built and compiled, roughly in the order they were added:

| System | Where | State (as of 2026-08-15 — see above for current) |
|---|---|---|
| Stats, ruleset, five-stat DCC math | `Stats/` | Working |
| Rooms, encounters, **staged waves** | `Rooms/` | Compiled, waves untested |
| Skills, ranks, mark-and-advance | `Skills/` | Working |
| AI Favor | `Favor/` | Working, DM-driven only |
| Pickups, carry, throw, inventory | `Items/` | Working |
| Pause / queue / initiative | `Combat/DCCGameState` | Wired, lightly tested |
| **Boss phases** (charge, lien) | `Combat/DCCBossComponent` | Compiled, never run |
| Climbing + sprint on shared stamina | `Characters/` | Testable — see below |
| Nameplates, fist-bump friendship | `Characters/` | Working |
| XP, levels, gold | `Stats/` | Working |
| Achievements | `Progression/` | Compiled, **needs its table assigned** |
| Quests, markers, HUD tracker | `Quests/` | Compiled, **needs its table assigned** |
| Talking NPCs | `Dialogue/` | Compiled, never placed |
| AI narrator (local LLM) | `Narration/` | Compiled, **off by default** |
| Segmented health, mana regen | `Stats/`, `UI/DCCHUD` | Compiled, never seen |

### The immediate next step

**The three DataTables are imported.** ✅ `DT_DCCAchievements` (20 rows), `DT_DCCQuests`
(7 rows), `DT_DCCDialogue` (14 rows) all exist under `/Game/DCC/` and are saved. They no longer
need assigning either — each is now loaded as a **C++ constructor default** via
`ConstructorHelpers::FObjectFinder`, so the quiet-failure mode is gone. Still overridable per
Blueprint in the Details panel if a floor ever wants its own table.

**Everything is built and DEPLOYED as of 2026-08-15 17:15.** Development Editor target,
editor closed, real `Link [x64] UnrealEditor-DungeonCrawlerDenver.dll`, no numbered
hot-reload artifacts. The session's whole batch is live: the three table constructor
defaults, `CastTimeSeconds` + the cast-time movement root, gold/`bBanksOnPickup`/`ClaimValue`,
`FDCCLootDrop` chance-rolled drops, the seven `ADCCPickup_*` variants, `ADCCFloorExit`,
the Unenhanced/Enhanced stat split, `UDCCEquipmentComponent`, and the `DCCFloatingText`
billboard fix.

Nothing is owed a build. What remains is editor-side placement and wiring — see
`Docs/POST_BUILD_TASKS.md` §0.

**✅ Items 2–5 below (quest markers, `HUDWidgetClass`, bartender, Voldemortgage encounter) were
all completed 2026-08-19, verified live over MCP rather than trusted from a doc — this list had
gone stale (POST_BUILD_TASKS.md already recorded 2 and 3 as done on 2026-08-16, but this section
was never updated to match).** Details:

- Quest markers and `HUDWidgetClass`: already done as of 2026-08-16, confirmed by reading the
  live values back (`ObjectiveTag`/`MarkerLabel` set on both markers, `HUDWidgetClass` reads
  `None` on the player Blueprint CDO).
- **Rooms 5 (Bar) and 7 (Diving Pool) did not exist as geometry**, so "place the bartender" and
  "author the Voldemortgage encounter" both turned out to be level-building tasks, not just
  wiring. Built minimal blockouts for both: `DCCRoom_Bar` (SafeRoom) with an `SM_Cube` floor,
  `SM_bar`/`SM_bar_top`, three stools, two lamps, and the `ADCCDialogueNPC` (`SpeakerId =
  Bartender`) behind the counter — all in outliner folder `DCC/Room05_Bar`. `DCCRoom_DivingPool`
  (Combat) with an `SM_Cube` floor and a `NavMeshBoundsVolume`, plus `DCCEncounter_Voldemortgage`
  with both waves authored (Wave 0 immediate: Voldemortgage `bIsBoss=true` + 3 DebtEaters; Wave 1
  at 50% boss health: 5 more DebtEaters) and linked via `DCCRoom.Encounter` — in
  `DC/Room07_DivingPool`. Both are pure blockout, no art pass.
- **Correction to the "`set_properties` can't write nested arrays" claim** (POST_BUILD_TASKS.md
  §8e, and the MCP gotchas section below): it can. `ADCCEncounter::Waves` is
  `TArray<FDCCEncounterWave>` where each wave holds its own `TArray<FDCCEnemySpawn>` — a doubly-
  nested array of structs — and a single `set_properties` call with the whole JSON tree wrote it
  correctly, confirmed by reading it back. The §8e claim predates the 2026-08-16 `set_properties`
  fix and was never retested afterward. Try `set_properties` first on any "can't write this
  property" problem before concluding it needs a workaround.

**✅ Dressing, connection, exit, and a real PIE playtest all done 2026-08-19, same session.**
The Bar got walls + a ceiling; the Diving Pool got a water plane, an 8-bench seating ring, and
an `ADCCFloorExit`. Room02's floor already physically overlapped the Bar's (no corridor needed,
just a ~25-unit height seam left as-is); a new floor strip + nav volume bridges the Bar to the
Diving Pool. Full detail in `POST_BUILD_TASKS.md`'s §8e update. Highlights:

- **PIE playtest confirmed the encounter chain works end to end**, autonomous-AI-only (no
  input-injection tool exists in this MCP toolset, so this was observation via logs + live state
  reads, not manual play): spawning the player inside the arena bounds triggered
  `DCCRoom_DivingPool` → `DCCEncounter_Voldemortgage`, which spawned exactly the authored Wave 0
  roster (Voldemortgage `bIsBoss` + 3 DebtEaters), and combat resolved for real — skills fired,
  including the documented "mana spent even on a fizzle" rule verbatim in the log, and hit/miss/
  crit rolls landed against Evade with real damage.
- **What PIE did NOT prove**: nothing was attacking Voldemortgage (no player input, no allies in
  the arena), so Late Notice, Balloon Payment, and the Lien were never exercised. That needs an
  actual play session or a temporary ally squad.
- **Found and fixed a real pre-existing bug**: Room02's `DCCRoom` had `Encounter = None` — the
  exact `"Combat room with no Encounter assigned"` warning floor-01-design.md already flagged.
  Its `DCCEncounter` existed in the level the whole time, just never linked. Fixed via
  `set_properties`, confirmed by read-back. Room01 was already correct.
- Also incidentally reconfirmed in the same PIE sessions: Fire Fingers fires and deals damage,
  CJ's mana regenerates, and enemies actually die in combat — in the sandbox test area near
  spawn (x1500–3000), not inside the themed Room01/02.

**Floor connectivity — done (2026-08-19).** Queue → Grotto → (climb) → Falls → Kitchens → Bar →
Pantry, plus Bar → Diving Pool, is now walkable end to end. **All seven rooms of Floor 1 exist
as geometry.** Two finds worth knowing:
- An existing but previously undocumented "Approach" plaza/ramp system already carried most of
  the Queue→Grotto route; only the last ~950 units needed a new floor slab.
- **Room02 is built larger than `floor-01-design.md` states** — real bounds ≈ x6660-8660,
  y4310-6360, not y4700-5300. Its west doorway is the gap between `Room02_Wall_West_A` (ends
  y5050) and `_West_B` (starts y5320).

Deliberately did **not** connect a separate, undocumented ground path
(`Approach_North_*`/`Approach_East_*`) that already runs from spawn to right at Room02's west
wall — the design doc is explicit that climbing is "the only way on" to Falls/Kitchens, and
finishing that path would bypass it. Worth asking Zoe what it was for before touching it.
Full detail in `floor-01-design.md`'s "Room-to-room connection" section.

**What's left:** `Docs/POST_BUILD_TASKS.md`'s "Verify" checklist (nameplates, segmented health,
etc. — visual checks nobody has confirmed on screen), a real playtest of Voldemortgage's phase
transitions with someone actually fighting him, and a playtest of the `HeavyLifting` quest now
that the Pantry can satisfy it. Everything on the floor is an **art pass**, not a build.

**Collecting works now (2026-08-19).** The old note here said the `HeavyLifting` quest could
never fire because there are no `BP_Pickup_*` Blueprints. There are still none and **none are
needed** — the pickups are C++ subclasses in `Items/DCCPickupVariants.h`, placed directly as
class instances via `add_to_scene_from_class`. The Pantry (`DCCRoom_Pantry`, x8700-9800,
y5800-6600) now holds the full mass-tier set, so the objective has something to act on. Untested.

**One rule worth keeping when placing loot:** duplicate the generic tier props freely, but
**never duplicate `ADCCPickup_StrongboxCrate`.** It is `bBanksOnPickup = false` at 250 gold — the
floor's single payday — and the Pantry's whole twist is that one crawler must commit to carrying
it through the boss fight. A second copy silently removes that choice. It was *moved* from Room
01's throw sandbox into the Pantry rather than copied, for exactly this reason.

### Design documents — read these before adding content

- `Docs/story-premise.md` — **the overarching theme and story, not just floor content.** Read
  this FIRST, before any of the others below — it's the thing they're all in service of, and
  it names what was previously only implicit (who Mile High Recovery Solutions is, why Denver
  got repossessed, what the party's throughline is beyond "survive the next floor"). Living
  document — update it in place as the story develops, same as `floor-01-design.md`.
- `Docs/floor-01-design.md` — **Floor 1 is the Cliff Diver Cantina under new management.**
  Seven rooms with world coordinates, the Voldemortgage fight, what does not exist yet.
- `Docs/dungeon-tone-bible.md` — the System AI's voice. Read before writing player-facing text.
- `Docs/enemy-roster.md`, `Docs/DT_DCCEnemies.json` — stat blocks and boss phase structure.
- `Docs/races-and-classes.md` — **race and class are selected on the THIRD FLOOR, not at
  character creation.** Crawlers start as plain humans; Floors 1–2 are Tutorial Floors and
  cap Skill Upgrades at the Rank 5 tier. Also: Game Guides, and why the Bar's bartender
  should become Floor 1's.
- `Docs/travel.md` — **how a player reaches the mine and the town**: the four travel gates
  (live) and `ADCCRover` (written, needs a build). Read before adding another Oathgate — the
  class ships with a 3-entry `EnemyPool` that ambushes anyone using it as transport.
- `Docs/towns.md` — **settlements and the pattern for building more** (2026-09-02). Tailings
  exists; read the "pattern for the next town" section before starting another.
- `Docs/model-assignments.md` — **who has a model and who is wearing someone else's.** Update
  it in the same pass that adds a character. Check it before sourcing any new art.
- `Docs/next-evolution-plan.md` — **the current forward plan** (2026-09-01): Viewer Chatter,
  the four puzzle primitives, enemies-as-verbs, the three quest tiers, and the C++/MCP batch
  split that keeps them off separate build cycles. Read alongside `story-premise.md`.
- `Docs/ai-narrator-integration.md` — how the local LLM connects and what it may never do.
- `Docs/multiplayer-plan.md` — Enhanced Input migration, session model, predicted failures.
- `Docs/POST_BUILD_TASKS.md` — the live wiring checklist.

### Climbing is universal now, and no longer needs a skill armed (2026-08-19)
> **SUPERSEDED 2026-11-13 by Zoe's hold-to-climb design:** equip Climbing, HOLD left mouse to grab the wall
> ahead and climb, release (or run out of stamina) to drop. Walls are never grabbed without Climbing equipped.
> Falls over 6 m hurt (fatal at 20 m). See ROADMAP's 2026-11-13 log. The text below is history.


**Root cause of "skills/climbing don't seem to work" (session found this by reading the input
code, not guessing):** `TrySelectClimbTargetUnderCursor` only ran at all when a skill was armed
from the menu (`SelectedSkillId` non-None) — with nothing armed, a primary click skipped
climb-detection entirely and fell straight to fist-bump/attack. So clicking a wall with no skill
selected did *nothing*, silently, with no feedback either way. This is very likely most of what
read as "skills don't work" too — the same click-with-nothing-armed silently no-ops.

**Fixed by generalizing climbing rather than gating it:**
- `TrySelectClimbTargetUnderCursor` (now returns `bool`) runs on **every** primary click, skill
  armed or not, and always consumes the click if the cursor is over anything wall-like.
- **Any wall-like surface is now climbable**, not just placed `ADCCClimbPoint` actors. A hit
  whose normal isn't floor/ceiling-like (`|Normal.Z| <= 0.5`) and isn't a character triggers
  `ADCCClimbPoint::FindOrCreateForSurfaceHit` — reuses a nearby point if one already covers that
  spot (authored or previously synthesized), otherwise probes up/down from the hit to estimate
  the wall's climbable span and spawns a new point sized to it. Box is left unrotated on purpose:
  `IsWithinReach`/`GetAttachLocationFor` work off the component's world AABB and the character's
  own position, not the box's facing, so precise orientation was never required.
- **Generic surfaces skip the Y/N confirm** (authored `ADCCClimbPoint`s, e.g. the Falls, still
  ask as before) — click resolves immediately to either climbing or a `ShowLocalNotice` failure
  message ("Too far away to climb that." / "There is nothing to climb there."), routed to the
  server via the new `Server_RequestGenericClimb` RPC, which re-traces near the client's claimed
  hit point rather than trusting it outright (same "nominate, don't dictate" pattern as
  `Server_CastSkill`).
- **Deliberate guardrail against hijacking combat clicks:** `GenericClimbReachDistance` (260,
  `EditDefaultsOnly` on the character) keeps this narrow — since this game's attacks are
  proximity-based (`AttemptAttackNearest`), not aimed, the only realistic false-positive is
  standing right up against a wall while looking straight at it, which is a fair trade against
  "walls not gated behind an armed skill". Worth watching for in play; tune the constant down
  (or reintroduce a require-skill-armed gate) if it ever fires during a fight unintentionally.

**This is a reflection change (new `UFUNCTION`s, new `UPROPERTY`, changed function signature) —
DebugGame compile-check passed 2026-08-19, but it has NOT been deployed.** Needs the usual close
editor → Development Editor build → reopen cycle before any of the above is actually live or
testable in PIE.

**Known-good test loop, once deployed:** look at *any* wall (no need to arm Climbing first) →
click → climbs immediately, or shows a "too far" / "nothing to climb" notice. The player has
`Climbing`, `Vault`, `MartialArts` at Rank 1. Hold LMB to rise, RMB to descend, Space to let go.
**There is no climb animation for the player skeleton**, so it plays in an idle pose — that is
cosmetic, not a bug.

### ⚠️ Roughly 4 days of work is sitting uncommitted (found 2026-08-22)

The last commit is 2026-08-18. `git status` shows **59 untracked `Source/` files** alone —
`DCCAllyCharacters`, `DCCEnemyCharacter_DebtEater`/`_Voldemortgage`, `DCCDungeonGenerator`,
`DCCPopulationSubsystem`, `DCCCrowdSpawner`/`_Crawler`, `DCCIntroDirector`, `DCCRadiationAura`,
`DCCGameMode`, the whole `Equipment/` module, `DCCFloorExit`, `DCCQuestMarkers_Floor01`,
`DCCLootTableRoller`, `DCCPickup_Equipment`, plus a long tail of new `Content/` — plenty of it
already imported and playable (`DT_DCCEquipment`, `DT_DCCLootTables`, `DT_DCCAchievements`,
`DT_DCCQuests`, `DT_DCCDialogue`, `SK_Voldemortgage`, `SK_DebtEater`, the whole `Shared/Anims/`
set), by file mtime spanning back roughly 27–40 hours before this session and further. **This
session's own new work sits on top of all of it, uncommitted the same way.** None of it is lost —
it is all present on disk — but it exists nowhere except this one working copy. Worth a `git add`
+ commit (or several, by feature) at the next natural checkpoint, before anything happens to this
machine. Not committed automatically here per this file's own rule: only commit when asked.

This also means large parts of this file's own "Current state" narrative below predate 4 days of
undocumented work and should be read as a partial, stale picture rather than ground truth — trust
what you can verify on disk (or in the editor, once MCP is back) over what this file claims.

### Loot menu rework, chests, and ally auto-gear (2026-08-22, dispatch session, no MCP)

Written and Development-Editor-built with the editor closed the whole session (confirmed no
numbered hot-reload DLL). **Not yet seen in PIE** — this needs Zoe at the editor. Full detail is
in this session's commit; the shape of it:

- **"Search the body" is gone, replaced by a loot menu shared by corpses and chests.** First E
  press on either opens **Check Inventory / Loot All / Stow / Leave**. Check Inventory drills into
  an itemised list — click through each row and take just that item, or Back out. Stow hands
  everything left to the looter's own inventory as a **Stowed Remains** record (visible on the Tab
  menu's Inventory page, click a row to empty it) and retires the source — a corpse fades on its
  usual timer, a chest just sits there empty.
- **New interface, `IDCCLootSource`** (`Items/DCCLootSource.h`) is what makes one menu work on two
  unrelated actor types: `ADCCCharacterBase` (a corpse) and the new `ADCCLootContainer` (a chest,
  deliberately **not** derived from `ADCCCharacterBase` — same reasoning as `ADCCDialogueNPC`, see
  that class's own comment on why combat baggage should not follow a prop that never fights).
- **Real bug fixed along the way, corpse-only:** `FDCCLootDrop::EquipmentItemId` documented itself
  as forwarded onto a spawned pickup, but `ADCCCharacterBase::RollLootOntoBody`/`LootRemains` never
  carried it — `FDCCRolledLoot` had no such field, so an equipment entry on any *character's*
  `LootTable` was silently inert. **`Items/DCCLootTableRoller.cpp` already did this correctly**
  (found mid-session, already forwards `EquipmentItemId` at its own spawn site) — it just has no
  persistent "hold until searched" state, which is why it wasn't a fit for the corpse/chest menu
  below. Fixed the corpse side by adding the field and routing both rolling and spawning through
  two new shared helpers, `DCCLoot::RollLootTable` / `DCCLoot::SpawnRolledLoot`
  (`Items/DCCLootTypes.h/.cpp`) — a corpse and the new chest both call these instead of each
  carrying its own copy.
- **`ADCCPickup_Equipment`** (`Items/DCCPickup_Equipment.h` — already existed, found mid-session
  after a build error caught a duplicate-class collision; the first draft of this pass had written
  a second one into `DCCPickupVariants.h` before discovering it) is a generic ground pickup that
  resolves its own mesh from `DT_DCCEquipment` in `BeginPlay` — no per-weapon C++ class needed for
  loot. `EquipmentItemId` and `GoldValue` are now replicated on `ADCCPickup` (they were not), which
  is what lets a remote client see the right ground mesh at all.
- **Four curated chest subclasses** (`Items/DCCLootContainerVariants.h`), each with a guaranteed
  (`Chance = 1.0`) loot table, ready to place the moment MCP is back:
  `ADCCLootContainer_KitchenDrawer` (mallet/knife/skillet), `ADCCLootContainer_Armory` (axe/sword/
  shield), `ADCCLootContainer_Quartermaster` (the new ranged rows below), `ADCCLootContainer_
  ManagementLockbox` (hat/blazer/name tag + real gold).
- **Three new `DT_DCCEquipment.csv` rows for ranged gear** (`Docs/DT_DCCEquipment.csv`):
  `ConfettiCrossbow`, `NailGunPro`, `FlareGunEmergency` — real `AttackRangeOverride` values, not
  reskinned melee. **No pack installed in this project has anything bow- or gun-shaped** (checked
  every folder under `Content/` for `*bow*`/`*gun*`/`*rifle*`/`*crossbow*` meshes — nothing but
  World Partition hash-name false positives), so `ConfettiCrossbow` ships with an empty `Mesh`
  column on purpose, and the other two reuse an existing tool/lamp mesh as a placeholder the same
  way `ADCCPickup_Jukebox` reuses a kitchen appliance. Two free packs worth downloading to fix this
  properly, found this session: **Free Fantasy Weapon Sample Pack** on Fab (Prop Garden LLC,
  permanently free Unreal-sponsored content, 12 weapons incl. bow-shaped ones, static + skeletal
  mesh) and **ASIS Free Gun Weapons Pack** on itch.io (FBX + textures, a community giveaway — FBX
  imports directly through the existing pipeline, no Blender conversion needed). Only Zoe can
  download either (Fab/itch are account-gated); once landed under `Content/`, swap the `Mesh`
  column in the CSV and re-import.
- **Allies now pick up and equip gear on their own.** `ADCCEnemyCharacter::TryAutoEquipNearbyGear`
  (ally-only, throttled every ~3s like the other AI scans) compares any nearby unequipped-gear
  pickup against whatever is already in that slot — summed ability bonuses + weapon die + Evade —
  and equips it only on a strict improvement, so an ally never undresses itself for something
  worse. Calls the exact same `ADCCPickup::OnPickedUp` a player's own pickup uses.
- **`[DCC Talk]` diagnostic added for the still-open "can't interact with allies" report.**
  Zoe reported this is still broken as of this session, and nothing in the code read as visibly
  wrong on inspection — `IsHostileTo` correctly derives from `bIsAlly` via `IsCrawler()`, the sweep
  correctly finds a living ally within range. Per this file's own established pattern, the fix is
  to instrument rather than guess: every failed E-press now logs which of the four gates refused it
  (nothing hit / not a character / hostile-or-dead-or-out-of-reach / too far), tagged `[DCC Talk]`.
  **One PIE run with this filter answers the question that four sessions of re-reading the code
  have not.** The leading theory, unconfirmed: the party's own history of `bIsAlly` being `true` on
  a Blueprint CDO and `false` on the actually-placed instance (trap #2, hit at least twice already)
  — and Zoe's own recent manual map edits are exactly the kind of change that could have
  reintroduced it on whichever allies are in the level now.

**Queued for whoever has MCP next:** re-import `Docs/DT_DCCEquipment.csv` (schema unchanged, just
three new rows — plain re-import, no restart needed first); place the four curated chests
somewhere reachable; a PIE run filtered on `[DCC Talk]` to close out the ally-interaction report;
and, once Zoe has downloaded either weapon pack above, point `ConfettiCrossbow`/`NailGunPro`/
`FlareGunEmergency`'s `Mesh` columns at real meshes and re-import.

### Gotcha that cost real time this session

**Blueprint defaults do not reach already-placed instances.** All four ally Blueprints had
`bIsAlly = true` on the CDO while the four actors in the level had `false` serialised on them,
so CJ, Scott, Kyle and Marcel spent a two-player playtest beating the player to death. Fixed and
saved. **When checking whether placed actors behave a certain way, read the instances, not the
class defaults** — and note that World Partition will not return unloaded actors from
`find_actors` at all. Moving the viewport camera (`EditorAppToolset.SetCameraTransform`) to the
region streams them in.

### Fab packs installed

**~30 packs, not the 6 originally listed.** Enumerate with `AssetTools.list_folders("/Game")`
rather than trusting any written list — Zoe adds packs between sessions.

Sci-fi options worth knowing: **`Ice_Station`** (the best interior kit — crates, computers,
generator, tanks, pipes, doors, walls, holograms), **`Modular_Sewer_Kit`** (a true modular dungeon
kit: tunnels, large rooms, arches, stairs), `Megastructure_Scifi_World` (grand hallways),
`Sci_Fi_city`, `Synthwave_city`, `Alien_Portal`, `Evil_planet`, `Abandonned_Brutalist`.

`Cave_Camp` **is already placed** in Room 01 — stalactites, tent, bedroll, table, campfire emitter,
crates and two point lights. Room 01 is committed to a cave look; sci-fi went into Room 02 rather
than re-theming over it.

VFX inventory is thin: the only fire is `Legendary_Forge/Fx/Fire/NS_fire`, plus `NS_sparks` there
and `Variant_Combat/VFX/NS_Damage`. Everything else is smoke, snow, leaves and waterfalls. **There
is no purpose-built spell VFX in any pack** — a spell-effects pack is the one asset purchase that
would actually pay for itself.

Note: Claude cannot browse Zoe's Fab *library* — that is account-side behind Epic auth. Only
packs already downloaded into `Content/` are visible. Ask her to download; don't attempt to fetch.

Not yet verified: the **server target has never been built.** Launcher installs don't ship
precompiled server binaries, so expect that to be its own piece of work. Per the design doc,
hosting is secondary and must not block playtesting. (Attempting it as a compile-check workaround
does **not** work — see below.)

### The Live Coding lock blocks *all* builds, not just the editor target

Confirmed 2026-08-14. With the editor open, UBT refuses immediately with *"Unable to build while
Live Coding is active"* and never reaches compilation. This applies to the **Server target too**,
so there is no way to compile-check C++ while the editor runs — not even against a different
binary. Batch C++ work and accept it is unverified until the next close.

### Build workflow, as it actually stands (2026-08-15)

There is **no uncompiled backlog** — everything in `Source/` is built and cleanly linked.

The old constraint, for context:

- Editor **open** → builds are unsafe. With Live Coding on, UBT refuses. With Live Coding off,
  it silently hot-reloads instead (see the warning near the top of this file). Neither is a way
  to verify C++.
- Editor **closed** → clean builds work, but MCP dies with it, so no editor automation.

#### The DebugGame compile-check works. Use it. (Confirmed 2026-08-15)

With Live Coding **off** and the editor **open**, building the **`DungeonCrawlerDenver`** target
(the standalone game target, *not* `DungeonCrawlerDenverEditor`) in the **DebugGame**
configuration compiles and links cleanly, every time, without disturbing the running editor:

```
"D:/UE/UE_5.8/Engine/Build/BatchFiles/Build.bat" DungeonCrawlerDenver Win64 DebugGame \
  -Project="C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver/DungeonCrawlerDenver.uproject" -WaitMutex
```

It produces `Binaries/Win64/DungeonCrawlerDenver-Win64-DebugGame.exe` — a **standalone exe the
editor never loads**, which is exactly why it works. No UBT refusal, no hot reload, no numbered
DLLs, editor and MCP session both untouched. Verified across three builds including one that
added new `UPROPERTY`s.

Cold build ≈ 9 min; **incremental ≈ 1.5–3.5 min**, because UBT recompiles only changed files
(adaptive non-unity pulls them out of the unity blob automatically).

**What it does and does not buy you:**

- ✅ Catches compile and link errors immediately, with the editor open and MCP alive.
- ❌ Does **not** deploy anything into the running editor. The editor keeps executing the old
  `UnrealEditor-DungeonCrawlerDenver.dll`. To actually *run* a change — reflection or not — the
  editor still has to close for a Development Editor build.

So it converts "write C++ blind, discover errors at the next restart" into "write C++ verified,
deploy at the next restart". Batching reflection changes still matters; guessing whether they
compile no longer does.

MCP itself needs reconnecting after every editor restart. That is a session id the server
forgets (`LogModelContextProtocol: Error: Unknown session id ...; client should reinitialize`),
not a credential — the plugin has no authentication code at all, so there is nothing to
configure and nothing to fix.

## Working style

Build first, teach at checkpoints — Zoe wants working results, then a walkthrough of what
was built at natural milestones, not step-by-step narration. She is actively learning game
dev; explain the *why* behind UE-specific idioms when you do explain.

Prefer asking her to do a quick manual step (restart the editor, flip a setting) over
spending minutes researching an automatic workaround.

### Fourteener items, chests and the trailer (2026-11-13)
Items are in `Arcade/DCCClimbItems` (tools on Q/T, boosts, artifacts) and chests in `Arcade/DCCSupplyChest`. The kit
lives on the arena (`ADCCArcadeClimb::Kits`) and is mirrored to the owner's `UDCCWalletComponent` for the HUD.
Artifacts bank into the profile's `Trophies` only if the carrier survives the round. The ad is rendered by
`Tools/make_trailer.py` (`DCC.Trailer capture`, then ffmpeg at `D:\Tools\ffmpeg`).

### Repo Rally (2026-11-13)
A city shift, not laps. Players drive wreckers. Right mouse aims the hook, Q fires it, the mark reels onto the back bumper, and the truck tows it to the yard. The drop pays for speed and how clean the car is. Hooking the wrong car is a fine. Marks go parked, then boxed in, then driving, then they spook and leave the ground. Gas is on a tab that can go red; debt makes rival crews hunt you. They do not take marks. Roles (runner, net, winch, tank) last the shift and are not saved. The kit is mirrored onto the **rover**, because the crawler is unpossessed while driving. Rhee, Jan and Diaz explain it from the yard (`SourceArt/Voice/NPC/impound_lines.json`). Since 2026-11-13 the city is a vaporwave one built from the Synthwave/Cyberpunk/Sci-Fi packs (Claude took it over from Grok at Zoe's request).
Traffic is solid to trucks through `ADCCRover::TrafficShell` (WorldDynamic, blocks only Pawn), so it never jams itself. While a car is hooked, the
rover replicates `RallyTow`/`bRallyTowDocked`/`bRallyInYard`; the chase camera frames both cars, and the HUD shows the [Q] drop prompt.

### Controls: the belt, not a key per thing (2026-11-16)
Zoe asked for fewer keys. On foot it is WASD, Space, Shift, Ctrl, E, Tab, Esc, B, the mouse, and **1-9 = the belt**
(`ADCCCharacterBase::GetBelt`). A new usable thing gets a **belt slot** (a `EDCCBelt` kind, an entry in `GetBelt`, a
case in `UseBeltEntry`), never a new key. Aimed things arm and use on the next left click; the rest use on their
number. All food and drink share one slot; the middle mouse button picks which (2026-11-17). Rare actions are buttons (the Tab menu's row under the tabs, the B menu). Never add a `BindKey` or a
`WasInputKeyJustPressed` for a gameplay action without asking her. Test runs: `python Tools/run_test.py` (see its header).

### Gotcha: runtime-spawned rovers need a controller to move (2026-11-13)
`ADCCRover` moves with `UFloatingPawnMovement`, which only applies input when a *local controller* possesses the pawn.
Rovers spawned at runtime get none, so autopilot cars (Repo Rally traffic, crews, running marks) sat still at full
throttle. `ADCCArcadeKart::TickTraffic` gives them `SpawnDefaultController()`, and `ADCCRover::Tick` reads keys only
when the controller is a PlayerController (an AIController is also "locally controlled" on the server).

### NPC Forge and item placeholders (2026-11-14)
- **Background people:** `Tools/npc_forge.py` (Blender + MPFB/MakeHuman, seed + role -> the same person every time) ->
  `Tools/import_npcs.py` (MCP) -> `UnrealEditor.exe <project> -ExecCmds="DCC.NpcRetarget"` (editor-only C++, builds the
  IK rigs and retargets the mannequin clips to `NPC_*`). All NPCs share one skeleton. Asset packs: `D:\Assets\MakeHuman`.
- **Blender -> Unreal FBX units:** skeletal = centimetres natively (scene unit 0.01, scale x100, apply, `apply_unit_scale=True,
  FBX_SCALE_NONE`); metres make the importer scale the root bone and retargeted clips fling the body away. Static =
  `apply_unit_scale=False, FBX_SCALE_NONE`. Name the armature object `Armature` or it becomes an extra root bone.
- **MCP imports keep no materials or texture links.** Rebuild them (master material + instances), and set
  `bUsedWithSkeletalMesh` on any material a skeletal mesh or overlay uses, or it will not draw in the game.
- **`DataTableTools.get_rows` fails the whole call if any row name is missing**, and returns its JSON as a string inside
  `returnValue` (parse twice).
- Equipment: clothing and trinkets use `GroundMesh` (what lies on the floor), never `Mesh` (what is worn).
- Characters in arcade arenas get `M_CharacterShade` as an overlay (`ApplyCharacterShade`), graded from the arena's sky.
