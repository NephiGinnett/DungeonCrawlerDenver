# Multiplayer — controls, sessions, and what breaks first

Five players: Zoe (DM), Scott, CJ, Marcel, Vaughn, Kyle. Synchronous, co-op.

---

## Where we actually are

> **Update 2026-11-12.** Sessions now have a front end: F2 opens the party menu (host in place, invite codes,
> join, who's here, start the run for everyone; see ROADMAP's log for that day). Late-joiner routing and crawler
> identity (`?Crawler=`) were already in `ADCCGameMode`, and the listen-server billboard fixes landed earlier.
> The World Dungeon replicates a seed, not geometry. The rest of this page is the original plan. The two-client
> test and the pause-under-latency check below are still owed, and so is a packaged build for friends.

Better than it looks in one respect and worse in another.

**Good:** the game has been written server-authoritative from the first commit.
Damage, dice, skills, climbing, fist bumps, quests and XP all resolve on the server
and reach clients by replication. Every client-callable path is a *request* the
server re-validates. That is the part that is genuinely painful to retrofit, and it
is done.

**Bad:** it has never been run with two clients. Not once. Everything below is
therefore a plan against code that is *theoretically* networked, and the first
two-player PIE session will find things this document does not predict.

**The single highest-value next step is not on this list: it is running PIE with
`NumPlayers=2` and writing down what breaks.** Everything else is speculation until
that happens.

---

## Part 1 — Controls

### The problem with what we have

Input is currently raw `PlayerInputComponent->BindKey` calls in
`ADCCCharacterBase::SetupPlayerInputComponent`. That was the right call to get a
fight playable without authoring input assets, and it is explicitly commented as
temporary. It does not survive contact with five people because:

- **No rebinding.** Five players, five keyboard layouts, at least one left-hander.
- **No gamepad.** At least two of the group will want one.
- **It fights the template.** The third-person template uses Enhanced Input; we bind
  raw keys alongside it, so two systems own the keyboard.
- **Context is hardcoded.** `OnPrimaryPressed` branches through climb / prompt /
  skill / fist bump / attack in C++. That is five behaviours on one key resolved by
  an if-chain, which is exactly what Enhanced Input's Mapping Contexts do properly.

### The migration

Move to **Enhanced Input**, matching the template rather than living beside it.

**Input Actions** to author (`Content/DCC/Input/`):

| Action | Type | Replaces |
|---|---|---|
| `IA_Primary` | Digital (pressed/released) | LMB |
| `IA_Secondary` | Digital | RMB |
| `IA_Sprint` | Digital | Left Shift |
| `IA_Interact` | Digital | E |
| `IA_Throw` | Digital | R |
| `IA_Stow` | Digital | F |
| `IA_Menu` | Digital | Tab |
| `IA_Pause` | Digital | P |
| `IA_Release` | Digital | Space (let go of wall) |
| `IA_Confirm` / `IA_Decline` | Digital | Y / N |
| `IA_SkillSlot` | Digital, 1-5 | Number keys |

**Mapping Contexts** are where the real win is. Instead of one key branching five
ways in C++, push and pop context by state:

| Context | Priority | Pushed when |
|---|---|---|
| `IMC_Default` | 0 | Always |
| `IMC_Climbing` | 10 | `ActiveClimbPoint` set |
| `IMC_Prompt` | 20 | `PendingClimbPoint` set |
| `IMC_Menu` | 30 | Character menu open |

`IMC_Climbing` maps `IA_Primary` straight to "climb up" with no branch. The if-chain
in `OnPrimaryPressed` disappears, and each state's bindings become readable in one
place. This also makes the "Begin climbing?" prompt correctly *modal* — right now it
is enforced by an early return that is easy to forget when adding a sixth meaning to
left click.

**Gamepad** falls out nearly free once actions exist: add gamepad keys to the same
contexts. The skill slots want a radial or a shoulder-modifier rather than five face
buttons.

**Rebinding** also falls out: Enhanced Input has `PlayerMappableKeySettings` and the
Common UI rebinding widgets read it.

### Do this before the multiplayer session work

Not because it is more urgent, but because **the input rewrite touches
`ADCCCharacterBase`, and so does everything else on this list.** Doing it first
means the session work is not merging against a moving input layer.

---

## Part 2 — Sessions

### Listen server, not dedicated

The design doc says hosting is secondary and must not block playtesting, and there
are three build targets specifically so the project never grows client-only
assumptions. That stays true — but for the group's actual sessions, **Zoe hosts a
listen server**.

Reasoning:
- The dedicated server target has **never been built.** Launcher installs ship no
  precompiled server binaries, so it is its own piece of work.
- The narrator runs server-side and talks to a model **on Zoe's machine.** A listen
  server puts those in the same process for free.
- Five friends, scheduled sessions, one host. There is no matchmaking problem here.

The Server target keeps existing and should keep compiling. It just is not the path
to the first five-player game.

### Connection

Start with the crudest thing that works: **direct IP**, over a LAN or a mesh VPN
(Tailscale/ZeroTier), which turns a NAT-traversal project into a non-problem.

`open <ip>` from the console is enough for the first session. A proper front end
comes later and should not gate playtesting.

### Drop-in, and why the Safe Room already matters

`ADCCRoom::AllowsAsyncLogin()` is already derived from room type rather than
authored, and already returns true for `SafeRoom`. That was built for exactly this:
a player who drops or joins late materialises in the Bar, not mid-fight.

What is missing is the plumbing — `GameMode::PostLogin` choosing a PlayerStart in an
async-login room rather than the default one. Small, and the design decision behind
it is already made.

### Character selection

Each player picks a crawler. The background/recommendation system and
`DT_DCCBackgrounds.csv` already exist for this; what does not exist is a screen.

Interim: a console command or a `GameMode` default mapping player index to a
Blueprint. Ugly, fine, unblocks a session.

---

## Part 3 — What will break first

Predictions, in rough order of confidence. Worth reading before the first two-client
test, and worth correcting afterwards.

**1. The nameplates and quest markers billboard to the wrong camera.**
Both call `GetWorld()->GetFirstPlayerController()`. On a listen server that is the
host, so every remote client's nameplates face *Zoe's* camera. Certain to be wrong;
the fix is to use the local player controller instead.

**2. `ADCCHUD::GetOwningCrawler` and the Canvas HUD.**
HUD is per-client so this should be fine, but the quest tracker iterates
`ADCCQuestMarker` actors and asks `IsMarkerActive()` — which is set from a
server-replicated quest state. Should work; unverified.

**3. Fist bump and climb traces use `GetPlayerViewPoint` on the client, then
re-validate on the server.** The server re-trace happens at the server's idea of
where everyone is. Under latency the client may click a valid target and be refused.
The re-validation is correct and must stay; the range tolerances may need widening.

**4. Pause under latency.** Any player pauses, everyone queues, actions resolve in
initiative order. The design is sound. What is untested is what happens when a pause
RPC arrives while another client is mid-action, and whether the dilated time scale
replicates cleanly. **This is the most likely source of a genuinely hard bug**,
because it is the one system whose whole job is to manipulate time.

**5. `ADCCFloatingText` and the damage tickers.**
Spawned locally on each machine from `OnHealthChanged`, which fires on server via
`ApplyDamage` and on clients via `OnRep`. Should be correct by construction. Watch
for doubles on the listen server host, which gets both paths.

**6. Ragdoll on death.** Physics is simulated per-machine; corpses will land
differently on each client. Cosmetic and acceptable.

**7. The achievement component is on every character, including mobs.**
Harmless today because mobs have no table assigned, but it is replicated state on
every spawned enemy. Should be crawler-only. Cheap fix, worth doing before enemy
counts go up.

---

## Suggested order

1. **Run PIE with `NumPlayers=2` and write down what actually breaks.** Before any
   of the below. It costs ten minutes and will reorder this list.
2. Fix the two certain bugs: local-player billboarding, and achievement components
   on mobs.
3. Enhanced Input migration, since everything else touches the same file.
4. Direct-IP listen server session with two humans.
5. Safe-room drop-in via `PostLogin`.
6. Character selection screen.
7. Dedicated server target, only if hosting ever becomes a real requirement.
