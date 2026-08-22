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
