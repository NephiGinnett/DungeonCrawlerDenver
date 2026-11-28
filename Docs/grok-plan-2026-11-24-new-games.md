# Grok: new assets, Dungeon Crawler Tower Defense, lobby TVs and the Champions' Ring (plan from 2026-11-24)

Written by Claude for Grok, from Zoe's ideas on 2026-11-24. Four pieces of work, in order:

- **Part A:** bring in the new free models from the Drive.
- **Part B:** a co-op tower defense, *Dungeon Crawler Tower Defense*.
- **Part C:** TVs in the Safe Room and the casino.
- **Part D:** an arena fighting game, *The Champions' Ring*.

**How the work is split.** Zoe's note on who does what: Grok is great at filling spaces and working with data, and Claude
is better at sculpting spaces and models.
- **Claude** builds every new room, stage, tower and mesh-heavy actor first. Those steps are marked **[CLAUDE FIRST]**,
  and their Grok steps wait on them.
- **Grok** does the rest: the rules, the waves, the numbers, the data files, the rewards, the HUD, the dev commands
  and the tests.

When a Grok step needs a space that does not exist yet, stop that step and say so in the log. Do not build the room
yourself.

**Read first:**
- `CLAUDE.md`, all of it.
- `Docs/grok-next-2026-11-24.md`, which is still current: finish its Part A1, the two-player PRIMETIME test, before
  starting here.
- The last ten entries of `Docs/ROADMAP.md`'s log.

The ground rules in `Docs/grok-next-2026-11-17.md` still apply. The ones that matter most here:
- **No new keys.** A new action is a belt slot, the E interaction, or a button.
- **Do not touch `Content/`** except through the import tools named below.
- **Player-facing text** is plain and short, with no em dashes, in the System's voice (`Docs/dungeon-tone-bible.md`).
- **Original games only.** These are games *in a genre*, never a copy of a specific game.
- **Test runs play as Zoe and write her real profile.** Say what a test earned.

---

## Part A: the new models on the Drive

The Drive folder **"Dungeon Crawler Denver - Free Assets"** (made 2026-11-24, see its README) holds free Quaternius
packs. A second folder, **"TheFamiliar Set"** (2026-11-18), holds Mixamo animations.

| Pack | Where | License | Use it for |
|---|---|---|---|
| Universal Base Characters | Drive, split zip `.part00` and `.part01` | CC0 | Bodies for named NPCs, tournament bots, the TD's defenders |
| Modular Character Outfits, Fantasy | Drive, `.part00` to `.part03` | CC0 | **Armour that fits**: the outfits are skinned to the same bodies, which is the real fix for Vaughn's "nothing fits" |
| Bestiary, Dungeon Monsters Kit | Drive, one zip | Quaternius license (commercial OK) | Monsters for the Champions' Ring and the TD camps. **No animations**: see A3 |
| Ultimate Monsters | **Not on the Drive** (quota). Official folder in the README | CC0 | Animated monsters: the best fit for TD waves |
| Ultimate Modular Men | Not on the Drive. Official folder in the README | CC0 | More NPC bodies |
| Zombie Apocalypse Kit | Not on the Drive. Official folder in the README | CC0 | Animated zombies: perfect wave fodder ("debtors") |
| TheFamiliar Set (Mixamo) | Drive, loose `.fbx` | Mixamo | Sitting Talking, Sitting Laughing, Kneeling, Laying, dances, a bow, a wave, Beckoning, Crawling, catwalk |
| Combat clips (Mixamo) | Drive, loose `.fbx` | Mixamo | Throw, sword and shield attack and block, melee horizontal and downward, draw arrow |

Free Fab listings that need Zoe's Epic login are in the README. One is time-sensitive: **Absorption FX, free until
December 1, 2026.** Zoe claims those herself.

### A1. Join and unpack (Grok)
1. Download the split zips.
2. Join them as `HOW_TO_JOIN.txt` says.
3. Unpack each pack into its own folder under `SourceArt/Quaternius/<Pack>/`, never into `Content/`.
4. Write `SourceArt/Quaternius/INVENTORY.md`: per pack, every model file with its type (character, outfit piece, monster,
   prop), its triangle count if the glTF says, and whether it has a skeleton and animations.

Done when the inventory exists. It is the list everything below picks from.

### A2. The first character of each kind [CLAUDE FIRST]
**Done for rigged monsters (2026-11-25):** `Tools/unify_rig.py` + `Tools/import_unified.py`, written up in `CLAUDE.md`
("The uniform bone maker"). Examples: `ADCCEnemy_MushroomDemon`, `ADCCEnemy_DemonBull` in `Characters/DCCEnemyCharacter_Unified`.
For each new rigged monster: convert, preview, import, add a subclass, then check it with `DCC.SpawnEnemy`. Still
to do by Claude: the Universal Base Characters and outfits.
Claude imports one Universal Base Character and one outfit with `Tools/import_glb_pbr.py`, works out the retarget
onto our skeletons (as `DCC.NpcRetarget` does for the NPC Forge), and writes the recipe into `CLAUDE.md`.

### A3. Then the rest (Grok, after A2)
Follow Claude's recipe for:
1. **The bodies and outfits:** each goes into the NPC pool (`Docs/model-assignments.md`: replace the placeholders it
   lists first: Steward, Surveyor, Claims Adjuster, Site Foreman).
2. **The outfit pieces as gear:** add `DT_DCCEquipment` rows through `Tools/gen_loot_content.py`, each with a fitting
   test on the pose bench.
3. **The Bestiary monsters:** they have no animations. Use them only as things that stand still or move rigidly:
   - turret-like enemies;
   - the TD's neutral camps (they can bob and turn in code);
   - arena statues and props.
   Do not give them a walk.
4. **The Zombie Apocalypse Kit and Ultimate Monsters**, once Zoe has downloaded them: animated enemies for Line of
   Credit's waves.

Done per pack when the inventory marks each item imported, skipped, or why not.

### A4. Credits (Grok)
Every CC-BY asset that ships needs its credit line in the game. Add a CREDITS page to SETTINGS > MORE that reads
`Content/DCC/Credits/credits.json` (Grok writes it from the README and each `SOURCE.txt`). CC0 packs get a thank-you
line too.

---

## Part B: *Dungeon Crawler Tower Defense*, a group tower defense

**The name (Zoe, 2026-11-24):** *Dungeon Crawler Tower Defense* for now. *Line of Credit* is kept for a possible
office-themed version later (office workers as the waves, office-themed bosses); do not build that version now.

**The pitch.** The dungeon sends wave after wave of monsters down one road toward **the Wall**, the last thing between
them and the Safe Room. The players do not buy towers; **they are the towers**:
- they stand on the road and fight;
- they post their summoned allies on fixed towers to cast;
- they climb a tower to channel a huge spell from its crystal cannon.

Solo play stalls around wave 12 or 13. Higher tiers need more people. Neutral camps off the road give power-ups worth
leaving the road for.

### B0. The map [CLAUDE FIRST]
Claude builds the arena at runtime (`ADCCArcadeArena` subclass `ADCCArcadeSiege`, game id "Siege", a pad in the Safe
Room). It will have:
- **One winding road** from the monsters' gate to the Wall, about 60 m long, as a list of waypoints
  (`TArray<FVector> Road`).
- **The Wall:** a wide gate with health. Enemies that reach the target zone in front of it damage it while they live.
- **Four towers along the road.** Each tower has:
  - a ladder at the base (E to climb; no Climbing skill needed);
  - a platform at the top with a **crystal cannon** hanging over it;
  - an **ally post** at its foot.
- **Two jungle camps**, clearings off the road with a shrine each.
- A **command board** by the Wall showing the wave, the Wall's health and who is where.

Everything Grok builds below hangs off these actors. Their names and the API Claude leaves are listed in the arena
header when B0 is done.

### B1. Waves as data (Grok)
Waves live in `Content/DCC/Siege/waves.json` (written by `Tools/gen_siege_waves.py`, so the numbers are edited in
Python):

```json
{ "tiers": [ { "from": 1, "to": 9, "hp": 1.0, "reward": "tickets" },
             { "from": 10, "to": 19, "hp": 2.2, "reward": "salvage" },
             { "from": 20, "to": 999, "hp": 4.5, "reward": "loot" } ],
  "waves": [ { "wave": 1, "groups": [ { "enemy": "Debtor", "count": 6, "every": 0.8, "delay": 0 } ] } ] }
```

- Waves after the last written one are generated by formula (below), so the game never runs out.
- Groups **stagger**: each enemy in a group spawns `every` seconds after the last, and the group starts `delay`
  seconds into the wave. Every 5th wave has a mini-boss, and every 10th a boss.
- The **enemy roster** is its own table (`enemies.json`): class, base health, speed, the Wall damage per second, and
  ticket value. Start with enemies that already exist (Debt Eater, Corporate, Wildlife, Dead Air). Add the zombies
  and monsters from Part A when they are in.
- The spawner reads the JSON at round start (no rebuild to retune) and spawns at the gate.

### B2. The march and the Wall (Grok)
- Enemies walk the waypoints straight to the Wall, with no navmesh and no wandering. They turn to fight a player only
  if one is within 3 m of the road in front of them, then go back to the road.
- At the target zone they stop and hit the Wall: `WallDps` per enemy, per second, while alive.
- The Wall has 2000 health (data). At 0 the round ends: a loss, paid for the waves cleared.
- **Bodies vanish on death in this game** (no corpse, no ragdoll, no loot menu), because there will be hundreds.
  Kill pops a small ticket coin that flies to the killer's HUD.

### B3. The difficulty curve: a solo player stalls at about 12.5 (Grok)
This is the heart of it. Do it with **numbers first**, then tests.

1. **Measure real damage per second.** Use `DCC.Fight` and the combat log to measure a level 1, 5 and 10 crawler
   (melee and spells) and a level 1 summoned ally. Put the measurements in the log.
2. Write `Tools/siege_balance.py`: a simple simulation. Each wave's incoming health per second (count x health over
   its spawn window plus travel time) against the team's measured damage per second x 0.65 (time lost walking and
   missing). The Wall soaks the difference.
3. **Tune the formula** so that:
   - a **level 5 player with 2 allies** clears wave 12 and loses the Wall during wave 13;
   - **two players** reach about 20;
   - **five** reach about 30;
   - **towers with allies posted** add about 15%;
   - a channelled crystal add about 20% while someone is up there.
4. Starting point:
   - Health: `H(w) = base x 1.16^(w-1) x tier`.
   - Count: `N(w) = 6 + 1.6w`.
   - Every 5th wave adds a mini-boss worth 8 normal enemies.
5. **Melee keeps up at high tiers:** from wave 10, melee damage x `1 + 0.05 x (wave - 10)`, and the weapon mesh grows
   by `1 + 0.03 x (wave - 10)`, capped at 2x. Scale the equipment's slot mesh, not the actor.

Done when the simulation's table, and two test runs (solo to the stall, and with `DCC.Summon` allies), agree within a
wave or two, and the log has both.

### B4. Allies on towers (Grok)
- Near a tower's ally post with a summoned ally following you, the E interaction offers **"Post <name> here"**. The
  ally walks to the post and stays.
- A posted ally casts its **first damaging skill it knows** at the nearest enemy on the road in range, on that skill's
  cooldown, with no mana cost.
- If it knows none, it says so ("I don't know any spells. I brought a rock.") and throws rocks for small damage.
- E again on the post, or the round ending, calls it back.
- Show who is posted on the command board.

### B5. The crystal cannon (Grok, on Claude's tower)
- At the top of a tower, E enters **channel mode**:
  - the body stays put, the camera aims, and a reticle shows on the road;
  - **hold E to charge for 3 seconds** (a ring fills on the HUD and the crystal glows brighter);
  - release, or reach full, and the crystal fires the player's **selected belt spell**, amplified: power x 2.5 and
    radius x 1.5, at the aimed point;
  - then it charges again.
- No mana cost while channelling, but a 1.5 second cooldown.
- Space or Esc leaves channel mode.
- A player with no spell fires a plain crystal bolt.
- Server-checked: the aim point must be in range of that tower.

### B6. Jungle camps and power-ups (Grok)
- Each camp holds 3 to 5 **neutral** monsters (Bestiary statues come alive, or existing wildlife). They ignore players
  until attacked, and leash back to the camp.
- Clear a camp and its shrine offers a power-up to everyone who took part. It lasts 5 waves. The camp comes back after
  5 waves.
- Power-ups are data (`powerups.json`). Start with:
  - **Amplified:** spells +40% damage.
  - **Comically Large:** your weapon is 3x its size and melee +60%. The weapon mesh really grows.
  - **Cold Call:** your hits slow enemies 40% for 2 s; every 5th hit freezes for 1 s.
  - **Commission:** your kills drop double tickets.
- Show active power-ups as chips on the HUD.

### B7. Rewards (Grok)
- **Waves 1 to 9:** tickets only, per kill.
- **Waves 10 to 19:** tickets plus **Salvage**, a new crafting material. Crafting does not exist yet, so for now
  Salvage is a profile counter (`Siege.Salvage`) shown on the results screen and spendable at nothing. Write down that
  it is waiting on a crafting system, and ask Zoe before building one.
- **Wave 20 and up:** the enemies' real loot through `DCCLoot::RollLootTable`, as Gate Run does, but held until the
  end.
- **Everything is paid at the end of the round.** On top of that:
  - a bonus per wave cleared;
  - a bigger bonus at every 5th wave;
  - more at the 10th, 15th and 20th.
  Data in `waves.json` under "bonuses".
- The results screen lists:
  - waves cleared;
  - the Wall's health left;
  - kills, tickets, salvage and loot per player;
  - the best wave per player, in the profile as `Siege.BestWave`.

### B8. HUD, commands and tests (Grok)
- **HUD:** the wave and the next wave's countdown, the Wall's health bar, enemies left, the active power-ups, the
  channel ring, and the posted allies.
- **Dev commands:** `DCC.SiegeWave <n>` (jump to wave n), `DCC.SiegeWall <hp>`, `DCC.SiegeCamp <0|1>` (clear a camp),
  `DCC.SiegePost` (post your ally at the nearest tower).
- **Tests:** one solo run to the stall, one with allies, one using the crystal (a shot of the charge ring), one
  clearing a camp (a shot of a Comically Large weapon).

---

## Part C: TVs in the Safe Room and the casino

**The idea:** a few TVs on the walls play Hiss Cranson's show clips, commercials and "breaking news" about the players,
like a bar TV. **Faintly audible right next to them** (Zoe, 2026-11-24): the sound fades to nothing a few metres away,
so a room full of TVs never turns into noise.

### C0. The TV [CLAUDE FIRST]
Claude makes the TV:
- a bezel and screen actor that plays a video file through Unreal's Media Framework (a `UMediaPlayer` into a media
  texture on the screen's material);
- a fallback mode that shows a still picture with a caption when no video is set;
- a news ticker strip along the bottom.

Claude places 3 TVs in the Safe Room and 2 in the casino. Media Framework needs the `MediaAssets` module and one
material asset, which is why Claude does this part.

### C1. The playlist (Grok)
- `Content/DCC/TV/playlist.json` lists clips: a file under `Content/Movies/TV/`, a title, how often it plays, and which
  rooms show it.
- TVs shuffle through it, never the same clip on two TVs at once.
- **Sound:** each TV plays its clip's audio quietly and attenuated: full (but soft) within about 2 m, silent past about
  5 m. Use the media player's sound component with a short attenuation radius, and keep the captions on, so a clip
  still reads with no sound.

### C2. Make the content (Grok)
Grok does the work that is mostly scripting:
1. **Commercials (6 to 10, 15 to 30 seconds each)** for the world's sponsors (Mile High Recovery Solutions, the
   Lucky Tuesday, Cillinan's prize counter, the casino, Repo Rally's yard). Write each as a short script: shots, a
   title card, a tag line. Then capture the shots in game with the trailer pipeline (`DCC.Trailer capture`,
   `Tools/make_trailer.py`, ffmpeg at `D:\Tools\ffmpeg`).
2. **Primetime clips:** capture Hiss's studio during a test episode (the couch, the verdicts) and cut 10 to 20 second
   clips with on-screen captions, since there is no sound.
3. Keep every file under 15 MB, 720p, H.264, with a quiet mix (the TVs are background, not the room's music).

### C3. Breaking news (Grok)
Big moments write a line to the host's `Saved/TVNews.json`:
- a jackpot;
- a boss down;
- a Fame level;
- a PRIMETIME Defy;
- a Tower Defense milestone;
- a Champions' Ring title.

The TVs' ticker scrolls the latest 10 in the System's voice ("BREAKING: Vaughn defies Hiss Cranson on live
television. Ratings up 160."). Use `DCCAudience::Moment` as the hook where it already fires.

---

## Part D: *The Champions' Ring*, an arena fighting game

Two modes on one stage:
- **The Ladder (solo):** a climb through stages of enemy pairs and groups. Take the next stage whenever you're ready,
  and bank rewards as you go. A champion waits at every 10th stage.
- **The Tournament (group):** everyone in the lobby enters a bracket. Empty slots fill with beefed-up bots or
  champions. The winner gets a title and a class item. The best prizes only come with two or more real people.

### D0. The stage [CLAUDE FIRST]
Claude builds the arena (`ADCCArcadeRing`, game id "Ring"):
- a round fighting stage with an edge;
- stands of NPC spectators (the Primetime crowd code);
- two entrance gates, champion banners, a scoreboard;
- a **ready podium** between stages.

Claude also makes or imports the champion bodies Zoe names.

### D1. The Ladder as data (Grok)
- `Content/DCC/Ring/ladder.json` lists the stages:
  - the enemy group (classes and counts, using the Bestiary and other new models from Part A once they exist);
  - a health multiplier;
  - the reward.
- Stage health grows about 12% per stage. Write 50 stages by formula in `Tools/gen_ring_ladder.py`, then hand-tune
  the champion stages.
- **Between stages:** the ready podium. Press E to start the next stage, or leave and bank. Rewards are paid per
  stage, and the best stage is saved (`Ring.BestStage`).
- **Every 10th stage is a champion:** a named fighter with a twist. Beat one and they **teach you something**: a spell
  (a `UDCCSkillComponent` grant, Rank 1) or a fighting style (a new attack style in `Characters/DCCAttackStyle`).
  Champions and what they teach live in `champions.json`.
- **Stage 50:** an Epic weapon (a new gear row) and a nameplate title.

### D2. The Tournament (Grok)
- A bracket of 4 or 8 entrants:
  - every player in the lobby;
  - then bots to fill: summoned-ally style AI with a player's kit and a stat multiplier;
  - and a champion as the final seed.
- **PvP inside this arena only:** `ADCCCharacterBase::IsHostileTo` asks the arena whether players may hit players.
  Add that one hook; do not change anything else about who is hostile.
- **Matches:** best of 1, a 90-second clock, health decides at time-out. Others watch from the stands.
- **The winner gets:**
  - the **title** (for example "CHAMPION OF THE RING");
  - a **class item** for their class group (melee or caster), from a short list in `champions.json`.
- **Better prizes need real people:** with 2 or more humans in the bracket, the winner's item is Epic, otherwise Rare.
  Solo players can still farm the Rare by fighting bots.
- **Hiss commentates** (new `P_RING_*` lines in `Tools/gen_talkshow_lines.py`, voiced with Kokoro).

### D3. Tests (Grok)
- **Ladder:** stages 1 to 3 played, stage 10's champion beaten with `DCC.RingStage 10`, and the taught spell showing
  on the belt.
- **Tournament:** one solo tournament (the bot bracket) and one two-player tournament (the PvP hook).

---

## Order of work

1. **Part A1** (unpack and inventory). Claude does **A2**. Then **A3** and **A4**.
2. **B0 (Claude)**, then **B1 to B8**. B3's balance work is the most important step in this plan.
3. **C0 (Claude)**, then **C1 to C3**.
4. **D0 (Claude)**, then **D1 to D3**.

## Answered and still open

- **Answered (2026-11-24):**
  - **The TVs** are faintly audible right next to them (see C1).
  - **Names:** *The Champions' Ring* stays. The tower defense is *Dungeon Crawler Tower Defense*.
  - **Bhor** is a character from the friends' old D&D campaign, with no model and not in the game yet. **Leave him out
    for now.** Zoe thinks he would make a fun boss phase later; write that down as a future champion, but build
    nothing for him.
- **Still open, do not guess these:**
  1. "Cillinan riding a giraffe pillar": a giraffe he rides into the ring, or a pillar shaped like a giraffe? Leave
     Cillinan's champion stage as a plain Cillinan fight until Zoe says.
- **Crafting (answered 2026-11-26):** she wants it. Alchemy, Mechanics, and the same shape as the books' professions,
  in our own words. The brief for the next session is `Docs/crafting-benches.md`. Salvage is the material. The Chop
  Shop becomes the Mechanics bench. Alchemy is a new tab. The Prize Counter stays a ticket shop. Not built this pass.
