# Dead Air, rebuilt as Hiss Cranson's show (plan, 2026-11-23)

Zoe: "Instead of it being the player use the body cam let's scrap that and just have it be Hiss Cranson hosting his
talk show and sending the players through various difficult encounters and then interviewing them at the end or
making snide (professional) remarks about them not making it."

Then: "I was picturing that Hiss would just interview the survivors at the end of each round and either play an
antagonizing or friendly role either buffing or adding more traps for the players."

And: "players could still be descending the mountain just getting interviewed or talked about. I'd say keep the
current sets and just have the TV studio be where players visit between rounds. And players who are out could just
sit in the audience along with random npcs."

**Status (2026-11-23): the first playable is built** (`Arcade/DCCHauntPrimetime.cpp`, `UI/DCCHUD_Primetime.cpp`). Built so far: the studio, the loop, Gauntlet / Lights Out / Hold Out and the finale, the couch, Producer's Pet and Marked, Blessings (Overtime, Lighting Department, Extra Takes) and Twists (Special Guest, Tight Schedule, Budget Cuts), the send-offs, the audience and the ratings payout. Not yet: Trust Fall, Floor Is Lava, Price Is Fright, the Ratings Boss as its own enemy, cheering or booing from the audience, the instant replay.

## The pitch

**DEAD AIR is now a live episode of PRIMETIME.** Every round is an encounter Hiss sends the contestants into. Every
round ends on his couch: he interviews whoever survived, and how that goes decides the next round. Charm him and he
quietly helps you. Annoy him and he gets creative with the traps. The audience loves it either way, and annoying him
is worth more Fame, so there is a real choice in it.

Everyone plays in the usual third person. No body cam, no tape and no filming button.

## The shape: down the mine, up to the studio

- **The rounds are the descent.** The crew still goes down the Lucky Tuesday, level by level, on the current mine
  sets (the generator, the timbered drifts, the stopes and the things in the dark). Each round is the next level
  down, a little deeper and a little worse, with one of the round types below laid over it.
- **The studio is between levels.** When a level ends, survivors are pulled up to Hiss's studio for the couch, then
  sent back down to the level below. During a level, Hiss talks about the crew over the broadcast.
- **The out are the audience.** Anyone knocked out sits in the studio audience among the background NPCs (the NPC
  Forge crowd) and watches the rest on the studio's big screens. They can cheer or boo during the couch, which
  nudges Hiss's mood a little (a small extra, not a deciding vote).

## The loop

```
Cold open -> [ Round -> Couch -> Hiss's verdict ] x 4 or 5 -> Finale -> Ratings
```

1. **Cold open (30 s).** The studio. Hiss gives the OPEN line and an INTRO for each contestant (both already
   written in `talkshow_lines.json`).
2. **Round (about 90 s).** The next level down the mine, with one round type from the deck below laid over it: a
   goal and a clock. Hiss commentates in the gold
   lower third: CLUTCH, CLOSE_CALL, DOWN, FUMBLE.
3. **The couch (about 15 s a survivor).** Survivors are pulled back to the studio. Hiss asks each one a question,
   and they answer with one of three buttons:
   - **Charm:** compliments, gratitude, playing along. He warms up.
   - **Banter:** trading jabs. Neutral, good for ratings.
   - **Defy:** mocking the show, the sponsors or Hiss. He cools, and the crowd goes wild (the most Fame).

   Players who did not survive get no interview. They get a **send-off** instead: a polite, warm and brutal
   remark about how they went out, on an "In Memoriam" card. Then they take a seat in the audience.
4. **Hiss's verdict.** His mood toward each contestant, and toward the team as a whole, sets up the next round. He
   announces it on air:
   - **Friendly → a Blessing:** something helpful in the next round.
   - **Antagonized → a Twist:** something nasty added to it.
   - Personal moods hit that one player ("Producer's Pet" for the charmer; "Marked" for the one who defied him).
     The team's mood changes the round itself.
5. **Finale.** The last round is always the Ratings Boss. All the Blessings and Twists so far are still in play.
6. **Ratings.** Ratings pay tickets (as views did), and Fame goes through `DCCAudience`. Defiance pays the most
   Fame; surviving pays the most tickets.

A player downed in a level is out: they watch the rest from the audience. If the team clears the level, Hiss offers
one of them back as a Blessing ("Extra Takes").

## Hiss's mood

- One number per contestant, from -3 (antagonized) to +3 (adoring), plus a team mood that is their average.
- **Moves it:** your answer (Charm +1, Defy -1); how you played (a clutch save +1; a fumble on live TV -1; hitting
  a sponsor's prop -1); and whether you cleared his round (+1 for the team).
- The verdict only acts at the ends (±2 and beyond); in between he stays a professional.
- **Fame for defiance:** every Defy pays Fame now, and every Twist it causes pays more Fame in the next round when
  you survive it. Being his favourite is safe; being his problem is good television.

### Blessings (friendly)

| Blessing | Effect next round |
|---|---|
| Producer's Pet | That player gets a shield that takes one hit |
| Extra Takes | One player from the audience comes back for the next level |
| Lighting Department | The set's lights stay on (a big deal in Lights Out) |
| Cue Cards | The goal or the safe door glows |
| Overtime | +20 seconds on the clock |
| Catering | Everyone starts the round fully healed, with a snack on the belt |

### Twists (antagonized)

| Twist | Effect next round |
|---|---|
| Marked | That player is the monsters' favourite target |
| Budget Cuts | Half the lights are off |
| Special Guest | An extra stalker enemy joins the round |
| Fresh Paint | More traps on the set (spike plates, falling rigging) |
| Tight Schedule | 20 seconds off the clock |
| Sponsor Rules | A random restriction: no jumping, no weapons or no sprint (breaking it costs ratings, not health) |

## The round deck

Hiss draws one per round, never the same one twice in a row. Each is built from systems we already have.

| Round | What happens | Built from |
|---|---|---|
| **Lights Out** | The level goes dark. Find three switches while the Dead Air things hunt by sound. | Mine generator, Dead Air enemies, `ADCCBreakerSwitch` |
| **Hot Seat** | One contestant is strapped into a chair. Everyone else holds off waves until it lets go. | Encounter waves |
| **The Floor Is Lava** | The floor drops tile by tile. Last ones standing win. | Gate Run hazards |
| **Trust Fall** | Pressure plates that only open the door while someone else is on theirs (with a crowbar option, per the puzzle rule). | `ADCCPressurePlate`, `ADCCPuzzleDoor` |
| **Price Is Fright** | Three doors: prizes, a monster, or a monster holding prizes. The team votes. | Cage vote widget |
| **The Gauntlet** | A trap corridor sprint to Hiss's desk. First one there gets a bonus. | Dead Air mine corridors |
| **Ratings Boss** (finale) | A big enemy "sponsored" by the audience. | Boss phases |

## What Hiss says

All of it is pre-written, voiced lines, the same pipeline as now (`Tools/gen_talkshow_lines.py` -> Kokoro), roughly
200 new lines:

- **Rounds:** `ROUND_<NAME>_INTRO` and `_CLEAR` / `_FAIL`; `CLUTCH`, `CLOSE_CALL`, `DOWN_<CrawlerName>`, `FUMBLE`
- **The couch:**
  - `Q_<mood>`: his question, which changes with how he feels about you
  - `REPLY_CHARM_<mood>`, `REPLY_BANTER_<mood>`, `REPLY_DEFY_<mood>`
- **Verdicts:** `BLESS_<name>` and `TWIST_<name>` (the on-air announcement of each)
- **Send-offs:** `SENDOFF_<round>` and `SENDOFF_GENERIC`
- **Finale:** `FINALE`, `SIGNOFF`

**Tie-in to Open Mic Week:** whoever wins Hiss's voice audition records these. Until then, Kokoro voices them.

## What goes, what stays

- **Goes:** the body cam, tape, the record button, the viewfinder, uploading from the cage, and footage scoring.
- **Stays:** the mine generator (sets built from it), the Dead Air enemies, the sound design, the audience and Fame
  system, ticket payouts, and the profile record (`Haunt.BestViews` becomes `Primetime.BestRatings`).
- **Maybe:** the old reel as an end-of-episode instant replay, if cheap.

## How it is built

- A new arena, `ADCCArcadePrimetime`. Dead Air's pad points at it; the old arena stays until this has had a play
  pass.
- A replicated state machine on the arena:
  - `Phase`: Open, Round, Couch, Verdict, Finale, Ratings.
  - The current round and its clock.
  - Each contestant's state (In, Down, Out) and Hiss's mood toward them.
  - The active Blessings and Twists, and the ratings.
- **Rounds:** each is a small class (`Begin` / `Tick` / `End`) that builds its set beside the studio.
- **Blessings and Twists:** each is a small modifier the round reads when it begins, so a new one never touches the
  rounds.
- **The couch:** reuses the existing button UI. Clients send their choice; the host moves the moods, picks Hiss's
  reply and plays it to everyone.
- **Build cost:** one reflection-level build for the arena and its replicated state. Rounds, modifiers and lines
  after that are mostly body edits and content.

## Order of work

1. **First playable:** the studio, the loop, three rounds (Lights Out, Hot Seat, The Gauntlet), the couch with three
   answers, two Blessings (Producer's Pet, Overtime), two Twists (Special Guest, Budget Cuts), the send-offs and the
   ratings. Lines in Kokoro.
2. **More:** the rest of the rounds, Blessings and Twists, and the Ratings Boss.
3. **Polish:** the instant replay, the studio dressing, and a trailer.

## Decided

1. **Setting:** the current mine sets for the rounds; the TV studio is only between levels.
2. **Players who are out:** sit in the studio audience among the NPCs and watch on the big screens (cheer or boo as a
   small extra).
3. **The studio:** built at runtime near the mine, with Hiss's desk, the couch, big screens showing the level, and
   NPC Forge background people in the seats.
