# Dead Air: the Lucky Tuesday mine. Traps, ghost stories, and notes on the bodies

Written 2026-11-15 when Dead Air moved underground. **Built the same day**: all ten traps, the five stories and their notes, and Mount Fourteener's later visitors (ROADMAP 2026-11-15 says what was seen working). The tables below are still the design reference. The game itself is in
`Source/DungeonCrawlerDenver/Arcade/DCCArcadeHaunt.cpp`. ROADMAP's 2026-11-15 entries say what is already there.

**The setting in one line.** The Lucky Tuesday was a silver mine under the mountain. It closed in the summer of 1893,
when the silver price fell through the floor. The last shift went down on a Tuesday, and the cage never came back
for them. The stream calls it "the most haunted hole in Colorado". The sponsor calls it "a heritage experience".

**What the crew is doing.** They are filming. Every trap and every story below is judged by one question: does it
make better footage? Something going wrong on camera is the point. Something going wrong off camera is just a bad
day.

---

## 1. Traps and pitfalls

The house rule from `Puzzles/` applies here too. **Every trap gives a warning first, and every trap can be got
past.** Five friends must never sit out a round because one plank gave way. A trap costs health, time, tape or
position. It never takes a player out of the round for good.

| # | Trap | The warning | What it does | How to get past it | Footage |
|---|---|---|---|---|---|
| 1 | **Rotten planks over a winze** (a shaft dug down from a tunnel floor) | Planks across the tunnel, a creak underfoot, a dark gap the helmet lamp can just see | Over 140 kg on it at once, or anyone standing still on it for 2 s, and it gives way. You drop to the **sump level** below (one short tunnel and a ladder back up) and take fall damage | Cross one at a time and keep moving, or go round by another tunnel | Someone falling through **on camera** is the best shot in the game (like a teammate getting hurt) |
| 2 | **Firedamp pocket** (explosive gas) | Helmet lamps in that tunnel burn **blue** and hiss | After 4 s with a lit lamp inside, it flashes: fire damage in the pocket, and everyone in it is knocked down | Lamp off (a new key, L) and walk through blind, or sprint | A flash on camera scores big. A crew walking through in the dark is tense footage too, scored as a "suspense" bonus |
| 3 | **Hanging wall** (a rockfall) | Dust trickling from the roof, a timber groaning, and three knocks (see story C) | A few seconds later the roof drops. Rocks fall (Fourteener's `FDCCRockfall`) and a rubble block seals the tunnel for 40 s | Get clear, then dig through (E, 5 s) or go round | Getting the collapse in frame scores. Being under it costs half your health |
| 4 | **Runaway ore cart** | A rumble on the rails and a squeal getting louder. The rails glint in your lamp | A cart rolls down a sloped tunnel and knocks down anyone on the rails | Step off the rails (they are always marked). Or push the cart into a monster first | A crewmate flattened by a cart on camera. Monsters hit by a cart count as "monster attack" footage |
| 5 | **Flooded sump** | Water at the bottom of a slope, a drip echo | Wading speed, and **tape drains twice as fast** while you are in it (the camera fogs) | Stay out, or wade quickly. Some prizes are under the water | Low. A ghost seen in the water's reflection is rare and worth a lot |
| 6 | **Old powder crate** | A stencil, "CLEAR CREEK GIANT POWDER", sweating crystals | A hit, a throw or a fall sets it off: area damage (uses the explosives' `SplashRadius` path) | Leave it, or use it. Throw it at a monster or a blocked tunnel | Big, either way |
| 7 | **Blackdamp** (bad air) | The canary in the cage goes quiet on the HUD, and the edges of the screen darken | Stamina does not come back while you are in it, and after 10 s it costs health | Get out. The canary (story D) shows where the air is good | Low. It is pressure, not a moment |
| 8 | **The phantom bell** | The cage bell rings early, three times | Nothing physical. It is a lie. The status line says the cage is leaving, and it is not | Read the clock: the real bell is the round timer | Whoever runs back for nothing is great comedy if someone films them |
| 9 | **Lamp-snuffer** (a ghost does it) | Cold air, breath fogging, the cold-blue light nearby | A ghost blows out one crewmate's helmet lamp for 8 s | Stay close to someone whose lamp is lit. Relight with L | The ghost is in frame while it does it, so this is a chance at the best ghost shot |
| 10 | **The cave-in clock** | The last 45 s of the round: rumbling, and dust in every room | Rockfalls start in the deepest stopes and work up toward the cage | Leave in time | The run back, filmed, is the finale |

Build order for traps, cheapest first: 6 (powder crates, mostly existing systems) → 3 (rockfall, existing code) →
1 (planks, a trigger plus a lower level) → 4 (cart on rails) → 2, 7 and 9 (the lamp toggle is new). 5, 8 and 10
are mostly lighting, sound and the status line.

---

## 2. Ghost stories, told in notes on the bodies

**How it works.** The bodies are skeletons in old work clothes, slumped against the rock. Some have a scrap of paper
or a tag on them. **E reads it.** A note read **while you are recording** is footage: the audience loves a reading.
Each story has 3 to 5 notes scattered through the mine. **Find every note in a story in one round** and that
story's ghost turns up for a payoff shot, and the System announces it.

Notes are short, in a miner's 1893 voice. The System never writes them. It only comments when a story completes.

### A. The Platinum Man

Foreman **Elias Rourke** would not believe the price crash. The white metal in the deep vein was platinum, he said,
and platinum does not care what silver does. He kept the shift down to dig it out.

- *Pay docked from 3.50 to 2.75 a day. "The white metal will make it up to you," says Rourke. Make it up to my wife.*
- *Rourke had us chase the white streak forty feet past the timbering. Said platinum doesn't wait for timber.*
- *ASSAY, Lucky Tuesday, deep vein, sample 6: Galena. Pyrite. Trace silver. PLATINUM: 0.0 oz. Do not tell the men.*
- *Rourke tore up the assay in front of us and said it was a forgery by the bank.*
- (On the foreman himself, deepest stope, still holding a hammer) *It is in here. I can hear it. Tell Margaret I was right.*

**Payoff:** Rourke's ghost appears at the end of the white streak, swinging at the rock, and does not stop.
**The System:** *"Story complete: The Platinum Man. Viewers have voted the white metal 'definitely platinum'. The
assay office has been notified and does not care."*

### B. The Knife in the Lunch Pail

Two partners on a drill, **Teodoro Ruiz** and **Halvard Lund**. One morning there is a knife in Teo's lunch pail
that is not his. The notes read like the start of a murder.

- *Found a knife in my pail this morning. Not mine. Halvard says not his either. He did not look at me when he said it.*
- *Knife is gone again. Halvard was down the drift alone at noon.*
- *Teo will not hold the steel for me now. He holds it for the new boy instead. Fine.*
- *Knife is back in my pail, wrapped in a rag this time. I am not sleeping in the bunkhouse.*
- (In the cage, the last note, under the floor plate) *It's for the hoist rope, Teo. If the cage does not come, you cut it
  and you climb. I could not say it in front of Rourke. Happy birthday. H.*

**Payoff:** the two ghosts stand together at the bottom of the shaft, looking up.
**The System:** *"Story complete: The Knife in the Lunch Pail. Chat predicted a murder. Chat is upset. Please rate
this twist."*

### C. The Tommyknockers' Due

The Cornish men on the shift left the crusts of their pasties on a ledge for the **tommyknockers**, the little folk
of the mine in old Cornish miners' stories. Three knocks meant: get out. Rourke called it superstition and banned it.

- *Left the crust on the ledge by No. 4 like Gran said. Rourke kicked it into the sump.*
- *Three knocks last night in the east drift. Pengelly pulled us out. The roof came down an hour later.*
- *Rourke has banned the crusts. Rats, he says. Pengelly says the knockers do not forget.*
- *Knocking all day now. Everywhere. No one will tell Rourke what it means.*

**Mechanic:** this one is real in the game. **Three knocks near you means a rockfall (trap 3) is coming nearby.**
And a **ledge with a tin plate** is in the mine: **leave a food item on it** and the knocks warn you earlier for the
rest of the round.
**Payoff:** something small and quick is caught on camera on the ledge, eating.
**The System:** *"Story complete: The Tommyknockers' Due. Legal would like to clarify that the tommyknockers are not
union. Please stop asking."*

### D. The Canary

**Maisie Odell** kept the canaries at the head of the shaft and sent one down with every shift. This one is gentle:
she was never down the mine. Her notes are the letters she tucked into the birds' cages for the men.

- *Here is Pip. He is the loud one. If he goes quiet, you go up. Love, Maisie.*
- *Mr. Rourke says Pip is costing the company 4 cents a week. I am paying it myself.*
- *Please send Pip up with the shift tonight. Please.*
- (On an empty canary cage, deep in the mine, the door open) *Pip came up on his own on Wednesday. I waited at the cage
  all night. I am still waiting. — M.*

**Mechanic:** a small warm light, **the ghost of the bird**, flits through the drifts. It always flies **toward good
air** (away from blackdamp, trap 7), and sometimes toward the next note.
**Payoff:** the bird lands on a crewmate's shoulder and stays for the rest of the round.
**The System:** *"Story complete: The Canary. We are not crying. This is a heritage experience."*

### E. The Bell at 4:15

The cage operator, **Amos Pike**, had orders from the bank to bring the cage up and lock the shaft at 4:15 on the
Tuesday the mine was sold. He rang the bell, and he did not wait for an answer.

- *Bell at 4:15. Three rings. We ran. Cage was already going up empty.*
- *We rang back. Nine rings, the signal for men on the bottom. Nothing came down.*
- *Pike was a good man. Pike had a note from the bank. Those are not the same thing.*
- (On the cage itself, scratched into the iron) *A. PIKE. 1893. I HEARD THEM. I HEARD THEM EVERY NIGHT.*

**Mechanic:** this is the phantom bell (trap 8). Once all four notes are read, **the phantom bell stops lying**: it
rings honestly, 30 s before the cage really goes.
**Payoff:** Pike's ghost appears at the cage controls for the last few seconds of the round, holding the cage for the
crew.
**The System:** *"Story complete: The Bell at 4:15. The bank that sold the Lucky Tuesday is now a subsidiary of this
programme's parent company. We have nothing further to add."*

### Loose notes (no story, just texture)

- *If you find this, I owe Duffy 40 cents. Do not pay him. He cheats at cribbage.*
- *Candle stub, matches, one boot. Where is the other boot. Who takes ONE boot.*
- *Day four. Water's sweet in the north sump. Rourke still says the cage is coming.*
- *Somebody keeps moving my pick. I put it down HERE. I am writing this down so I know I am not mad.*
- *Mother, the work is steady and the air is good. (That is not true. Do not show her this one.)*

---

## 3. What this needs, and in what order

**Phase 1: bodies and notes (the stories).** One new actor, a body with a note: a slumped skeleton (or a Forge
body under a tarp until bones are sourced), a note id, and E to read it. Reading shows the note on screen (the System
window style, `DCCMenu`) and scores footage if you are recording. Note text lives in a data table, like the dialogue,
so new notes need no code. Story completion is tracked per round on the arena. **Reflection change:** a new class,
so the editor must be closed for the build.

**Phase 2: the cheap traps.** Powder crates, the rockfall with its three-knock warning (which makes story C
mechanical), and the phantom bell plus the cave-in clock (status line, sound and lights).

**Phase 3: the bigger traps.** Rotten planks and the sump level under them, the runaway cart (and a slope in a
tunnel), the lamp toggle with firedamp, blackdamp and the lamp-snuffer, and the canary.

**Art still missing.** Bones or a skeleton mesh. Nothing installed looks like one, and the Forge bodies would do in
the meantime. Also a pasty and a tin plate (easy Blender placeholders), and a canary (a tiny Blender bird, or a
small glowing light, which honestly reads fine).
