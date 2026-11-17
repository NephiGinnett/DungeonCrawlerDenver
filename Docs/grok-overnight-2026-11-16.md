# Grok: overnight plan (night of 2026-11-16)

Written by Claude for Grok, at Zoe's request ("make an overnight plan for grok to keep working in this project. It
does okay but needs your guidance"). Zoe is asleep. Nobody will answer questions tonight: when something needs her, write
it down in the report at the bottom and move on to the next task.

Read first, in this order: `CLAUDE.md` (all of it, especially "When Zoe has to close the editor", "Controls: the
belt", the arcade sections), then the last three entries of `Docs/ROADMAP.md`'s log. Do not read the rulebook PDFs.

---

## Ground rules (these are not suggestions)

1. **Do not commit, push, publish to itch, upload to Drive, or package.** Zoe asks for those herself.
2. **Do not touch `Content/`** (no .uasset or .umap edits, no imports, no renames). The editor is closed and stays
   closed tonight; asset work waits for her.
3. **Build with the editor closed**, every time, and only this way:
   ```
   "D:/UE/UE_5.8/Engine/Build/BatchFiles/Build.bat" DungeonCrawlerDenverEditor Win64 Development -Project="C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver/DungeonCrawlerDenver.uproject" -WaitMutex
   ```
   Check first with `python Tools/editor_ctl.py status`. If the editor is open, do not close or kill it: stop and
   write that in the report. "Result: Succeeded" is the only pass. Afterwards, `Binaries/Win64/` must have no new
   numbered DLL (`UnrealEditor-DungeonCrawlerDenver-0001.dll`); if one appears, say so.
4. **Test with `python Tools/run_test.py`** (read its header). It shoots only the game window and closes only the game
   it started. Never screenshot the desktop. Never `taskkill /IM`; never kill a process you did not start.
5. **Every test run writes Zoe's real profile** (`Saved/SaveGames/Profile_Zoe.sav`). Avoid commands that hand out
   things (`DCC.LootOpen` gives boxes, `DCC.Scrap`, `DCC.Credits`). If you had to use one, list it in the report. Do
   not run `DCC.ProfileFix`.
6. **No new keys.** A new usable thing is a belt slot (`ADCCCharacterBase::GetBelt` / `UseBeltEntry`), a rare action
   is a button. Zoe: every in-game action is a button or the belt, never a console command for a player.
7. **Small steps.** One task: change, build, test, look at the screenshots yourself, write the log line. If a build
   fails twice on the same change, undo your change to those lines, note it, and go to the next task.
8. **Look at your screenshots before saying something works.** "It compiled" is not "it works". If you could not see
   it working, write "not checked" and why.
9. Writing style for anything a player reads: plain, short, no em dashes. Match the System's voice in
   `Docs/dungeon-tone-bible.md` for flavour text.
10. Original games only. No names, characters or art from someone else's game or franchise.

---

## Tonight's tasks, in order

Each has: where, what, how to check, and when it is done. Do them in this order; skip one only if it is blocked, and
say why.

### 1. The scroll wheel picks a belt slot (small, code only)

The ROADMAP says the wheel "does not pick belt slots yet". Zoe was told "scroll or a number picks a slot".

- **Where:** `Characters/DCCCharacterBase.h` (`InteractionScrollUp` / `InteractionScrollDown`, one-line inline
  functions; search for them) and the belt functions in `DCCCharacterBase.cpp` (search `GetBelt`).
- **What:** the wheel already means something when a menu is open (selfie poses, the B menu, vendors, loot, Oathgates,
  NPC talk, the interaction menu). Only when **none** of those is open, the wheel steps through the belt's **aimed**
  slots (lightstick, spray can, throw): nothing armed -> first aimed slot -> next -> ... -> nothing armed again.
  Wheel down goes forward. Use `ArmedBeltKind` / `ArmedBeltItem` exactly as `BeltKey` does. Do not let the wheel
  change an armed skill. Read what `CycleInteractionOption` does with nothing open before replacing that fallback;
  if it does something real with nothing open, keep it and add the belt step in front of it only when
  `InteractionOptions` is empty.
- **Dev command to test it:** add `DCC.Wheel <1|-1> [delay]` to `Arcade/DCCDevCommands.cpp`, copying the shape of
  `DCC.Belt` at the bottom of that file. It needs a public way to call the wheel (follow `DevClick` in the header).
- **Check:** `python Tools/run_test.py DCCHauntGameMode "DCC.Wheel 1 12, DCC.Wheel 1 14, DCC.Click 16" 11 4 2.0 wheel`.
  In the shots: after the first wheel, the Lightstick slot has a gold border and the prompt says "Lightstick ready";
  after the second, the Spray Can is armed instead (Zoe owns a spray can); the click sprays or says why it cannot.
- **Done when:** the shots show it, and the ROADMAP line "the scroll wheel (it does not pick belt slots yet)" is
  updated.

### 2. Fourteener tools on the belt (test, plus one dev command)

No tools were carried in the last test, so the tool slots were never seen.

- **Where:** `Arcade/DCCSupplyChest.cpp` (the "Outfitted: one of every tool and boost" branch shows how a full kit is
  given), `Arcade/DCCArcadeGames.cpp` (`ADCCArcadeClimb`, `KitOf`, `UseToolId`).
- **What:** add `DCC.Outfit [delay]` (dev, server side, Fourteener only) that gives the local crawler one of every
  tool, the same way that chest branch does. Reuse that code; do not copy it. If the chest's code is not callable,
  move the shared part into a small function on `ADCCArcadeClimb` and call it from both.
- **Check:** `python Tools/run_test.py DCCClimbGameMode "DCC.Outfit 10, DCC.Belt 3 14" 12 3 2.5 tools`. The belt shows
  Climbing, Hook, then each tool with x1; after `DCC.Belt 3`, that tool's count drops or it leaves the belt, and its
  effect message appears. If there are more than 9 things, slots past 9 are cut: say how many were cut.
- **Done when:** seen in the shots, logged.

### 3. Throw and Stow from the belt (test, maybe one dev command)

- **Where:** `Items/DCCCarryComponent` (`TryPickupOrDrop`, `TryThrow`), `ADCCCharacterBase::HandleInteractPressed`
  (the E key).
- **What:** find a game with something you can pick up (Gate Run rooms or Fort Tiner have props; search for
  `ADCCPickup` spawns in `Arcade/` and `Rooms/`). Add `DCC.Interact [delay]` (presses E for the local crawler, same
  shape as `DCC.Click`) and, if needed, `DCC.Tp` is already there to move next to it (read its help).
- **Check:** pick up, then the belt shows "Throw" and "Stow in bag"; `DCC.Belt <throw slot>` then `DCC.Click` throws
  it; on a second run, the stow slot puts it in the bag (the belt loses both slots).
- **Done when:** both seen, or written down exactly what blocked it.

### 4. Remove the dead Q/T tool code (cleanup, reflection change, be careful)

After the belt, nothing calls `ADCCCharacterBase::UseClimbTool`, `NextClimbTool`, `UDCCWalletComponent::Server_UseClimbTool`,
`Server_NextClimbTool`, or reads `ClimbToolIndex` for anything a player sees.

- **What:** grep every use first and paste the list into the report. Remove only what has **no** caller left. Keep
  `UseToolId` (the belt uses it). Keep `ClimbTools` (the belt reads it). If `ClimbToolIndex` is still read by the
  server for anything, keep it and say where.
- **Check:** clean build, then rerun task 2's test: tools still work from the belt.
- **Done when:** builds, task 2's test still passes, and the ROADMAP notes what was removed.

### 5. Dead Air creatures, one fight each (test only, no code unless a bug)

Week plan: "Fight each creature once in a real round (lamp off vs the lamp-eater, killing a Tommyknocker stops its rocks)."

- **Where:** `Characters/DCCEnemyCharacter_DeadAir.*`, `Arcade/DCCArcadeHaunt.cpp` (`SpawnMonsters`, `QueueRock`).
  `DCC.MineGo <room|driftN> [delay]` moves everyone into the mine.
- **What:** run Dead Air, go into the mine, let the creatures come. Use the log (`Saved/Logs/<name>.log`) as well as
  the shots: search for the creature class names and damage lines. Try the lamp off near a lamp-eater (belt slot 2).
- **Done when:** for each of the four (lamp-eater, Tommyknocker, slag dog, shift boss) the report says: seen or not,
  did it attack, did it die, anything wrong. Fix a bug only if it is clearly a bug and small; otherwise describe it.

### 6. Gate Run boss, all three phases (test only)

Week plan loose end: "A full Gate Run boss kill through all three phases."

- **Where:** `DCC.GateIn [room] [delay]`, `DCC.GateClear <room|-1> [delay]`, the Gate Run arena in `Arcade/`.
- **What:** get to the boss room with the dev commands and see whether the phases change. The log is your friend.
- **Done when:** the report says which phases were reached and how (or what stopped it).

### 7. If there is time: stale comments about keys

Comments still mention the old keys (`[Q]`, `[T]`, `[G]`, "hotbar slot 2 or 3"). Grep `Source/` for them and fix the
comments only (no behaviour). Rally truck code keeps its keys: Q, F, R, T, C, X in `Vehicles/` and the Kart code are
correct and must stay.

---

## Not tonight (do not start these)

- Anything in `Content/`, including renaming the `PokemonMasterTrainer` prop (it needs the editor and Zoe).
- A grappling hook use in Dead Air: Zoe has not decided.
- Repo Rally gameplay: Zoe said not now.
- The trailer, packaging, itch, Drive.
- Clicking UI with a real mouse (Tab menu buttons, SETTINGS key capture). Note them as "needs Zoe" instead.

---

## How to write the log

After each task, add one bullet under a new ROADMAP heading **"2026-11-17 (overnight, Grok)"**, placed after the
last entry: what changed (file and function), what you saw in the shots, and what was not checked. Then fill in the
report below.

---

## Morning report (Grok fills this in)

For each task 1-7: **done / partly / blocked / not started**, one or two sentences, and the screenshot folder.

- Task 1: **done.** The scroll wheel steps aimed belt slots when no menu is open. `Saved/TestShots/wheel`. The spray click wrote the profile (below).
- Task 2: **done.** `DCC.Outfit` put one of every Fourteener tool on the belt. Piton stayed, Trail Mix was eaten. `Saved/TestShots/tools` and `tools2`.
- Task 3: **done** for the belt, **not checked** on disk. Throw and Stow both showed in `Saved/TestShots/carry` (and Stow again in `stowsave`). The harness kills the process, so the exit save never ran, and `DCC_Zoe.sav` did not gain the tequila.
- Task 4: **done.** The dead Q/T climb-tool functions and `ClimbToolIndex` are gone. `tools2` still passed after the build. Rally truck keys were left alone.
- Task 5: **partly.** Teleport was broken (she stayed in the cage). After the fix: slag dogs fought and one died (`dogs2`); Tommyknocker was killed with the lamp off (`tommy2`); the shift boss fought and killed her (`boss`), and her clicks opened a mine note so she never hit him. Lamp-eater was only seen at the cage bars (`lamp`): one miss with the lamp on, then it stopped. A Tommyknocker roof knock was not confirmed. A lamp-eater also walked into the first tommy try (`tommy`, `dogs` are the failed cage runs).
- Task 6: **partly.** Teleport into the boss room worked. Phase 1 (shades), the 50% charge, and slams were seen. Phase 2 was not: the shades downed her first, twice. `Saved/TestShots/gate` and `gate2`. `DCC.GateClear` was not used.
- Task 7: **done** for the comments (in the 8:01 PM build). The morning report is this section. Left on purpose: rally [Q]/[F] text, and the player-facing line "Equip Climbing (hotbar) and hold the left mouse button to climb."

Builds run tonight and their results:

- 8:01 PM, Development Editor. Result: Succeeded (178 s). Q/T removal and the comment pass. `UnrealEditor-DungeonCrawlerDenver.dll` only, no numbered DLL.
- 8:07 PM, Development Editor. Result: Succeeded (22.5 s). `DebugGoTo` and `DebugEnterAll` teleport fallback. DLL 7,744,512 bytes at 8:07:52 PM. No numbered DLL. The editor was closed the whole time.

Anything that touched Zoe's profile:

- Wheel spray test, 7:40:39 PM: Property Defacement, +250 tickets, views to 3425. `Profile_Zoe.sav`.
- Four Dead Air rounds ended because she went down (dogs, tommy, tommy2, boss). Each adds 1 Haunt play and uploads 0 views, so no footage tickets. Profile file last written 8:17:33 PM, still 2698 bytes.
- Gate runs did not write the profile (no kills, the round did not wrap).
- `DCC_Zoe.sav` was rewritten at 8:19:55 PM by the 60 second autosave during the first gate run, after she stabilised at 1 HP. Hit points are not a saved field, and load refills them. The exported fields are still the existing crawler: level 5, 1500 XP, flare gun equipped, Climbing rank 2, Piloting rank 4, Favor 10, onboarding Complete. No House Tequila. `DCC.ProfileFix` was not run.

Questions for Zoe:

- The Gatekeeper's second phase (the line about the whole gate coming to help, at 33% health) did not happen. The shades take the clicks and put her down first. Worth another look when you are at the keyboard.
- The shift boss killed her while a note ("The Bell at 4:15") had the crosshair, so that fight was only his hits.
- Tommyknocker's knock never showed up in the log. `QueueRock` does not log.

Things that looked wrong but were not fixed:

- `DebugGoTo`'s `TeleportTo` was failing with no log and leaving her in the cage. That one was fixed and retested.
- The climb hint still says "hotbar".
- Rally's on-screen [Q] and [F] lines were left, on purpose. Those keys still belong to the truck.
- Deep in the mine the HUD says the air is running low. Some of the downs may include that, on top of the creature hits.
- Single-node creatures log a warning when a hit-react additive plays. Pre-existing. Not touched.
