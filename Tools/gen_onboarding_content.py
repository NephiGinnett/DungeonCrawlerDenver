"""The first ten minutes, as data: System windows, the first-fight table, and the System AI's voice.

One file is the source of truth for the wording. Running it writes:

  Docs/DT_DCCSystemWindows.json      rows for FDCCSystemWindowDef   (import: create -> add_rows -> set_rows)
  Docs/DT_DCCRandomEncounters.json   rows for FDCCRandomEncounterRow
  SourceArt/Voice/SystemAI/<voice>/VO_<Row>.wav   one line per voiced row (Kokoro, offline, on this PC)

Usage:
  python Tools/gen_onboarding_content.py                  # tables only
  python Tools/gen_onboarding_content.py --voice          # tables + every voice line in the chosen voice
  python Tools/gen_onboarding_content.py --audition       # the same two lines in each candidate voice

Kokoro's model files are borrowed from The Familiar (TheFamiliar/voices). Nothing here touches the network.
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "Docs"
VOICE_OUT = ROOT / "SourceArt" / "Voice" / "SystemAI"
KOKORO_DIR = Path(r"C:\Users\kmett\Downloads\TheFamiliar\voices")

# The UE asset path every voiced row points at. Import the .wav files into this folder with these names.
VOICE_ASSET = "/Game/DCC/Audio/SystemAI/VO_{row}.VO_{row}"

# --- The System AI's voice ---------------------------------------------------------------------------
#
# The tone bible's first rule is "corporate cheer over menace", and The House (Friendly Stakes) adds the
# second half: it has already seen how this ends, and it is enjoying itself. So: a bright, polished
# British concierge, slightly too fast, slightly too pleased. Blends are Kokoro "name:weight+name:weight".
VOICES = {
    "concierge": ("bf_emma:0.55+af_nova:0.45", 1.06),   # default: hotel front desk that knows your blood type
    "gameshow":  ("am_puck:0.6+am_michael:0.4", 1.08),  # daytime TV host, all teeth
    "thehouse":  ("bm_george:0.6+bm_fable:0.4", 0.96),  # first take: Zoe found it slow and robotic
    # The House, second pass (2026-11-05): quicker, and single voices or light blends, because heavy
    # blends between two speakers are part of what flattens the delivery.
    "house_george": ("bm_george", 1.12),
    "house_smooth": ("bm_george:0.75+bm_lewis:0.25", 1.10),
    "house_fable":  ("bm_fable", 1.12),
    "house_daniel": ("bm_daniel", 1.10),
    "hostess":   ("bf_isabella:0.6+af_bella:0.4", 1.02),  # warmer; the friendliest possible threat
}
DEFAULT_VOICE = "thehouse"

INFO, REWARD, WARNING = "Info", "Reward", "Warning"

# EDCCHudFeature bits - keep in step with Onboarding/DCCOnboardingTypes.h
VITALS, HOTBAR, QUESTS, FEED, M_CHAR, M_INV, M_SKILLS, M_SOCIAL = (1 << i for i in range(8))

# --- System windows --------------------------------------------------------------------------------------
# (row, header, title, body, style, unlocks, voiced)
#
# The bedroom is silent on purpose (see DCCIntroDirector.h, "THE SILENCE IS THE POINT"): personal-item
# windows are unvoiced, and the System's first spoken words in the whole game are the announcement.
WINDOWS = [
    ("PersonalItem", "Item", "Something of Yours",
     "You picked it up without thinking. It is the most normal thing you will hold for a very long time.",
     REWARD, 0, False),
    ("PersonalItem_CJ", "Item", "The Board Game",
     "Two pieces missing, rules annotated in three colours of pen. Somebody always wanted to play one more round.",
     REWARD, 0, False),
    ("PersonalItem_Scott", "Item", "The Lacrosse Stick",
     "The mesh is shot and the tape is older than some of your friends. It still feels right in your hands.",
     REWARD, 0, False),
    ("PersonalItem_Marcel", "Item", "The Lucky Dice",
     "Rolled a natural twenty once, in front of witnesses. You have been chasing that feeling ever since.",
     REWARD, 0, False),

    ("WorldDungeonAnnouncement", "Global Broadcast", "The World Dungeon Is Open",
     "Good morning, Earth! Every structure on your planet has been reclaimed under the terms of your "
     "civilisation's outstanding balance. | Surviving residents of Denver may now report to the World Dungeon "
     "for processing. Look up. We've made a very large arrow. | Participation is voluntary. The surface is not.",
     WARNING, 0, True),

    ("WelcomeCrawler", "System", "You Have Been Registered",
     "Congratulations, Crawler! You are now a contestant on the most watched program in the known universe. "
     "| Your statistics have been measured. Most of them are disappointing. That's fine. Disappointing is "
     "relatable, and relatable sells.",
     INFO, 0, True),
    ("TheRules", "System", "The Rules, Abridged",
     "Go down. Do not stop going down. | Kill things for experience. Open boxes for loot. Entertain the "
     "audience and the audience will occasionally save your life. Bore them and we will find something "
     "less boring to put in the room with you. | Your first trial is solo. Good luck! It is statistically "
     "irrelevant.",
     INFO, 0, True),

    ("FirstFightWon", "Trial Complete", "You Survived",
     "First trial cleared. The audience is cautiously interested. | A safe room has opened, with hot food, "
     "a bed, and your interface. Or you can keep going on your own, which the ratings would adore.",
     REWARD, 0, True),
    ("ContinueAlone", "Warning", "Bold Choice",
     "You skipped the safe room. Chat is screaming. Several sponsors have just leaned forward. | Your "
     "interface has been issued anyway. We are not monsters. The monsters are downstairs.",
     WARNING, 0, True),

    ("SafeRoomWelcome", "Safe Room", "Welcome to Your Safe Room",
     "Nothing can hurt you in here, by contract. Breathe. Eat something. Find your friends. | Your vital "
     "signs are now on screen. Health is how much of you is left. Mana is how much magic you can afford. "
     "Stamina is how much running away you have in you.",
     INFO, VITALS | FEED, True),
    ("InterfaceIssued", "System", "Interface Issued",
     "Your full crawler interface is now available. Press Tab to open it. | Each page will explain itself "
     "the first time you visit. Please read them. We wrote them ourselves, and we are very sensitive.",
     REWARD, HOTBAR | QUESTS | M_CHAR | M_INV | M_SKILLS | M_SOCIAL, True),

    ("Tab_Character", "Interface", "Your Character Sheet",
     "Five stats: Strength, Intelligence, Constitution, Dexterity and Charisma. The number is your score; "
     "the bonus beside it is what gets added to your dice. | Rolls are a d20 plus that bonus plus your skill "
     "rank, against a difficulty. Evade is how hard you are to hit. AI Favor is how much the audience likes you.",
     INFO, 0, True),
    ("Tab_Skills", "Interface", "Skills",
     "Skills improve by using them, win or lose. Every few hours of play we roll to see if practice paid "
     "off. | You can try a skill you don't have, at disadvantage. That is how you learn new ones. Spells "
     "are the exception. Please do not try spells you don't have. It is very messy.",
     INFO, 0, True),
    ("Tab_Inventory", "Interface", "Inventory and Equipment",
     "Everything you carry is here. Gear goes in five slots: main hand, off hand, head, body and trinket. "
     "| Equipment changes your enhanced stats, never your base ones. Take it off and you go back to being "
     "you. Cursed items are the exception, which is what makes them fun.",
     INFO, 0, True),
    ("Tab_Party", "Interface", "Your Party",
     "These are the people you are surviving with. Their health is your business now. | A crawler who goes "
     "down is not dead yet. Get to them. The audience loves a rescue almost as much as it loves a funeral.",
     INFO, 0, True),
    ("Tab_Achievements", "Interface", "Achievements",
     "We are watching everything you do, and some of it deserves a trophy. Achievements come with loot "
     "boxes. | Loot boxes must be opened in a safe room. Yes, the timing is deliberate.",
     REWARD, 0, True),
    ("Tab_Chatter", "Interface", "The Audience",
     "This is what the viewers are saying about you, live. They are not always kind. | They are, however, "
     "always right. Keep them entertained and AI Favor follows: a reroll, an extra action, a nudge when "
     "it matters most.",
     INFO, 0, True),
]

# --- The first-fight table ---------------------------------------------------------------------------------
# Every row uses an enemy that already has a model and animations in the project, renamed for the joke.
# (row, name, intro, weight, [(class, count, level, display_name)])
E = "/Script/DungeonCrawlerDenver."
ENCOUNTERS = [
    ("MillerMoths", "Miller Moth Season",
     "It's June in Denver, which means the moths are back, and this year they have been to the gym.",
     1.0, [(E + "DCCEnemyCharacter_SurveyDrone", 3, 1, "Miller Moth (Seasonal)")]),
    ("KombuchaMother", "The Kombucha Mother",
     "Someone left a jar in a RiNo taproom for eleven years. It has opinions about your gut health.",
     1.0, [(E + "DCCEnemyCharacter_FinePrint", 1, 2, "Kombucha Mother")]),
    ("StorageAuction", "Storage Unit Auction",
     "Lot four four seven one. Contents unknown. Contents, it turns out, hungry.",
     0.8, [(E + "DCCEnemyCharacter_UnclaimedProperty", 1, 2, "Storage Unit Lot #4471")]),
    ("Xeriscape", "Xeriscape Enforcement",
     "Your lawn was in violation of the watering schedule. Enforcement has arrived, and it is mostly spines.",
     1.0, [(E + "DCCEnemyCharacter_Thornback", 2, 1, "Xeriscape Enforcement Officer")]),
    ("Dispensary", "Dispensary Overflow",
     "These mushrooms are not licensed for sale in the state of Colorado. They are, however, licensed to bite.",
     1.0, [(E + "DCCEnemyCharacter_BreakRoomMold", 3, 1, "Recreational Mushroom (Unlicensed)")]),
    ("HOAPresident", "The HOA President",
     "You have been cited for an unapproved mailbox colour. The President will be handling this personally.",
     0.8, [(E + "DCCEnemyCharacter_ShellCompany", 1, 2, "HOA President (Snapping)")]),
    ("Doorbell", "Motion Detected",
     "Your smart doorbell has been recording you for six years. It would like to discuss what it saw.",
     0.8, [(E + "DCCEnemyCharacter_Beholder", 1, 1, "Smart Doorbell, Always Recording")]),
    ("MallCops", "Off-Duty Mall Cops",
     "Cherry Creek mall security, off the clock and out of patience. They have been waiting for this their whole lives.",
     1.0, [(E + "DCCEnemyCharacter_Guard", 2, 1, "Mall Cop, Off the Clock")]),
    ("Collections", "Collections Has Called Several Times",
     "You have an outstanding balance. They are here to discuss your options. You don't have any.",
     0.9, [(E + "DCCEnemyCharacter_DebtEater", 2, 1, "Collections Agent")]),
    ("Orientation", "New Hire Orientation",
     "Welcome to the team! Your manager would like a quick sync. The interns would like your lunch.",
     0.9, [(E + "DCCEnemyCharacter_InternManager", 1, 1, "Shift Lead"),
           (E + "DCCEnemyCharacter_Intern", 2, 1, "Unpaid Intern")]),
    ("TheNeighbors", "The Neighbors",
     "They brought a casserole. They brought it at you.",
     0.4, [(E + "DCCEnemy_NuclearDad", 1, 1, "Dad, Still Grilling"),
           (E + "DCCEnemy_NuclearMom", 1, 1, "Mom, Insisting You Eat"),
           (E + "DCCEnemy_NuclearKid", 1, 1, "The Kid, Showing You Something")]),
]


def spoken(text: str) -> str:
    """What the voice says. '|' paragraph marks are kept (Speaker.say voices a paragraph at a time); '#' becomes 'number'."""
    return text.replace("#", "number ")


def window_rows(voiced_rows):
    rows = []
    for row, header, title, body, style, unlocks, voiced in WINDOWS:
        rows.append({
            "Name": row,
            "header": header, "title": title, "body": body, "style": style,
            "voice": VOICE_ASSET.format(row=row) if voiced and row in voiced_rows else "",
            "unlockFeatures": unlocks,
        })
    return rows


def encounter_rows(voiced_rows):
    rows = []
    for row, name, intro, weight, spawns in ENCOUNTERS:
        vo_row = f"Encounter_{row}"
        rows.append({
            "Name": row,
            "encounterName": name, "introLine": intro, "weight": weight,
            "introVoice": VOICE_ASSET.format(row=vo_row) if vo_row in voiced_rows else "",
            "spawns": [{"enemyClass": c, "count": n, "level": lvl, "displayNameOverride": dn}
                       for c, n, lvl, dn in spawns],
        })
    return rows


def voice_jobs():
    """(row, text) for everything the System says out loud."""
    jobs = [(row, f"{title}. | {spoken(body)}") for row, _h, title, body, _s, _u, voiced in WINDOWS if voiced]
    jobs += [(f"Encounter_{row}", f"{name}! {intro}") for row, name, intro, _w, _s in ENCOUNTERS]
    return jobs


class Speaker:
    def __init__(self):
        import numpy as np  # noqa: F401  (fail early with a clear message)
        from kokoro_onnx import Kokoro
        self.kokoro = Kokoro(str(KOKORO_DIR / "kokoro-v1.0.onnx"), str(KOKORO_DIR / "voices-v1.0.bin"))
        self.known = set(self.kokoro.get_voices())

    def style(self, spec):
        mix, total = None, 0.0
        for part in spec.split("+"):
            name, _, w = part.partition(":")
            if name not in self.known:
                print(f"  (voice {name} not in this Kokoro build, skipped)")
                continue
            w = float(w or 1)
            v = self.kokoro.get_voice_style(name) * w
            mix = v if mix is None else mix + v
            total += w
        return mix / total

    def say(self, text, spec, speed, out: Path):
        import numpy as np
        import soundfile as sf
        lang = "en-gb" if spec.split("+")[0][:2] in ("bf", "bm") else "en-us"
        style = self.style(spec)
        # A paragraph at a time ("|" marks them). The first version spoke one sentence at a time with a
        # fixed gap, which reset the intonation every sentence and was most of why it sounded robotic.
        # Paragraphs keep the natural flow; only a very long one is split, since the model truncates.
        chunks = []
        for para in (p.strip() for p in text.split("|")):
            if not para:
                continue
            if len(para) <= 380:
                chunks.append(para)
            else:
                chunks += [s for s in re.split(r"(?<=[.!?])\s+", para) if s.strip()]
        pieces, rate = [], 24000
        for chunk in chunks:
            audio, rate = self.kokoro.create(chunk, voice=style, speed=speed, lang=lang)
            pieces.append(np.asarray(audio))
            pieces.append(np.zeros(int(rate * 0.12), dtype=pieces[-1].dtype))
        out.parent.mkdir(parents=True, exist_ok=True)
        sf.write(str(out), np.concatenate(pieces), rate, subtype="PCM_16")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--voice", action="store_true", help="generate every voice line")
    ap.add_argument("--audition", action="store_true", help="the same lines in each candidate voice")
    ap.add_argument("--as", dest="voice_name", default=DEFAULT_VOICE, choices=sorted(VOICES))
    ap.add_argument("--only", default="", help="audition only voices whose name starts with this")
    ap.add_argument("--studio-lines", action="store_true", help="write the teleprompter script for Companion's Voice Studio")
    args = ap.parse_args()

    jobs = voice_jobs()
    voiced_rows = {row for row, _ in jobs}

    (DOCS / "DT_DCCSystemWindows.json").write_text(json.dumps(window_rows(voiced_rows), indent=2), encoding="utf-8")
    (DOCS / "DT_DCCRandomEncounters.json").write_text(json.dumps(encounter_rows(voiced_rows), indent=2), encoding="utf-8")
    print(f"tables: {len(WINDOWS)} windows, {len(ENCOUNTERS)} encounters")

    if args.audition:
        speaker = Speaker()
        sample = [j for j in jobs if j[0] in ("WorldDungeonAnnouncement", "Encounter_HOAPresident")]
        for name, (spec, speed) in VOICES.items():
            if not name.startswith(args.only):
                continue
            for row, text in sample:
                out = VOICE_OUT / "_audition" / f"{name}_{row}.wav"
                speaker.say(text, spec, speed, out)
                print("  wrote", out.relative_to(ROOT))

    if args.studio_lines:
        # Teleprompter for the Voice Studio (Companion/studio.html). A .js file rather than .json because the
        # studio also works opened straight from disk in a browser, where fetching a local .json is blocked.
        studio = [{"row": row, "text": text.replace(" | ", "\n\n").replace("|", "\n\n")} for row, text in jobs]
        out = ROOT / "Companion" / "studio" / "lines.js"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text("// Generated by Tools/gen_onboarding_content.py --studio-lines. Edit the wording there.\n"
                       f"window.DCC_LINES = {json.dumps(studio, indent=2)};\n", encoding="utf-8")
        print("wrote", out.relative_to(ROOT), len(studio), "lines")
    if args.voice:
        speaker = Speaker()
        spec, speed = VOICES[args.voice_name]
        for row, text in jobs:
            out = VOICE_OUT / args.voice_name / f"VO_{row}.wav"
            speaker.say(text, spec, speed, out)
            print("  wrote", out.relative_to(ROOT))


if __name__ == "__main__":
    sys.exit(main())
