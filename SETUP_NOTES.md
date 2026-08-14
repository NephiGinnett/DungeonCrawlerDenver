# SETUP_NOTES.md

How the Claude ↔ Unreal Editor bridge works on this machine, and what you have to do by hand.

Last verified: 2026-08-14.

---

## TL;DR — starting a session

1. **Open the editor** (double-click `DungeonCrawlerDenver.uproject`, or run `OpenDungeonCrawlerDenver.bat`).
2. **Start Claude Code from this folder** — not from `D:\UE\UE_5.8`. The `.mcp.json` that wires up the
   bridge is picked up from the working directory.
3. That's it. There is no separate bridge process to launch, no `pip install`, no Python server.

If Claude says the Unreal tools are missing, see [Troubleshooting](#troubleshooting).

---

## What's actually installed

**Nothing new was installed.** This is worth being explicit about, because the obvious move when
setting up "Unreal + Claude" is to go find a community `UnrealMCP` plugin on GitHub. **Don't.**

UE 5.8 ships its own first-party MCP server as the built-in **`ModelContextProtocol`** plugin, and
this project already has it enabled. A third-party bridge would be redundant, would fight over
port 8000, and would need rebuilding against every engine update. The native one is already
working and is maintained by Epic.

### The pieces

| Piece | What it does | Where |
|---|---|---|
| `ModelContextProtocol` plugin | Hosts the MCP server inside the editor | Enabled in `.uproject` |
| `.mcp.json` | Tells Claude Code where to find it | This folder |
| `*Toolset` plugins | Provide the actual tools | Enabled in `.uproject` |

The server listens on **`http://127.0.0.1:8000/mcp`**. It is **loopback-only** — `127.0.0.1` is not
reachable from other machines on your network, so this is not something other people can connect to.

`.mcp.json` is only these six lines:

```json
{
	"mcpServers":
	{
		"unreal-mcp": { "type": "http", "url": "http://127.0.0.1:8000/mcp" }
	}
}
```

### Key insight: the server is only the transport

`ModelContextProtocol` being enabled and port 8000 answering proves **nothing** about what Claude can
actually do. The tools come from separate `*Toolset` plugins. Without `EditorToolset`, there is no way
to spawn an actor or touch a Blueprint at all.

Currently enabled toolsets: `EditorToolset`, `GameplayTagsToolset`, `AIModuleToolset`, `UMGToolSet`,
`ConfigSettingsToolset`, `LiveCodingToolset`.

That yields ~22 toolsets covering: actors, scene/outliner, Blueprints, materials, static & skeletal
meshes, DataTables, CurveTables, DataAssets, StringTables, textures, UMG widgets, gameplay tags,
behavior trees, config settings, the output log, and PIE session control.

We deliberately **do not** use the `AllToolsets` aggregator — it pulls in 21 experimental plugins
(Niagara, PCG, Chaos Cloth) whose tools we don't need and whose load cost we do pay.

---

## Manual steps you have to do (can't be automated)

These are the ones that genuinely require you:

1. **The editor must be running.** The MCP server lives *inside* the editor process. No editor, no tools.
2. **Adding a new toolset plugin means restarting the editor.** Toolsets register only at startup.
   Enabling one on a running editor does nothing until restart.
3. **Reflection-level C++ changes mean closing the editor for a full build** — new `UCLASS`/`UFUNCTION`/
   `UPROPERTY` or changed signatures. See the Live Coding warning in `CLAUDE.md`; a "Success" from
   Live Coding is *not* evidence the change was safe.
4. **Building requires the editor fully closed** — UnrealBuildTool refuses while Live Coding is active.

Auto-start is already configured (`bAutoStartServer=True` in
`Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`), so the server comes up with the editor.
Note that setting **only takes effect at editor startup** — toggling it on a running editor does nothing.

---

## Troubleshooting

**Claude reports no Unreal tools / connection refused**

1. Is the editor actually open?
2. Is port 8000 listening?
   ```bash
   powershell -c "Get-NetTCPConnection -LocalPort 8000 -State Listen -ErrorAction SilentlyContinue"
   ```
3. Start it by hand from the editor console (backtick key):
   ```
   ModelContextProtocol.StartServer
   ```
4. Confirm toolsets registered — search the editor's Output Log for:
   ```
   LogToolsetRegistry: Display: Registering Toolset
   ```
   One line per toolset. If you see only `AgentSkillToolset`, the toolset plugins aren't loading.

**"Convert Project" dialog on opening the `.uproject`** → **Cancel.** Never "Open a copy" — it forks the
project into a new folder and strands your built binaries and source. Nothing needs converting.
`EngineAssociation` is correctly `"5.8"`.

---

## Sanity check performed (2026-08-14)

Confirmed working end-to-end against the live editor, not just assumed:

- `list_toolsets` → 22 toolsets returned
- `get_current_level` → `/Game/ThirdPerson/Lvl_ThirdPerson`
- Read **and wrote** a Blueprint property, then saved the asset to disk (see below)

Read *and* write were both exercised — a read-only check would not have proven much.

---

## Changes made this session

### 1. GameMode wiring — the pause system is now live

`BP_ThirdPersonGameMode`'s `GameStateClass` was unset, which left the entire real-time-with-pause
system inert (`P` would appear to do nothing — looking like a bug that wasn't one). This was item #1
on the "wiring still required" list in `CLAUDE.md`.

Set via MCP and saved:

```
/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode
  gameStateClass = /Script/DungeonCrawlerDenver.DCCGameState
```

**Worth playtesting:** press `P` in PIE — you should now get pause/queue behaviour rather than nothing.

### 2. Disk space rescue — C: was completely full

`C:` had **0 bytes free** (929 GB used), which was silently failing writes — the first command run this
session died with "No space left on device". This would have blocked builds, plugin work, and Unreal's
own autosave.

| Action | Reclaimed |
|---|---|
| Cleared `AppData\Local\NVIDIA\DXCache` (shader cache, regenerates itself) | 10.3 GB |
| Cleared user + Windows temp folders | ~4 GB |
| **Moved `Videos` → `D:\UserData\Videos`** (318 files, verified 0 failures) | 163.8 GB |
| **Moved `Pictures` → `D:\UserData\Pictures`** (5,747 files, verified 0 failures) | 55.5 GB |

**Result: C: 0 GB → 227.65 GB free.** D: still has 1,139 GB.

The two folders were moved with `robocopy /MOVE` and then **properly redirected** via
`SHSetKnownFolderPath`, so Windows' known-folder registry now points at the new locations. Your
Videos and Pictures shortcuts in Explorer work exactly as before and apps will save there
automatically — nothing is left dangling at the old paths. Move logs are in `D:\UserData\_movelogs\`.

Note: OneDrive was checked first and is **not** involved — its own `Pictures` folder is separate and
was left untouched.

Two follow-ups for you:
- **Recycle Bin still holds ~2.2 GB.** I left it alone deliberately — emptying it permanently destroys
  whatever's in there, and that's your call, not mine. Empty it if you want the space.
- If Explorer's sidebar looks stale, sign out and back in (or restart `explorer.exe`) to refresh.

---

## Next up

`CLAUDE.md` lists three wiring tasks. #1 is now done. Remaining:

2. **Author starter skills.** Nothing populates `KnownSkills`, so the skill system has no content to
   run. Intended path is a DataTable of `FDCCSkillDef` rows — and the `DataTableTools` toolset can
   create and populate that over MCP, so this is very doable in a single session.
3. **Re-theme Room 01 with the `Cave_Camp` Fab pack** (242 assets: rock walls, stalactites, crates and
   barrels for cover, tents, bedrolls, a `P_Fire` particle). Not yet placed.

### Open question — what do you want first?

Rather than guessing at scope, pick a direction:

- **(a) Starter skills** — most unblocking. Turns the skill system from dead code into something
  playable, and gives the group new verbs in the next session.
- **(b) Re-theme Room 01** — most *visible*. Turns the grey-box into somewhere that looks like a
  dungeon, which is the better thing to show the group.
- **(c) The Wisdom/5-stat question** — `EDCCAbility` still has six stats; the real DCC ruleset has five
  (no Wisdom). This is flagged in `CLAUDE.md` as deliberately deferred because the preview PDFs lack
  the Health and Evade formulas needed to finish the job. Still blocked on missing information — don't
  start this one until the full rules are available.
- **(d) Build the server target** — never attempted. Per the design doc, hosting is secondary and must
  not block playtesting, so this is low priority.

Recommendation: **(a) then (b)**. Skills are the bigger unlock, and re-theming is a satisfying way to
close out a session once the mechanics work.
