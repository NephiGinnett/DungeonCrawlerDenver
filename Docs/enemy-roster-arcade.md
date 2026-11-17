# The arcade's enemies (2026-11-16)

Original enemies for each game, built to the style guide (`Docs/style-guide.md`): NPC Forge people for anything
human, creatures only where the game's fiction has them, and nothing that is someone else's character. Status:
**built** (in the game), **reskinned** (existing logic, new body), **to build**.

## Dead Air: the Lucky Tuesday mine
The mine's things are the mine's own history coming back wrong. They should be heard before they are seen.
Code: `Characters/DCCEnemyCharacter_DeadAir`. `ADCCArcadeHaunt::SpawnMonsters` puts them down the drift in turn (lamp-eater, three slag dogs, Tommyknocker), with the shift boss in the deepest stope. They replaced the dungeon wildlife.

| Enemy | What it does | Body | Status |
|---|---|---|---|
| Ghost miners | Stand, flicker, vanish when approached; filming them rates | Forge workers in the hologram material | built |
| **Lamp-eater** | Creeps toward light; it only hunts crawlers whose lamp is on, so turning your lamp off (L) makes it lose you | see-through Forge worker, dark violet hologram, brighter while hunting | built 2026-11-16 |
| **Tommyknocker** | Never comes for you. Every 14-22 s it knocks three times and drops rock where the nearest crawler stands (the rockfall trap, given a body). Kill it and the knocking stops | a small Forge worker with a candle-stub glow, off to the side of its stope | built 2026-11-16 |
| **Slag dog** | Fast (faster than a walk), weak, in threes | no dog exists in the packs: MonsterForSurvival's Slime, soot-black with an ember-orange rim and glow (a running lump of furnace slag) | built 2026-11-16 |
| **The shift boss** | The deepest stope's one big presence: slow, hits hard (d10). The cage keeps him out like everything else | a large Forge worker (1.45x) with a red lamp you see down the drift | built 2026-11-16 |

## Gate Run: the gates
| Enemy | What it does | Body | Status |
|---|---|---|---|
| The Gatekeeper | Boss: slams with a telegraphed ring, calls shades at 2/3 and 1/3 | large violet-tinted Forge worker | built |
| Champions | One named fighter in the duel room | gold-tinted Forge worker | built |
| Gate shades | Weak, fast, see-through; the ambush room and the boss's adds | violet hologram Forge body | built |
| Dungeon pool (debt eaters, office monsters, wildlife) | Fight-room filler | mixed | existing |

## Repo Rally: the city
| Enemy | What it does | Body | Status |
|---|---|---|---|
| Angry owners | Chase the wrecker, yell, sometimes shoot | Forge people (`ADCCRallyThug`) | built |
| Rival repo crews | Hunt trucks that are in debt | Forge people in trucks | built |

## Office monsters (the dungeon floors)
| Enemy | Body | Status |
|---|---|---|
| Regional Intake Manager | Forge office worker (was the Minion look-alike "DonMinion") | reskinned 2026-11-16 |
| The Notary | Forge office worker (was "BuffMinion") | reskinned 2026-11-16 |
| Security (bar guard) | large Forge worker (was "BuffMinion") | reskinned 2026-11-16 |

## Clean-up still owed
- Delete `/Game/DCC/Characters/Minions/` (SK_DonMinion, SK_BuffMinion) once nothing references it.
- Rename or remove the IP-named props in `/Game/DCC/Equipment/Props/` (`PokemonMasterTrainer`, `MinionEyes`,
  `DunePopcorn`) and their `DCCBench.json` entries. They are not used by any item today. Needs the editor.
