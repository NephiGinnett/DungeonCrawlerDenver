"""Write the Champions' Ring ladder and the champion list."""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "Content", "DCC", "Ring")
KINDS = ["debtor", "clerk", "hound", "stalker", "brute"]
CHAMPS = {
    10: ("The Notary", "notary", "StaticBurst"),
    20: ("The Bull", "brute", "FireFingers"),
    30: ("The Auditor", "auditor", "FirstAid"),
    40: ("The Gatekeeper", "boss", "Whiteout"),
    50: ("The House", "gatekeeper", ""),
}

# Shown beside the stage number. A name of "Stage N" is hidden on purpose.
NAMES = {
    1: "Late Notice", 2: "The Window", 3: "Good Boy", 4: "After Hours", 5: "The Lien",
    6: "First and Last", 7: "Stamp Required", 8: "Off Leash", 9: "The Shadow File",
    11: "Balloon Payment", 12: "Unpaid", 13: "Please Hold", 14: "Kennel Fees", 15: "Quiet Hours",
    16: "The Repo", 17: "Past Due", 18: "The Counter", 19: "Heel",
    21: "Paper Trail", 22: "Overtime", 23: "Collections", 24: "Second Desk", 25: "Bad Dog",
    26: "Lights Out", 27: "The Mallet", 28: "Small Claims", 29: "Take a Number",
    31: "The Yard Dog", 32: "Still There", 33: "Charge It", 34: "Final Notice", 35: "Receipts",
    36: "Loose Dog", 37: "The Stairwell", 38: "Heavy Hands", 39: "Interest",
    41: "The Window Again", 42: "Night Shift", 43: "No Witnesses", 44: "The Enforcer", 45: "Garnished",
    46: "Closed Account", 47: "Last Dog", 48: "The Hallway", 49: "Breaker",
}


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    stages = []
    champs = []
    for s in range(1, 51):
        hp = int(round(80 * (1.12 ** (s - 1))))
        if s in CHAMPS:
            name, kind, skill = CHAMPS[s]
            stages.append({"stage": s, "name": name, "kind": kind, "hp": hp, "count": 1, "skill": skill})
            champs.append({"stage": s, "name": name, "kind": kind, "skill": skill})
        else:
            kind = KINDS[(s - 1) % len(KINDS)]
            count = 1 + (s - 1) // 8
            stages.append({
                "stage": s,
                "name": NAMES[s],
                "kind": kind,
                "hp": max(20, hp // count),
                "count": count,
                "skill": "",
            })
    with open(os.path.join(OUT_DIR, "ladder.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump({"stages": stages}, f, indent=2)
        f.write("\n")
    with open(os.path.join(OUT_DIR, "champions.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump({"champions": champs}, f, indent=2)
        f.write("\n")
    print("wrote ladder and champions,", len(stages), "stages")


if __name__ == "__main__":
    main()
