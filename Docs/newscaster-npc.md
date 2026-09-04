# The Newscaster NPC — design doc

Zoe's ask (2026-09-02): an NPC who looks like one of our alien races, styled as a newscaster,
with a genuinely expressive talking face — "the more expressive we can go, the better." This is
a **planning doc**; nothing here is built yet unless a later DISPATCH.md entry says otherwise.

---

## Concept

**"The Feed Anchor."** The System AI doesn't just narrate in text (see `dungeon-tone-bible.md`) —
it has a face now, and that face is a captured alien host reading the day's dungeon numbers like
a quarterly earnings call. Same voice already established for the System AI (never angry, never
threatening, *disappointed by low engagement numbers*), just given a body and a desk. This is a
strong fit for an existing gap: the System AI has been text-only everywhere so far, and Zoe's own
tone bible explicitly wants "an out-of-office reply" register — a newscaster is that register
made literal.

Placement isn't decided in this pass, but the natural fit is a monitor/screen prop somewhere
public (a SafeRoom, a lobby) that Zoe can drop the anchor's model behind, playing on a loop or
triggered by floor events (boss announced, achievement claimed, floor cleared) — narratively the
same beat as the "LEVEL BOSS" banner (`DCCHUD.cpp`, added 2026-08-30) but delivered in-world by a
character instead of a HUD overlay.

---

## Who plays the anchor: ParagonZinx

Checked rather than assumed, per this project's own standing rule (`CLAUDE.md`'s "inspect the
scene first" pattern) — I pulled the actual skeleton data before picking anything:

- **`/Game/ParagonZinx/Characters/Heroes/Zinx/Meshes/Zinx`**, confirmed **genuinely unused** —
  `model-assignments.md` lists it under "what the library still has spare."
- Reads as alien on sight, not just by asset name: the skeleton carries `ear_l`/`ear_r`, a `tail`
  bone, and a `hair_ponytail` chain — an upright, roughly human-scale humanoid (bounds ≈
  70×41×188cm) with clearly non-human features. Distinct silhouette from every crawler and every
  existing NPC in `model-assignments.md`, so nobody at the desk gets mistaken for a party member
  or a reused enemy — exactly the readability problem that page flags as this project's
  "actual problem, stated plainly" for its other placeholder NPCs.
- **Full bone-based facial rig already on the skeleton** — the actual finding that makes the
  facial animation plan below possible without new art. See the next section.

I did not exhaustively survey every Paragon/creature skeleton in the project for this — Zinx was
the first strong "alien + unused + has a real face rig" match and it's a good one. If it turns
out to clash with a scene Zoe's building around it, `ParagonMuriel` (partially used, Guildmaster)
and other unused creature skeletons are worth the same check before falling back to a mannequin.

---

## Facial animation — the actual finding, and the recommendation

**This project has no AnimBlueprint anywhere, confirmed and repeatedly noted in `CLAUDE.md`.**
That's the hard constraint every option below has to survive.

### What's actually on the Zinx skeleton (checked via `SkeletalMeshTools`)

```
get_morph_target_names → []          (zero morph targets / blend shapes)
get_bone_names → 174 bones, including:
  C_jaw, tongue
  C_lip_upper_mid, L/R_lip_upper_outer, L/R_lip_corner, C_lip_lower_mid, L/R_lip_lower_outer
  C_brow_mid, L/R_brow_mid, L/R_brow_outer
  L/R_eye, L/R_eye_lid_upper_mid, L/R_eye_lid_lower_mid
  head, faceAttach
```

So the honest starting point is the **opposite** of what I expected going in: this mesh has **no
blend shapes to drive**, but it has a **complete bone-based facial rig** — jaw, individual lip
segments (not just one mouth bone), brows, and eyes with separate eyelid bones. That's
Paragon-hero-grade facial bone coverage, sitting unused.

### The three options, honestly costed

**Option A — single jaw bone bob, driven by dialogue audio amplitude.**
Cheapest possible version: sample the currently-playing dialogue `USoundWave`'s amplitude each
frame (or a precomputed envelope), map it to a jaw-open rotation, done. Buildable in under an
hour. The problem: it only uses 1 of the ~15 available facial bones. On a rig this complete, it
would visibly waste the actual asset — the eyes would stay dead, the brows static, no lip shape
at all. Fine as a fallback if B turns out to fight the engine somehow, not recommended as the
first attempt given what's actually on this skeleton.

**Option B — full procedural bone puppet, driven the same way, using the whole rig. RECOMMENDED.**
Same audio-amplitude input as A, but distributed across the rig instead of one bone:

- **Jaw** (`C_jaw`): primary open/close, directly proportional to amplitude — the actual talking
  motion, same signal as A.
- **Lip corners** (`L/R_lip_corner`) and **lip mid bones**: a secondary, phase-shifted sine
  layered under the amplitude signal, so the mouth doesn't just hinge open like a puppet but
  visibly *shapes* — corners widen slightly on louder syllables, narrow between words. This is
  what turns "jaw bobbing" into something that reads as speech rather than a nutcracker.
  Two thresholds are all it needs. `Nutcracker` results are a real risk here — see the
  build-order note below.
- **Brows** (`C_brow_mid`, `L/R_brow_mid`/`_outer`): infrequent punctuation, not continuous —
  raise briefly on emphasis (loudest moment in a rolling window = "the point being made"), settle
  back. This is the single highest-value addition for "expressive" per unit of effort; a talking
  head with static brows reads as inert no matter how good the mouth is.
- **Eyes + eyelids** (`L/R_eye`, `L/R_eye_lid_upper/lower_mid`): a slow idle drift/saccade so the
  gaze isn't a dead stare, plus a randomized blink timer (roughly every 2-6 seconds, a fast
  close/open over ~120ms) via the eyelid bones. Cheap, and blinking alone does most of the work of
  making a static face look alive — well-documented in character animation generally, not
  specific to this rig.

**How to drive it without an AnimBlueprint:** `USkeletalMeshComponent::SetBoneTransformByName`
(component-space, additive over whatever the base pose/single-node animation is already playing)
is a real, public, engine-level function built for exactly this — layering a procedural override
onto individual bones at runtime with no Animation Blueprint graph involved at all. This project's
crawlers already run on `AnimationSingleNode` with no AnimBP (`CLAUDE.md`'s own HockeyWings note),
so this technique is proven-compatible with how every character here already animates. All of it
lives in one new component (`Dialogue/DCCTalkingFaceComponent` or similar) ticking on the anchor
character, reading whatever `USoundWave`/`USoundCue` is currently playing as its dialogue line.

Rough scope: one new C++ component (~150-250 lines), no new art, no new AnimBP, no morph target
authoring. The single real unknown is tuning — bone rotation limits and phase offsets that read
as "talking" instead of "chattering teeth" only reveal themselves once it's actually running in
PIE against real dialogue audio, so budget one iteration pass watching it play, not just one
write-and-ship.

**Option C — morph targets / blend shapes.**
The higher-fidelity industry-standard approach (visemes, real mouth shapes per phoneme) — but
this mesh has **zero** morph targets to drive, confirmed above, so Option C means authoring a
brand-new blend shape set from scratch (Blender sculpting, one shape per viseme, re-import),
which is a real production task on the order of the shared-skeleton rig-fitting work already
documented elsewhere in this project, not a quick add. This project has never authored a morph
target for any character. Worth doing eventually if the anchor becomes a recurring, camera-close
character the party spends a lot of time looking at — not worth it for a first pass when Option B
gets most of the visual payoff from an asset that already supports it for free.

### Recommendation

**Build Option B.** It uses the actual rig that's sitting on this mesh unused, needs no new art,
respects the no-AnimBlueprint constraint using an engine-native mechanism this project's animation
approach already relies on elsewhere, and gets brows + eyes + a shaped mouth instead of a single
hinging jaw. Treat Option A as the fallback only if `SetBoneTransformByName` fights the rig in
practice (unlikely, but the honest caveat is that this specific technique hasn't been tried in
this project before — everything else here is single-node animation or full skeletal takeover via
possession, not a per-bone procedural layer). Option C is the real upgrade path if the anchor
becomes a bigger, closer-camera fixture later.

---

## Visual design

Matches the HQ tower's established corporate-dystopia language (`DISPATCH.md`, 2026-08-30 HQ
furniture/NPC pass and the LEVEL BOSS banner) rather than inventing a new visual register — same
"deadpan office" materials already placed across that floor, not a separate sci-fi broadcast look.

- **Desk:** reuse the `Modern_House_V1` desk assets already placed in the HQ tower's Claims
  Processing / Reception rooms (same pack, already imported, already reads as "this building").
  A newscaster desk is the same furniture with a monitor and mic added, not a new prop hunt.
- **Microphone:** `/Game/Beach_house/Meshes/Smalle_props/SM_mic` — confirmed present in the
  project already, no import needed. Small enough to sit on a desk without a scale pass.
- **Screen / monitor:** `/Game/Modern_House_V1/Mesh/SM_TV` or `/Game/Modern_villa_kit/Meshes/TV/
  SM_tv` (both already imported) as the "ON AIR" monitor behind or beside the anchor, running
  whatever the floor's current headline is — reusing the same beat as the LEVEL BOSS banner
  (boss announced / floor cleared / achievement claimed) as on-screen chyron text rather than
  inventing a fourth notification surface.
- **"On air" framing:** an `ADCCHUD`-drawn overlay reusing `DrawSegmentedBar`'s existing Canvas
  block-drawing helper for a simple lower-third chyron bar (name + headline text), and a small
  pulsing red "● ON AIR" tag in a corner while the anchor is speaking — same drawing primitives
  already used for the LEVEL BOSS card and the enemy health bars, not a new UMG widget system.
- **Wardrobe:** Zinx's base skin is fine for a first pass (already imported, no material work
  needed); a press-badge/lanyard Trinket-slot prop, reusing the equipment system's existing
  rigid-attachment sockets, is the cheap "reads as a newscaster" costume tell if a later pass
  wants one, rather than reskinning the mesh.

---

## What this doc is not

Not a placement decision (which room, which floor), not a script (that's `dungeon-tone-bible.md`
territory once this exists), and not a commitment to build Option B tonight unless a build window
allows it. Flagging explicitly per Zoe's own ask: this was scoped as planning first, implementation
second only if time allows.
