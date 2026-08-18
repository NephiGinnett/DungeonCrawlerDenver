# Roadmap to a playable five-player session

**Written 2026-08-16**, against a clean Development Editor build with the whole
animation/HUD/escort batch deployed.

Read this before picking up new feature work. It exists because the obvious next
task is usually not the blocking one.

---

## The core finding

**The gap is not gameplay systems.** Stats on the real DCC ruleset, skills with
rank-and-advance, quests, achievements, XP, gold, loot, equipment, boss phases and
the pause/queue combat loop are all built, server-authoritative, and compiling. That
is the expensive, hard-to-retrofit half and it is done.

What is missing is what asset packs are genuinely good at: **animation, VFX, audio,
and level geometry** — plus one piece of networking code.

### Buy / build / avoid

| | |
|---|---|
| **Keep building in C++** | Anything touching the DCC ruleset, replication, or the session. Nobody sells it and it is the differentiator. |
| **Buy instead of building** | Animation, VFX, audio, level-building tooling. |
| **Actively avoid** | RPG / inventory / quest / dialogue kits, and "dungeon crawler template" projects. They assume they own the character, HUD and GameMode — all three already written here. Adopting one means deleting working code to bend a generic fantasy system toward a ruleset it was never designed for. |

---

## Priority ladder

Ordered by what blocks what. Each unblocks the next.

### P0 — Let five people actually connect

**The only true blocker.** The game has never run with more than two players in PIE,
and there is no join path at all: no direct-IP flow, and `GameMode::PostLogin` does
not yet choose a PlayerStart in an async-login room. `ADCCRoom::AllowsAsyncLogin()`
already returns true for `SafeRoom` — the decision is made, the plumbing is not.

Code, not assets. Listen server + direct IP over Tailscale/ZeroTier, per
`Docs/multiplayer-plan.md`. Until this exists, no group playtest can happen at all.

### P1 — One IK Retargeter, Mannequin → `SK_DCCCrawler_Skeleton`

The highest-leverage hour on this list. Because all four crawlers were consolidated
onto one skeleton, **one** retargeter setup makes every Mannequin-compatible
animation ever published work on all four characters. It is the bridge from 24 Meshy
clips to thousands, most of them free. Do this before buying any animation.

### P2 — Greybox rooms 5, 6 and 7, and connect the floor

The Bar, the Pantry and the Diving Pool do not exist. **The Diving Pool is the boss
arena, so the floor currently has no ending.** Block them out as grey shapes and only
art-pass what survives playtesting.

Reminder from `floor-01-design.md`: check every new bounding box with `find_actors`
before building in it. Room 02 was originally constructed inside cave rock by
skipping that step.

### P3 — Real animation blending

Every character runs a single looping clip through `AnimationSingleNode`. No
AnimBlueprint, no blend spaces, no montages, no hit reactions — `Montage` appears
nowhere in `Source/`. Combat resolves correctly and looks like a slideshow. Depends
on P1.

### P4 — Spell VFX and any audio at all

No purpose-built spell effect exists (Fire Fingers borrows a forge flame), and there
is **zero audio anywhere in the project** — no `SoundBase`, no cues, nothing in
`Source/`. Silence is the loudest prototype tell, and this is the cheapest
perceived-quality win on the list.

---

## Recommended resources

### Free — pursue these first

Everything here is permanently free, not free-this-month.

| Resource | Licence | Solves |
|---|---|---|
| [Paragon asset packs](https://www.unrealengine.com/en-US/paragon) | Free forever | **The ranged weapon answer.** 39 AAA heroes; Sparrow has a bow, Twinblast dual pistols, Murdock a rifle. Each ships weapon meshes *plus the animation set for using them*. Pairs directly with the IK Retargeter step (P1). |
| [Niagara Examples Pack](https://www.fab.com/listings/0e188eca-4e54-4fb2-a9ed-d8b8a565e600) | Free | **The weapon effects answer.** Epic's 50+ systems: bullet impacts, projectile trails, sparks, explosions, hit dissolves, weapon buff/debuff. Ships a gallery level demoing each. |
| [Infinity Blade collection](https://www.unrealengine.com/en-US/blog/free-infinity-blade-collection-marketplace-release/) | Free forever | Five packs, ~$4M of art. *Weapons* is melee-only so it won't fix ranged; *Effects* is a large fantasy VFX library, which is what Fire Fingers and Closing Time need. |
| [Game Animation Sample](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016) | Free | 500+ animations + motion-matching locomotion. Biggest single fix for single-clip playback with no blending. |
| [Sonniss GDC archive](https://sonniss.com/gameaudiogdc/) | Royalty-free, no attribution | **The project has zero audio.** 200GB+ of professional SFX, free every GDC, commercial use forever. |
| [Kenney assets](https://kenney.nl/assets) | CC0 | 30-piece weapon pack incl. firearms, plus CC0 UI audio. Stylised/low-poly — greybox and UI filler, not final art. |
| [Freesound](https://freesound.org/) | Mixed (filter CC0) | The specific one-offs: a jukebox starting, a balloon inflating, a till drawer. |
| [Quixel Megascans via Bridge](https://dev.epicgames.com/documentation/en-us/unreal-engine/free-epic-games-content-for-unreal-engine) | Free with UE | Photoreal surfaces/props to art-pass rooms 5-7 once the greybox layout is proven. |

**Start with Paragon + Niagara Examples.** Between them: ranged weapons, the
animations to fire them, and the impacts when they land — the whole combat-feel gap,
for nothing.

**Note on what does NOT exist locally:** searched all ~30 installed packs — there is
no bow, crossbow or firearm mesh anywhere. The Combat variant ships full *pistol
animations* with no pistol mesh, so Paragon's guns would slot straight into them.

### Paid — in buying order

| Resource | Solves |
|---|---|
| [Ultimate Greyboxing Kit](https://www.fab.com/listings/826d9096-26ec-46a4-92e6-7712499d78ec) | 73 modular shapes, 1m grid, proper collision. Buy first — directly attacks P2. |
| [Dash by Polygonflow](https://www.polygonflow.io/) | World-building co-pilot: scattering, physics placement, procedural layout without PCG complexity. The direct answer to "I need to be better at creating levels". Subscription. |
| [Hit Reaction Animation Pack](https://www.fab.com/listings/effa9c6e-1571-4a53-b235-a5411ddf5401) | 96 hit reactions, Mannequin-compatible. Getting hit currently produces no visible response, which is why fights read as two people ignoring each other. |
| [Combat Animation Pack (unarmed)](https://www.fab.com/listings/7c387f8e-afff-452c-a3cd-cdd60e44db3f) | 39 unarmed animations on Manny/Quinn. Crawlers fight with fists and furniture — unarmed is the right set, not swords. |
| [Magic Projectiles VFX](https://www.fab.com/listings/526c5afc-ee43-4d70-a4a7-483b32f2ed36) | Muzzle/travel/impact across several elements. Only if the free spell pack proves thin. |

Note: Claude cannot browse Zoe's Fab library (account-side, behind Epic auth). Only
packs already downloaded into `Content/` are visible — ask her to download.

---

## Artifact

A formatted version of this assessment:
https://claude.ai/code/artifact/80932c5e-1770-4ca5-b361-adc0389b431b
