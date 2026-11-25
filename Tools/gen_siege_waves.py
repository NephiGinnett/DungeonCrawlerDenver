"""Write Content/DCC/Siege/waves.json and print the wall-fall estimate.

The incoming curve is hand-aimed. A single exponential cannot stall a solo
party at wave 13 and a full party near wave 30 with one 2000-point wall.
DCC.Fight was not run. The DPS numbers are estimates: solo 21, two players 44,
five players 109 (a level-5 crawler around 18-24, an ally around 7-9, times
about 0.65 for time spent walking and missing).
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Content", "DCC", "Siege", "waves.json")

KINDS = ["debtor", "clerk", "hound", "stalker", "brute"]
BOSSES = {10: "The Collector", 20: "The Bull", 30: "The Auditor", 40: "The House"}


def incoming(w):
    if w <= 12:
        return 8 + w * 0.9
    if w == 13:
        return 130
    if w <= 19:
        return 36 + (w - 14) * 2
    if w <= 29:
        return 55 + (w - 20) * 6
    return 120 + (w - 30) * 8


def wave(w):
    total = max(1, int(round(incoming(w) * 20)))
    n = 5 + w
    bodies = []
    if w % 10 == 0:
        boss_hp = max(40, int(total * 0.4))
        rest = max(0, total - boss_hp)
        fodder = max(1, n - 1)
        each = max(8, rest // fodder)
        bodies.append({"kind": "boss", "hp": boss_hp, "count": 1, "name": BOSSES.get(w, "The House")})
        bodies.append({"kind": KINDS[(w // 5) % len(KINDS)], "hp": each, "count": fodder, "name": "Debtor"})
    else:
        each = max(8, total // n)
        kind = KINDS[(w - 1) % len(KINDS)]
        if w % 5 == 0:
            big = min(total - 8 * (n - 1), each * 2)
            small_n = n - 1
            small = max(8, (total - big) // small_n)
            bodies.append({"kind": "brute", "hp": big, "count": 1, "name": "The Brute"})
            bodies.append({"kind": kind, "hp": small, "count": small_n, "name": kind[:1].upper() + kind[1:]})
        else:
            bodies.append({"kind": kind, "hp": each, "count": n, "name": kind[:1].upper() + kind[1:]})
    return {"wave": w, "speed": 320, "gap": 0.65, "bodies": bodies}


def fall(dps):
    wall = 2000
    for w in range(1, 80):
        overflow = max(0.0, incoming(w) - dps) * 20
        wall -= overflow
        if wall <= 0:
            return w, int(overflow)
    return 80, wall


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    data = {"wall": 2000, "waves": [wave(w) for w in range(1, 41)]}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    print("wrote", OUT)
    for label, dps in (("solo", 21), ("two", 44), ("five", 109)):
        w, soak = fall(dps)
        print(f"{label:5} dps {dps:3}  wall falls on wave {w}  (last overflow {soak})")
    print("DCC.Fight was not measured. DPS figures are estimates.")


if __name__ == "__main__":
    main()
