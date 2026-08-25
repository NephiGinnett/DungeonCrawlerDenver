# Hotbar icons + a GTA-style radial wheel — implementation plan

Scoping pass only, 2026-08-23 — no code yet. Blocked on Zoe downloading assets from her own
Fab library (same pattern as every other asset pass: she pulls the raw files into Downloads
from fab.com in her browser, then whoever has MCP next imports them). This doc is written so
that session can start building the moment the files land in `Content/`, not re-derive this.

## What already exists (checked against the code, not assumed)

- **A real hotbar already exists.** `ADCCHUD::DrawHotbar` — five fixed slots, keys 1-5
  (`SelectSkillSlot1..5` → `ADCCCharacterBase::SelectSkill`), shows name/Rank/mana/live
  cooldown-drain. **Text and rects only — no icons at all today**, and slots map directly to
  `KnownSkills[0..4]` by array order, not an authored slot assignment. Good bones; the icon
  work is additive, no redesign needed.
- **`FDCCSkillDef` has no icon field of any kind.** Needed regardless of which UI direction
  wins.
- **The scroll wheel is already claimed**, and already multiplexed: `InteractionScrollUp/Down`
  check Vendor menu → Loot menu → Interaction menu, in that order, falling through to the
  interaction cycle as the default. A GTA-style radial that wants scroll-to-cycle has to slot
  in as a fourth branch in that same chain (gated on "is the radial open"), not fight the
  existing bindings — this project has already established the pattern for exactly this kind
  of multiplexing.
- **The pause/queue system is a natural home for a radial's "deliberate choice" moment.**
  `ADCCGameState::SetCombatPaused` already slows time (not a hard stop) and lets a player queue
  one action, resolved in initiative order on resume. Opening a radial full-kit picker is a
  plausible trigger for this same pause, rather than a separate mechanic bolted on beside it.
- **Enhanced Input is a listed dependency but unused.** Every existing binding, including all
  of the above, is the legacy `BindKey` system. Not a blocker — a hold-to-open radial is
  perfectly buildable on `BindKey` with `IE_Pressed`/`IE_Released` — just worth knowing there's
  no Enhanced Input Action asset to hook into if the eventual plugin evaluation expects one.

## Recommendation: build both, they serve different moments

Not an either/or. Proposed split, **flagged for Zoe's sign-off since the pause-tie-in changes
live UX flow**:

1. **Upgrade the existing hotbar with real icons.** Cheap, low-risk, no new interaction model.
   `AHUD::DrawTexture` is the same family of Canvas primitive `DrawRect`/`DrawText` already use
   throughout this HUD — this is additive, not a new subsystem.
2. **Add a new radial wheel as the "pick from your whole kit" moment**, separate from the
   fast always-on hotbar. Proposed trigger: hold **Q** (currently unbound) to open a circular
   fan of every known Skill (not just the 5 hotbar-bound ones) centred on the cursor, select by
   mouse angle, release to confirm and arm it (same `SelectSkill` call the hotbar already uses)
   — genuinely GTA-wheel-shaped. **Proposed, not decided:** opening it calls
   `SetCombatPaused(true)` automatically, so a full-kit decision never happens under time
   pressure, closing it (confirm or cancel) does not force-unpause — reuses the tactical-pause
   system exactly as built rather than inventing a second one. This is the one part of this
   plan that changes moment-to-moment feel, so it's a proposal to confirm, not something to
   silently ship.

## On the purchased "Radial Selector" plugin — evaluate once it lands, don't pre-commit to it

Marketplace "radial selector" plugins are, near-universally, built on UMG/Slate. This project
has deliberately avoided UMG everywhere else in its own HUD for one specific, repeatedly-stated
reason: a UMG asset needs the interactive UMG toolset in a *live* editor to author, which this
project's MCP-driven workflow can't always reach, whereas the existing Canvas HUD "works the
moment it compiles." Adopting the plugin wholesale would likely mean either carving out a UMG
exception just for this one feature (reintroducing the exact authoring problem the rest of the
HUD was built to avoid), or extracting only its interaction *ideas* (angle math, hold-to-open,
wedge highlighting) and reimplementing them natively in Canvas — which is genuinely not a big
lift; a radial layout is a handful of `FMath::Atan2` calls and `DrawTexture` in a loop.

**Commitment: actually open and inspect the plugin once it's imported before ruling on this**,
rather than pre-judging from its marketplace description. If it turns out to be more
data/Blueprint-driven than expected, or interoperates with Canvas cleanly, that changes the
call. Written down now so the reasoning doesn't have to be redone.

## Icon mapping, once the packs land

Both packs are described as transparent PNGs, categorised — a plain texture import, no
Blender/conversion pipeline needed (unlike the Meshy character pipeline). Plan:

1. Import each pack's PNGs as `UTexture2D` assets under `/Game/DCC/UI/Icons/Rogue/` and
   `/Game/DCC/UI/Icons/Psychic/`.
2. Add `TSoftObjectPtr<UTexture2D> Icon` to `FDCCSkillDef` — soft, matching how
   `CastEffect`/`ImpactEffect` already avoid dragging every Skill's assets into memory just
   because the table loaded.
3. Hand-assign per Skill row by archetype/flavour: Debuff and stealth/poison-coded Skills draw
   from the Rogue pack, Buff and mental-effect-coded Skills from the Psychic pack. Whatever
   doesn't fit either theme keeps the current text-only rendering as a graceful fallback rather
   than forcing a mismatched icon onto it.

## What Zoe needs to download (all three, from her own Fab library)

- **"500 Rogue Skills, Stealth & Poison Ability Icons"**
- **"500 FREE Psion & Mindweaver Psychic Icons"**
- **"Radial Selector" (Cesar Augusto)** — for evaluation per the section above, even if the
  final build ends up native.

Same handoff as every other asset pass: she downloads to her own Downloads folder from
fab.com; whoever has MCP next picks them up from there into `Content/`.
