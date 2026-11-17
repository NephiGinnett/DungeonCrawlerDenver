# Dungeon Crawler Denver: style guide (2026-11-16)

One look for the whole arcade, so the games feel like channels of the same show. The fiction is the one the books
built: the dungeon is a TV production, and every game is an episode. So: **a lit set, not a natural place.** Strong
practical lights, saturated accent colours, deep shadows between them, worn real-world props, and bodies that read
as people at a glance.

## The rules
1. **Every scene has a key light you could point to**: a lamp, a neon sign, a brazier, a sky. No flat ambient fill.
   Darkness between lights is good; it is what makes the lights matter.
2. **One accent colour per game**, used for its signs, pickups and HUD trim, and a second only for danger.
3. **Worn, real props over clean or cartoon ones.** Rust, dents, mud, chipped paint. Fab packs with a realistic
   finish fit; toy-like and cartoon packs do not (the Minion bodies are gone for this reason).
4. **People are NPC Forge people.** Crawlers keep their own Meshy bodies. Monsters may be creatures, but never
   another franchise's character, and never a crawler's body.
5. **Text in the world is a prop**: painted boards, stencils, neon, warning signs. It is in the fiction (mine company
   notices, impound yard rules, gate runes), never a floating game instruction.
6. **No borrowed names.** No item, prop, enemy or place named after someone else's IP.

## Per game
| Game | Accent | Danger | Key lights | Surfaces | Packs that fit |
|---|---|---|---|---|---|
| Dead Air (the Lucky Tuesday mine) | sodium amber `#FFB45A` | firedamp blue `#5A8CFF` | helmet lamps, hanging miner's lamps, the cage lamp | wet rock, old timber, rusted iron, canvas | Cave_Camp, Desert_cave, SoulCave, Legendary_Forge |
| Repo Rally (the vaporwave city) | magenta `#FF4FD8` | warning red | neon, streetlamps, the impound floodlights | asphalt, chrome, glass | Synthwave, Cyberpunk, Sci_Fi_city |
| Fourteener (Mount Fourteener, Fort Tiner) | gold `#FFD27A` | ice white | the sun, the campfire, braziers | granite, snow, logs | Viking_village, Cave_Camp, Desert_cave |
| Gate Run (the gates) | violet `#8C5AFF` | slam red | room glows, portal light, fire bowls | dark stone, glowing inlays | Alien_Portal, Legendary_Forge |
| The Den | gold and cyan | | the prize counter, neon | carpet, wood, arcade cabinets | Beach_house, Synthwave |

## HUD
Dark translucent panels (`#000` at 55-70%), white body text, the game's accent for headings and the one key number,
red only for danger. Buttons for every action a player needs (never a console command). Cards explain a mechanic
the first time it matters, then get out of the way.

## The enemy roster
See `Docs/enemy-roster-arcade.md`.
