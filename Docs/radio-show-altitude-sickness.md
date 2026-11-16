# ALTITUDE SICKNESS with Dale Brandvold — KMHR 5280 AM

*"The only station broadcasting from below the waterline of the truth."*

A late-night call-in show on Denver's last working AM station. Dale has been right about everything, which doesn't
help him. The city really was repossessed by an off-world production company, and every time he gets close to saying
so, a sponsor cuts in. Sometimes the sponsor is him. The station is owned by Mile High Recovery Solutions. Dale does not
know this. Everyone else does.

**Runtime:** about 14 minutes read straight through. Every segment also stands alone, so the rover radio can shuffle them.

---

## How to use this script

### Cast (five readers + Zoe)

| Role | Who it suits | Notes |
|---|---|---|
| **DALE BRANDVOLD**, host | Your loudest reader | Gravel, conviction, no pauses. Never laughs at his own jokes. |
| **TERRY**, producer/call screener | Your deadpan reader | Sounds like he's eating. Has given up. |
| **THE ANNOUNCER** (all ad reads) | Zoe, or whoever does the best "smile voice" | Too warm. Reads the side effects faster than the claims. |
| **CALLERS** (4 of them) | The other two readers, two callers each | Phone voices. Overact. |
| **BRAD from LIQUIDATOR** | Anyone who can yell | One appearance. Maximum volume. |

Mixed casting is fine: one person can play every caller.

### Cut markers

- `✂ CUT OK` marks a clean seam. Delete everything between two of these and the show still flows.
- `✂ TRIM` marks a joke that runs long. Cut from the marker to the end of the paragraph if timing drags.
- `★ KEEP` marks a beat that pays off later. If you cut it, cut its payoff too (named in the marker).
- `[SFX: ...]` is a sound cue for the edit, never read aloud.
- *Italics in brackets* are stage directions, never read aloud.

### The bare-minimum cut (about 6 minutes)

Keep Segments 1, 3, 5 and 8. Read the ads in full: they're the point.

---

## Recording and mixing

The script is also in the **Voice Studio** (`npm run studio` in `Companion/`), under the **KMHR 5280 AM**
headings: 102 lines in 9 segments, one heading per segment. Lines saved there go to
`SourceArt/Voice/NPC/radio_lines.json`. **Edit lines in the studio from now on; this document is the reading copy.**
"New line" on a radio line inserts right after it, because the mixer plays lines in file order.

Two ways to record, both mixed by `Tools/radio_mix.py`:

**A. One line at a time in the studio** (Clean preset; the mixer adds the radio sound). Then:

```bash
python Tools/radio_mix.py studio
```

That writes `SourceArt/Voice/Radio/out/KMHR_S01.wav` … `KMHR_S08.wav`, one file per station ID
(`KMHR_S09_01.wav` …), and `KMHR_full_show.wav`. Lines get a 0.3 s breath between them. A line that starts
with "—", or follows a line ending in "—", cuts in 0.25 s early, so interruptions overlap. Unrecorded lines are
skipped and listed. If a segment is missing lines in the middle, it will have jumps in it.

**B. Everyone at once** (a Discord call recorded with Craig, one file per person, one folder per segment):

```bash
python Tools/radio_mix.py show "D:/Recordings/KMHR"
```

Add a `cast.json` like `{"Scott": "dale", "CJ": "caller", "Zoe": "announcer"}` so it knows who plays whom.
Takes are funnier this way. The catch is that a person playing two roles in one segment (Zoe as the announcer
and as the Vantage caller) gets one sound for the whole file.

**The sound by role:** Dale and Terry get an AM-radio band, callers a phone line, the announcer stays clean and
warm, and Brad is crushed and limited. Output is 48 kHz mono WAV at -16 LUFS, ready for Unreal.

**Music and effects:** put `music_*.wav` (looped, ducked under voices) and `sfx_*.wav` in the segment's folder
(`SourceArt/Voice/Radio/S01/` for studio mode). A `segment.json` places effects:
`{"sfx": {"sfx_static.wav": {"at": "end"}}}`. Details are at the top of `Tools/radio_mix.py`.

---

## SEGMENT 1 — COLD OPEN *(~1:10)*

`[SFX: AM static, tuning whine, settles on a theme sting: a cheap synth fanfare that sounds like a car-dealership jingle.]`

**DALE:** It's 2:17 in the morning, the air up here is thin and so is the official story. This is *Altitude Sickness*,
I'm Dale Brandvold, and I want you to do something for me. Go to your window. Look east. Look at the airport.

**DALE:** Now tell me why it's *closer* than it was last week.

**TERRY:** *[mouth full]* It's not closer, Dale.

**DALE:** Terry measured it. Terry measured it with his car.

**TERRY:** I measured my commute.

**DALE:** Which got *shorter*, Terry. Ground doesn't shrink. Unless somebody is *folding* it.

`★ KEEP — "folding the ground" pays off in Segment 7.`

**DALE:** We'll get to the folding. We'll get to the horse. We'll get to why every pigeon on Colfax is now facing the
same direction. But first —

**ANNOUNCER:** *[instant, too warm]* — but first, a word from Summit Compliance Group.

**DALE:** I didn't say that. Terry, did I say that?

`✂ CUT OK`

---

## SEGMENT 2 — AD: SUMMIT COMPLIANCE GROUP *(~0:45)*

`[SFX: soft piano, birdsong that loops slightly too obviously.]`

**ANNOUNCER:** Do you ever lie awake wondering who's watching? At Summit Compliance Group, we have great news. Everyone
is. And they love what you're doing.

**ANNOUNCER:** Summit Compliance Group helps Denver residents get *ahead* of their obligations. Whether you owe money,
time, or a debt you can't quite remember signing for, our friendly Recovery Partners will meet you wherever you are.
We already know where you are.

**ANNOUNCER:** Summit Compliance Group. *Your cooperation is being recorded for quality and entertainment purposes.*

**ANNOUNCER:** *[very fast]* Summit-Compliance-Group-is-a-subsidiary-of-Mile-High-Recovery-Solutions-compliance-is-not-optional-participation-may-result-in-relocation-to-a-lower-level-offer-void-where-the-ground-is-still-yours.

`✂ CUT OK`

---

## SEGMENT 3 — THE HORSE *(~2:00)*

`[SFX: theme sting, short.]`

**DALE:** We're back, and I have been informed I did not read that ad, which is exactly what someone who read that ad
would be told. Let's talk about the horse.

**TERRY:** Here we go.

**DALE:** Thirty-two feet of blue fiberglass. Glowing red eyes. Killed its own sculptor. And the official explanation
is "public art." Public *art*, Terry. You know what else is public art? A *beacon*.

**DALE:** Last Tuesday, 3:40 AM, the eyes went from red to green. For nine seconds. I have a caller who saw it. Terry,
put her through.

**TERRY:** Line two, Dale. It's Marjorie from Aurora.

**CALLER 1 (MARJORIE):** *[elderly, very calm]* Hi Dale, long-time listener. I saw the green eyes.

**DALE:** Thank you, Marjorie. Thank you. Tell them what happened next.

**CALLER 1 (MARJORIE):** The horse turned its head and looked at my Buick.

**DALE:** It *turned its head.*

**CALLER 1 (MARJORIE):** And then a man in a polo shirt came out of nowhere and put a little sticker on my windshield.
It said *"Asset Tagged."* And a smiley face.

`★ KEEP — "Asset Tagged" sticker pays off in Segment 8.`

**DALE:** And what did you do, Marjorie?

**CALLER 1 (MARJORIE):** I went home, Dale. And when I got there, my house had a little *"Asset Tagged"* sticker on it
too.

**DALE:** *[slow]* They're inventorying us.

**TERRY:** Or the HOA got new stickers.

**DALE:** The HOA has never once smiled, Terry.

`✂ TRIM — cut from here to the end of the segment if it's running long.`

**CALLER 1 (MARJORIE):** Also my grandson got a job offer. It says "Crawler, entry level, no experience necessary,
benefits include continued existence."

**DALE:** Tell him not to sign it, Marjorie.

**CALLER 1 (MARJORIE):** He already signed it, Dale. It signed itself. It was very convenient.

`✂ CUT OK`

---

## SEGMENT 4 — AD: LIQUIDATOR ENERGY *(~0:40)*

`[SFX: power chord, a can opening at deafening volume, a scream that might be joy.]`

**BRAD:** *[maximum volume]* ARE YOU TIRED OF BEING ALIVE AT A NORMAL SPEED?

**BRAD:** LIQUIDATOR ENERGY. FORTY-ONE TIMES THE CAFFEINE OF A REGULAR SCREAM. FLAVORS INCLUDE: BLUE. ANGRY BLUE.
AND **FINAL NOTICE**, NOW WITH REAL BERRY FLAVOR!

**BRAD:** I DRANK FOUR LIQUIDATORS AND OUTRAN A REPOSSESSION. *[beat]* I'M STILL RUNNING. I CAN'T STOP. THIS IS A
RECORDING. I'M IN NEBRASKA.

**ANNOUNCER:** *[calm, normal speed]* LIQUIDATOR. Liquidate your fatigue. Liquidate your sleep. Liquidate your assets
for convenient in-can credit. Do not consume more than one LIQUIDATOR per heartbeat.

`✂ CUT OK`

---

## SEGMENT 5 — THE CALL-IN HOUR *(~3:00)*

`[SFX: phone-line beep.]`

**DALE:** Phones are on fire tonight. Terry, who's next?

**TERRY:** Line four. Says his name is "Not Gary."

**CALLER 2 (NOT GARY):** *[whispering, wind noise]* Dale. Dale, I work night maintenance at the airport.

**DALE:** Go ahead, friend. You're safe here.

**CALLER 2 (NOT GARY):** I'm not, actually, but I appreciate it. Dale, there's a level under the baggage claim. And a
level under that one. I went down eleven floors before I found a vending machine.

**DALE:** What was in the vending machine?

**CALLER 2 (NOT GARY):** Health potions, Dale. Dollar-fifty. And a sign that said *"Floor 1 — Tutorial — Have Fun!"*
with the "Fun" in quotation marks.

**DALE:** *[to the audience]* The *quotation marks*, people.

**CALLER 2 (NOT GARY):** And Dale, there was a camera on the ceiling. And it *waved* at me.

**TERRY:** Cameras don't have hands.

**CALLER 2 (NOT GARY):** This one does now. I have to go. Something's offering me a sponsorship.

`[SFX: line drops.]`

`✂ CUT OK — Not Gary can be cut cleanly. Keep the next caller.`

**TERRY:** Line one. It's, uh — it says "Vantage Wellness."

**DALE:** That's a sponsor, Terry, don't —

**ANNOUNCER:** *[on the caller line, with phone EQ]* Hi Dale! Big fan.

**DALE:** Get off my phone line.

**ANNOUNCER:** We noticed you're stressed, Dale. Your heart rate spiked forty beats when Not Gary said "eleven
floors." Have you considered *not knowing things?*

**DALE:** How do you know my heart rate?

**ANNOUNCER:** Vantage Wellness offers a full line of soothing products to help you stop noticing. Try our new
**Unbothered™ Tea**, steeped in the warm feeling that someone else is handling it.

**DALE:** *[shaking]* Terry, kill the line.

**TERRY:** I can't. The button's sponsored.

`★ KEEP — the sponsored button pays off in Segment 8.`

**ANNOUNCER:** Vantage Wellness. *Peace of mind is a subscription.*

`✂ CUT OK`

---

## SEGMENT 6 — AD: BRIGHT PATH SOLUTIONS *(~0:50)*

`[SFX: acoustic guitar, a golden retriever bark, a child laughing on a loop.]`

**ANNOUNCER:** At Bright Path Solutions, we believe every Denver family deserves a fresh start.

**ANNOUNCER:** That's why we're offering every resident a free, no-obligation **Life Restructuring**. Our experts
will review your home, your car, your savings and your skeleton, and find the hidden value inside.

**ANNOUNCER:** Many families are surprised to learn they qualify for **Down-sizing**. One level down. Then another.
Families who go all the way down report the most exciting results of all.

**ANNOUNCER:** Bright Path Solutions. *The light at the end of the tunnel is our logo.*

`✂ TRIM — the disclaimer below can go if you're short on time.`

**ANNOUNCER:** *[fast]* Results-not-typical-most-families-are-not-surprised-they-are-screaming-Bright-Path-does-not-
guarantee-a-path-or-that-it-will-be-bright-or-that-it-goes-anywhere-but-down.

`✂ CUT OK`

---

## SEGMENT 7 — DALE'S THEORY BOARD *(~2:30)*

`[SFX: a marker squeaking on a whiteboard.]`

**DALE:** All right. Board's up. Terry, read me the strings.

**TERRY:** *[flat]* Red string: the horse. Red string: the airport getting closer. Red string: the stickers. Blue
string: your ex-wife.

**DALE:** The blue string is a separate investigation.

`✂ TRIM — the ex-wife joke can go.`

**DALE:** Here's what I know, people. Somebody bought this city. Not the land, the *show rights*. The ground is being
folded, like Terry's commute, so it'll fit on a *set*. Every pigeon on Colfax faces the same way because pigeons
face the *camera*. And every time I get close —

**ANNOUNCER:** — every time you get close to a great deal —

**DALE:** NO. Not this time. Every time I get close, a sponsor —

**ANNOUNCER:** — a sponsor like Mile High Recovery Solutions —

**DALE:** — *who?*

`[SFX: dead silence. Three full seconds. Even the static stops.]`

**TERRY:** *[quietly]* Dale. That's on the paycheck.

**DALE:** What's on the paycheck?

**TERRY:** Mile High Recovery Solutions. They own the station. They've owned the station since March.

**DALE:** *[long pause]* Since *March*?

**TERRY:** They bought the station, the tower, the parking lot, and the concept of AM radio.

**DALE:** Then why would they let me say any of this?

**TERRY:** *[eating again]* 'Cause you rate, Dale. People love you. Your numbers are through the roof. Off-planet,
mostly.

**DALE:** *[barely a whisper]* I have an audience.

**TERRY:** You have a *huge* audience.

`✂ CUT OK`

---

## SEGMENT 8 — SIGN-OFF *(~1:40)*

`[SFX: the theme sting plays, but slowed down and slightly wrong.]`

**DALE:** Folks. Listeners. *[beat]* Viewers.

**DALE:** I've spent twenty years telling you they were watching. Tonight I found out they're watching *me*. And
they're *enjoying it*. And I don't — Terry, I don't know how to feel about that.

**TERRY:** You could feel good about it. They sent a gift basket.

**DALE:** There's a *gift basket?*

**TERRY:** Came in around midnight. There's LIQUIDATOR in it. And a sticker.

**DALE:** What does the sticker say, Terry.

**TERRY:** *"Asset Tagged."* Smiley face.

`(Payoff for Segment 3. If you cut Marjorie, change the sticker line to: "It says 'Talent.' Smiley face.")`

**DALE:** *[resigned, then rallying]* Then here's my message to the folks in the polo shirts. To the people who folded
my city. To whoever is up there with the popcorn. I see you. I *know* you. And I will not be —

**ANNOUNCER:** *[gentle]* Dale, your segment's over.

**DALE:** I'm not done.

**ANNOUNCER:** Your contract says you're done.

**DALE:** Terry. Terry, cut the feed.

**TERRY:** Button's sponsored, Dale.

`(Payoff for Segment 5. If you cut the Vantage call, change this to "Button's broken, Dale.")`

**DALE:** *[final, into the mic]* Stay paranoid, Denver. Check under your floors. And if somebody offers you a
sponsorship —

**ANNOUNCER:** — say yes! *Altitude Sickness* was brought to you by Summit Compliance Group, Vantage Wellness, Bright
Path Solutions, and LIQUIDATOR. KMHR 5280 AM is a Mile High Recovery Solutions property. Dale Brandvold is a Mile High
Recovery Solutions property. You are listening on a Mile High Recovery Solutions property.

**ANNOUNCER:** Goodnight, Denver. Don't change the dial. *[beat]* You can't.

`[SFX: static swells, then cuts to silence like someone pulled a plug.]`

---

## Bonus: standalone station IDs and bumpers *(5–10 s each)*

Quick lines to drop between music tracks or to cover a hard cut. Any reader can do these.

1. **ANNOUNCER:** You're listening to KMHR 5280 AM. Five thousand two hundred and eighty feet above sea level, and
   sinking.
2. **DALE:** This is Dale Brandvold. If you're hearing this, they haven't gotten to the transmitter yet. *[beat]*
   Wait.
3. **TERRY:** KMHR. It's on.
4. **ANNOUNCER:** Traffic update: I-25 is now three feet shorter than it was this morning. Please merge accordingly.
5. **ANNOUNCER:** Weather: clear skies, light ash, a seventy percent chance of a floor opening up beneath you.
   Back to Dale.
6. **BRAD:** *[distant, out of breath]* STILL RUNNING. ENTERING KANSAS. LIQUIDATOR.
7. **DALE:** Remember, folks: the horse sees everything. The horse is not your friend. *[beat]* The horse might be
   *a* friend.
8. **ANNOUNCER:** This silence was sponsored. *[two seconds of silence]* Thank you.

---

## Lines to customise for the group

Swap these in for inside jokes. Each is a single line and can be replaced without touching anything around it.

- Caller names (*Marjorie from Aurora*, *Not Gary*): use the group's own call-in aliases.
- The airport vending machine's prices (Segment 5).
- What's in the gift basket (Segment 8).
- What the theory board's "blue string" is (Segment 7).
