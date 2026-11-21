# Next builds: quests, enemies, and fights

Saved 2026-11-21 so it can be reviewed later. Nothing in this file has been started. It is a menu. Pick an order when you want one. Until then, no build is in progress.

Each build is one close-the-editor Development link, tested with `python Tools/run_test.py`, then a ROADMAP log line.

The arcade is the product. A build earns its place when a minigame or a short run is more fun for a session. Long floor campaigns, new games, and new keys stay off this list.

## What is already there

The OBJECTIVES card (`DCCArcade::BoardSteps`, compiled 2026-11-21, not seen in a run) is a starter tour:

- THE ASCENT: leave Fort Tiner, High Camp, the summit. Then the card points at Dead Air.
- THE TAPE and THE AUDIT open after a summit. Filming and landing a hit open THE GATE and THE YARD.
- THE STRIP (Neon Mile, 500 m) and THE RUN (clear one tale floor) stay available from the first session.
- Neon Mile and Dungeon Crawler Denver stay unlocked. A player named Zoe can vote for anything.
- Every series quest pays the same 100 XP once.

That tour ends. After the summit, the gate clear, and the tenth tow, the card goes quiet. The first summit also plays a camp-radio line about Vaughn, a drone, and a hole under the pasture (`HazardLine` in `ADCCArcadeClimb`). Fort Tiner has eight lines in the same vein. That pasture story is out. The fort stays.

Reaching the summit today celebrates, pays, and then waits. When everyone is up, or daylight runs out, the round pays and sends the party back to the Safe Room after 25 seconds. There is no way down into the mine from the peak. Dead Air is a separate pad that unlocks after `Climb.Summits` is at least 1.

Fights, by game:

- **Dead Air** has four original creatures plus ghosts: lamp-eater (hunts lamps that are on), Tommyknocker (knocks, then drops rock), slag dogs in threes, the shift boss. One trick each. A second shift is the same episode.
- **Gate Run** has a real boss (the Gatekeeper: telegraphed slam, shades at two-thirds and one-third, a lien) and a lot of rooms (fight, trial, vault, duel, quiet room, gauntlet, toll, outage, clock, holding pen). The new rooms change the floor. The bodies inside them are still shades and the old dungeon pool.
- **Stress Test** is The Quarterly: forty seconds, it records every kind of damage, it never falls over. The quest is "land a hit." The seven jobs (Repo Driver, Ridge Hand, Call Sign, Line Coach, Mile Runner, Gate Warden, Floor Walker) barely show up in that round.
- **Fourteener** hazards are rockfall and birds. The peak pays out and sends you home. Nothing on the summit offers the mine, and nothing on the face shows a crew still working the claim.
- **Repo Rally** has angry owners and rival crews that hunt a red tab. The tow itself (clamp, struggle bar, overhead tricks, yard run) is the fight, and several of those beats are compiled and still unfelt in a real drive.
- **Tale floors** are short and pay. The bodies are the reskinned office roster (Intake Manager, Notary, Security) plus older dungeon creatures. A floor has no rule of its own.

Bounties already cross games once each (High Camp, Souvenir, Proof, Going Viral, Property Defacement, Shift's Over, Dressed for It). This plan adds card steps. It does not add a second quest system.

## Rules for every build

- A new action is a belt slot, a button, or something you already do (move, spray, film, swing, aim). No new keys.
- New `UCLASS` work is batched into that build's one editor-close link. Check `Binaries/Win64/` for a numbered DLL before calling it deployed.
- Player text is short, in the System's voice, with no em dashes. World text sits on a board.
- Records go on the arcade profile (`Quest.<Id>`, `Bounty.<Id>`). Never into the Dungeon Crawler Denver character save.
- Locks stay as they are. Neon Mile and the dungeon stay open. The crowbar rule holds: the cheap way and the expensive way both finish a room.
- Tests avoid paying Zoe's real profile when a free dev command exists.
- The OBJECTIVES step ships in the same build as the moment it describes. The card never asks for something the game cannot do yet.
- No `Content/` edits, commits, or itch pushes unless you ask.

## Still unseen, and worth a look inside whichever build touches it

A lot of 11/19 through 11/21 is linked and not playtested: the OBJECTIVES card, the dark way-home ring in the Gatekeeper's hall, climb-around-a-corner, the Gatekeeper's second shade call, and the combat swings on a body. The first hour of a build that touches that game includes one test of the old beat before adding the new one. If the old beat is broken, that fix is the build.

---

## The mountain, in one sentence

Mount Fourteener is a claim people will not leave alone. Fort Tiner is the camp at the bottom, and it stays the camp: the goat, the trading post, the routes board, the cabins. The peak is where the air gave out on the last crew. The value they were after is still in the rock, and somebody is always trying to get a little more of it. Dead Air is that attempt. You reach it by going down from the summit.

No pasture. No Vaughn scene. No drones watching the climbers. The eight Fort Tiner lines that tell that story get replaced with crews, old stakes, and one more load. Talking to camp is chatter, not a quest step.

---

## Build A — Descend into the mine

**Why.** The summit and the mine are the same mountain, and today they are two menu pads. Every time a crawler tops out, the game should offer the next scrape: go down into what they are still pulling out of it.

**What ships.**

- The moment anyone reaches the summit, a button sits on the side of the HUD, beside the summit line: **DESCEND INTO THE MINE**. Under it, one line: "Lucky Tuesday is under the peak. The whole party goes."
- It is there on the first summit and on every summit after. It is not a one-time story beat.
- Two clicks inside a few seconds, the same confirm as SAFE ROOM, because travel takes the whole party. The status line names who called it, so a friend still on the wall sees it coming.
- A board on the side of the summit cairn says the same words. Looking at it offers E, and E is the same confirm. The flag, the "SUMMIT 14,115 FT" sign, and the fireworks stay.
- Pressing it pays the climb the way the round already pays, then `DCCArcade::Travel` to Dead Air. Leaving it alone still ends the round as it does now: payout, then the Safe Room after 25 seconds, or when daylight runs out.
- Dead Air's first status line, if you came from this button, is "You came down from the peak. The shift starts now." The Safe Room pad still opens after a summit, so a later session can clock in without climbing again.
- THE ASCENT's handoff becomes that button. After a summit the card says "Descend from the summit, or take Dead Air from the Safe Room." It does not send anyone back to camp.
- The first-summit radio line stops talking about Vaughn and the pasture. It says the claim is still open and the mine is under you.
- Copy only, in the same pass: High Camp's board notes the last crew's cache. The route board keeps the routes and gains one line, "They never stopped digging. The top is where the air gave out." The eight drone and pasture lines in `DCCFortTinerLines.inl` become lines about old stakes and one more load. Fort Tiner's layout does not change.

**Done when.** A dev climb that teleports to the summit shows the side button and the board. The first click arms it, the second logs travel to Dead Air. A second summit in a later launch shows the button again. Ignoring it still returns to the Safe Room. A shot of the summit shows the new board beside the flag. Neon Mile and the dungeon never lock.

**Size.** Small. HUD button, one summit board, travel, and the line rewrite. No new class.

**One default, easy to flip.** The button arms when the first crawler summits, so a fast climber can offer the mine before the clock. If you would rather it wait until the whole crew is up, that is a one-line change.

---

## Build B — One more scrape

**Why.** Fourteener is one of the two games you asked to polish. Build A says the mountain is still being worked. This build puts one live attempt on the face, so the climb shows somebody squeezing a little more out of it.

**What ships.**

- First, one climb that wraps an outside corner (`DCC.ClimbTest`, plus a real hold on a face that turns). If wrap fails, fixing it is this build and the scrape waits.
- Above High Camp, one core rig on a ledge. It is a prop built in code, still running. On a timer it marks a disc on the rock, the same read as a bird shadow, and then spits chips across that disc. Step off it. It does not chase, and it does not shoot.
- A cache bolted beside the rig, separate from the normal supply chests. Taking it is the player's cut: a small ticket purse, or one climb item, and a note on the lid ("left for the next shift"). Taking it makes the next rockfall prefer you for the rest of the round. Leaving it is free.
- The spray can, already on the belt, paints the rig and shuts it up for the round. Proof of Summit is unchanged. That tag is still the top tenth of the mountain.
- THE ASCENT, between High Camp and the summit, says someone is still working the face. That step completes when you pass the rig, whether or not you take the cache.

**Done when.** Shots from below High Camp show the rig and the disc. The log shows chips on the disc, a take that marks the climber for the next rockfall, a tag that silences the rig, and a corner wrap that does not drop the climber. `DCC.ClimbTest` still passes.

**Size.** Medium. One actor spawned by `ADCCArcadeClimb`. Reflection, so the editor closes once.

---

## Build C — The Quarterly talks back

**Why.** Stress Test is the short fight, and right now it is a sack of hit points that writes down a number. The seven jobs are swings, shots, spells, and heals. This is the round where mixing them should matter.

**What ships.**

- The Quarterly already takes every kind of damage. It now says the kind out loud on the status line: swing, shot, spell, throw. Fists count as swings.
- The same kind three times in a row pays half until you switch. The line says the joke is old. Healing a friend counts as a switch, so Line Coach is in the round.
- At fifteen seconds a joint on the construct lights. Hits on that joint pay double until the clock ends. The joint is a replicated aim point and a ring drawn on the body. No new mesh.
- Ratings stay (Meets, Exceeds, Standing Ovation). THE AUDIT grows one step: "three kinds in one round," completed by the same profile stat the round already writes, plus a new `Audit.Kinds` count.
- No new key. Aim stays hold-right-mouse, fire on left click.

**Done when.** A punches-only dev round logs the half-pay. A mixed round does not. A shot shows the joint ring and the kind on the status line. The card in the Safe Room shows the new step after the first hit is already logged.

**Size.** Small to medium. Mostly `ADCCArcadeAudit`. One new replicated field is enough to need the editor closed.

---

## Build D — Gate rooms get a body of their own

**Why.** The rooms are already the fun part of Gate Run. Toll, outage, clock, and the pen each have a rule, then fill with the same shades. A room should be recognizable with the lights out.

**What ships.** One Forge body each, tinted through `DCCCreatureLook`. No new Meshy, no Content edits. Each has one rule.

- **The Window** (toll). Stays in the booth. The line of shades still comes. Clearing the window opens the booth early. Holding the booth the whole time still clears the room, and it pays a little more. Both ways work.
- **Brownouts** (outage). They path only while the room is dark. When the lights return they stop where they are. You do not need a lamp. The room's own lights are the switch.
- **Overtime** (clock). The waves still do not wait. Anyone standing still is the one they pick, and a tick on the status line warns you a second before they commit. Moving is the answer.
- **The Trustee** (pen). Stays inside the ring. If you hug the burning fence it shoves you toward it. Killing it still clears the pen. Stepping out still burns.
- The duel champion keeps its one body and gains a single raised-weapon beat before the swing, long enough to sidestep. This is a tell, not a parry system.
- The Gatekeeper's second shade call gets one confirmed test in this same build. If that call never fires, fixing it comes before the new bodies.

A gate that rolls two or more middle rooms already includes one of the new roles. THE GATE gains a step, "clear a toll, an outage, a clock, or a pen," set by a `Gate.Roles` flag when that room pays.

**Done when.** One seed log shows the Window and the clock (the 11/20 seed 36450 already rolled a clock). A second run stands in an outage and the log shows the brownouts stop when the lights return. Shots of each body in its room. The way-home ring is still dark until the Gatekeeper drops.

**Size.** Medium-large. Four small enemy subclasses in one link.

---

## Build E — Dead Air, the second shift

**Why.** The first four creatures are a good episode, and a second visit is the same episode. The verbs of this game are the lamp and the camera. The new bodies should answer those, the way the lamp-eater already answers the lamp.

**What ships.** Three additions. The cage stays safe from all of them.

- **The Still.** Closes the distance while the body cam is off. While you are recording it, it stops and poses, and that footage rates high. You can still hit it. Hitting it is worse television and a normal fight. REC is already a belt slot.
- **The Canary.** Small, no interest in you. It sings on the way to a firedamp pocket. Follow it and you get a warning and a light. Ignore it and the blue gas still comes, on the timer the mine already uses. Killing it removes the warning for that pocket.
- **The Foreman,** rare, in a deep stope after the shift boss has been met once on this profile. He puts out lamps in a radius, which wakes any lamp-eater still in the mine. Bringing him down sets `Quest.Foreman` and a bounty row beside Shift's Over, paid once, same ticket style.

Spawn order stays a turn down the drift, so a solo player still meets one idea at a time. THE TAPE grows a step: film a Still, or follow a Canary, and bring the tape up.

**Done when.** `DCC.MineGo` can stand you in front of each. The log shows the Still stopping while REC is on, the Canary's song before a gas pocket, and the Foreman snuffing a lamp. A blast or a rockfall still does nothing inside the cage.

**Size.** Medium. Three subclasses, one link. The Foreman reuses the shift boss's scale and the lamp-eater's light check.

---

## Build F — Someone cuts the line (Repo Rally)

**Why.** The other game you asked to polish. Owners already chase and sometimes shoot. Rival crews already hunt a red tab. The tow is still a drive to the yard once the magnet sticks. The new beat is someone who wants the car more than they want you.

**What ships.** Feel numbers (weight, drift, fuel, trick pay) stay where they are. This build does not retune them.

- One rival wrecker, only while you are towing, and only when the tab is red or the mark pays at the top of the board. It drives for the line, not for your cab.
- If it reaches the line, it cuts. The mark drops, the struggle bar clears, and you can hook again. Ramming it, or reaching the yard first, keeps the tow. One rival at a time.
- One work order in the rotation is bogus. The clamp still grabs. A second later the status line says BOGUS and a crew steps out at the bumper, using the owners who already shoot. Learning it once is the point, so the same plate is bogus every time on that profile.
- THE YARD grows a step: "shake a rival off a tow," `Quest.Cut`. The tenth car stays the old step.

**Done when.** A dev tow with a red tab logs the rival spawning, a cut, and a ram that keeps the line. A shot shows the rival on the line and the BOGUS line. A guest window still sees the tow where the host sees it. The yard run's curb crawl (parked cars) gets one drive in this same test. If it still stops forever in front of a parked car, that fix is part of this build.

**Size.** Medium-large, and it needs a real drive, not only a log.

---

## Build G — One rule on the tale floor

**Why.** THE RUN is "clear a floor." The floors are short, which is right. The fights are walk up and swing. One announced rule makes a run feel like an episode without building a new dungeon.

**What ships.** The generator already picks a seed per floor. It also picks one verb, painted on the floor's board as you come down the stairs. One verb, not three.

- **Wet set.** Puddles on the floor. A spark jumps between anyone standing in water within a few metres, crawlers and enemies both. Spreading out, or throwing from the dry tile, is the answer. Thrown props already deal damage without the attack-only hook, so this stays on the spark, not inside `ModifyIncomingDamage`.
- **Live set.** The floor is filming. A kill on camera adds a little AI Favor. An unlit corner is marked on the board. Standing there keeps the Intake Manager from calling you out to the others.
- **Audit.** A clerk with a clipboard. Enemies who have not been hit for eight seconds start filling back up. The clerk is the one to drop if you want the regen to stop. He is an office Forge body, the same family as the Notary.

The board names the verb in one line ("WET SET. Water carries a spark."). Clearing the floor still completes THE RUN. A second step, "clear a floor that had a rule," sets `Quest.Verb` the first time any of the three pays.

**Done when.** Three seeds log three different verbs. A wet-set test logs a spark jump. An audit test logs regen stopping when the clerk drops. Stand-ins still fill empty seats, and a rewrite still saves a downed crawler.

**Size.** Medium. One floor flag on the generator, three small behaviours, one link.

---

## Parked, on purpose

- **Sparring and the rank trial.** Still unbuilt, and still a good Safe Room side room (a gym dummy, three tells, sidestep or a short block). A block is a new input. It waits until you say you want that button.
- **Casino Cascade's visual pass.** The room, the five games, the staff, the song, and the achievements are in. The wheel slices and the roulette pockets are still hard to read from a standing spot, and nobody has sat through a paid hand. Real work, and it is not a quest, an enemy, or a fight. Say if you want it pulled into this queue.
- **Mixamo swings judged by eye.** Stab, slash, overhead, and the sword-and-shield walks are imported. Axes and hammers went back to the old loops at your request. A look-and-fix pass is its own hour with the editor open. It is not one of these builds.
- **More games, Floor 1 rooms, towns, the Companion, the Table.** Out of this menu.

## Suggested order

A is the tie you asked for, and it is the smallest build. B makes the mountain look like the sentence A tells. F is the other polish game.

| Order | Build | You will feel it in |
|---|---|---|
| 1 | A. Descend into the mine | The summit, every time |
| 2 | B. One more scrape | The face above High Camp |
| 3 | C. The Quarterly talks back | Stress Test, one 40 second round |
| 4 | D. Gate room bodies | Gate Run |
| 5 | E. Dead Air's second shift | The mine you just descended into |
| 6 | F. Someone cuts the line | Repo Rally |
| 7 | G. One rule on the tale floor | Departures |

A fight-first order is D, C, E, then A and B, then F, then G. Rally first is F, then A, then the rest.

## Open when you come back

Name the build to do first, or a new order. Say if the summit button should wait for the whole crew instead of arming on the first person up. Say if the parked sparring room or the casino read-from-standing pass should move onto the list. Button wording, the rig, and the ticket purse can be picked in the System's voice and written into the ROADMAP log so you can change them.
