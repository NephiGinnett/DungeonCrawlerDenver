# Planning list for work

2026-11-26. Things that still need a decision, a model, or a writing pass. The game already has a lot of this. This is what is left.

## What changed tonight

The Ring ladder is the same 50 stages. A full climb now pays 958 tickets: 10 plus Stage/3 on each card (908 across the fifty) and a 50 ticket purse when the House falls. A tournament with two or more people pays 750 tickets and the title. A solo exhibition pays 200 tickets and the flare gun. The mallet at stage 50 is still the Meat Tenderiser. This is compiled into the editor (9:36 PM, 2026-11-26). It has not been played, and it is not in a friend build yet.

Clearing a minigame once puts one existing piece of gear in the bag, with the XP:

- Fourteener: Fire Axe
- Dead Air: Cordless Nail Gun
- Stress Test: Complaint Folder
- Gate Run: Ceremonial Sword
- Repo Rally: Loss Prevention Sidearm
- Neon Mile: Bar Gun
- Tower defense, if the wall holds through wave 5: Load-Bearing Shield
- A dungeon floor: Chef's Knife
- The Ring's prize stays the mallet at stage 50, not a second item on stage 1

The OBJECTIVES card is now ACHIEVEMENTS. It shows the clear you have left, then a few rows from your own achievement table. The Prize Counter's Bounties tab is now Achievements, and it lists those same table rows so you can claim them. The gold line in a game no longer says QUEST. It names the clear and the prize, and it goes away once that clear is done.

Crafting is not built. The handoff is Docs/crafting-benches.md in the project, and the short version is below.

## Ticket sinks (you asked)

There are sinks. They were just small next to a 900 ticket climb, and a few of the menus spend something else.

Tickets leave the wallet at:

- The Prize Counter. Hats and looks run from 40 (party hat) to 500 (sponsor orbs). The crown is 400, the jetpack 300, the pit boss plate 450.
- Booth guns: confetti crossbow 80, flare gun 100, nail gun 120.
- A spray can: 60.
- The summon tubes in the Den: 100 for one ally, 900 for ten. A finished Ring climb is about one ten-pull.
- Casino Cascade. Slots are 2 a pull. Table stakes are 5. The house is meant to keep about 10 percent.
- Fourteener, at the Trading Post: food is 2 to 9, tools are 10 to 16. The grappling hook upgrades for 60, then 120, then 180.

These do not spend tickets:

- The Chop Shop spends scrap (Repo Rally and Neon Mile).
- Mine Outfitters spends Dead Air credits.
- Bounties pay tickets. They are not a sink.

If someone earns a climb on another PC and rejoins, tickets they bring are capped at 1500 since the last sync (3000 if the host has never seen that crawler). One climb fits under that. A climb plus a long night of other games can get clipped. That cap was already there.

## Crafting, for another session

Alchemy and Mechanics, in our own words, not the book's recipes. No new key. You stand at the bench and press E.

- Mechanics is the Chop Shop. Scrap still upgrades the truck. Salvage, which tower defense already drops and nothing spends, buys three recipes: Shop Nail (the nail gun), Patch Kit (a small heal), Brace (the next lockdown is 5 seconds shorter).
- Alchemy is a new tab beside it. House Tonic, Shift Coffee, Blue Lamp. Drinks go on the food slot you already have.
- Cook and Tailor wait.
- The Prize Counter stays a shop. It is the ticket sink. Do not turn it into a bench.
- Mine Outfitters and the Trading Post stay shops.

Full notes: Docs/crafting-benches.md. Salvage is the material. Do not guess Cillinan's giraffe. That question is still open.

## Items that need a model, or a decision

These rows exist and draw nothing, or draw a stand-in:

- Grand-Opening Confetti Crossbow. No mesh.
- Bolt-Action Hunting Rifle. No mesh.
- Loss Prevention Riot Shotgun. No mesh.
- Grease-Proof Apron, Assistant Manager's Blazer, the DAVE name tag, the high-vis vest. No mesh. Body and trinket slots.
- CJ's Board Game, Scott's Lacrosse Stick, Marcel's Lucky Dice. Personal trinkets, no mesh.
- Cordless Nail Gun is a hand drill. Emergency Flare Gun is a revolver. Both work. A real nail gun and a real flare gun are already in Downloads if we want to swap them.
- The Ring champion weapon is the kitchen mallet. A friends' tournament pays tickets and a title, and has no item of its own.

Packs already on disk, not turned into gear yet:

- FPS weapon bundle, the sniper, and Weapons Free. Keep the joke guns and the corporate ones. Ask before a realistic military rifle goes in.
- RPG magic staff pack. Staves should help spells, not just add a damage die.
- ShadowKnight pieces, Polytope modular armour, GanzSe accessories. Helmets and pauldrons can hang on sockets. Skinned mannequin armour cannot go on the crawler bodies. List those for a rebake rather than forcing them.

## Spells

The Ring teaches four, one per champion: Static Burst (stage 10), Fire Fingers (20), First Aid (30), Whiteout (40). Stage 50 teaches nothing.

A book-skill list is written in Docs/DT_DCCSkills_BookSkills.json (Club, Axe, Dagger, Longsword, Warhammer, Polearm, improvised weapons, throwing, explosives, First Aid, and more). It is data, not all of it confirmed live in the skill table. Needs a pass: which of these are already skills, which are only a document, and which need an icon.

Still not done from the Fab plan:

- Skill and spell icons (Part B1), once an icon pack is actually the one we want.
- No new spell batch this session. The last FX pass already gave the live spells their own cast and impact.

## Classes

Seven starter jobs exist, in this order, and the save stores the index, so do not reorder them: Repo Driver, Ridge Hand, Call Sign, Line Coach, Mile Runner, Gate Warden, Floor Walker. Picking is once. It grants kit and 25 tickets. Levels through 5 grant the next real skill.

Not designed yet: more jobs themed to the minigames (a mountaineer, a caller, a floor director), with bonuses that help in ordinary fights, not one trick. Names in the Denver voice. No new keys. Write them before anyone codes them.

## Character lines

Already recorded or written: the impound yard (Rhee, Jan, Diaz), Fort Tiner, the Primetime snick-tape interview (Hiss, CJ, Vaughn).

Still needs writing, and some of it needs a voice:

- Ring champions. The Notary, the Bull, the Auditor, the Gatekeeper, and the House have names and no lines.
- Soldier and droid announcers. The packs are on disk. They are not hooked to tower defense or the Ring. Hiss stays the host. They are the arena.
- Casino Cascade staff. The room is built. A short set of "rules, play, shop" lines is the whole job.
- Cillinan. He is placed. Do not move him. The giraffe entrance is still an open question. Do not guess it.
- Bhor. Left out on purpose. A possible later boss, no model, no lines.
- The rest of the table (Scott, Marcel, Kyle) has almost no lines of their own. Primetime is the place those start.
- Bench lines, once Alchemy and Mechanics exist. Two or three each. Not a quest.

## Still on the build list

In the order already agreed, after tonight's payout and the achievement board:

1. Menu sounds from Ultimate UI Menus. One helper, quiet, under the game.
2. The soldier and droid announcers, tower defense first, then the Ring.
3. Weapons and armour from the packs above, with icons and a grip on the pose bench.
4. Loose props in the tower defense hall (bottles, cans, cups, small boxes only, cap 60, no physics until the shelves are checked).
5. Sitting on chairs.
6. Lamps that flicker when something heavy dies.
7. A real play of the Ring: stages 1 to 3, then stage 10, and the taught spell on the belt. That test pays tickets and can teach a skill, so snapshot Profile_Zoe.sav and DCC_Zoe.sav first and put them back.

After that, when you want it: the crafting tab from the brief, the Paragon champions (Serath, Muriel, Zinx) as Ring or tower-defense faces, the staffs, and the animation catalogue. Room building stays with Claude.

## Left open on purpose

- Cillinan's giraffe. A mount, or a pillar? Plain fight until you say.
- Whether a friends' tournament should also drop a unique item. Tonight it pays 750 tickets and the title.
- Homebrew classes. Write the names before they get coded.
