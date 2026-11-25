# Grok: put the new Fab packs to work (plan from 2026-11-25)

Written by Claude for Grok. On 2026-11-24 and 25, Zoe added about 30 Fab packs to `Content/`: spell effects, icons,
UI sounds, announcers, weapons, armour, ready-made enemies, animations, props and new places. None of them is used by
the game yet. This plan wires them in.

It is **separate from** `Docs/grok-plan-2026-11-24-new-games.md` (the tower defense, the TVs, the Champions' Ring)
and comes **after** `Docs/grok-next-2026-11-24.md`. Where a task here helps one of those games, it says so.

**How the work is split** (Zoe's rule): Grok fills spaces and wires data; Claude sculpts rooms and models.
- Steps marked **[CLAUDE]** are room-building. They're listed so nobody does them twice. Skip them.
- Everything else is Grok's.

**Read first:**
- `CLAUDE.md`, especially "The uniform bone maker", "Importing models with full PBR", "HUD panels" and
  "Controls: the belt".
- `Docs/fab-library-audit-2026-11-24.md`: what each pack is for.

**Ground rules** (from `Docs/grok-next-2026-11-17.md`; all of them still apply):
- **Do not edit or move the packs' own assets.** Use them by reference. If one needs a change (a material flag, a
  socket), make a copy under `Content/DCC/` and change the copy.
- **No new keys.**
- **Player-facing text:** plain, short, no em dashes, the System's voice.
- **Test with `python Tools/run_test.py`**, look at your own screenshots, and log what you changed.
- **Some packs may still be half-downloaded.** If an asset fails to load ("Invalid value for PACKAGE_FILE_TAG",
  for example `Polytope_Studio/.../T_Quinn_01_CCRCCPlastic_MSK`), skip it, note it, and do not delete it.

**The packs, by folder:**

| Folder in `Content/` | What it is |
|---|---|
| `Free_Spells`, `Free_Magic`, `Basic_VFX`, `AbsorbtionFX`, `_SplineVFX`, `NiagaraExamples` | Spell and hit effects (Niagara) |
| `UltimateUIMenusSFX` | Menu and UI sounds |
| `RadioSoldier_Announcer`, `Droid_Announcer` | Announcer voice lines |
| `FPS_Weapon_Bundle`, `WeaponSniper`, `Weapons_Free` | Weapons |
| `ShadowKight`, `Polytope_Studio` (modular armours), `GanzSe_Accessories` | Armour and accessories |
| `Ghoul`, `Guard`, `ParagonSevarog`, `ParagonWraith` | Ready-made enemies with their own animations |
| `FreeSampleAnimationSet`, `Knife_FPS_Animations`, `Attaku` (finger poses), `Sample`, `MyProject` | Animations and demo content |
| `Gothic_props_1`, `ModernStreetProps_01`, `Barrels_Boxes`, `Torn_Banner_Free`, `StageProps` | Props |
| `BroadcastStudio`, `Scene_OldMine`, `AbandonedPowerPlant`, `Clinic`, `MetroMaintenanceStation` | Places [CLAUDE] |
| The late-night copy: space station, derelict corridor, villages, inferno, saloon, sci-fi kits, Paragon heroes, staffs | See Part H |

Two more sets aren't in `Content/` yet: the 500-icon packs (Rogue Skills, and Psion & Mindweaver). If they don't
show up in `Content/` (they may have downloaded somewhere else), ask Zoe before B1.

---

## Part A: spells that look like spells

The skill system already has the slots: every skill row (`FDCCSkillDef`) has `CastEffect`, `ImpactEffect` and
`ImpactEffectZOffset` (Niagara), and `Icon`. Today the spell list in `Skills/DCCSpellBook.cpp` borrows a campfire, sparks,
snow and dust motes from environment packs. Replace those with real spell effects.

### A1. Catalogue the effects
List every Niagara system in the six effect folders into `Docs/vfx-catalogue.md`. For each, record:
- its path;
- what it looks like, in a few words (fire burst, ice shards, heal ring, shield, lightning, buff aura, projectile
  trail, explosion);
- whether it **loops** (an aura) or **plays once** (a hit);
- its rough size.

Shoot the best 30 with a dev command that spawns one by path in front of the camera. Write `DCC.ShowFx <path> [delay]`
if nothing like it exists.

### A2. Give every spell its own cast and impact
1. For each spell in `DCCSpellBook.cpp` and each spell skill row (`Tools/gen_loot_content.py`), pick a cast effect and
   an impact effect from the catalogue, by element (fire, ice, lightning, poison, holy, shadow, force, heal, buff).
2. Put the choices in one place: a table in `DCCSpellBook.cpp` or the generator, not scattered.
3. Effects must fit the spell's size: a 2 m blast should not wear a 10 m explosion. Scale with the existing
   `ImpactEffectZOffset` and a scale field if one exists; if not, ask Claude before adding a reflected property.
4. **The crawler-level spell shows** (the 2026-11-22 "spell shows") get the bigger effects.

Done when every spell has a matching cast and impact, shown in a test sheet (one shot per element).

### A3. Hits and kills
- Use the small one-shot effects for weapon hits by damage type: blunt, slash, fire.
- Add a death puff for the enemies that skip the ragdoll (`bUseRagdollOnDeath = false`): the Debt Eater, the new
  bone-maker monsters.
- **Keep them cheap:** a short lifetime, and none for hits further than about 25 m away.

### A4. The tower defense crystal (for the 2026-11-24 plan, Part B5)
Pick the crystal cannon's effects:
- a charging glow that grows over 3 s (looping, scaled by charge);
- a beam or projectile for the release;
- a big impact.

Write the paths into that plan's B5 section, so whoever builds B5 uses them.

---

## Part B: icons and sounds

### B1. Skill and spell icons
1. Once the icon packs are in, match every skill and spell to an icon by meaning.
2. Fill each skill row's `Icon` (through `Tools/gen_loot_content.py`, so the choice lives in Python).
3. The belt and the Skills page already read `Def.Icon`. Check both in a test and send shots.

### B2. Menu sounds
The menus are silent or use code-made blips. From `UltimateUIMenusSFX`, pick:
- hover, click, confirm, back and error;
- open and close a panel;
- a purchase, a loot box open, a level up.

Play them from one small helper, for example `DCCUiSound::Play(EUiSound::Click)`, so every menu shares one set.
Call it from:
- the HUD's hit-box click handler;
- the Slate menus (the character sheet, the event page, the cosmetic shop);
- the loot box screen.

Keep them quiet: UI sounds sit under the game, never over Hiss.

### B3. Announcers
1. List the announcer lines in both packs (`RadioSoldier_Announcer`, `Droid_Announcer`).
2. Map them to moments: round start, wave incoming, boss incoming, victory, defeat, countdowns.
3. Write the map to `Content/DCC/Announcers/announcers.json`, keyed by moment.
4. Add a helper that plays a moment's line (2D, every client).
5. Use it first in the tower defense (wave incoming, the Wall is under attack, boss wave) and the Champions' Ring
   (round start, fight, winner).

They are the arena voices; Hiss stays the host.

---

## Part C: weapons and armour

### C1. Weapons
1. For each weapon mesh in `FPS_Weapon_Bundle`, `WeaponSniper` and `Weapons_Free`, decide whether it fits the game's
   tone (comedic, improvised, corporate-satire guns are good; realistic military rifles less so; ask Zoe if unsure).
2. For the keepers, add gear rows through `Tools/gen_loot_content.py`:
   - names and flavour in the tone bible's voice;
   - slot, damage die, range for ranged weapons, rarity;
   - a weapon skill id, per the existing pattern.
3. Several existing rows wait on real meshes: `ConfettiCrossbow`, `NailGunPro`, `FlareGunEmergency`. Point them at
   real meshes. Zoe's Downloads folder also has a flare gun, a nail gun and a revolver from the earlier sourcing (FBX,
   GLB and Blender files); import those with `Tools/import_glb_pbr.py` where they fit better.
4. Then run `DCC.MakeItemIcons` so every new weapon gets its picture.
5. Fit each grip on the pose bench (the FIT buttons), save, and log which bodies you checked.

### C2. Armour and accessories
1. Add the `ShadowKight` pieces, the Polytope modular armours and the `GanzSe_Accessories` pieces as Head, Body and
   Trinket gear.
   - Body armour is rigid: Body slot pieces hang on the spine socket.
   - The Polytope armour is made for the UE Mannequin. Pieces skinned to the Mannequin skeleton cannot be worn by
     crawlers as skinned meshes; use the rigid pieces (helmets, pauldrons, belts) as socket props, and list the
     skinned ones for Claude.
2. **Auto-fit** each Head and Body piece on every crawler body, as in `grok-next-2026-11-24.md` C1, and save the fits.
3. Run `DCC.MakeItemIcons` for the new rows.

---

## Part D: the ready-made enemies

### D1. The four that come with animations
`Ghoul`, `Guard` (Chinese Ancient Guard), `ParagonSevarog`, `ParagonWraith`. Find out each one's skeleton.
- **Already the UE Mannequin skeleton:** it can use the Mannequin clips (as the Brute, Analyst and Courier mobs do),
  or its own.
- **Its own skeleton with its own clips:** make an enemy class that uses its own idle, walk, run, attack and death
  clips. Copy the pattern of the existing mobs that are not on the crawler skeleton.
- **Rigged but with too few clips:** run it through the bone maker (`Tools/unify_rig.py`, see `CLAUDE.md`) onto the
  crawler skeleton, as with `ADCCEnemy_MushroomDemon`.

Give each a role and a short stat block in the tone bible's voice:
- **Sevarog** is a natural Ring champion or tower-defense boss (he has a huge hammer and full combat animations).
- **Wraith** is a ranged caster.
- **The Ghoul and the Guard** are wave fodder.

Check each with `DCC.SpawnEnemy`, turning the camera to face it (a shot from behind the player shows nothing).

### D2. Their sounds
Paragon characters ship with voice and effort sounds (`ParagonWraith/Audio`, and Sevarog's). Hook attack, hurt and
death sounds through the existing character sound path, if it takes a sound per class.

---

## Part E: animation packs

### E1. What's in them
List the clips in `FreeSampleAnimationSet`, `Knife_FPS_Animations` and `Attaku` (finger poses), with their
skeletons, in `Docs/animation-catalogue.md`.

### E2. Use what fits
- Clips on the Mannequin skeleton can go to the NPC Forge people (`DCC.NpcRetarget` retargets Mannequin clips to them)
  and to the Mannequin-based mobs: more idles, talks and reactions for the Safe Room crowd and the studio audience.
- **For the crawlers:** clips that would help (a dodge, more attack variety, a death, hit reactions) go on a list for
  Claude, who runs the crawler rebake (`Tools/rebake_mixamo_batch.py`). Do not rebake crawler clips yourself.
- Ignore first-person-only clips (the knife FPS arms) unless one reads well in third person.

---

## Part F: props and furniture (fill, do not sculpt)

`Gothic_props_1`, `ModernStreetProps_01`, `Barrels_Boxes`, `Torn_Banner_Free`, `StageProps`.

- **Dress existing spaces only:**
  - Gate Run's themed rooms (crates, barrels and banners in the dungeon themes, gothic props in the spooky ones);
  - the Safe Room's corners;
  - Repo Rally's city sidewalks (street props).
- Use each arena's existing `Prop(...)` helper with collision off for small clutter. Keep counts modest (a few dozen
  per room); performance matters with five players.
- Write the dressing as data (a list per room theme), so it is easy to tune.
- `StageProps` (trusses, lights, stage pieces) are for the Champions' Ring and the studio, which Claude builds; leave
  those.

---

## Part G: places [CLAUDE]

Claude builds these. Listed so nobody does them twice.

| Folder | Becomes |
|---|---|
| `BroadcastStudio` | The PRIMETIME studio (replaces the block-built set) |
| `Scene_OldMine` | Dead Air's mine dressing (timbers, carts, tracks) |
| `AbandonedPowerPlant`, `Clinic`, `MetroMaintenanceStation` | The tower defense map and the Champions' Ring stage, and Gate Run themes later |
| `StageProps` | The Ring's arena dressing and the studio's rigs |

Grok's part here is only **F-style fill** after Claude's room exists, if Claude asks.

---

## Part H: the late-night copy (added 2026-11-25 afternoon)

After this plan was first written, another batch landed in `Content/` (record: `Docs/fab-copies-2026-11-25.md`).
**Zoe's call: Claude does the world-making.** Every place, kit, sky and level below is **[CLAUDE]**. Grok's part is
the characters, the staffs, the animation catalogue and the Ring's stage names.

### H1. Places and kits [CLAUDE]
Listed so nobody does them twice. Do not open or travel to their sample maps in a test run.

| Folder | What it is | Claude's plan for it |
|---|---|---|
| `SciFi_Space_Station` | Space station interior | Gate Run's sci-fi theme rooms; a Primetime "away" set |
| `DerelictCorridor` | Rotting sci-fi corridors | Tower defense road and Gate Run's derelict theme |
| `StarterBundle` (modular sci-fi hallways, interiors, comms) | Large modular sci-fi kit | Gate Run room kit, tower defense posts |
| `ModSciInteriors`, `ModSci_Engineer`, `ModSci_EngiProps` | Modular sci-fi engineering rooms and props | Gate Run's machine rooms, the tower defense crystal chamber |
| `ElderboomVillage`, `Forest_village` | Fantasy villages | A new Gate Run theme (the village gate), Den-adjacent hub later |
| `Inferno_World_Free` | Hell landscape | Gate Run's boss floor theme; a Ring champion stage |
| `Scene_Saloon` | Historic saloon | A Neon Mile or Casino back room; a Primetime set |
| Factory Environment (merged into `Meshes`, `Materials`, `Textures`, `Fx`) | Industrial factory | With `AbandonedPowerPlant`: the tower defense map |
| `StylizedVolClouds` | Sky and cloud presets | Arena skies (each arena builds its own sky) |
| `SuperGrid` | Prototype grid materials | Blockout material for new rooms |

Grok's part here is only Part F-style fill after a room exists, if Claude asks. For that fill (and Part F), every
prop's size in cm is in `SourceArt/Kits/<Pack>.json`, and `Docs/kits.md` says which packs are rooms and which are pieces.

### H2. New characters [GROK]
| Folder | What it is | Suggested role |
|---|---|---|
| `ParagonSerath` | Winged warrior, full Paragon animation set | A Ring champion (late ladder boss) |
| `ParagonMuriel` | Caster and healer, full set | A Ring champion who heals her adds |
| `ParagonZinx` | Cyborg medic, full set and audio | Tower defense elite, or an arcade NPC |
| `Characters/Echo` | UE5 sample hero with rigs | Check the skeleton; an NPC or a Primetime guest look |
| `Characters/Paragon`, `UE4_Mannequin`, `UE5_Mannequins`, `UEFN_Mannequin` | Shared Paragon and mannequin bases | Leave alone. They are dependencies of the packs above |
| `MetaHumans/Kellan` (from the Game Animation Sample) | A MetaHuman | Leave alone unless Claude asks |

Do these the same way as Part D1: find the skeleton, use the pack's own clips when it has a full set, and give each
one a role and a stat block in the tone bible's voice. Check with `DCC.SpawnEnemy`, with the camera facing it.
**The Paragon heroes are big downloads.** If an asset fails to load, skip it and note it.

### H3. Magic staffs [GROK]
`RPG_Magic_Staff_Pack`: staves as weapons, the same route as Part C1 (rows through `Tools/gen_loot_content.py`,
icons with `DCC.MakeItemIcons`, a grip on the bench). Give spellcasting perks or bonuses to them rather than flat
damage, so a caster has a reason to carry one.

### H4. The Game Animation Sample [GROK]
Merged into `Levels`, `IsolatedExamples`, `Blueprints`, `Widgets` and `Sequences`. Its value is the **animation
set** (a big Mannequin locomotion library). Add its clips to `Docs/animation-catalogue.md` (Part E1), and pick
idles, turns and walks for the NPC Forge people and the studio audience (Part E2). Do not wire its motion-matching
Blueprints into our characters.

### H5. The Champions' Ring: stage names [GROK]
All 50 stages in `Content/DCC/Ring/ladder.json` are named "Stage N". Give each a real name in the System's voice
(from `Tools/gen_ring_ladder.py`, not by hand). The title strip shows the name only when it is not "Stage N".

---

## Part I: the simple things every place needs (added 2026-11-25 evening) [GROK]

Zoe: "make sure simple things are included, like opening and closing doors." Claude built the first one; the rest are
Grok's. They all live in the set layer (`Arcade/DCCSets.cpp`, `Arcade/DCCSetDoor.h/.cpp`), so every place gets them at
once. Read `Docs/kits.md` first.

**Done (Claude): working doors.** `UDCCSetDoorComponent`. Every set's doors are found by name and size
(`LooksLikeDoor`) and open by themselves when any character comes within about 2.5 m, then shut 2 s after the doorway
is clear. Narrow leaves swing away from the opener; wide doors slide along the wall. `SetLocked(true)` keeps one shut.

### I1. Door sounds
A creak or a slide when a door starts to open and when it shuts. Add two `USoundBase*` to the component, chosen by
`bSlide`, from what the packs have (search `Content/` for door, creak, slide, hiss; `UltimateUIMenusSFX` as a last
resort). Play at the door, quiet enough that ten doors on a wave do not stack into noise (one sound per door per second).

### I2. Lockdowns (a twist, uses SetLocked)
Hiss (Primetime) or a tower defense event can lock doors: "LOCKDOWN. The side door is staying shut." In tower defense
a locked route door turns a wave back to the yard for 15 s. Add a small `DCCSets::LockDoorsNear(World, At, Radius,
bLock)` helper; do not change the door's opening rules.

### I3. Loose props
Small clutter in a set (bottles, cans, buckets, small boxes: under about 60 cm and not attached to a wall) should get
knocked around: on load, turn on physics for meshes under that size whose names say what they are (bottle, can, cup,
bucket, box_small, debris_small...). Keep it a short allow-list, not "everything small": a physics bottle on a shelf
falls off at load. Cap it at 60 per set for cost. A `FOptions::bLooseProps` switch, on by default.

### I4. Sit on seats
Chairs, stools, benches and sofas in a set: E sits, E or moving stands. There is already a sit for the casino
(`DCC.CasinoSit` and what it calls); reuse it. Find seats by mesh name (chair, stool, bench, sofa, couch, seat) and
height (seat top 40-60 cm). The Broadcast Studio set is full of them; Primetime's interview couch can use this.

### I5. Lamps that react
A set's point and spot lights flicker for a second when something heavy dies or explodes within 6 m. Cheap and makes
the Power Plant feel alive. Only lights that came with the set.

Test each in the tower defense hall (`DCCSiegeGameMode`, the Power Plant set) with `python Tools/run_test.py`, and the
studio once Claude builds Primetime there. Log in `Docs/ROADMAP.md`.

---

## Order of work

1. **A1 and A2** (spells). This is the most visible change to every game.
2. **B2** (menu sounds) and **B3** (announcers).
3. **C1 and C2** (weapons and armour), with icons and fits.
4. **D1 and D2** (enemies).
5. **B1** (icons), once the icon packs are found.
6. **E** (animations), then **F** (props).
7. **H5** (Ring stage names) whenever there is a spare half hour; **H2, H3, H4** after D.
8. **Part I** (simple things): I1 and I2 first, they are small and make the new tower defense hall feel finished.

Log each finished part in `Docs/ROADMAP.md`, with shots, as before.
