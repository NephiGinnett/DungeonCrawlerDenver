# Model Assignments — who has a model, who still needs one

**Started 2026-09-01** at Zoe's ask: *"please keep track of which characters and enemies need
models. please source them from our library when possible."*

**Living document. Update it in the same pass that adds a character** — the whole point is that
nobody has to re-derive this by grepping constructors.

Sourcing rule, in order of preference:
1. Something already installed under `Content/` (enumerate with `AssetTools.list_folders`, not
   from memory — Zoe adds packs between sessions).
2. A Meshy generation, converted via `Tools/glb_to_fbx.py` and bound to
   `SK_DCCCrawler_Skeleton` where it needs to share the crawler animation set.
3. A new pack — **only Zoe can download these** (Fab/itch are account-gated). Ask; don't fetch.

---

## Crawlers (the party)

| Character | Model | Skeleton | State |
|---|---|---|---|
| CJ | `SK_HockeyWings` | `SK_DCCCrawler_Skeleton` | ✅ |
| Scott | `SK_OfficeAirplane_Shared` | shared | ✅ |
| Kyle | `SK_ArmsOut_Shared` | shared | ✅ |
| Marcel | `SK_DiceWinged_Shared` | shared | ✅ |
| Vaughn | `SK_Vaughn_Shared` | shared | ✅ fixed 2026-08-31 — the class had pointed at a path that was never created |

## Bosses and named enemies

| Enemy | Model | State |
|---|---|---|
| Voldemortgage | `SK_Voldemortgage` | ✅ |
| Debt Eater | `SK_DebtEater` | ✅ |
| Fine Print (Oathgate) | `Slime_SK` | ✅ |
| The Arbiter (Oathgate) | `ParagonSerath/Serath` | ✅ |
| Nuclear Dad / Mom / Kid | mannequin-derived | ⚠️ placeholder — a family that reads as a *family* would land the Rocky Flats joke far harder |
| Test Boss | mannequin | ✅ (a test asset; no model owed) |

## Wildlife (added 2026-09-01)

All five reuse **MonsterForSurvivalGame**, which shipped fully animated and sat unused.

| Enemy | Model | State |
|---|---|---|
| The Auditor | `Beholder_SK` | ✅ |
| Thornback | `Cactus_SK` | ✅ |
| Unclaimed Property | `ChestMonster_SK` | ✅ |
| Break Room Mold | `Mushroom_SK` (AngryVersion anims) | ✅ |
| Shell Company | `TurtleShell_SK` | ✅ |

## The Mine (added 2026-09-01)

| Enemy / NPC | Model | State |
|---|---|---|
| Legacy System | `TurtleShell_SK` @ 1.6 scale | ⚠️ **shares Shell Company's mesh.** Works — it reads as bigger and slower — but two enemies with one silhouette is a real cost on a floor where the answer to each is different. First candidate for a Meshy generation. |
| Reconciliation | `Beholder_SK` | ⚠️ shares The Auditor's mesh, same caveat. A *pair* of identical bodies is at least thematically right here. |
| Survey Drone | `Swarm08_SK` | ✅ full anim set (idle/move/attack/hit/die), previously unused. `Swarm09_SK` is still free if a second drone type is wanted. |
| Claims Adjuster Pell | `SK_DonMinion` | ⚠️ placeholder — a suit would be better |
| Site Foreman Dunbar | `SK_BuffMinion` | ⚠️ placeholder — wants hi-vis / hard hat |
| Steward, Local 4-C | `SK_ArmsOut_Shared` | ⚠️ placeholder, and it is **CJ's own crawler mesh** — actively confusing. Highest-priority swap on this page. |
| The Surveyor | `SK_DiceWinged_Shared` | ⚠️ same problem — Marcel's mesh. Should be an older, plainer body. |

## NPCs elsewhere

| NPC | Model | State |
|---|---|---|
| Bartender / Lift Attendant | `SK_DonMinion` | ✅ acceptable |
| Guildmaster (Game Guide) | `ParagonMuriel` | ✅ |
| Vendors, guards | mannequin / minions | ⚠️ placeholder |

---

## The actual problem, stated plainly

**Seven characters are wearing another character's face.** Reusing a monster mesh across two
monsters is a cosmetic compromise; putting *a player character's model* on an NPC is a
readability bug — a player who sees the Steward at the mine mouth has every reason to think
Kyle walked up there.

Priority order for whatever gets fixed first:

1. **Steward, Local 4-C** and **The Surveyor** — both are wearing party members' bodies, and
   both are characters the party talks to at length.
2. **Legacy System** — the one enemy on the floor whose whole design is "this is not what you
   think it is," currently sharing a silhouette with an ordinary tank.
3. **The Nuclear Family** — built, never placed, and the joke depends entirely on the visual.

## What the library still has spare

Genuinely unused and available: `Swarm09_SK`, `ParagonZinx`, `ParagonMuriel` (partly used),
`AIGeneratedRobots1` (MechanicalHeron / MotorcycleDog / RobotOstrich — **static** meshes, so
they need attaching to a character rather than driving one), `DeadBodies_Poses_nikoff`,
`AE_BR_Props`, `Weapon_Pack`, `Atris_swords`.

**`Weapon_Pack` and `Atris_swords` are worth a look before buying anything** — the equipment
notes in CLAUDE.md record that `ConfettiCrossbow` shipped with an empty `Mesh` column because
no bow- or gun-shaped mesh could be found. That search predates noticing these two packs.

Environment packs that turned out to matter and were not being used: **`SoulCave`**
(`SM_EntranceRockCliff2-7` are mountain-scale cave mouths, ~3300×3200×2100 each),
**`Desert_cave`** (mine carts, cases, rope, ground meshes), **`SciFi_Cliff`**, and
`Alien_Cave_biome` for anything that should stop looking man-made.
