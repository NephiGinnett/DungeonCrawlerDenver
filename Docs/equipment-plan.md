# Equipment plan (2026-11-16)

What a crawler can own or carry in each game, what pays for it, and where the gaps are. Written for the arcade
direction: every game should give you something to come back for, and the games should point at each other.

## Three currencies

| Currency | Earned in | Spent at |
|---|---|---|
| **Tickets** | every game (Fourteener points 1:1, Dead Air views / 100, Scroller ranks, Neon Mile 1 per 25 m, Gate Run, the starter kit's 25), bounties | Prize Counter (cosmetics, booth guns, spray can), Fort Tiner's Trading Post |
| **Scrap** | Repo Rally, Neon Mile | Chop Shop (truck upgrades) |
| **Mine credits** | Dead Air (footage that made it up, notes read, stories finished) | Mine Outfitters |

Price ladder today: cosmetics 40-400, booth guns 80-120, the spray can 60, Trading Post supplies 2-22. A good round
pays roughly 30-80 tickets, so a cosmetic is one to eight rounds away. That is about right; keep new prices inside it.

## What each game gives you

| Game | Keeps (permanent) | Round only | Trophies |
|---|---|---|---|
| **The Den** | cosmetics, booth guns, spray can (Prize Counter) | | sells trophies |
| **Fourteener** | the belt grappling hook's levels (Trading Post, tickets) | Trading Post kit (tools, food, boosts), chest tools | artifacts, if carried down alive |
| **Dead Air** | Outfitters levels: air tanks, lamp, lightsticks, hard hat, gas meter | lamp, lightsticks | |
| **Repo Rally** | Chop Shop levels: skid plating, long line, pit crew | the rally tool kit, roles | seized goods |
| **World Dungeon** | starter kit (one of five), equipment, skills | | |

## Gaps

1. ~~**Fourteener has no permanent progression.**~~ Started 2026-11-16: the belt grappling hook levels up at the Trading Post. Everything you buy at the Trading Post is gone after the round.
   Dead Air and Repo Rally both have a levelled shop. Fourteener should too.
2. **The mine and the mountain are one place but share no gear.** Dead Air is *inside* Fourteener; nothing you
   earn in one helps in the other.
3. **Booth guns only matter in the World Dungeon.** In the arcade, hands start empty, so a gun bought at the counter
   does nothing until you go down a gate.
4. **Starter kits are invisible in the arcade** for the same reason. That is fine; worth saying in the kit picker.

## Plan

- **Mountaineering levels at the Trading Post** (fixes gap 1), for tickets, kept forever, three levels each:
  *Grip* (climbing stamina drains 8% slower a level), *Pack* (one more Trading Post slot a level),
  *Weathered* (hunger grows 10% slower a level). Same shop shape as the Outfitters, so no new UI.
- **One shared item between mine and mountain** (fixes gap 2): the Outfitters' *Hard Hat* also softens Fourteener's
  rockfall, and the Trading Post's *Chalk Bag* also steadies your camera in Dead Air. The description says so in
  both shops, so each game advertises the other.
- **Bounties already tie the games together** (`Arcade/DCCBounties`): High Camp, Souvenir and Proof send Dead Air
  players up the mountain; Going Viral, Property Defacement and Shift's Over send climbers into the mine.
- Leave booth guns as World Dungeon gear (gap 3), and say so on the counter: "for the gates".

## Balancing done in this pass

- Slag dogs: a pack of three dealt about 15 damage a second (a full health bar in eight seconds). Now about 7:
  cooldown 0.9 s to 1.3 s, Strength 3 to 2.
- Tommyknockers: rocks every 18-26 s, not 14-22 s, since the mine drops its own rocks too.
- Not yet measured in a real round: the shift boss (d10, slow), the lamp-eater (only hunts lit lamps).
