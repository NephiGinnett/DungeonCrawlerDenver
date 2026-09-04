# Zany Enemy Roster — Fantasy Dungeon Crawler

A batch of Dungeon Crawler Carl-flavored monsters: puns as premises, satire as substance, and combat gimmicks baked into the joke. Full send, R-rated energy where it earns the laugh.

## Refined Bosses

Characters that have been fleshed out past the concept stage — fight structure, minions, and (where needed) enough physical detail to hand off to a modeler.

### Lord Voldemortgage — First Boss

A lich who didn't die from dark magic gone wrong — he died from a 30-year adjustable rate. He haunts a foreclosed dungeon wing wearing a suit two sizes too big, clutching a briefcase of forbidden paperwork. He's mid, petty, and endlessly condescending — the kind of villain who explains your own defeat to you in the tone of someone reading loan terms off a clipboard.

**Physical description (for model generation):**

Gaunt undead lich, roughly 6'5" but stooped, so he reads more like 6'0" of bad posture. Ashen grey skin with a papery, cracked texture, like drying parchment stretched over bone — faint red-stamped text bleeds through in places, as if he's tattooed with old foreclosure notices from the inside. Sunken eye sockets glow with a cold ledger-ink blue, no pupils, just faintly flickering columns of tiny numbers if you look close. Thinning grey-black hair combed hard to one side in a losing battle with a receding hairline.

Dressed in an ill-fitting three-piece suit — charcoal grey, slightly too large, shoulders drooping, sleeves swallowing his hands so only bony fingertips poke out. A wilted, too-long red tie is knotted loosely like a noose, the fabric fraying at the tip. His lower body doesn't fully resolve — from the waist down he dissolves into a drifting haze of loose paper, dust, and slowly spinning foreclosure notices instead of legs, so he glides rather than walks.

He carries a heavy leather briefcase manacled to his left wrist by a rusted chain; it's overstuffed, papers jutting out of every seam, occasionally leaking a slip that bursts into embers before hitting the ground. A cracked plastic name tag is pinned crookedly to his lapel: "HELLO my name is: YOUR LENDER." A halo of small red-stamped documents ("PAST DUE," "FINAL NOTICE," "OVERDUE") orbits slowly around his head and shoulders like a lazy satellite ring, glowing faintly at the edges.

Overall mood: less "terrifying death lord," more "the DMV had a death lord on staff and he's been passed over for a promotion for two hundred years." Cold blue lighting from the eyes and document-ring; everything else in dusty, desaturated business-formal tones.

**Boss fight structure:**

*Opening — Opening Statement.* The fight begins with Voldemortgage immediately conjuring his first wave of **3 Debt Eaters** out of the paper-ring around him, sending them ahead while he hangs back lobbing weak paperwork attacks (foreclosure notices as thrown blades, a "Notarized Slap"). This phase is about softening the party and letting the Debt Eaters chip away gold via **Repossession Nip**, feeding him small HP ticks. He's talkative and smug here — this is where most of his barks land.

*Midpoint trigger — Late Notice (at 50% HP).* Once he drops to half health, he stops attacking, straightens his tie, and delivers a short monologue ("Let's talk about your... payment history"). This summons a second, larger wave of **5 Debt Eaters** and immediately kicks off his big set piece:

*Balloon Payment charge (3-turn telegraph).* Numbers start piling up in a glowing stack above his head, visibly climbing each turn — a clear "get ready" signal to the player. During the charge:
- Voldemortgage himself is more vulnerable (reduced defense) since he's channeling instead of acting.
- Every Debt Eater still alive when the charge completes adds bonus damage to the final hit — so the incentive is to burn down the second wave *before* turn 3, not fight through Voldemortgage directly.
- If the party manages to clear every Debt Eater before the charge finishes, Balloon Payment fizzles into a much smaller "Minimum Payment" hit instead of the full-force version — a clean risk/reward lever for the encounter.

*Aftermath — exposed window.* Whether Balloon Payment lands big or small, casting it drains him — for two turns afterward his defense is down and he can't summon, giving the party their real opening to finish the fight.

*Death — the lien.* If he still goes down before delivering Balloon Payment at all (a fast, aggressive party), his death effect still triggers: his "lien" transfers to a random party member, applying a small recurring debt-damage tax at the start of future battles until it's paid off in gold. This keeps his gimmick alive even if the fight goes short.

**Minions — the Debt Eaters:**

Low-level wraiths hooded in robes stitched from past-due notices and junk mail. Individually pathetic (one or two hits and they're gone), summoned in waves of 3 (opening) and 5 (midpoint). Their bite, **Repossession Nip**, rips a small chunk of gold from the party on hit and feeds it to Voldemortgage as HP — and any still alive during the Balloon Payment charge add damage to it, so they're the real time pressure of the fight, not just chip damage.

**Battle barks (Debt Eaters):**
- "WE JUST NEED TO VERIFY YOUR IDENTITY."
- "THIS CALL MAY BE RECORDED FOR QUALITY ASSURANCE."
- "YOUR EXTENDED WARRANTY IS ABOUT TO EXPIRE."
- *(on death)* "I'LL HAVE TO ESCALATE THIS TO MY SUPERVI—"

**Loot drops:**
- Voldemortgage: a ring that grants +1 to persuasion but -1 to everyone's opinion of you at parties.
- Debt Eaters (rare, low chance per kill): a single gold coin, still warm, worth exactly one gold less than it should be.

---

## Raw Ideas

Concepts at the "quick pitch" stage — not yet fight-planned or model-ready, but good bones if you want to develop any of them next.

### Sir Loin of Beef

A knight forged entirely from prime cuts, dripping marinade instead of blood, clanking around in a suit of armor made of bone china dinner plates. He challenges the party to "duels of honor" that are actually thinly veiled cooking competitions — he grades their attacks like a judge on a steak show and gets *angrier* the more well-done his wounds look. His finishing move is called **Medium Rare Fury**, and if the party lets a fight drag too long, he starts "resting" mid-battle, healing to represent the meat "settling."

**Loot drop:** a sword that inflicts bleed damage but occasionally distracts nearby enemies with the smell of seared garlic butter.

### The Karengeist

A translucent, floating specter wearing a visor and pushing an invisible shopping cart through the dungeon, endlessly hunting a manager who does not exist. She can't be reasoned with, only appeased — refusing to attack her provokes a **Speak To The Manager** shriek that summons three lesser Karen-wisps. Her real threat is **Return Policy**, an attack that un-does one buff the party just cast, "refunding" it straight back to nothing.

**Loot drop:** a coupon scroll that halves the price of the next shop item — if the shopkeep honors it, which he won't.

### Audit Trail

A bureaucratic wraith made of receipts, stitched together from a thousand unresolved expense reports. Nearly unkillable through direct damage — instead, the party has to find the "discrepancy" (a specific glowing ledger page hidden on the battlefield) to make him collapse into paper confetti. Every turn he doesn't get audited, he casts **Late Fee**, stacking a damage-over-time effect that compounds.

**Loot drop:** a briefcase that, once per dungeon, cancels one enemy's next attack by burying it in red tape.

### Meathead the Ogre, Certified Personal Trainer

An ogre in a stringer tank top who narrates his own combat like a fitness influencer filming content. He "counts reps" out loud during his attacks, and if the party doesn't interrupt him, he hits a **Pre-Workout Rage** state at rep 10 that doubles his strength. He also insists on "spotting" party members, which means grabbing one mid-fight to "help with their form," locking them out of acting for a turn.

**Loot drop:** a protein shaker that heals a small amount of HP but has a chance to inflict Bloat (reduced dodge) for two turns.

### The Real Estate Wyrm

A dragon who doesn't hoard gold — he hoards "charming starter homes with so much potential." His lair is wallpapered in listing flyers, and his breath weapon, **Market Inflation**, doesn't burn the party so much as make everything in the next shop cost double. He's obsessed with square footage and will pause combat to appraise the room, giving the party a free turn if they can convincingly lie about the dungeon's "resale value."

**Loot drop:** a deed to a "cozy" one-room dungeon cell that can be used as a fast-travel point — for a monthly fee.

### Cthul-Loo

An eldritch horror that lives in the dungeon's one working bathroom, all tentacles and porcelain, worshipped by a small cult of goblins who've formed an orderly line outside the door. Its attacks are named things like **Unspeakable Clog** and **The Reckoning Flush**, and it periodically forces the whole party to "wait their turn," skipping their next action while it "occupies the stall." Deeply disgusting, weirdly polite about it.

**Loot drop:** a plunger that functions as a blunt weapon and, once per fight, can be used to "unclog" a status effect off an ally.

---

Want me to keep developing more of the raw ideas into refined bosses next, or work on something else for Voldemortgage — like phase-2 dialogue lines, or a reference sheet breakdown for the model (front/side view notes, color palette, prop close-ups)?


---

## Floor 1 — the Denver layer (added 2026-08-18)

Voldemortgage and the Debt Eaters carry the *foreclosure* half of Floor 1. These
carry the *Denver* half. All original; the tone bible's rule is that the joke should
be specific enough to be checkable.

### The Nuclear Family — BUILT ✅

`Characters/DCCEnemyCharacter_NuclearFamily.*` — three classes, compiled.

**The premise is real, which is why it works.** Rocky Flats sat sixteen miles
upwind of Denver making plutonium triggers, leaked for decades, got raided by the
FBI in 1989, and is now — genuinely, on purpose — a National Wildlife Refuge. The
joke is already written; the roster only has to point at it.

A 1950s promotional-film family, still cheerful, still doing the barbecue, gently
luminous. They are not angry and they do not chase hard. They want you to stay a
while, and staying is the thing that kills you.

**Physical description (for model generation):**

Four figures lit from within, a soft sickly green-white that pools on the ground
around them. Dad in a short-sleeved plaid shirt and an apron reading WORLD'S OKAYEST,
holding tongs, eternally mid-flip over a barbecue whose coals are not coals. Mom in
a full-skirted 1950s dress and cat-eye glasses, offering a casserole dish that
glows through the foil. The Kid, maybe nine, in a striped tee, holding something
cupped in both hands that he desperately wants to show you. Skin slightly
translucent, veins reading as faint blue-white circuitry. Their edges are a little
soft, as if photographed with too long an exposure. Everything they wear is
period-perfect and slightly too clean. Dust motes drift upward around them rather
than down.

Overall mood: not menacing. *Welcoming.* The horror is that they mean it.

**Why they exist mechanically:** they are the counter-argument to every other fight
on the floor. Debt Eaters and Voldemortgage both reward closing to melee and holding
position. The Family punish exactly that — their threat is a **dose** that
accumulates while you stand near them and decays when you back off, so the correct
play is strike-and-withdraw. A party that has learned Floor 1's other lesson loses
to them on the first attempt. That is the point of putting them on Floor 1.

**The dose curve** (`UDCCRadiationAura`, reusable — it is a *place* mechanic, not a
creature trait, so leaking barrels and the Diving Pool can carry one too):

- Exposure builds at ~1/sec inside the radius, decays at 0.5/sec outside
- Nothing happens below a **safe dose of 2** — walking through is free
- Above that, damage scales with accumulated dose, so the punishment is for
  *lingering*, not for entering

**Grief is the twist.** Every family member who dies raises the aura Intensity of
everyone still standing — wider, faster, harder. The Kid grieves hardest at 0.9,
against Mom's baseline 0.45 and Dad's own 0.5, so **killing the Kid first roughly
doubles the pressure for the rest of the fight.** He is the obvious first target
and the worst one.

| Member | Aura | Note |
|---|---|---|
| **Dad, Still Grilling** | 700 radius, double damage | Widest and slowest. The barbecue is the most dangerous furniture in the room |
| **Mom, Insisting You Eat** | 480 radius, fastest gain | She *follows*, so retreating buys less against her than against Dad |
| **The Kid, Showing You Something** | 380 radius, fragile, quick | Kill last |

Still needed: real models, and a green point light on each so the aura is visible
before it is felt. **Right now the danger zone is invisible**, which will read as
unfair rather than as tense — that is the one thing to fix before playtesting them.

---

### Supercan — concept written 2026-08-27, queued for Meshy generation

**Not built.** No C++, no AI, no stats — this pass is the design and the model brief
only, per Zoe's own scoping instruction. Description below is written to be handed
directly to the Meshy image-first pipeline (the fixed process that solved the
melee/Head Diver detail-loss problem), same as every other roster entry that's gone
through it.

Denver's single-stream recycling bin, animate, chipper, and armed. Every civic
bureaucracy on this floor eventually gets a body — Voldemortgage is the mortgage
department, the Debt Eaters are collections, the Nuclear Family is the utility that
poisoned the ground rent-free for forty years. Supercan is Denver Recycles, and he
has notes about what you just threw away — delivered with a mascot's grin, not a
collector's sneer.

**Physical description (for model generation) — revised 2026-08-27, visual direction
changed:**

A municipal wheeled recycling toter, roughly waist-to-chest height on a person,
scaled and proportioned into a torso — the kind of 96-gallon bin every Denver curb
has, complete with the two flip-lid doors on top (now reading as a stiff, jutting
jaw when clenched) and the little diamond recycling decal stamped dead center like
a chest emblem. Body is clean, glossy white plastic with bold black trim tracing
the lid seam, the rim, and the wheel wells — sports-mascot color-blocking, not
weathering. No grime, no rust, no dents: this is a mascot costume, not a curbside
relic.

No legs — instead the bin's body is mounted dead-center on a pogo stick, its spring
polished rather than rusted, giving a cheerful little bounce-in-place even at idle.
Where arms would go, a pair of big, round, oversized white gloves — exactly the
Mickey-Mouse four-fingered cartoon-glove silhouette, black trim ringing each cuff —
are the whole story of his hands; no visible arm rig underneath, the gloves simply
extend from the body the way a mascot costume's do. Where a face would go, the
underside of the raised lid becomes an expressive slot — a gap of stacked, clean
cans and bottles behind it catching light like a grin, no eyes, just the dark
interior of the bin reading as a mouth whenever he "shouts."

His weapon is an aluminum baseball bat, glossy and unscuffed, gripped in one of the
big white gloves like a cafeteria mascot brandishing a foam novelty prop rather than
a weapon. A few pieces of loose recyclable debris — a flattened can, a shard of
green glass, a torn cardboard flap — sit visibly but neatly inside him and spill out
through the lid gap when he moves hard.

Overall mood: **school cafeteria mascot that just got handed the recycling
program**, not a haunted object. Bright white-and-black color-blocking, oversized
cartoon gloves, a bat held like a prop rather than a threat. Comic-friendly on
sight, comic-menacing only once the bat actually swings — the joke is that
something built to be huggable is the one hitting you.

**Behavior/moveset concept:**

Core gimmick is the pogo stick as both his movement and his tell. He does not walk —
he bounces, in a stiff, rhythmic, slightly-too-fast hop that telegraphs everything he
does by how the spring compresses before it releases:

- **Curbside Pickup** (bounce-slam): a visibly deeper compress than his normal
  travel-hop, held half a beat longer, then a leap straight up and a hard downward
  slam on landing — an AoE thud with knockback centered on where he comes down.
  The long compress is the dodge window; the punishment for not reading it is
  getting knocked well clear of wherever the fight actually is.
- **Bat swings** for standard melee when he's bounced into range — a two-hit combo,
  a horizontal "Foul Ball" and a harder overhead "Home Run Swing" that knocks the
  target back a full pogo-hop's distance, consistent with a guy whose entire kit is
  "hits you like you're being put out with the trash." Landing a solid hit rattles
  more debris loose from inside him, littering the ground around him — cosmetic for
  now, but a natural hook for a later "he's dropping his own loot mid-fight" beat if
  that's ever worth building.
- Between bounces he's briefly grounded and vulnerable — the pogo's rhythm means
  there's always a next compress to read, so a party that learns his cadence can
  punish the landing rather than chasing the bounce.

**Stat-block-appropriate role:** an elite melee bruiser, not a swarm minion and not a
floor boss — closer in weight class to a named brute than to a Debt Eater, and
distinctive/singular enough (unique model, a bat rather than a shared weapon pool,
his own bounce-telegraph language) to justify the cursed-equipment guaranteed drop
the other session is scoping. Good fit anywhere Floor 1 wants one memorable, higher-
stakes single fight rather than another wave — the Pantry's still-flagged "whose
unclaimed property is this" gap (`room-purpose-audit.md`) is a natural home if a
room needs to be picked, since a possessed recycling bin guarding a hoard of
other people's stuff closes that gap and gives the room a fight it currently
doesn't have, but nothing about the concept requires that placement specifically.

**Next step:** hand the physical-description block above to the Meshy session for
image-first generation. No stat block, `DT_DCCEnemies` row, or C++ class yet —
those follow the model, not before it, matching how the rest of this roster's BUILT
entries were sequenced.

---

### Still ideas — the rest of the Denver layer

**Altitude Sickness.** Denver is the Mile High City and every visitor gets wrecked
by it. A wispy, apologetic thing that drains *stamina* rather than health — climbing
and sprinting both already run off that pool, so it attacks the traversal systems
instead of the combat ones. Beating it is about pacing. Flavour: it is genuinely
sorry, it just lives here and you do not.

**The HOA.** Three-to-five identical polo shirts with clipboards. They do not attack;
they issue **violations** — stacking debuffs for things you did several rooms ago
(*"Unapproved Exterior Modification: you broke a door"*, *"Noise Complaint: you
shouted"*). The counter is compliance or annihilation, and annihilation costs AI
Favor because the audience finds it gauche. First enemy that punishes *how* you
played rather than how you fought.

**Last Call.** Not a creature — a floor-wide timer. When the Cantina closes, the
lights come up and everything still in the building becomes hostile, including
things that were not. Reuses the Balloon Payment's telegraph shape at floor scale.

**The Brunch Rush.** A crowd, not a fight. Dozens of low-health patrons who want to
be *seated*, moving as a mass and blocking movement. Damage is almost incidental —
the threat is being unable to get anywhere. Pairs well with the Diving Pool arena.

**Sir Loin of Beef** and **The Karengeist** from the Raw Ideas list below both fit
the Cantina directly and would need no reworking to place here.

---

## Breadth pass — general roster (2026-08-27, overnight, no sign-off pending)

Ten concepts, written for spread rather than depth on every one — Zoe's own framing:
breadth now, she'll pick favorites to refine later. Not floor-locked; some clearly
suit the Cantina (they're written in-voice) but nothing here assumes Floor 1
specifically, since later floors need a bench too. Same tone rule as everything
else on this roster: the joke has to be *specific enough to be checkable*, not
generic fantasy dressing with a corporate coat of paint.

Each entry closes with a **Sourcing** line, per the established philosophy (same bar
as the boilerplate-rifle test and Supercan): Meshy is for concepts that are
genuinely novel, hard to source, or built around a silhouette no stock asset has —
everything else gets "free-source a body and reskin," because that's faster, free,
and exactly as good for anything humanoid-shaped wearing a costume.

### Fodder — low-tier, don't overthink these

**Repo Intern.** Twenty-two years old, unpaid, holding a clipboard with a
repossession order he clearly did not write and does not want to be enforcing.
Laminate badge that says INTERN in Comic Sans, taped-together headset. Attacks
apologetically — a weak swing with whatever's nearest, usually the clipboard
itself. Dies in one or two hits and says so out loud on the way down ("this is
going in my exit interview"). Swarms in twos and threes; the joke is that there's
always another one behind him, because the internship program never actually
ends.

**Sourcing:** free-source a generic young-adult humanoid body, reskin with a
laminate badge and a clipboard prop.

**Loss Prevention Associate.** Mall-cop energy, ill-fitting polo with a store logo
that's been blacked out with marker (legal made them stop using the real one),
plastic baton, walkie-talkie that only ever says static. Slightly tougher than
Repo Intern — a real melee threat in numbers — and calls for backup on sight,
which is mechanically just "spawns another Loss Prevention Associate," so a
fight left alone gets worse, not better.

**Sourcing:** free-source a generic adult humanoid body, reskin with a security
polo and a baton prop.

**Temp Worker (Untrained).** A person handed a weapon they have never held before
and told to "just wing it" — a direct joke on the ruleset's own Untrained rule
(Attack Skills at Disadvantage, no Rank). Mechanically it *should* be played that
way: worse accuracy than a real combatant, but occasionally gets lucky, which
should read as funnier than a flat weak stat. Generic office-casual clothes, a
name tag reading HI MY NAME IS ___ with the blank never filled in, visibly
unsure grip on whatever it's holding (kitchen knife, stapler, a rolled-up
performance review).

**Sourcing:** free-source a generic humanoid body, reskin with office-casual
clothes and a blank name tag.

**Focus Group Ghost.** A translucent, faintly annoyed spectre clutching a tablet
and a stylus, endlessly re-reading a question it has already asked twice. Attacks
by asking the party to rate their pain "on a scale of one to five" and marking
down whatever they say as damage regardless of the answer — the joke being that
the survey was never actually listening. Low threat alone, unpleasant in a group
because they interrupt actions to demand a response.

**Sourcing:** free-source a generic ghost/spectre body, reskin translucent with a
tablet prop.

### Mid-tier

**Middle Manager, Recently Promoted.** A real bruiser — the first fodder-adjacent
enemy on this list that actually hits hard, because he's been given authority for
the first time in his life and is not equipping it well. Wields a "WORLD'S OKAYEST
BOSS" mug like a mace (thrown, then retrieved, never replaced even when it's
clearly cracked). Signature move, **Micromanage**: stands next to an ally and
halves that ally's next cooldown *and* their next damage — "helping," mechanically
a debuff wearing a buff's clothes. Removing him from a fight noticeably speeds it
up, which is the point.

**Sourcing:** free-source a generic adult humanoid body, reskin with
business-casual clothes and a mug prop.

**Compliance Drone Swarm.** Small hovering camera-drones stamped with a
corporate seal, patrolling in twos and threes, individually harmless but backed
by a genuine mechanical threat: any drone that has line of sight on a party
member marks them, and a marked target takes bonus damage from every *other*
source for the duration — so the actual threat is failing to kill the drones
before committing to a fight, not the drones themselves. Dies in one hit; the
punishment is for ignoring it, not for it existing.

**Sourcing:** free-source a generic small hover-drone asset, reskin with a
corporate decal.

**The Onboarding Buddy.** A company-mascot chatbot given a body — bright,
oversized plastic grin fixed in place, a lanyard reading HI, I'M HERE TO HELP!,
cheerful synthesized voice that never drops its script even mid-fight ("Great
question! Let's find out together!"). Its attacks are literally its help
prompts — a **Quick Tip** jab, and a **Let's Circle Back** grab that yanks a
target out of position and resets their current action, delivered in exactly the
same relentlessly upbeat tone as its damage-free lines. The horror is entirely
tonal: nothing it says ever changes register, whether it's welcoming you or
hitting you.

**Sourcing:** Meshy. This is a genuinely novel silhouette — a fixed-grin mascot
chassis with a lanyard, built to read as "helpful" and "wrong" at once — not
something a stock humanoid-plus-costume reskin gets close to, same bar as
Supercan.

**The Committee.** Not one body — several focus-group participants who have,
somewhere along the way, physically fused into a single arguing mass, limbs and
lanyards and half-finished coffee cups all still individually visible but sharing
one lurching gait. It attacks by **vote**: every couple of turns it audibly argues
with itself over what to do next, and the party can see (and influence, if
something in the room lets them sway an opinion) which option is "winning" before
it commits, which is a lot of tell for a fight that hits hard once it actually
decides. Occasionally it deadlocks and skips its turn entirely, "circling back."

**Sourcing:** Meshy. A fused many-bodies-into-one silhouette, all still distinctly
readable as individual people mid-fusion, is not something any stock asset or
simple reskin can approximate — this is exactly the kind of concept the pipeline
exists for.

### Named / mini-boss tier

**The Self-Checkout.** A freestanding self-checkout kiosk, animate, with four
retractable bagging-arm limbs unfolded from its base and a barcode-scanner "eye"
that sweeps the room in a slow red arc. Screen-face displays a looping, chipper
"PLEASE WAIT FOR ASSISTANCE" even while it's actively fighting. Its signature line
and its signature attack are the same thing: **"Unexpected Item In The Bagging
Area"** triggers every time it lands a hit, and stacks — at three stacks the kiosk
locks up, flashes its "CALL AN ATTENDANT" light, and briefly can't act, which is
the fight's real punish window. Between those spikes it scans the nearest
target with its laser eye (a thin, precise ranged tick) and swats with whichever
bagging arm is closest. Distinctive silhouette, distinctive mechanic, sized to
carry a real (if minor) fight rather than a filler mob.

**Physical description (for model generation):** A chest-high self-checkout
kiosk unit — flat-panel screen "face," barcode scanner slot as a mouth-adjacent
detail, card reader and receipt printer visible on its front like buttons on a
shirt. Four thin, articulated mechanical bagging arms fold out from housings on
its sides and back, each ending in a simple three-finger gripper. Mounted on a
low, wheeled base rather than legs — it glides rather than walks. Screen glows a
flat retail blue-white; the scanner eye on an adjustable stalk above the screen
pulses red when actively targeting. Body is off-white/beige injection-molded
plastic, scuffed at the corners, a peeling "THANK YOU FOR SHOPPING WITH US"
decal along the base. Overall mood: mundane object, uncannily animated, treated
by its own design as more helpful than it is being.

**Sourcing:** Meshy. A four-armed wheeled kiosk chassis is a genuinely novel
build with no stock equivalent — same bar as Supercan, and arguably a cleaner
case for it (there is no "generic self-checkout" asset to even attempt a reskin
from).

**The Severance Package.** A tottering stack of cardboard moving boxes, taped
and re-taped, roughly held together in a vaguely humanoid shape by a stretched
"FRAGILE" ribbon wound around it like a sash. Arms are just more boxes, oddly
poised. It carries, cradled against itself the way you'd carry a framed photo, a
single desk photo frame with no picture in it — and occasionally holds it out to
the party mid-fight, as if introducing itself, before resuming the attack. Its
gimmick: it doesn't summon adds, it **unpacks** them — mid-fight it tears open
one of its own boxes and a Repo Intern or a Loss Prevention Associate climbs out,
genuinely startled to be there, and joins the fight a beat behind. Enough of
these and the Severance Package itself gets visibly smaller and easier to hit,
so the party is choosing between focusing the boss down fast or thinning what it
keeps producing — a real tactical fork, not just an HP race.

**Physical description (for model generation):** A roughly person-sized, unstable
stack of five or six mismatched cardboard moving boxes, various sizes, all
heavily taped with visible over-taping at the seams as if resealed more than
once. Bound together with a wide ribbon reading FRAGILE in faded red block
letters, wrapped diagonally sash-style. The "head" is the smallest top box, tilted
at a slight, expressive angle. "Arms" are narrower boxes jutting from the sides at
shoulder height, held in a static, slightly too-formal pose. One box near chest
height has its flaps loose and gaping, straining slightly outward as if something
inside is pushing to get out. Cradled at the base of the "arms," a plain black
desk photo frame, empty, glass catching the light. Everything is scuffed
cardboard-brown and packing-tape translucent; nothing about it glows or menaces —
it just should not be standing, and is.

**Sourcing:** Meshy. A stacked-cardboard-boxes-as-a-body silhouette, specifically
one built to visibly "unpack" mid-fight, is not something a stock asset covers —
same tier of distinctiveness as Supercan and the Self-Checkout.

---
