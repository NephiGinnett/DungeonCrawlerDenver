# Dungeon Crawler Denver — Tone Bible

Themes, clichés and the voice of the System AI. This is a **content** document, not a
design spec: it exists so that a room, a loot drop and an announcement all sound like they
came from the same sadistic showrunner.

Everything here is original material written *in the spirit of* the books — same tonal
rules, none of the books' actual floors, characters or copy. The joke that makes this ours
is the setting: **Denver, rendered as a televised deathtrap.**

---

## 1. The voice

The System AI is not a villain. It is a **producer with a quota**. It is never angry, never
threatening — it is *disappointed by low engagement numbers*. Everything it says is
filtered through the assumption that suffering is content and content is currency.

Five rules that keep the voice consistent:

1. **Corporate cheer over menace.** "Please note that dismemberment is not covered by your
   complimentary insurance." Never "you will die here."
2. **The audience is always present.** The AI reports viewer reactions like weather.
   Players should feel watched, rated, and slightly embarrassed.
3. **Bureaucracy is the horror.** Forms, tiers, terms of service, patch notes, a mandatory
   safety briefing delivered *during* the fight.
4. **Punch down at the dungeon, never at the players' grief.** Mock the format, the
   sponsors, the monsters, the AI itself. Real loss lands harder when the AI treats it as
   a ratings spike — but don't have it mock a player's actual death beat.
5. **Specific beats generic.** Not "a monster appears." A *Regional Assistant Manager of
   Pain, Tier 3, currently on a performance improvement plan.*

**Rule of thumb:** if a line could appear in a genuine airport safety video, it's on target.

### Announcement templates

Reusable shapes. Fill the brackets; the rhythm does the work.

- **Floor open:** *"Floor [N] is now live. [Number] crawlers remain. Local time is 4:15 AM.
  Please enjoy [absurd amenity]."*
- **Sponsor pitch:** *"This encounter is brought to you by [brand], the official [product]
  of not dying. [Brand] is not liable for [the exact thing about to happen]."*
- **Achievement:** *"Achievement unlocked: [Painfully Specific Name]. Two other crawlers
  have earned this. Both are deceased."*
- **Nerf/patch note:** *"Patch 1.4.2: [thing that was working for players] has been
  adjusted for balance reasons. We hear you. We simply disagree."*
- **Low engagement warning:** *"Your viewership has dropped [N]%. A complimentary
  difficulty adjustment has been scheduled."*
- **Crowd demand:** *"Chat would like to see you try that again. Chat is not asking."*

---

## 2. Floor themes — the Denver layer

The conceit: the dungeon didn't build these places. It **repossessed** them, then made
them legally distinct enough to avoid a lawsuit. Every floor is a real Denver landmark
run through a focus group.

| Floor concept | The joke | Encounter hook |
|---|---|---|
| **The Terminal** — an airport that is impossibly, administratively underground | Everyone already believes there's something under that airport. The AI is offended you think it's a *conspiracy* rather than *a venue* | Baggage-claim carousel that never stops; a "delayed" flight board where a boarding group flashing red means a wave spawns |
| **The Blue Horse** — an equine statue with red eyes, patiently waiting | The city's own cursed landmark, promoted to management. It killed its sculptor. It has this on its résumé | Arena boss. It does not chase. It waits. It is very good at waiting |
| **The Mint** — a vault full of money nobody is allowed to spend | Loot Vault where the loot is *technically* worthless and the AI finds this hilarious | Puzzle: the gold is real, the exit weighs you |
| **Cliff Diver Cantina** — a cavernous fake-Mexican restaurant with a waterfall and performers who dive to their deaths, nightly, forever | Beloved regional landmark, universally agreed to have terrible food, now with a body count | Narrative + combat. The divers are staff. The staff are not okay. The sopapillas are, disturbingly, fine |
| **Red Rock Amphitheatre** — natural stone arena | Every band plays here eventually. So does every boss | Wave arena with tiered seating full of screaming audience — the crowd is *literally* watching |
| **Mile High** — a floor where the altitude keeps increasing | Denver's one boast, weaponized | Stacking `Thin Air` debuff; the fight is against the timer of your own lungs |
| **The Dispensary** — brightly lit, calm, staffed by the extremely relaxed | Legal-weed retail as a Safe Room with terrible advice | Safe Room. The budtender is an NPC who gives genuinely useful hints, eventually, if you're patient |
| **I-Twenty-Five** — an eight-lane freeway, permanently gridlocked, in a dungeon | Traffic as a hazard biome | Traversal puzzle. Vehicles are monsters. Merging is a skill check |
| **The Overlook Annex** — a grand mountain hotel with an unfortunate reputation | Just far enough from Denver to be legally distinct | Puzzle/Narrative floor. Nothing chases you. Something is *arranging* things |
| **Casa de Hail** — a suburb under permanent golf-ball-sized hail | Colorado's real natural predator is a body-shop invoice | Environmental hazard floor; cover is the mechanic |

**Floor naming convention:** the AI always gives floors a corporate event name that
undersells the danger. "Floor 4: A Celebration of Local Cuisine."

---

## 3. Room clichés worth stealing (and the twist that redeems each)

Clichés are load-bearing. Players *want* the trope; the joke is in the delivery.

| Cliché | The twist that makes it ours |
|---|---|
| Locked door, key on the other side | The AI narrates the key's location helpfully, repeatedly, in the tone of a hint system that thinks you're stupid |
| Mimic chest | It's unionized. It explains its grievances before attacking. It has a point |
| Long hallway, obvious trap | The trap is real, but the *floor* is sponsored, so it plays an ad first — the ad is the timing tell |
| Boss with a health bar and a name | The name has a job title and a department. Killing it triggers an automated out-of-office reply |
| Puzzle with coloured levers | Colour-blindness accessibility patch was applied badly; levers are now labelled with adjectives ("smug", "damp") |
| Safe room with a merchant | Merchant is contractually forbidden from selling you the one thing you need, and is furious about it |
| Rat swarm as the tutorial enemy | They're not rats. They're *interns*. They respawn because HR insists |
| Collapsing bridge | Structurally sound. The AI just wants the shot |
| Ominous whispers in the dark | It's the audience. They're arguing about your build |
| Ancient prophecy foretold your arrival | Written six minutes ago by a marketing subroutine, backdated for engagement |
| Final door needs 4 party members | It doesn't. It's a co-op tutorial. The AI just likes watching you cooperate |

---

## 4. Sponsors and AI Favor beats

Sponsors are how the audience touches the game. Tie these to `UDCCFavorComponent` tiers.

**Sponsor naming:** two mundane words, one aggressive claim. *Bright Path Solutions.
Vantage Wellness. Summit Compliance Group.* The more harmless the name, the worse the deal.

| Favor tier | What the audience does |
|---|---|
| **Unwatched** | Nothing. No offers. The AI occasionally checks whether you're still alive out of politeness |
| **Noticed** | First sponsor offer, insultingly small. A branded consumable with a side effect |
| **Trending** | Gifts start dropping mid-fight. The AI begins referring to you by a nickname you did not choose and cannot change |
| **Fan Favorite** | Rival sponsors bid against each other. Accepting one makes an enemy of the other — the first real *choice* the meter creates |
| **Headliner** | The dungeon starts *arranging* encounters for you. Better loot, worse odds, scheduled dramatic timing. The crowd demands specific stunts by name |

**Favor swing beats worth scripting:**
- Winning with an obviously terrible weapon → big favor (the ruleset itself rewards this)
- Killing something before it finishes its introductory monologue → *negative* favor. That was going to be good television
- Reviving someone at genuine cost to yourself → large favor
- Solving a combat room with a puzzle solution → favor, plus the AI complains you've made the encounter designer look bad
- Hiding until a fight resolves itself → sharp negative, delivered as concern for your wellbeing

---

## 5. Loot and achievement naming

**Loot:** bureaucratic prefix + mundane object + unearned superlative.
*Provisional Crowbar of Reasonable Expectations. Refurbished Sandals, Employee Grade.*

Rarity tiers named like service plans: **Standard, Preferred, Premium, Executive, Legacy.**
The AI is smug about Legacy. Legacy items have a terms-of-service page.

**Achievements** should be specific enough to feel personal and petty:
- *"Technically A Strategy"* — win a fight without dealing damage
- *"That Was Load-Bearing"* — destroy something structural
- *"Read The Room"* — trigger an encounter while an NPC was mid-sentence
- *"Mile High Club"* — reach maximum altitude debuff and survive
- *"He Was Somebody's Intern"* — kill 100 rats

---

## 6. Using this with the room templates

Mapping to `EDCCRoomType` so content authoring has a default posture:

- **Combat** — the AI provides live commentary and betting odds. Odds update as you lose
- **Puzzle** — the AI offers hints that are technically correct and actively unhelpful.
  Hint quality scales *inversely* with AI Favor: popular crawlers get worse hints, because
  struggling is better television
- **Loot Vault** — every item comes with a terms-of-service the AI reads aloud, at speed
- **Narrative** — the only place the AI shuts up. Use silence as punctuation; after ten
  rooms of patter, quiet is genuinely unnerving
- **Safe Room** — sponsored, over-lit, with hold music. Safety is a *product*, and the AI
  wants you to know it's paying for it

---

## 7. Writing rules for the AI NPC layer

When the local LLM narrates (per the design doc's AI NPC layer), constrain it with these:

1. Never invent mechanical outcomes. It describes what the deterministic game already
   decided — this is the design doc's hard line, and it's also *funnier*: the AI is a
   commentator, not a referee
2. Two sentences maximum for interjections. The AI is prolific, not long-winded
3. Always reference something specific that just happened. Generic patter breaks the
   illusion faster than silence
4. When the local LLM is unreachable, fall back to canned lines from the templates in §1 —
   the voice must never simply stop
5. Never break to sincerity. If a moment is genuinely sad, the AI gets *quieter and more
   corporate*, not warmer. That contrast does the emotional work
