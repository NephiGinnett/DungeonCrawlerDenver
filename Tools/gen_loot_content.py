"""Loot content: new gear, new boons, rarity for existing gear, and every loot box tier's table.

    python Tools/gen_loot_content.py      # writes the JSON in Docs/ and checks every mesh path exists

Edit the wording here, not in the JSON. Then, with the editor open:

    python Tools/import_loot_content.py

Voice: Docs/dungeon-tone-bible.md section 5. Loot is "bureaucratic prefix + mundane object + unearned
superlative", and the System is smug about the top tier. Rarity uses the books' box tiers
(Bronze, Silver, Gold, Platinum, Legendary, Celestial), which drive the colour of every item card.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "Docs"
CONTENT = ROOT / "Content"

SOCKET = {"MainHand": "hand_rSocket", "OffHand": "hand_lSocket", "Head": "headSocket",
          "Body": "spine_03Socket", "Trinket": "spine_03Socket"}

M = {  # short names for meshes that exist in installed packs
    "dumbbell": "/Game/GymEAP/Meshes/Dumbbells/SM_Dumbbells_5",
    "kettlebell": "/Game/GymEAP/Meshes/Accesories/SM_Kettlebell",
    "mug": "/Game/Legendary_Forge/Meshes/Mug/Sm_mug",
    "book": "/Game/Beach_house/Meshes/Book/SM_book_01",
    "ledger": "/Game/Modern_villa_kit/Meshes/Books/SM_book_02",
    "pan": "/Game/Modern_villa_kit/Meshes/Kitchen_props/SM_pan_01",
    "butterknife": "/Game/Modern_villa_kit/Meshes/Kitchen_props/SM_knife_03",
    "chisel": "/Game/UnrealBucket/Library/Tools/CarpenterTools/SM_Chisel_A",
    "handsaw": "/Game/UnrealBucket/Library/Tools/CarpenterTools/SM_HandSaw",
    "kettle": "/Game/UnrealBucket/Library/Various/Kettle/SM_Kettle_A",
    "helm_a": "/Game/UnrealBucket/Library/Various/VikingHelmet/SM_VikingHelmet_A",
    "helm_b": "/Game/UnrealBucket/Library/Various/VikingHelmet/SM_VikingHelmet_B",
    "grenade": "/Game/UnrealBucket/Library/Weapon/Grenade/SM_Grenade",
    "halberd": "/Game/UnrealBucket/Library/Weapon/Halberd/SM_Halberd",
    "forge_hammer": "/Game/Legendary_Forge/Meshes/Props/Tools/SM_hammer_02",
    "forge_hammer_big": "/Game/Legendary_Forge/Meshes/Props/Tools/SM_hammer_06",
    "forge_sword_3": "/Game/Legendary_Forge/Meshes/Swords/Sm_sword_03",
    "forge_sword_7": "/Game/Legendary_Forge/Meshes/Swords/Sm_sword_07",
    "dagger": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Dagger_1",
    "sickle": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Sickle",
    "spear": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Spear",
    "mace": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Mace",
    "greathammer": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_GreatHammer",
    "greataxe": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_GreatAxe",
    "wp_shield": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Shield",
    "shield_3": "/Game/Viking_village/Meshes/Shields/SM_shield_03",
    "shield_4": "/Game/Viking_village/Meshes/Shields/SM_shield_04",
    "shield_5": "/Game/Viking_village/Meshes/Shields/SM_shield_05",
    "briefcase": "/Game/Thai_Market/Meshes/Thai_market/SM_bag_04",
    "atris_b": "/Game/Atris_swords/Meshes/SM_Sword1b",
    "atris_g": "/Game/Atris_swords/Meshes/SM_Sword1g",
    "atris_k": "/Game/Atris_swords/Meshes/SM_Sword1k",
    # skill gear (2026-11-13)
    "stick_tall": "/Game/Viking_village/Meshes/Weapons/SM_Axe_stick_tall",
    "stick_small": "/Game/Viking_village/Meshes/Weapons/SM_axe_stick_small",
    "axe_02": "/Game/Viking_village/Meshes/Weapons/SM_axe_02",
    "axe_07": "/Game/Viking_village/Meshes/Weapons/SM_axe_07",
    "dagger_2": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Dagger_2",
    "knife_4": "/Game/Modern_villa_kit/Meshes/Kitchen_props/SM_knife_04",
    "forge_sword_1": "/Game/Legendary_Forge/Meshes/Swords/Sm_sword_01",
    "sledge": "/Game/Legendary_Forge/Meshes/Props/Tools/SM_hammer_05",
    "bottle": "/Game/Cave_Camp/Meshes/SM_bottle",
    "bowl": "/Game/Weapon_Pack/Mesh/Props/Interior/SM_Bowl",
    "throwing": "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Throwing",
}

GOLD_LIGHT = {"R": 1.0, "G": 0.78, "B": 0.30, "A": 1.0}
BLUE_LIGHT = {"R": 0.45, "G": 0.70, "B": 1.0, "A": 1.0}
WHITE_LIGHT = {"R": 1.0, "G": 0.95, "B": 0.85, "A": 1.0}


def gear(item_id, name, slot, rarity, desc, mesh=None, str_=0, int_=0, con=0, dex=0, cha=0, die=0, dmg="Physical",
         evade=0, reach=0.0, gold=10, kg=1.0, resist=None, light=None, cursed=False,
         skill=None, splash=0.0, handling=None, fumble=1, cooldown=0.0):
    row = {
        "Name": item_id, "ItemId": item_id, "DisplayName": name, "Slot": slot, "Description": desc,
        "BonusStrength": str_, "BonusIntelligence": int_, "BonusConstitution": con, "BonusDexterity": dex,
        "BonusCharisma": cha, "WeaponDamageDie": die, "WeaponDamageType": dmg, "EvadeBonus": evade,
        "Rarity": rarity, "AttackRangeOverride": reach, "MassKilograms": kg,
        "Mesh": f"{M[mesh]}.{M[mesh].rsplit('/', 1)[1]}" if mesh else "",
        "AttachSocket": SOCKET[slot], "GoldValue": gold, "bIsCursed": cursed,
        # Skills (2026-11-13): the weapon names its Attack Skill; explosives name a handling skill too.
        "WeaponSkillId": skill or "", "SplashRadius": float(splash), "HandlingSkillId": handling or "",
        "FumbleOnNatural": fumble, "AttackCooldownOverride": float(cooldown),
    }
    if resist:
        row["ResistanceDamageType"], row["ResistanceLevel"] = resist
    if light:
        row.update({"bEmitsLight": True, "LightColor": light, "LightIntensity": 45.0, "LightAttenuationRadius": 650.0})
    return row


NEW_GEAR = [
    # --- Bronze: standard issue, lightly used, nobody's first choice -----------------------------------
    gear("FiveKiloDumbbell", "Dumbbell, 5 kg, Lightly Used", "MainHand", "Bronze",
         "Abandoned in the hotel gym by a guest who had a plan. The plan did not survive January.",
         "dumbbell", str_=1, die=6, dmg="Bludgeoning", gold=12, kg=5.0),
    gear("WorldsOkayestMug", "World's Okayest Crawler Mug", "Trinket", "Bronze",
         "A gift from the System's secret-santa programme. The System drew its own name.",
         "mug", con=1, gold=6),
    gear("AspirationalButterKnife", "Butter Knife, Aspirational", "MainHand", "Bronze",
         "Rated for butter. Has ambitions. Management encourages internal mobility.",
         "butterknife", dex=1, die=4, dmg="Piercing", gold=4, kg=0.1),
    gear("UnsupervisedChisel", "Chisel, Unsupervised", "MainHand", "Bronze",
         "Last seen in a woodworking class with a nine-to-one student ratio. You are the one.",
         "chisel", dex=1, die=4, dmg="Piercing", gold=9, kg=0.3),
    gear("StuckNonStickPan", "Non-Stick Pan, Stuck", "MainHand", "Bronze",
         "The coating gave up years ago. The pan did not. Neither should you, says the System, briefly.",
         "pan", die=6, dmg="Bludgeoning", evade=1, gold=10, kg=1.2),
    gear("ExpiredVisitorLanyard", "Visitor Lanyard, Expired", "Trinket", "Bronze",
         "Valid for one (1) visit on a date that no longer exists. Security waves you through anyway. Security is dead.",
         cha=1, gold=3),
    gear("DecorativeHardHat", "Hard Hat, Decorative", "Head", "Bronze",
         "From a groundbreaking ceremony. The ground did break, eventually, all at once.",
         evade=1, gold=8),
    gear("ClipboardOfImpliedAuthority", "Clipboard of Implied Authority", "OffHand", "Bronze",
         "Carry it purposefully and nobody asks what you are doing here. Monsters, it turns out, also respect a clipboard.",
         "book", cha=1, evade=1, gold=7, kg=0.4),
    gear("CustomerOfTheMonthPlaque", "Customer of the Month Plaque", "Trinket", "Bronze",
         "Awarded to you, apparently. It will not come off. It is very proud of you. That is the curse.",
         str_=0, cha=2, dex=-1, gold=1, cursed=True),

    # --- Silver: Preferred tier. Works. Has a warranty. The warranty is void. -----------------------------
    gear("ApprenticeForgeHammer", "Apprentice's Forge Hammer", "MainHand", "Silver",
         "Swung ten thousand times by somebody learning. Now swung once, properly, by somebody surviving.",
         "forge_hammer", str_=1, die=8, dmg="Bludgeoning", gold=40, kg=2.0),
    gear("FullyCommittedHandSaw", "Hand Saw, Fully Committed", "MainHand", "Silver",
         "Once it starts, it finishes. The System is filing this under motivational content.",
         "handsaw", dex=1, die=8, gold=35, kg=0.9),
    gear("RecentlyBoiledKettle", "Kettle, Recently Boiled", "MainHand", "Silver",
         "Still hot. Will be hot for a suspiciously long time. Nobody can find where it plugs in.",
         "kettle", die=6, dmg="Fire", con=1, gold=30, kg=1.5),
    gear("GiftShopVikingShield", "Gift-Shop Viking Shield", "OffHand", "Silver",
         "From the museum shop, between the horned helmets and the novelty mead. It has never seen a Viking. It has now seen a lot.",
         "shield_3", con=1, evade=2, gold=45, kg=3.0),
    gear("ExecutiveLetterOpener", "Letter Opener, Executive Edition", "MainHand", "Silver",
         "Opens letters. Opens other things. The engraving reads TO OUR VALUED PARTNER. It does not say partner in what.",
         "dagger", dex=2, die=6, dmg="Piercing", gold=50, kg=0.3),
    gear("BrandedFleeceVest", "Branded Fleece Vest", "Body", "Silver",
         "The uniform of a man who says 'circle back.' Warm, smug, and surprisingly hard to stab through.",
         con=1, cha=1, evade=1, gold=40, kg=0.8),
    gear("SomeoneElsesBifocals", "Bifocals, Someone Else's Prescription", "Head", "Silver",
         "Everything is clearer and slightly to the left. You can read the fine print now. You wish you couldn't.",
         int_=2, dex=-1, gold=30),
    gear("KettlebellOnABungee", "Kettlebell on a Bungee Cord", "MainHand", "Silver",
         "Crossfit's one contribution to the dungeon economy. The bungee cord was not a design decision.",
         "kettlebell", str_=1, dex=-1, die=10, dmg="Bludgeoning", gold=35, kg=12.0),

    # --- Gold: Premium. Comes with a nicer box and a worse ending. ---------------------------------------
    gear("HistoricallyInaccurateHelm", "Horned Helmet (Historically Inaccurate)", "Head", "Gold",
         "Vikings never wore horns. The helmet does not care. Neither, after the first fight, will you.",
         "helm_a", str_=1, con=1, evade=1, gold=110, kg=2.5),
    gear("UnsignedJourneymanBlade", "Journeyman's Blade, Unsigned", "MainHand", "Gold",
         "Good enough to sell, not good enough to sign. Most of Denver's skyline was built on the same principle.",
         "forge_sword_3", str_=1, dex=1, die=10, gold=140, kg=1.6),
    gear("SickleOfRollingDeadlines", "Sickle of Rolling Deadlines", "MainHand", "Gold",
         "Every swing pushes something back by a quarter. The thing it pushes back is usually alive.",
         "sickle", dex=2, die=8, gold=120, kg=0.8),
    gear("SpearOfAnnualReview", "Spear of the Annual Review", "MainHand", "Gold",
         "Keeps your problems at arm's length, plus about a metre. Feedback is delivered pointy end first.",
         "spear", str_=1, die=10, dmg="Piercing", reach=280.0, gold=130, kg=2.2),
    gear("HardShellBriefcase", "Hard-Shell Briefcase, Combination Lost", "OffHand", "Gold",
         "Nobody knows what is inside. It blocks a sword like a promotion blocks a career. Shake it and something legal rattles.",
         "briefcase", int_=1, evade=2, gold=95, kg=2.0),
    gear("PineappleNotTheFruit", "Pineapple (Not the Fruit)", "MainHand", "Gold",
         "The pin is in. Sponsors are begging you to take the pin out. Throws a long way; explains itself on arrival.",
         "grenade", die=12, dmg="Fire", reach=900.0, gold=150, kg=0.6),
    gear("TenYearServiceRing", "Ten-Year Service Ring", "Trinket", "Gold",
         "Presented at a ceremony nobody attended, for a decade nobody noticed. Heavier than it looks. So was the decade.",
         con=1, cha=1, gold=120),
    gear("MaceOfRegulatoryCompliance", "Mace of Regulatory Compliance", "MainHand", "Gold",
         "Every spike is a clause. Every clause has been tested in court. The court is now a dungeon level.",
         "mace", con=1, die=10, dmg="Bludgeoning", gold=135, kg=2.8),

    # --- Platinum: Executive. Corner office energy, in a weapon. ---------------------------------------
    gear("DemolitionNotice", "Demolition Notice (Great Hammer)", "MainHand", "Platinum",
         "Served personally. Every wall it meets is retroactively zoned for removal.",
         "greathammer", str_=2, dex=-1, die=12, dmg="Bludgeoning", gold=300, kg=9.0),
    gear("HalberdOfZoningVariance", "Halberd of Zoning Variance", "MainHand", "Platinum",
         "Grants you permission to be somewhere you are not allowed. Mostly inside other people's personal space.",
         "halberd", str_=1, con=1, die=12, dmg="Piercing", reach=300.0, gold=320, kg=4.0),
    gear("BladeOfStoreCredit", "Blade of Store Credit", "MainHand", "Platinum",
         "Non-refundable. Exchangeable only for violence of equal or lesser value.",
         "atris_b", dex=2, cha=1, die=10, gold=280, kg=1.3),
    gear("ChieftainsHelmRental", "Chieftain's Helm, Rental", "Head", "Platinum",
         "Return by Sunday or incur fees. The fees are a Viking. He is on his way.",
         "helm_b", str_=2, con=1, evade=1, gold=260, kg=3.0),
    gear("HeatLampBulwark", "Heat-Lamp Bulwark", "OffHand", "Platinum",
         "Kept the fries warm through two fire inspections. Fire has stopped trying.",
         "shield_5", evade=3, resist=("Fire", "Resistant"), gold=290, kg=4.0),
    gear("MonogrammedExecutiveFleece", "Executive Fleece, Monogrammed", "Body", "Platinum",
         "The initials are not yours. The confidence is now yours. That is how fleece works.",
         con=2, cha=2, evade=1, gold=270, kg=0.9),
    gear("NoiseCancellingHeadset", "Noise-Cancelling Headset", "Head", "Platinum",
         "Blocks out the screaming, the announcements and most of the System's notes. The System has noted this.",
         int_=2, resist=("Psychic", "Resistant"), gold=240),

    # --- Legendary: Legacy plan. Every one of these has a terms-of-service page. -------------------------
    gear("MasterworkBladeSignedInPen", "Masterwork Blade, Signed in Pen", "MainHand", "Legendary",
         "The smith signed it in biro because the engraver was on strike. Terms of service, page 1: the blade owns you, legally.",
         "forge_sword_7", str_=2, dex=2, die=12, gold=600, kg=1.8),
    gear("HostileTakeover", "Hostile Takeover (Great Axe)", "MainHand", "Legendary",
         "Acquires whatever it hits. Board approval is assumed. Terms of service, page 4: severance is literal.",
         "greataxe", str_=3, dex=-1, die=14, gold=650, kg=7.0),
    gear("GoldenParachute", "The Golden Parachute", "MainHand", "Legendary",
         "However badly this goes, it goes well for whoever holds this. Terms of service, page 9: the parachute does not open.",
         "atris_g", cha=3, die=12, gold=700, kg=1.2, light=GOLD_LIGHT),
    gear("AegisOfTheTermsOfService", "Aegis of the Terms of Service", "OffHand", "Legendary",
         "Four hundred pages, laminated, riveted to a shield. Nothing gets through. Nothing has ever read it either.",
         "shield_4", int_=1, evade=4, gold=620, kg=5.0),
    gear("CrownOfMiddleManagement", "Crown of Middle Management", "Head", "Legendary",
         "Rules over everyone below you. Answers to everyone above you. Fits nobody, but it fits you best.",
         int_=1, cha=3, gold=580),
    gear("PlateOfLimitedLiability", "Plate of Limited Liability", "Body", "Legendary",
         "Whatever happens to you, legally, happened to the plate. Terms of service, page 12: the plate has a lawyer.",
         con=3, dex=-1, evade=2, resist=("Physical", "Resistant"), gold=640, kg=12.0),
    gear("NonTransferableSponsorSignet", "Sponsor's Signet (Non-Transferable)", "Trinket", "Legendary",
         "Makes you better at everything. You are also, now, the property of a beverage company. It will not come off.",
         str_=2, int_=2, con=2, dex=2, cha=2, gold=900, cursed=True),

    # --- Celestial: the System is smug about these, and it is right to be. -------------------------------
    gear("EndOfQuarter", "End of Quarter", "MainHand", "Celestial",
         "Everything owed is collected at once. The blade glows the colour of a closed fiscal year.",
         "atris_k", str_=3, dex=3, die=14, gold=1500, kg=1.4, light=WHITE_LIGHT),
    gear("ThunderHammerOnLease", "Thunder Hammer, On Lease", "MainHand", "Celestial",
         "Mythologically significant. Contractually temporary. Hit something with it before the lease is reviewed.",
         "forge_hammer_big", str_=4, die=14, dmg="Electric", gold=1600, kg=6.0, light=BLUE_LIGHT),
    gear("HaloOfMaximumEngagement", "Halo of Maximum Engagement", "Head", "Celestial",
         "The audience will not look away from you. That is the blessing. That is also the other thing.",
         int_=2, cha=4, evade=1, gold=1400, light=GOLD_LIGHT),
    gear("DeedToDenverNotarised", "Deed to Denver, Notarised", "Trinket", "Celestial",
         "The whole city, signed back over to somebody who lives in it. Mile High Recovery Solutions would like to discuss this.",
         con=3, cha=3, gold=2000),
]

# Rarity for gear that already exists in DT_DCCEquipment (it was authored before rarity was a column).
EXISTING_RARITY = {
    "TenderiserMallet": "Bronze", "ChefsKnife": "Bronze", "ServiceTray": "Bronze", "PatioSunHat": "Bronze",
    "GreaseApron": "Bronze", "NameTagDave": "Bronze", "Torch": "Bronze", "ThrownBottle": "Bronze",
    "HighVisibilitySafetyVest": "Bronze", "ComplaintFolderOverstuffed": "Bronze",
    "CastIronSkillet": "Silver", "FireAxe": "Silver", "FeatureWallShield": "Silver", "ManagersBlazer": "Silver",
    "ServiceKnifeSet": "Silver", "SodaGun": "Silver", "FlareGunEmergency": "Silver",
    "SeveranceCheckUncashed": "Silver", "Boombox": "Silver",
    "WallSword": "Gold", "ConfettiCrossbow": "Gold", "NailGunPro": "Gold", "SecuritySidearm": "Gold",
    "HuntingRifleColorado": "Platinum", "RiotShotgun": "Platinum", "ConfiscatedSMG": "Platinum",
    "BoilerplateRifle": "Legendary", "SyndicateBadge": "Celestial",
}


def boon(boon_id, name, desc, *effects):
    rows = []
    for e in effects:
        rows.append({"InstantHealthChange": e.get("hp", 0), "ModifiedAbility": e.get("stat", "Strength"),
                     "ModifierAmount": e.get("amount", 0), "ModifierDurationSeconds": float(e.get("secs", 0))})
    return {"Name": boon_id, "BoonId": boon_id, "DisplayName": name, "Description": desc, "Effects": rows}


NEW_BOONS = [
    boon("ComplimentaryRefill", "Complimentary Refill",
         "The System tops you up, the way a waiter tops up a glass you did not ask to have topped up.", {"hp": 10}),
    boon("SponsoredHydration", "Sponsored Hydration",
         "This moment of not being thirsty is brought to you by a water brand with a mountain on it. Not this mountain.",
         {"stat": "Constitution", "amount": 1, "secs": 600}),
    boon("ViralMoment", "Viral Moment",
         "Somebody clipped that. Somebody is setting it to music. For five minutes, everyone likes you.",
         {"stat": "Charisma", "amount": 2, "secs": 300}),
    boon("PerformanceEnhancerLegal", "Performance Enhancer (Legal in Denver)",
         "Approved by a board that met once, in a parking lot. You feel great. Do not read the label.",
         {"stat": "Strength", "amount": 2, "secs": 240}),
    boon("SecondOpinion", "A Second Opinion",
         "The System has consulted a second System. They agree you should be smarter, briefly.",
         {"stat": "Intelligence", "amount": 2, "secs": 300}),
    boon("CaffeineDrip", "Caffeine, Intravenous",
         "Every bean in the break room, administered at once. Your hands are very fast and you can hear colours.",
         {"stat": "Dexterity", "amount": 2, "secs": 180}),
    boon("ExtendedWarranty", "Extended Warranty",
         "Covers you for another six hundred seconds of ordinary wear and tear. Acts of monster are excluded.",
         {"hp": 10, "stat": "Constitution", "amount": 2, "secs": 600}),
    boon("StandingOvation", "Standing Ovation",
         "The audience is on its feet. The applause physically heals you. Nobody can explain this and nobody is trying.",
         {"hp": 30}, {"stat": "Charisma", "amount": 1, "secs": 300}),
    boon("PlotArmor", "Plot Armour",
         "The writers have decided you are not dying in this episode. They reserve the right to revisit that.",
         {"stat": "Constitution", "amount": 3, "secs": 300}),
    boon("DirectorsCut", "The Director's Cut",
         "Every scene you are in is now longer, faster and better lit.",
         {"stat": "Strength", "amount": 2, "secs": 300}, {"stat": "Dexterity", "amount": 2, "secs": 300}),
    boon("AudienceFavorite", "Audience Favourite",
         "They chant your name. They spell it wrong. It works anyway.",
         {"hp": 20}, {"stat": "Charisma", "amount": 3, "secs": 600}),
    boon("DivineIntervention", "Divine Intervention (Sponsored)",
         "Something enormous reaches down and fixes you, then leaves a business card.",
         {"hp": 60}, {"stat": "Constitution", "amount": 3, "secs": 600}, {"stat": "Strength", "amount": 2, "secs": 600}),
]

COINS = "/Script/DungeonCrawlerDenver.DCCPickup_Coins"
GEAR = "/Script/DungeonCrawlerDenver.DCCPickup_Equipment"


def table(tier, gold, items, boons):
    entries = [{"PickupClass": COINS, "Chance": 1.0, "MinGold": gold[0], "MaxGold": gold[1],
                "ScatterRadius": 120.0, "EquipmentItemId": ""}]
    for item_id, chance in items:
        entries.append({"PickupClass": GEAR, "Chance": chance, "MinGold": 0, "MaxGold": 0,
                        "ScatterRadius": 120.0, "EquipmentItemId": item_id})
    return {"Name": tier, "DisplayName": f"{tier} Box", "ItemEntries": entries,
            "BoonEntries": [{"BoonId": b, "Chance": c} for b, c in boons]}


# Each entry rolls on its own, so the sum of chances is roughly how much gear a box holds.
# Mostly the box's own tier, some of the tier below, and one or two long shots from the tier above.
BOX_TABLES = [
    table("Bronze", (5, 25), [
        ("FiveKiloDumbbell", 0.18), ("WorldsOkayestMug", 0.18), ("AspirationalButterKnife", 0.18),
        ("UnsupervisedChisel", 0.15), ("StuckNonStickPan", 0.15), ("ExpiredVisitorLanyard", 0.15),
        ("DecorativeHardHat", 0.15), ("ClipboardOfImpliedAuthority", 0.15), ("CustomerOfTheMonthPlaque", 0.06),
        ("ThrownBottle", 0.15), ("ServiceTray", 0.12), ("NameTagDave", 0.10), ("HighVisibilitySafetyVest", 0.10),
        ("ApprenticeForgeHammer", 0.05), ("BrandedFleeceVest", 0.04),
    ], [("ComplimentaryRefill", 0.35), ("SecondWind", 0.15)]),
    table("Silver", (20, 60), [
        ("ApprenticeForgeHammer", 0.18), ("FullyCommittedHandSaw", 0.18), ("RecentlyBoiledKettle", 0.16),
        ("GiftShopVikingShield", 0.16), ("ExecutiveLetterOpener", 0.15), ("BrandedFleeceVest", 0.15),
        ("SomeoneElsesBifocals", 0.14), ("KettlebellOnABungee", 0.14), ("CastIronSkillet", 0.12),
        ("FireAxe", 0.12), ("ServiceKnifeSet", 0.12), ("SodaGun", 0.10), ("Boombox", 0.08),
        ("ClipboardOfImpliedAuthority", 0.12), ("DecorativeHardHat", 0.10),
        ("HistoricallyInaccurateHelm", 0.05), ("SickleOfRollingDeadlines", 0.04),
    ], [("SteadyHands", 0.25), ("SponsoredHydration", 0.25), ("ComplimentaryRefill", 0.25)]),
    table("Gold", (50, 140), [
        ("HistoricallyInaccurateHelm", 0.18), ("UnsignedJourneymanBlade", 0.18), ("SickleOfRollingDeadlines", 0.16),
        ("SpearOfAnnualReview", 0.16), ("HardShellBriefcase", 0.16), ("PineappleNotTheFruit", 0.14),
        ("TenYearServiceRing", 0.14), ("MaceOfRegulatoryCompliance", 0.14), ("WallSword", 0.10),
        ("ConfettiCrossbow", 0.10), ("NailGunPro", 0.10), ("SecuritySidearm", 0.08),
        ("ExecutiveLetterOpener", 0.12), ("KettlebellOnABungee", 0.10),
        ("ChieftainsHelmRental", 0.05), ("BladeOfStoreCredit", 0.04),
    ], [("ViralMoment", 0.30), ("SecondOpinion", 0.25), ("MinorFortitude", 0.25), ("CaffeineDrip", 0.20)]),
    table("Platinum", (120, 300), [
        ("DemolitionNotice", 0.18), ("HalberdOfZoningVariance", 0.16), ("BladeOfStoreCredit", 0.18),
        ("ChieftainsHelmRental", 0.16), ("HeatLampBulwark", 0.16), ("MonogrammedExecutiveFleece", 0.16),
        ("NoiseCancellingHeadset", 0.14), ("HuntingRifleColorado", 0.10), ("RiotShotgun", 0.10),
        ("ConfiscatedSMG", 0.10), ("UnsignedJourneymanBlade", 0.12), ("PineappleNotTheFruit", 0.12),
        ("CrownOfMiddleManagement", 0.04), ("MasterworkBladeSignedInPen", 0.03),
    ], [("PerformanceEnhancerLegal", 0.30), ("ExtendedWarranty", 0.30), ("SilverTongue", 0.25), ("PlotArmor", 0.15)]),
    table("Legendary", (250, 600), [
        ("MasterworkBladeSignedInPen", 0.18), ("HostileTakeover", 0.16), ("GoldenParachute", 0.15),
        ("AegisOfTheTermsOfService", 0.15), ("CrownOfMiddleManagement", 0.16), ("PlateOfLimitedLiability", 0.15),
        ("NonTransferableSponsorSignet", 0.08), ("BoilerplateRifle", 0.10), ("DemolitionNotice", 0.12),
        ("HalberdOfZoningVariance", 0.12), ("MonogrammedExecutiveFleece", 0.10),
        ("EndOfQuarter", 0.03), ("HaloOfMaximumEngagement", 0.03),
    ], [("StandingOvation", 0.35), ("DirectorsCut", 0.30), ("PlotArmor", 0.30), ("AdrenalineSurge", 0.25),
        ("IronWill", 0.20)]),
    table("Celestial", (600, 1200), [
        ("EndOfQuarter", 0.25), ("ThunderHammerOnLease", 0.25), ("HaloOfMaximumEngagement", 0.25),
        ("DeedToDenverNotarised", 0.20), ("SyndicateBadge", 0.20), ("GoldenParachute", 0.20),
        ("HostileTakeover", 0.20), ("AegisOfTheTermsOfService", 0.18), ("PlateOfLimitedLiability", 0.18),
        ("NonTransferableSponsorSignet", 0.12),
    ], [("DivineIntervention", 0.40), ("AudienceFavorite", 0.40), ("SponsorsBlessing", 0.40), ("DirectorsCut", 0.30),
        ("ClarityOfPurpose", 0.25)]),
]


# ---------------------------------------------------------------------------------------------------
# Skills (2026-11-13). From the Crawlers Preview: "The name of the weapon is also the name of the Skill
# used to wield that weapon." Rank adds to the attack roll; Rank 5 adds a damage die; untrained is
# Disadvantage and teaches the skill. Explosives are thrown with Throwing and handled with Explosives
# Handling, Goblin Explosives (goes off in hand on a natural 1-4) or Incendiary Device Handling.
# ---------------------------------------------------------------------------------------------------

# Which Attack Skill each piece of batch-1 gear uses.
BATCH1_SKILLS = {
    "FiveKiloDumbbell": "ImprovisedWeapons", "AspirationalButterKnife": "Dagger", "UnsupervisedChisel": "Dagger",
    "StuckNonStickPan": "ImprovisedWeapons", "ApprenticeForgeHammer": "Club", "FullyCommittedHandSaw": "ImprovisedWeapons",
    "RecentlyBoiledKettle": "ImprovisedWeapons", "ExecutiveLetterOpener": "Dagger", "KettlebellOnABungee": "ImprovisedWeapons",
    "UnsignedJourneymanBlade": "Longsword", "SickleOfRollingDeadlines": "Dagger", "SpearOfAnnualReview": "Polearm",
    "MaceOfRegulatoryCompliance": "Club", "DemolitionNotice": "Warhammer", "HalberdOfZoningVariance": "Polearm",
    "BladeOfStoreCredit": "Longsword", "MasterworkBladeSignedInPen": "Longsword", "HostileTakeover": "Axe",
    "GoldenParachute": "Longsword", "EndOfQuarter": "Longsword", "ThunderHammerOnLease": "Warhammer",
}
for _row in NEW_GEAR:
    if _row["ItemId"] in BATCH1_SKILLS:
        _row["WeaponSkillId"] = BATCH1_SKILLS[_row["ItemId"]]
    if _row["ItemId"] == "PineappleNotTheFruit":  # the grenade becomes a real explosive
        _row.update({"WeaponSkillId": "Throwing", "SplashRadius": 380.0, "HandlingSkillId": "ExplosivesHandling",
                     "FumbleOnNatural": 1, "AttackCooldownOverride": 3.0, "WeaponDamageDie": 12})

# And existing gear (authored before skills were a column).
EXISTING_SKILLS = {
    "TenderiserMallet": "Club", "ChefsKnife": "Dagger", "CastIronSkillet": "ImprovisedWeapons", "FireAxe": "Axe",
    "WallSword": "Longsword", "ThrownBottle": "Throwing", "ServiceKnifeSet": "Throwing", "SodaGun": "ImprovisedWeapons",
}

SKILL_GEAR = [
    # --- Club --------------------------------------------------------------------------------------
    gear("TableLegLiberated", "Table Leg, Liberated", "MainHand", "Bronze",
         "The table did not survive the conversation. The leg did, and it has opinions now.",
         "stick_small", die=6, dmg="Bludgeoning", gold=12, kg=1.5, skill="Club"),
    gear("RockiesGiveawayBat", "Baseball Bat, Giveaway Night", "MainHand", "Silver",
         "Free to the first ten thousand fans. The other fans were not so lucky. The most-used piece of sporting equipment in Denver, finally.",
         "stick_tall", str_=1, die=6, dmg="Bludgeoning", gold=45, kg=1.0, skill="Club"),
    # --- Axe ---------------------------------------------------------------------------------------
    gear("CampingSectionHatchet", "Hatchet, Camping Section", "MainHand", "Bronze",
         "From the aisle between the tents and the bear spray. It has never been camping. It has been to the dungeon.",
         "axe_02", die=6, dmg="Physical", gold=20, kg=1.0, skill="Axe"),
    gear("IronicallyBeardedAxe", "Bearded Axe, Worn Ironically", "MainHand", "Gold",
         "Bought for a photo in a Highlands brewery. The photo did numbers. So, now, does the axe.",
         "axe_07", str_=1, con=1, die=8, dmg="Physical", gold=140, kg=2.0, skill="Axe"),
    # --- Dagger ------------------------------------------------------------------------------------
    gear("ConfiscatedSwitchblade", "Switchblade, Confiscated at Security", "MainHand", "Silver",
         "Taken off a passenger at Concourse B in 2019 and kept in a drawer for exactly this. Security would like you to know it was right.",
         "dagger_2", dex=1, die=4, dmg="Piercing", gold=40, kg=0.2, skill="Dagger"),
    gear("BootKnifeRegrettable", "Boot Knife, Regrettable", "MainHand", "Bronze",
         "Lived in a boot for a decade. The knife is fine. Nobody wants to discuss the boot.",
         "knife_4", dex=1, die=4, dmg="Piercing", gold=15, kg=0.2, skill="Dagger"),
    # --- Longsword ---------------------------------------------------------------------------------
    gear("RenFaireLongsword", "Longsword, Renaissance Faire Grade", "MainHand", "Silver",
         "Sharpened, against every rule of the faire. The man who sold it bowed. Nobody asked him to.",
         "forge_sword_1", str_=1, die=8, dmg="Physical", gold=70, kg=1.5, skill="Longsword"),
    # --- Warhammer ---------------------------------------------------------------------------------
    gear("ContractorSledgehammer", "Sledgehammer, Contractor Grade", "MainHand", "Silver",
         "Two hands, one opinion. The contractor quoted for demolition. The dungeon counts.",
         "sledge", str_=1, dex=-1, die=10, dmg="Bludgeoning", gold=75, kg=5.0, skill="Warhammer"),
    # --- Throwing ----------------------------------------------------------------------------------
    gear("ThrowingStarsGiftShop", "Throwing Stars, Gift Shop", "MainHand", "Bronze",
         "Sold next to the snow globes. Sharper than the snow globes. Barely.",
         "throwing", dex=1, die=4, dmg="Piercing", reach=900.0, gold=18, kg=0.2, skill="Throwing"),

    # --- Explosives Handling (goes off in hand on a natural 1) --------------------------------------
    gear("DynamiteBundledSingle", "Dynamite, One Stick (Sold Bundled)", "MainHand", "Bronze",
         "Mining country always had plenty lying about. The shop sells them in a bundle of one. That is the bundle.",
         "grenade", die=10, dmg="Fire", reach=900.0, gold=25, kg=0.3,
         skill="Throwing", splash=300, handling="ExplosivesHandling", fumble=1, cooldown=2.5),
    gear("PipeBombHardwareSpecial", "Pipe Bomb, Hardware Store Special", "MainHand", "Silver",
         "Assembled from aisle 14 by someone who did not read the warnings on aisle 14. A classic for a reason. The reason is shrapnel.",
         "grenade", die=12, dmg="Piercing", reach=850.0, gold=60, kg=1.0,
         skill="Throwing", splash=320, handling="ExplosivesHandling", fumble=1, cooldown=3.0),
    gear("FireworksSurplus", "Fireworks, Fourth of July Surplus", "MainHand", "Bronze",
         "Illegal within Denver city limits, which no longer exist. Loud, bright, and only accidentally lethal.",
         "grenade", die=8, dmg="Fire", reach=1000.0, cha=1, gold=20, kg=0.5,
         skill="Throwing", splash=420, handling="ExplosivesHandling", fumble=1, cooldown=2.0),
    gear("DemolitionChargeUnion", "Demolition Charge, Union Labour", "MainHand", "Gold",
         "Placed to code, by a professional, after a fifteen-minute break. Brings down anything with a load path.",
         "grenade", die=20, dmg="Fire", reach=700.0, gold=220, kg=2.5,
         skill="Throwing", splash=480, handling="ExplosivesHandling", fumble=1, cooldown=5.0),

    # --- Incendiary Device Handling -----------------------------------------------------------------
    gear("MolotovWellSpirit", "Molotov, House Well Spirit", "MainHand", "Bronze",
         "The cheapest bottle behind the bar, put to its best use yet. The rag is a souvenir tea towel.",
         "bottle", die=10, dmg="Fire", reach=900.0, gold=22, kg=0.8,
         skill="Throwing", splash=300, handling="IncendiaryDeviceHandling", fumble=1, cooldown=2.5),
    gear("NapalmPotCeramic", "Napalm Pot, Ceramic", "MainHand", "Gold",
         "Thrown on a wheel, filled at a pump, thrown again. The books suggest a ceramic pot for exactly this. We listened.",
         "bowl", die=16, dmg="Fire", reach=800.0, gold=150, kg=2.0,
         skill="Throwing", splash=400, handling="IncendiaryDeviceHandling", fumble=2, cooldown=4.0),

    # --- Goblin Explosives (goes off in hand on a natural 1-4; 1-3 at Rank 5) -------------------------
    gear("GoblinBombProbablyStable", "Goblin Bomb, Probably Stable", "MainHand", "Silver",
         "Bigger bang for less gold. The label says STABLE in four alphabets. Three of them are lying.",
         "grenade", die=16, dmg="Fire", reach=850.0, gold=40, kg=1.0,
         skill="Throwing", splash=380, handling="GoblinExplosives", fumble=4, cooldown=3.0),
    gear("GoblinGrenadeExtraSpicy", "Goblin Grenade, Extra Spicy", "MainHand", "Gold",
         "A goblin grenade with a pepper painted on it. The pepper is a rating. The rating is a warning.",
         "grenade", die=20, dmg="Fire", reach=850.0, gold=110, kg=1.0,
         skill="Throwing", splash=420, handling="GoblinExplosives", fumble=4, cooldown=3.5),
    gear("GoblinSatchelDoNotShake", "Goblin Satchel Charge (Do Not Shake)", "MainHand", "Platinum",
         "A sack of goblin explosives with a handle sewn on. Do not shake. Do not drop. Do not, the System begs, fumble.",
         "briefcase", die=24, dmg="Fire", reach=700.0, gold=320, kg=4.0,
         skill="Throwing", splash=560, handling="GoblinExplosives", fumble=4, cooldown=6.0),
]


def consumable(item_id, name, desc, gold, heal, skill=None, per_rank=0, mesh="/Game/Beach_house/Meshes/Book/SM_book_02.SM_book_02"):
    return {"Name": item_id, "ItemId": item_id, "DisplayName": name, "Description": desc, "GoldValue": gold,
            "Tags": ["Item.Consumable", "Item.Medical"], "Mesh": mesh,
            "Effects": [{"InstantHealthChange": heal}], "LinkedSkillId": skill or "", "HealPerSkillRank": per_rank}


FIRST_AID = [
    consumable("BandageRollMostlySterile", "Bandage Roll, Mostly Sterile",
               "Mostly. The rest is a mystery the bandage is keeping to itself.", 15, 6, "FirstAid", 3),
    consumable("FirstAidKitCompliant", "First Aid Kit, Workplace Compliant",
               "Inspected every year by someone who ticked the box without opening it. Fortunately, it is full.", 40, 12, "FirstAid", 4),
    consumable("TraumaKitParamedic", "Trauma Kit, Paramedic's Personal",
               "Belonged to a paramedic who was very good at her job. Use it like she would have. Then use it again.", 120, 25, "FirstAid", 6),
]


def skill_row(skill_id, name, flavor, stat):
    return {"Name": skill_id, "SkillId": skill_id, "DisplayName": name, "FlavorText": flavor, "Archetype": "Check",
            "Targeting": "Self", "CastingAbility": stat, "ManaCost": 0, "CooldownSeconds": 0.0, "CastTimeSeconds": 0.0,
            "Range": 0.0, "Radius": 0.0, "ConeHalfAngleDegrees": 0.0, "DifficultyClass": 0, "PowerDie": 0,
            "EffectDuration": 0.0, "bCanAttemptUntrained": True, "bIsPassive": False}


# The books' skills the new gear uses (Crawlers Preview, Attack Skills and Utility Skills).
BOOK_SKILLS = [
    skill_row("Club", "Club", "Hit it with a stick. The first idea civilisation had, and still its most reliable.", "Strength"),
    skill_row("Axe", "Axe", "Chop. The dungeon is mostly wood, if you are motivated.", "Strength"),
    skill_row("Dagger", "Dagger", "Quick, close and personal.", "Dexterity"),
    skill_row("Longsword", "Longsword", "The one everyone pictures. The audience approves.", "Strength"),
    skill_row("Warhammer", "Warhammer", "Two hands, one outcome.", "Strength"),
    skill_row("Polearm", "Polearm", "Hit things from further away than they would like.", "Strength"),
    skill_row("ImprovisedWeapons", "Improvised Weapons", "Pick something up and hit someone with it. The audience loves a dumbbell.", "Strength"),
    skill_row("Throwing", "Throwing", "Passing the pigskin, or throwing dynamite. A miss lands as many feet off as it missed by.", "Strength"),
    skill_row("ExplosivesHandling", "Explosives Handling", "Handle standard explosives without becoming the explosion.", "Intelligence"),
    skill_row("GoblinExplosives", "Goblin Explosives", "So unstable that a natural 1 to 4 goes off in your hand. Rank 5: 1 to 3.", "Intelligence"),
    skill_row("IncendiaryDeviceHandling", "Incendiary Device Handling", "Fire, in a bottle, going the right way.", "Intelligence"),
    skill_row("FirstAid", "First Aid", "Minor medical aid. A success adds healing for every Rank.", "Intelligence"),
]

# Skill gear in the boxes too: explosives are the loudest thing a sponsor can send.
_BOX_EXTRAS = {
    "Bronze": [("DynamiteBundledSingle", 0.10), ("FireworksSurplus", 0.10), ("MolotovWellSpirit", 0.08), ("TableLegLiberated", 0.08)],
    "Silver": [("PipeBombHardwareSpecial", 0.10), ("GoblinBombProbablyStable", 0.10), ("RockiesGiveawayBat", 0.08), ("ConfiscatedSwitchblade", 0.08)],
    "Gold": [("DemolitionChargeUnion", 0.08), ("NapalmPotCeramic", 0.08), ("GoblinGrenadeExtraSpicy", 0.08), ("IronicallyBeardedAxe", 0.08)],
    "Platinum": [("GoblinSatchelDoNotShake", 0.12), ("DemolitionChargeUnion", 0.10)],
    "Legendary": [("GoblinSatchelDoNotShake", 0.12)],
}
for _t in BOX_TABLES:
    for _id, _chance in _BOX_EXTRAS.get(_t["Name"], []):
        _t["ItemEntries"].append({"PickupClass": GEAR, "Chance": _chance, "MinGold": 0, "MaxGold": 0,
                                  "ScatterRadius": 120.0, "EquipmentItemId": _id})


def stats_line(r):
    parts = []
    if r["WeaponDamageDie"]:
        parts.append(f"1d{r['WeaponDamageDie']} {r['WeaponDamageType']}")
    for key, short in (("BonusStrength", "STR"), ("BonusIntelligence", "INT"), ("BonusConstitution", "CON"),
                       ("BonusDexterity", "DEX"), ("BonusCharisma", "CHA")):
        if r[key]:
            parts.append(f"{r[key]:+d} {short}")
    if r["EvadeBonus"]:
        parts.append(f"{r['EvadeBonus']:+d} Evade")
    if r["AttackRangeOverride"]:
        parts.append(f"reach {r['AttackRangeOverride'] / 100:.0f} m")
    if r.get("ResistanceLevel"):
        parts.append(f"{r['ResistanceLevel']} to {r['ResistanceDamageType']}")
    if r["bIsCursed"]:
        parts.append("CURSED")
    if r.get("SplashRadius"):
        parts.append(f"BLAST {r['SplashRadius'] / 100:.1f} m, handled with {r['HandlingSkillId']}"
                     + (f" (goes off in hand on 1-{r['FumbleOnNatural']})" if r["FumbleOnNatural"] > 1 else ""))
    if r.get("WeaponSkillId"):
        parts.append(f"skill: {r['WeaponSkillId']}")
    return ", ".join(parts)


def write_catalogue(all_boons):
    """Docs/loot-catalogue.md: the same content as the JSON, for reading and editing wording."""
    lines = ["# Loot catalogue", "",
             "Generated by `Tools/gen_loot_content.py`. Edit the wording there and re-run; this file and the JSON follow.", ""]
    for tier in ("Bronze", "Silver", "Gold", "Platinum", "Legendary", "Celestial"):
        lines += [f"## {tier}", "", "| Item | Slot | Stats | Description |", "|---|---|---|---|"]
        for r in NEW_GEAR + SKILL_GEAR:
            if r["Rarity"] == tier:
                lines.append(f"| **{r['DisplayName']}** | {r['Slot']} | {stats_line(r)} | {r['Description']} |")
        old = [n for n, t in EXISTING_RARITY.items() if t == tier]
        if old:
            lines += ["", "Existing gear now rated " + tier + ": " + ", ".join(old) + "."]
        lines.append("")
    lines += ["## Boons", "", "| Boon | Effect | Description |", "|---|---|---|"]
    for b in all_boons:
        fx = []
        for e in b["Effects"]:
            if e.get("InstantHealthChange"):
                fx.append(f"+{e['InstantHealthChange']} HP")
            if e.get("ModifierAmount"):
                fx.append(f"{e['ModifierAmount']:+d} {e['ModifiedAbility'][:3].upper()} for {e['ModifierDurationSeconds'] / 60:.0f} min")
        lines.append(f"| **{b['DisplayName']}** | {', '.join(fx)} | {b['Description']} |")
    lines += ["", "## What each box holds", ""]
    for t in BOX_TABLES:
        coins = t["ItemEntries"][0]
        gear_list = ", ".join(f"{e['EquipmentItemId']} {e['Chance']:.0%}" for e in t["ItemEntries"][1:])
        boon_list = ", ".join(f"{b['BoonId']} {b['Chance']:.0%}" for b in t["BoonEntries"])
        lines += [f"**{t['Name']}**: {coins['MinGold']}-{coins['MaxGold']} gold, always. Gear: {gear_list}. Boons: {boon_list}.", ""]
    (DOCS / "loot-catalogue.md").write_text(chr(10).join(lines), encoding="utf-8")


def main():
    problems = []

    # Every mesh must exist, or the item is invisible in a hand and on the floor.
    for row in NEW_GEAR + SKILL_GEAR:
        if row["Mesh"]:
            package = row["Mesh"].split(".")[0]
            if not (CONTENT / (package[len("/Game/"):] + ".uasset")).exists():
                problems.append(f"{row['ItemId']}: mesh not found {package}")

    ids = [r["ItemId"] for r in NEW_GEAR + SKILL_GEAR]
    if len(ids) != len(set(ids)):
        problems.append("duplicate new item ids")
    clash = set(ids) & set(EXISTING_RARITY)
    if clash:
        problems.append(f"new ids clash with existing gear: {clash}")

    old_boons = json.loads((DOCS / "DT_DCCBoons.json").read_text(encoding="utf-8"))
    old_boons = [b for b in old_boons if b["Name"] not in {n["Name"] for n in NEW_BOONS}]
    all_boons = old_boons + NEW_BOONS
    boon_ids = {b["Name"] for b in all_boons}
    gear_ids = set(ids) | set(EXISTING_RARITY)
    for t in BOX_TABLES:
        for e in t["ItemEntries"][1:]:
            if e["EquipmentItemId"] not in gear_ids:
                problems.append(f"{t['Name']} box lists unknown gear {e['EquipmentItemId']}")
        for b in t["BoonEntries"]:
            if b["BoonId"] not in boon_ids:
                problems.append(f"{t['Name']} box lists unknown boon {b['BoonId']}")

    if problems:
        print("NOT WRITTEN:\n  " + "\n  ".join(problems))
        sys.exit(1)

    (DOCS / "DT_DCCEquipment_LootBatch1.json").write_text(json.dumps(NEW_GEAR, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (DOCS / "DT_DCCEquipment_SkillGear.json").write_text(json.dumps(SKILL_GEAR, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    # Fields to set on rows that already exist in the table (read, change these, write back whole).
    patch = {item: {"Rarity": tier} for item, tier in EXISTING_RARITY.items()}
    for item, skill in EXISTING_SKILLS.items():
        patch.setdefault(item, {})["WeaponSkillId"] = skill
    (DOCS / "DT_DCCEquipment_Patch.json").write_text(json.dumps(patch, indent=2) + "\n", encoding="utf-8")
    old_drinks = json.loads((DOCS / "DT_DCCConsumables.json").read_text(encoding="utf-8"))
    old_drinks = [d for d in old_drinks if d["Name"] not in {f["Name"] for f in FIRST_AID}]
    (DOCS / "DT_DCCConsumables.json").write_text(json.dumps(old_drinks + FIRST_AID, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    (DOCS / "DT_DCCSkills_BookSkills.json").write_text(json.dumps(BOOK_SKILLS, indent=1) + "\n", encoding="utf-8")
    (DOCS / "DT_DCCBoons.json").write_text(json.dumps(all_boons, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    (DOCS / "DT_DCCLootBoxTables.json").write_text(json.dumps(BOX_TABLES, indent=1) + "\n", encoding="utf-8")

    write_catalogue(all_boons)

    by_tier = {}
    for r in NEW_GEAR:
        by_tier[r["Rarity"]] = by_tier.get(r["Rarity"], 0) + 1
    print(f"{len(SKILL_GEAR)} skill gear, {len(FIRST_AID)} first aid kits, {len(BOOK_SKILLS)} book skills; "
          f"{len(NEW_GEAR)} batch-1 gear {by_tier}, {len(NEW_BOONS)} new boons ({len(all_boons)} total), "
          f"{len(EXISTING_RARITY)} existing items given a rarity")
    for t in BOX_TABLES:
        expected = sum(e["Chance"] for e in t["ItemEntries"][1:])
        boons = sum(b["Chance"] for b in t["BoonEntries"])
        print(f"  {t['Name']:9} {len(t['ItemEntries']) - 1:2} gear entries, ~{expected:.1f} gear + ~{boons:.1f} boons + gold per box")


if __name__ == "__main__":
    main()
