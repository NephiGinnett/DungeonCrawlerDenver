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
                "name": f"Stage {s}",
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
