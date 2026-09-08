# -*- coding: utf-8 -*-
"""Single source of truth for the named-crawler roster.

Emits BOTH Docs/crawler-roster.md (the readable roster) and
Docs/DT_DCCCrawlers.csv (import-ready game data) from the same list, so the
prose and the table can never drift apart. Re-run after editing CRAWLERS:

    python Tools/gen_crawler_roster.py

CSV rather than JSON because DataTableTools.import_file routes through
CSVImportFactory and accepts nothing else (see CLAUDE.md).
"""
import csv, io, os
from collections import Counter

# tier: Rival | Encounter | Rumour | Deceased  - see Docs/crawler-roster.md
C = lambda **kw: kw

CRAWLERS = [

# ---------------------------------------------------------------- RIVALS ----
C(id="RenataVasquezPham", name="Renata Vasquez-Pham", handle="The Closer",
  pronouns="she/her", tier="Rival", status="Alive", floor="1-3", age=51,
  before="Luxury realtor, Cherry Creek",
  deal="She sells other crawlers things that are not hers, and the paperwork always holds up.",
  story="Twenty-two years moving property nobody needed at prices nobody questioned. The Collapse "
        "changed her inventory and nothing else. She has sold the same safe room to three separate "
        "parties and been paid by all three. The System adores her, because contracts are its native "
        "language and she speaks it without an accent - it has quietly begun honouring her deeds as "
        "though she had the authority to write them. Nobody has established yet whether that means "
        "she does, or whether the System is simply enjoying the bit.",
  hook="Vendor selling map access, safe-room rights and 'cleared' passage. Some of it is real. "
       "Working out which is a Charisma contest, not a hidden roll.",
  sponsor="Front Range Title & Escrow"),

C(id="OtisLange", name="Otis Lange", handle="Overtime",
  pronouns="he/him", tier="Rival", status="Alive", floor="2+", age=63,
  before="RTD driver, Route 15 Colfax",
  deal="He is still driving the route, and the route now goes down.",
  story="Thirty-one years up and down Colfax. He came down in uniform and has not taken it off. "
        "Somewhere on Floor 2 he found something with wheels and started running a schedule, and he "
        "keeps it: he picks up, he does not exceed the limit, he does not deviate, and he does not "
        "stop for anything trying to kill you. He is not brave. He is on the clock, and the clock is "
        "the last piece of the old world still working.",
  hook="A moving safe zone on a fixed timetable. Miss the stop and wait for the next one. Never "
       "fights, cannot be hurried, will not be argued with.",
  sponsor="(declines - still, somehow, union)"),

C(id="BrickHalloran", name="Brick Halloran", handle="Mile High's Own",
  pronouns="he/him", tier="Rival", status="Alive", floor="1-4", age=29,
  before="Broncos practice squad, never activated",
  deal="The most sponsored crawler on the floor is a genuinely kind man being fattened for a ratings beat.",
  story="Two seasons carrying a blocking sled for a team that never played him, beloved by exactly "
        "one bar in Englewood. The System found him in week one and gave him everything - gear drops, "
        "soft spawns, a highlight package with music over it. He shares the gear. He calls out traps "
        "for strangers. He has no idea he is a story arc, and arcs end. Killing him yourself would "
        "pay an obscene amount of Favor and would be the single most hated thing this party could do.",
  hook="Joinable ally who buffs the party and is scheduled to die. The schedule may be breakable. "
       "The moral trap is the content.",
  sponsor="LIQUIDATOR Energy (three-floor exclusivity)"),

C(id="YukiOrdonezFrame", name="Yuki Ordonez-Frame", handle="Appetite",
  pronouns="she/her", tier="Rival", status="Alive", floor="3+", age=34,
  before="Competitive eater, taqueria on Federal",
  deal="She eats the dungeon, and the dungeon has started eating back.",
  story="Forty-eight tacos, regional record, laminated certificate behind the register. On Floor 1 "
        "she worked out that consuming monster parts lends you their traits for a while. She is four "
        "floors down now, formidable, and no longer entirely the shape she arrived in. She is warm, "
        "chatty and completely without malice. She would like to know what your party tastes like. "
        "Purely professionally.",
  hook="Rival who is stronger every time you meet her, always wearing an ability the party has "
       "already seen on something they killed.",
  sponsor="Rocky Mountain Rendering Co."),

C(id="TheSommelier", name="(withholds his name)", handle="The Sommelier",
  pronouns="he/him", tier="Rival", status="Alive", floor="2-5", age=47,
  before="Floor sommelier, Larimer Square",
  deal="He finds a monster's weakness by tasting its blood, and he will not shut up about the finish.",
  story="Twenty years on the floor of a restaurant that has not existed for some time. He refuses to "
        "give his name to anyone, on the grounds that a name is a liability inside a broadcast "
        "product - which is the most genre-aware sentence anyone in Denver has said out loud. He is "
        "right. The System cannot reference him in an announcement, cannot sponsor him and cannot "
        "build him into a story. His Favor is catastrophic and he considers this the point.",
  hook="Sells real boss resistances. His namelessness is a live mechanic: announcements skip him, "
       "which is a genuine tactical advantage the party can borrow.",
  sponsor="(unsponsorable - no name on file)"),

C(id="DoloresOkonkwoReyes", name="Dolores 'Dee' Okonkwo-Reyes", handle="The Appraiser",
  pronouns="she/her", tier="Rival", status="Alive", floor="1+", age=58,
  before="Estate liquidator",
  deal="She follows parties at a polite distance and prices what they leave behind.",
  story="Thirty years of walking into houses after somebody died and putting a number on what was "
        "left. The job did not change. She never starts anything, is unfailingly courteous, and is "
        "always about eleven metres behind you. If you clear a room she prices the room. If you die "
        "in it she prices you, respectfully, and the audience finds this unbearably funny.",
  hook="Non-hostile loot competitor. Can be raced, negotiated with or robbed - and robbing her costs "
       "Favor, because she is far more popular than the party.",
  sponsor="Estate Solutions of the Front Range"),

C(id="AugustinePell", name="Augustine Pell", handle="Compliance",
  pronouns="they/them", tier="Rival", status="Alive", floor="2-6", age=41,
  before="OSHA inspector, Region VIII",
  deal="They are citing the dungeon for safety violations, and the citations are landing.",
  story="Career federal inspector, with a clipboard that survived the Collapse for reasons nobody can "
        "explain. They have filed 611 violations against Dungeon Crawler World: Earth - inadequate "
        "egress, unguarded machinery, no fall protection on the Falls. The System is obliged to log "
        "every one. It has started, very quietly, fixing some of them, and a fixed hazard is a hazard "
        "your party no longer has to survive.",
  hook="Reporting a hazard to them permanently disables that trap for everyone. The System resents "
       "this and adjusts the difficulty somewhere the party is not looking.",
  sponsor="(offer pending - they have not signed the release)"),

C(id="MarisolTrejoAdeyemi", name="Marisol Trejo-Adeyemi", handle="Second Opinion",
  pronouns="she/her", tier="Rival", status="Alive", floor="1-3", age=38,
  before="Trauma nurse, Denver Health",
  deal="She will save anyone, including the thing that just tried to kill you.",
  story="Eleven years in a trauma bay where you do not get to pick. She has not adjusted that policy "
        "and does not intend to. She has stabilised crawlers, stabilised a Debt Eater, and once spent "
        "an hour working on a Survey Drone before establishing it had never been alive. Parties love "
        "her right up until the first time she saves someone they wanted dead.",
  hook="Healer who will absolutely revive an enemy in front of you. Recruitable, at the price of "
       "never getting to choose who lives.",
  sponsor="Cherry Creek Urgent Care (regional)"),

# ------------------------------------------------------------ ENCOUNTER ----
C(id="SisterAgnethaRoule", name="Sister Agnetha Roule", handle="The Complication",
  pronouns="she/her", tier="Encounter", status="Alive", floor="1-2", age=71,
  before="Hospice chaplain",
  deal="She keeps performing last rites, and it is corrupting the death metrics.",
  story="Forty years of sitting with people at the end, and she does the identical job here. The "
        "problem is upstream: a death with a witness who calls it sacred does not convert cleanly "
        "into whatever the show is actually harvesting, and somebody several layers above the System "
        "AI has begun to notice a shortfall in the Denver numbers.",
  hook="Meeting her enters the party in a ledger they cannot see. She will bless a body for free, "
       "and the party's next death behaves differently.",
  sponsor="(none)"),

C(id="CormacBui", name="Cormac Bui", handle="Merge",
  pronouns="he/him", tier="Encounter", status="Alive", floor="Traffic floors", age=44,
  before="Tow operator, I-25 corridor",
  deal="He towed wrecks off the freeway for twenty years and now tows things that are still moving.",
  story="Found a rig on the gridlock floor and put it straight back to work. He will haul your dead "
        "vehicle, your unconscious friend, or a monster you would rather fight somewhere with better "
        "footing. He invoices for all three. The invoices are real, itemised, and the System enforces "
        "them with enthusiasm.",
  hook="Vehicle recovery, plus the tactical option of dragging a boss into terrain that suits you. "
       "Unpaid invoices become a debt encounter later.",
  sponsor="Front Range Recovery & Salvage"),

C(id="NadiaFerreiraStokes", name="Nadia Ferreira-Stokes", handle="Hail Damage",
  pronouns="she/her", tier="Encounter", status="Alive", floor="2-4", age=36,
  before="Body shop owner, Aurora",
  deal="She repairs armour with a dent puller and absolutely no ceremony.",
  story="Built a business on Colorado's only real natural predator, the summer hailstorm, and she has "
        "never once been impressed by damage. She will fix your breastplate in four minutes, tell you "
        "what you did wrong to earn the dent, and quote you for the paint separately. She considers "
        "the monsters a supply-side problem.",
  hook="Field repair. Restores durability cheaply and identifies what kind of attack made each mark - "
       "free intel on what is hunting the party.",
  sponsor="Casa de Hail Auto Body"),

C(id="TerranceVoBell", name="Terrance Vo-Bell", handle="Encore",
  pronouns="he/him", tier="Encounter", status="Alive", floor="Red Rock", age=26,
  before="Sound engineer, Red Rocks",
  deal="He knows exactly where every sound in the amphitheatre goes, and so should you.",
  story="Six summers mixing front-of-house on the best natural stage on the continent. He came down "
        "with in-ear monitors still around his neck and has been mapping the acoustics of every arena "
        "since. He can tell you where a boss cannot hear you, which is a thing bosses do not know "
        "about themselves. He is having, by some distance, the best time of anyone in this document.",
  hook="Marks acoustic dead zones in arena fights - real stealth geometry on a floor that otherwise "
       "has none.",
  sponsor="Mile High Audio"),

C(id="HollyDrescherKaur", name="Holly Drescher-Kaur", handle="The Understudy",
  pronouns="she/her", tier="Encounter", status="Alive", floor="1", age=23,
  before="Cliff diver, Cliff Diver Cantina",
  deal="She survived the dive nobody survives, and management considers this a scheduling problem.",
  story="Nineteen when she started diving into a chlorinated pool for tourists eating bad enchiladas, "
        "twenty-three when the venue was repossessed with the staff still inside. She is the only "
        "diver who came back up. The Cantina's new management has her listed for tonight's show, and "
        "every night's show, in perpetuity, and the listing is the thing keeping her there.",
  hook="Floor 1 rescue. Freeing her means voiding a staff contract - a puzzle, not a fight. She joins "
       "as a mobility specialist who reaches places the party cannot.",
  sponsor="Cliff Diver Cantina (staff, non-negotiable)"),

C(id="EliasWinterhalder", name="Elias Winterhalder", handle="Local Time",
  pronouns="he/him", tier="Encounter", status="Alive", floor="Any", age=68,
  before="Broadcast meteorologist, 40 years",
  deal="He is still doing the forecast, and down here the forecast is accurate.",
  story="Four decades of pointing at a green wall and being wrong about Saturday with tremendous "
        "warmth. Underground, with no weather left to get wrong, he has quietly become the most "
        "reliable information source in Denver: he tracks hazard cycles, spawn timings and pressure "
        "changes, and delivers them in the exact cadence of a man telling you to take a jacket.",
  hook="Gives real timing windows for environmental hazards. The forecast is genuinely correct, which "
       "the party will not believe until it saves them.",
  sponsor="Channel 4 (defunct; he still says it)"),

C(id="PriyaBenavidezOkafor", name="Priya Benavidez-Okafor", handle="Chain of Custody",
  pronouns="she/her", tier="Encounter", status="Alive", floor="2-5", age=33,
  before="Evidence technician, DPD",
  deal="She is documenting everything the dungeon does, for a trial that cannot happen.",
  story="Bagged and tagged for nine years and cannot stop. She photographs kill sites, logs serial "
        "numbers on System-issued gear, and maintains a chain of custody on evidence for a "
        "prosecution with no court, no jurisdiction and no defendant that can be served. The System "
        "finds this hilarious. It has started leaving things out for her.",
  hook="Trades from her archive. Her logs are the party's only route to knowing what the show did to "
       "a previous party in the room they are standing in.",
  sponsor="(refuses, on evidentiary grounds)"),

C(id="RubenAstorgaMbeki", name="Ruben Astorga-Mbeki", handle="The Landlord",
  pronouns="he/him", tier="Encounter", status="Alive", floor="1-3", age=55,
  before="Slumlord, four buildings on Federal",
  deal="He was foreclosing on people before the Syndicate made it a format, and he wants credit for it.",
  story="Ran four buildings badly and profitably for twenty years, and arrived here genuinely excited "
        "- he recognised the business model immediately and assumed there would be a job in it. There "
        "was not. The System does not need middle management. He has spent three floors trying to get "
        "a meeting and has begun, slowly, to work out which side of the transaction he is on.",
  hook="Comic antagonist who becomes a tragedy on a delay. Sells information he does not realise is "
       "worthless, and later, information he does not realise is priceless.",
  sponsor="(applications: 14 - responses: 0)"),

C(id="JuneAbernathyCruz", name="June Abernathy-Cruz", handle="Show of Hands",
  pronouns="they/them", tier="Encounter", status="Alive", floor="2-4", age=30,
  before="ASL interpreter, city contracts",
  deal="They can talk to the things that were never given a voice module.",
  story="Interpreted for city council, hospitals and one very long murder trial. Down here they "
        "worked out that several monster types signal intent with their bodies in a way that is "
        "consistent, learnable, and completely ignored by everyone busy screaming. They have now held "
        "four conversations with things the party has only ever fought.",
  hook="Unlocks a non-combat resolution with at least one enemy family. The System does not offer "
       "this option and is visibly annoyed that it exists.",
  sponsor="(under review - 'accessibility' is a difficult sell to the audience)"),

# --------------------------------------------------------------- RUMOUR ----
C(id="TheQuietFloor", name="'The Quiet Floor' (unidentified)", handle="unassigned",
  pronouns="unknown", tier="Rumour", status="Unknown", floor="unknown", age=0,
  before="unknown",
  deal="Somebody is clearing rooms ahead of the party and leaving nothing behind to show for it.",
  story="Four separate parties have walked into an encounter already finished. No bodies, no loot, no "
        "damage to the room, and no announcement - and the last one is what frightens people, because "
        "the System announces everything. The prevailing theory in the bar is that they are very "
        "good. The less popular theory is that they are not a crawler.",
  hook="Long-running mystery seed. Can stay unresolved for floors. Pays off whenever it is useful and "
       "costs nothing until then.",
  sponsor="(n/a)"),

C(id="AugustKesslerBoone", name="August Kessler-Boone", handle="Six Figures",
  pronouns="he/him", tier="Rumour", status="Alive", floor="4+", age=48,
  before="Wealth manager",
  deal="He is buying other crawlers' Favor, and somebody is selling.",
  story="Nobody in Denver has met him. Everybody in Denver has heard the terms: real gold, up front, "
        "for a permanent share of your audience attention, System-witnessed and irreversible. Several "
        "parties are known to have signed. Nobody knows what he is doing with the accumulated Favor, "
        "and nobody has thought carefully about what a man with no audience of his own would want it "
        "for.",
  hook="Offstage economic threat. His buyers can appear as NPCs long before he does. A late "
       "escalation, not an early one.",
  sponsor="(owns three)"),

C(id="AdaKettleworth", name="Ada 'Mama' Kettleworth", handle="The Kitchen",
  pronouns="she/her", tier="Rumour", status="Alive", floor="3", age=66,
  before="Church basement cook, Five Points",
  deal="There is a room three floors down where nobody is allowed to fight, and she enforces it.",
  story="Fed a congregation on Sunday and anyone who turned up the rest of the week, for thirty-one "
        "years. She has apparently done the same thing here, in a room the System has not managed to "
        "monetise. The rule is the rule: you eat, you leave your weapons at the door, and you do not "
        "bring it inside. Two parties who tested this are said to have left considerably worse off "
        "than the people they came for.",
  hook="Neutral ground the party badly needs and cannot exploit. Whatever enforces her rule should "
       "never be shown directly.",
  sponsor="(none, and the System has stopped asking)"),

C(id="DamienOrtizHalliwell", name="Damien Ortiz-Halliwell", handle="First Descent",
  pronouns="he/him", tier="Rumour", status="Unknown", floor="unknown", age=39,
  before="Alpinist and cave diver",
  deal="He went down on day one, alone, and has never been announced dead.",
  story="Summited things without ropes and dived systems nobody had mapped, and walked into the "
        "Denver entry point eleven hours after the Collapse with a headlamp and no plan at all. The "
        "System announces every death. It has not announced his. Depending on who is telling it, that "
        "means he is the deepest living human being on Earth, or it means he found the thing at the "
        "bottom and it has better uses for him.",
  hook="The party's eventual proof that the floors do not end. Best kept as a name on a wall, "
       "scratched deeper than anyone should have been able to reach.",
  sponsor="(none - predates the sponsorship layer)"),

C(id="TheGardener", name="'The Gardener' (unidentified)", handle="unassigned",
  pronouns="unknown", tier="Rumour", status="Alive", floor="2-5", age=0,
  before="unknown",
  deal="Somebody is planting things in the dungeon, and some of them are coming up.",
  story="Pockets of growing matter keep appearing in rooms with no light and no soil - arranged, "
        "deliberate, and in at least two cases edible. The System has classified them as an "
        "unlicensed environmental modification and is charging a fee it has nobody to bill. Whoever "
        "is doing it moves ahead of the parties and has never been seen.",
  hook="Ambient hope. Small free heals in unexpected places. Reveal only if it is worth more revealed "
       "than not, which it probably is not.",
  sponsor="(n/a)"),

C(id="ColetteNwachukwuBarr", name="Colette Nwachukwu-Barr", handle="The Sponsor Killer",
  pronouns="she/her", tier="Rumour", status="Alive", floor="3-6", age=42,
  before="Class-action attorney",
  deal="She only kills crawlers who have signed sponsorship deals, and she has never once been wrong about who has.",
  story="Spent a career suing companies for what they put inside people. Down here she has decided "
        "that the sponsorship contract is the actual crime and the signatories are the actual "
        "accomplices, which is either a coherent moral position or six floors of rationalisation. Her "
        "Favor is enormous. The audience adores a heel with a thesis.",
  hook="A direct threat the moment the party takes their first sponsor. Turns accepting a deal into a "
       "real decision instead of free money.",
  sponsor="(kills for it; will not take one)"),

C(id="TheFourFifteen", name="'The 4:15' (unidentified)", handle="unassigned",
  pronouns="unknown", tier="Rumour", status="Unknown", floor="Any", age=0,
  before="unknown",
  deal="Something arrives in the same room every day at 4:15 AM local time, and local time is a thing the System keeps mentioning.",
  story="The floor-open announcement always gives the local time, and it is always wrong except once "
        "a day. Crawlers who have been down long enough have noticed that at 4:15 AM, whatever room "
        "they happen to be in, something is briefly present and then is not. Nobody has been hurt by "
        "it. That is somehow worse.",
  hook="A recurring timed beat that costs nothing to run. Never explain it. The moment it becomes a "
       "mechanic it stops being the best thing in the bar.",
  sponsor="(n/a)"),

C(id="WendellAmagiSoto", name="Wendell Amagi-Soto", handle="The Cartographer",
  pronouns="he/him", tier="Rumour", status="Deceased", floor="2", age=59,
  before="USGS surveyor",
  deal="He mapped two entire floors correctly and the map is still down there somewhere.",
  story="Thirty years surveying the Front Range, and he applied every bit of it: proper "
        "triangulation, elevation profiles, a legend. He finished Floor 2 and was most of the way "
        "through Floor 3 when a Debt Eater found him. The map is real, it is accurate, and it is in a "
        "bag in a room nobody has identified. Four parties are actively looking.",
  hook="A treasure hunt whose payoff is information, not gear. Whoever finds it gets the floor "
       "layout - worth more than any weapon, and far easier to author.",
  sponsor="(none)"),

# ------------------------------------------------------------- DECEASED ----
C(id="GraceOyelaranPike", name="Grace Oyelaran-Pike", handle="Ninety Seconds",
  pronouns="she/her", tier="Deceased", status="Deceased", floor="1", age=27,
  before="Line cook",
  deal="Held a doorway for ninety seconds so eleven strangers could get through it.",
  story="Nobody in that stairwell knew her name; they had it off the death announcement, which the "
        "System delivered in the same tone it uses for patch notes. Eleven people are alive on Floor "
        "2 because of ninety seconds of work by someone who was not strong, was not armed, and was "
        "nobody's idea of a hero. Her name is now scratched into the wall of that stairwell fourteen "
        "separate times, in fourteen different hands.",
  hook="The graffiti is the content. Put her name on that wall and let players find it without a "
       "quest marker.",
  sponsor="(none)"),

C(id="TobiasReinholtCruz", name="Tobias Reinholt-Cruz", handle="Statistically Speaking",
  pronouns="he/him", tier="Deceased", status="Deceased", floor="1", age=45,
  before="Actuary",
  deal="Calculated the survival odds correctly, announced them, and was killed by the resulting stampede.",
  story="He was right. That is the whole joke, and the System has never let it go - his numbers were "
        "accurate to within two percent, and the panic they caused killed forty people including him. "
        "The achievement named after him is awarded to any crawler who shares genuinely bad news with "
        "a group.",
  hook="An achievement with a person behind it. Ties the existing achievement system to a name "
       "instead of a pun.",
  sponsor="(posthumous, briefly)"),

C(id="IngridSolbergAdeyemi", name="Ingrid Solberg-Adeyemi", handle="The Record",
  pronouns="she/her", tier="Deceased", status="Deceased", floor="2", age=31,
  before="Ultramarathon runner",
  deal="Held the fastest floor clear in Denver for six days and died eleven metres from the exit.",
  story="Ran a hundred miles at a time for fun, and treated the dungeon as terrain, which worked "
        "extremely well right up until it did not. Her time still tops the Denver leaderboard. The "
        "System has left it there, because an unbeaten record set by a dead woman is better "
        "engagement than a living champion, and it says so openly.",
  hook="Her time is the number on the leaderboard the party is chasing. Beating it should feel bad.",
  sponsor="Continental Divide Nutrition"),

C(id="MarcusKettlebyOsei", name="Marcus Kettleby-Osei", handle="Two Floors",
  pronouns="he/him", tier="Deceased", status="Deceased", floor="2", age=52,
  before="Middle school principal",
  deal="Got sixty-one children down two floors and did not get down the third.",
  story="Every account agrees on the count and none of them agree on how. He was not a fighter and "
        "had no skills worth listing. What he had was thirty years of moving large numbers of "
        "frightened young people through a building calmly, and it turns out that is a skill the "
        "dungeon had no answer for, until it simply outlasted him.",
  hook="The reason there are children in the Floor 3 safe rooms. Do not show the death; show the "
       "sixty-one.",
  sponsor="(none)"),

C(id="LenaVasquezHolt", name="Lena Vasquez-Holt", handle="Unsponsored",
  pronouns="she/her", tier="Deceased", status="Deceased", floor="1", age=19,
  before="Student",
  deal="Turned down four sponsorship offers, and the fifth thing that found her was not an offer.",
  story="Nineteen, first year, and stubborn in the specific way that only works if you live through "
        "it. She refused every deal on the grounds that she had not agreed to be in a show and was "
        "not going to start now, which the audience found charming and then irritating. Her Favor "
        "bottomed out. Favor is what the System spends on soft spawns.",
  hook="The cautionary tale the bar tells about refusing sponsorship - deliberately in tension with "
       "Colette Nwachukwu-Barr, who kills people for accepting.",
  sponsor="(refused: 4)"),

C(id="HaroldMbekiStrand", name="Harold Mbeki-Strand", handle="The Regular",
  pronouns="he/him", tier="Deceased", status="Deceased", floor="1", age=74,
  before="Retired; drank in the same bar since 1979",
  deal="Died in his usual seat, in a bar that had been dragged underground with him still in it.",
  story="The Collapse took every roofed structure on Earth in the same instant, and a great many "
        "people were indoors. Harold was on his stool. Whatever the dungeon did to that building, it "
        "kept the bar and it kept him, and he stayed there eleven days being served by staff who were "
        "also not okay, and then he stopped. Nobody moved him for a while. It did not seem right.",
  hook="Sets the Bar's tone in one beat: the staff are not fine, and the regulars were here first.",
  sponsor="(none)"),

C(id="SamiraDoyleFontaine", name="Samira Doyle-Fontaine", handle="Patch 1.4.2",
  pronouns="she/her", tier="Deceased", status="Deceased", floor="3", age=35,
  before="QA lead, games industry",
  deal="Found an exploit that trivialised a boss, and was patched out along with it.",
  story="Twelve years of breaking software professionally, and she broke the Voldemortgage encounter "
        "inside forty minutes - a genuine, reproducible sequence break. She wrote it up. She shared "
        "it. Four parties used it. Then came a balance patch, delivered with the System's usual "
        "warmth, and the patch notes listed her by name under 'Fixed'.",
  hook="The party can still find her writeup. Any exploit they discover now carries the implication "
       "that using it puts them in the next patch notes.",
  sponsor="(revoked at time of patch)"),

C(id="OmarBellingerKhan", name="Omar Bellinger-Khan", handle="Volunteer",
  pronouns="he/him", tier="Deceased", status="Deceased", floor="1", age=22,
  before="Warehouse picker, DIA freight",
  deal="Signed up on day two because the alternative was the surface, and lasted nine hours.",
  story="The four million are volunteers, which is the distinction the System leans on whenever "
        "anyone objects to any of this. Omar volunteered. He had seen what was left up there and did "
        "the arithmetic in about a minute, and the arithmetic was correct - the show is the better "
        "option, which is the actual horror and not the monsters. Nine hours. The announcement gave "
        "his name, his sponsor status, and the local time.",
  hook="Use his name in the intake announcement. He is what 'volunteer' means, said plainly, exactly "
       "once, with no joke attached.",
  sponsor="(none - never reached the offer stage)"),
]

FIELDS = ["Name", "CrawlerId", "DisplayName", "BroadcastHandle", "Pronouns", "UseTier",
          "Status", "FloorRange", "Age", "FormerLife", "Deal", "Story", "MechanicalHook",
          "Sponsor"]

TIER_ORDER = ["Rival", "Encounter", "Rumour", "Deceased"]

TIER_BLURB = {
 "Rival": ("Rivals — promote to a full `ADCCCharacterBase`",
  "The only ones that ever become real actors with stats, and even then not all at once. "
  "`Docs/population-design.md` budgets around twelve full characters for the entire game, so "
  "**do not build all eight.** Pick the two or three the party keeps colliding with and leave the "
  "rest as names until they earn it."),
 "Encounter": ("Encounter crawlers — one scripted meeting each",
  "Real enough to talk to, thin enough to be an `ADCCDialogueNPC` with a mesh and a few lines. None "
  "of them need a stats component. Each exists to hand the party exactly one thing — a repair, a "
  "timing window, a route, a moral problem — and then stop."),
 "Rumour": ("Rumour crawlers — named, never rendered",
  "**These cost nothing to build and carry most of the world.** They are names in bar dialogue, in "
  "System announcements, and in other crawlers' warnings. This is how DCC itself works: the show "
  "reads names out constantly and you meet almost none of them. Any one can be promoted to Encounter "
  "or Rival later without rewriting a word — that is the point of holding them here."),
 "Deceased": ("The Deceased — names on the leaderboard, the wall and the announcements",
  "Already dead when the party arrives. They exist as graffiti, achievement names, leaderboard rows "
  "and things NPCs mention. They are also the answer to the attrition problem: "
  "`UDCCPopulationSubsystem` reports hundreds of thousands of deaths as a number, and a number is "
  "wallpaper. One name attached to it is not."),
}


def emit_markdown(path):
    o = io.StringIO()
    w = o.write
    w("# The Named Crawlers — Denver roster\n\n")
    # Counts are derived, never typed - a hardcoded "thirty-two" was wrong within
    # a minute of the first generation.
    w("%d authored crawlers. **Generated from `Tools/gen_crawler_roster.py` — edit that "
      "file, not this one.** This document and `Docs/DT_DCCCrawlers.csv` are emitted from the same "
      "list, so the prose and the table cannot drift apart.\n\n" % len(CRAWLERS))
    w("## How this fits the population tiers\n\n")
    w("`Docs/population-design.md` sets a budget of **around twelve** Tier 1 characters for the whole "
      "game, and warns that *“a procedural name with a random stat block is somebody nobody can "
      "care about.”* Both still hold. Nothing here contradicts them, because **being named is not "
      "the same as being built.**\n\n")
    w("The doc's real concern is cost: fifty half-simulated characters cost what fifty real ones cost "
      "and deliver what neither does. So this roster separates the two ideas that tier table had "
      "welded together:\n\n")
    w("- **Authored vs. generated.** Everyone here is authored, and everyone here has a *deal* — "
      "which is the doc's own test for whether a character lands.\n")
    w("- **Named vs. built.** Only the eight Rivals are ever candidates for a full "
      "`ADCCCharacterBase`, and even those get built two or three at a time. The rest "
      "are dialogue lines, announcements, graffiti and leaderboard rows.\n\n")
    w("That is also how the source material behaves. The System AI reads names out constantly, the "
      "audience knows hundreds of crawlers, and a given party meets a handful. A world with "
      "%d names in it and six bodies feels far larger than one with twelve of each.\n\n"
      % len(CRAWLERS))
    w("**Nothing here duplicates an existing NPC.** The current cast is corporate monsters "
      "(Voldemortgage, the Debt Eaters, the Auditor, the Notary) and service NPCs (the bartender, the "
      "cook, the divers, the Tailings elders, the new bookie and snack vendor). There were no named "
      "rival *crawlers* at all before this.\n\n")
    w("### Writing rules used\n\n")
    w("- **The old job is the whole build.** `Docs/dungeon-tone-bible.md` rule 5, specific beats "
      "generic — a tow operator, an OSHA inspector and a sommelier are three completely different "
      "characters with no new art required, which matters because art is the constraint here and code "
      "is not.\n")
    w("- **The System never mocks a real death beat** (tone bible rule 4). It mocks the format around "
      "one. Several entries below are built so the joke lands on the show, not the person.\n")
    w("- **The volunteer/survivor distinction** from `story-premise.md` is stated plainly exactly "
      "once, in Omar Bellinger-Khan's entry, with no joke attached.\n")
    w("- No reuse of the books' actual characters. All original, written in their spirit.\n\n")
    w("---\n\n")

    for tier in TIER_ORDER:
        title, blurb = TIER_BLURB[tier]
        group = [c for c in CRAWLERS if c["tier"] == tier]
        w("## %s (%d)\n\n%s\n\n" % (title, len(group), blurb))
        for c in group:
            w("### %s" % c["name"])
            if c["handle"] != "unassigned":
                w(" — *“%s”*" % c["handle"])
            w("\n\n")
            bits = [c["pronouns"]]
            if c["age"]:
                bits.append(str(c["age"]))
            bits += [c["before"], c["status"], "floors %s" % c["floor"]]
            w("<sub>%s</sub>\n\n" % " · ".join(b for b in bits if b and b != "unknown"))
            w("**The deal:** %s\n\n" % c["deal"])
            w("%s\n\n" % c["story"])
            w("**Hook:** %s  \n" % c["hook"])
            w("**Sponsor:** %s\n\n" % c["sponsor"])
        w("---\n\n")

    w("## Using this\n\n")
    w("1. **Free, no build.** Add the Rumour and Deceased names to `DT_DCCDialogue` as bar gossip, "
      "and to the System's announcement lines. That alone makes Denver feel populated, and it needs "
      "no C++ and no editor restart.\n")
    w("2. **Cheap.** The eight Encounter crawlers are `ADCCDialogueNPC` placements with a mesh, a "
      "handful of lines and one thing to give. Still no new C++.\n")
    w("3. **Only when earned.** Promote a Rival to a real character class. Dolores Okonkwo-Reyes is "
      "the natural first — `population-design.md` already nominated “The Appraiser” as "
      "the first promotion, because *“follows parties at a polite distance and takes what they "
      "leave behind”* is already a mechanic. This roster just gives them a person.\n\n")
    w("`Docs/DT_DCCCrawlers.csv` sits beside this file, ready to import once an `FDCCCrawlerDef` row "
      "struct exists. That struct is a reflection change and therefore a full build cycle, so it is "
      "deliberately **not** done yet — the roster is useful as prose and dialogue first, and the "
      "table should earn itself by being used.\n")

    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(o.getvalue())
    return o.getvalue().count("\n")


def emit_csv(path):
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=FIELDS, quoting=csv.QUOTE_ALL)
        wr.writeheader()
        for c in CRAWLERS:
            wr.writerow({
                "Name": c["id"], "CrawlerId": c["id"], "DisplayName": c["name"],
                "BroadcastHandle": "" if c["handle"] == "unassigned" else c["handle"],
                "Pronouns": c["pronouns"], "UseTier": c["tier"], "Status": c["status"],
                "FloorRange": c["floor"], "Age": c["age"], "FormerLife": c["before"],
                "Deal": c["deal"], "Story": c["story"], "MechanicalHook": c["hook"],
                "Sponsor": c["sponsor"]})


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    md = os.path.join(root, "Docs", "crawler-roster.md")
    cs = os.path.join(root, "Docs", "DT_DCCCrawlers.csv")

    ids = [c["id"] for c in CRAWLERS]
    assert len(set(ids)) == len(ids), "duplicate CrawlerId"
    for c in CRAWLERS:
        assert c["tier"] in TIER_ORDER, "bad tier on %s" % c["id"]

    lines = emit_markdown(md)
    emit_csv(cs)
    n = Counter(c["tier"] for c in CRAWLERS)
    print("%d crawlers -> %s (%d lines) + %s" % (len(CRAWLERS), md, lines, cs))
    print("  " + ", ".join("%s: %d" % (k, n[k]) for k in TIER_ORDER))
    print("  ids unique: ok")
