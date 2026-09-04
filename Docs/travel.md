# Getting Around — travel gates and the rover

**Started 2026-09-02.** The map had grown two large builds (the Front Range mine, the town of
Tailings) that were reachable only by flying the editor camera to them. This is how a player
gets there.

---

## Right now: travel gates (live, no build needed)

Four `ADCCOathgate`s in `DCC/TravelGates`, paired and **verified by destination read-back**:

| From | To |
|---|---|
| Spawn hub `(600, -600)` | Mine chamber `(16200, 16000)` |
| Mine `(16600, 16000)` | Tailings `(26000, 18000)` |

...and the return trip on each, since `PairedGate` is symmetric.

**`EnemyPool` was cleared on all four.** The class ships with a three-entry default pool, so a
freshly placed gate rolls an encounter on arrival — fine for the DM testing tool it was built
as, wrong for transport. Worth knowing before placing any more.

Gate positions were chosen by tracing for ground rather than by arithmetic; two of the four
first-choice spots turned out to have no floor under them at all.

---

## `ADCCRover` — a drivable car (written, compile-checked, NOT yet deployed)

`Vehicles/DCCRover.h`. Walk up, **E** to get in, **WASD** to drive, **E** to get out.

### Why it is not a Chaos vehicle

`ChaosVehiclesPlugin` **is** enabled — but a Chaos wheeled vehicle needs a **skeletal** mesh
rigged with wheel bones plus per-wheel setup classes, and this project has no such asset. All
267 vehicle-ish assets under `Content/` were checked: every car (the three Synthwave sets) is a
**static prop with no skeleton**, and there is not one `SK_` car among them. The plugin being
enabled gives capability, not content.

So the rover is an arcade pawn: static body, `UFloatingPawnMovement`, and a downward trace that
keeps it on the ground. It drives, it carries a crawler, and it needs no art anybody has to make
first. **If a rigged vehicle ever lands in `Content/`**, swapping this for a real
`AWheeledVehiclePawn` is contained — the enter/exit handshake is the part worth keeping.

Deliberately **not** physics-simulated: a simulated car on a floor built from scaled cubes and
3000-unit cliff meshes would spend most of its life on its roof, and the point here is transport.

### Two things that would have silently broken it

- **This project has no `AxisMappings`.** The player pawn runs on Enhanced Input mapping
  contexts owned by the character, and `Config/DefaultInput.ini` defines no axes at all. A
  `BindAxis("MoveForward")` on the rover would have compiled, deployed, and **never fired** —
  a car that looks drivable and does nothing. It binds raw keys instead, which also means it
  works the instant it is possessed without an IMC being pushed for it.
- **The driver is attached and hidden, never destroyed or teleported away**, so inventory,
  stats, quest state and anything being carried simply continue to exist. On exit they are
  placed *beside* the rover rather than inside it, or they materialise intersecting the body.

### ✅ Deployed and placed 2026-09-02

One rover, `Rover_SpawnHub`, in the intro arena at **(800, -800, 95)**, folder `DCC/TravelGates`.
The Synthwave body resolved from the constructor and suits the arena's neon far better than a
realistic car would have.

**`RideHeight` must exceed the Collision box half-height (90).** It was authored at 60, which
parks the box bottom — and therefore the body — **30 units under the floor**. Caught by measuring
the actor against a ground trace rather than by looking at it, since half-sunk reads as "sitting
on the ground" at a glance. Now 95 on both the placed instance and the class default.

The class-default fix needs the next build; **until then any NEWLY placed rover will sink** and
wants its `RideHeight` set to 95 by hand. The existing one is already correct.

A ground trace taken *after* placing the rover reports the rover, not the floor — it returned
z=180 (the box top) where the real ground is z=0. Trace before placing, or ignore the actor.

---

## Still open

- **No roads.** Gates and the rover both bypass the question of what the ground between
  places looks like. Whether Floor 1 wants a real connected landscape is a separate design call.
- The rover is single-occupant. Five players means five rovers, or a passenger seat.
- Nothing stops a player driving into the mine chamber, which is not sized for a car.

---

## Driving model (v2, 2026-09-02) — written, needs a build

First pass drove but had no collision, one flat speed, and no reason to be good at it.

| Control | Does |
|---|---|
| **W / S** | Throttle, ramped — `SecondsToReachCruise` 3.5s from standstill to `MaxCruiseSpeed` 4200 |
| **A / D** | Steer. Scales with speed, so a parked rover cannot pirouette |
| **Shift** | Drift — turn rate x2.1, and `DriftSlide` keeps 55% of the OLD heading so the car slides wide |
| **Space** | Boost to 7000 for 2s, 6s cooldown. Overrides the ramp outright so it reads as a shove |
| **Mouse X** | Swings the chase camera and eases back to centre — look into a corner without steering into it |
| **E** | Out |

### Collision

**The first version blocked nothing.** `Collision` responded `Overlap` to every channel, so the
movement sweep had nothing to stop against and the rover drove through walls. It now blocks
world geometry and overlaps only `ECC_Pawn`.

`NotifyHit` then splits by size, via `IsLargerThanRover` comparing horizontal footprints:

- **Bigger than the rover** — velocity is replaced with a gentle `BounceBackSpeed` 700 push along
  the hit normal, the speed ramp resets, boost cancels, and controls are ignored for
  `CrashRecoverySeconds` 0.55. Reads as an impact rather than as the car sticking to a wall with
  the throttle buried.
- **Smaller** — keeps 60% of its speed and carries on. Footprint rather than height, because a
  tall thin lamp post should not stop a car and a long low wall should.

### Piloting

A real skill (`DT_DCCSkills` row `Piloting`, passive, Dexterity), advancing through the project's
own **mark-and-advance** system rather than an XP pool — a mark makes it eligible for the next
1d20-vs-Rank advancement roll.

Every `PilotingTickSeconds` 5 of *actual movement* (above `PilotingMinSpeed` 300 — parking on the
throttle against a wall is not practice):

- **Known already** → `MarkSkillUsed`.
- **Not known** → `RecordUntrainedAttempt`, which makes it a candidate to be **learned outright**.
  So the first few drives teach you to drive.
- Character XP: 5, **+8 if the interval included a drift** — and **a crash wipes that bonus**,
  which is the entire risk side of drifting for progress.

`MaxSpeed` was removed; `MaxCruiseSpeed` replaces it.

---

## Ramps, flight and the trail (v3, 2026-09-02) — written, needs a build

### Ramp climbing — the actual fix

The old `SnapToGround` eased Z toward the traced ground with a single
`FInterpTo` at speed 8, in **both** directions. On a rising surface that is fatal: at cruise
the rover covers ~70 units a frame, the eased Z lags behind the slope, the nose buries into
the ramp face, and the (now blocking) collision box stops the car dead at the foot of it.

`UpdateGroundAndAir` splits the two cases:

- **Ground rising → follow it IMMEDIATELY**, no easing. This is the whole fix.
- **Ground falling away → still eased**, so cresting a rise does not drop the car like a stone.

It also now distinguishes *airborne* from *grounded* (`GroundedTolerance` 45), applies real
`Gravity` 2800 when in the air, and lands cleanly. `AlignToSurface` pitches and rolls the body
to the surface normal — but **only up to `MaxClimbAngleDegrees` 50**, above which it levels out
and lets the collision box refuse. That is what stops "climbs ramps" quietly becoming "climbs
buildings".

### Flight

**Space is context-sensitive.** On the ground it boosts. In the air, once
`FallenDistance >= FlightUnlockFallDistance` (450) and boost is available, the same key engages
flight. One button, because reaching for boost again at the top of an arc is what a player will
do anyway.

Requiring the *fall* rather than just being airborne is deliberate: launching off a ramp and
flying instantly would make the ramp pointless. You commit to the drop first.

While flying: `FlightSpeed` 5200, **Space** climbs, **Left Ctrl** descends, no ground following.
It ends on `FlightDurationSeconds` 12, or on touching down while not climbing — so it resolves
by landing rather than by remembering another key. Engaging it consumes the boost and starts the
cooldown, so it cannot be chained straight into another boost on landing.

### Trail

`Cyberpunk_City/FX/FX_Car/NS_Flying_car` on a `UNiagaraComponent` at the rear, activated while
boosting *or* flying. Chosen over `NS_sparks`/`NS_fire` because it is the Cyberpunk pack's own
flying-vehicle effect and reads as engine wash rather than as smoke borrowed from a campfire.
Niagara was already a module dependency.

### The playground ramp — measured 2026-09-02

Found at last. It is **not** near the origin: the playground sits at **x ~ 60000-68000**, which
is why two earlier scans of the origin region found nothing. It is also rolled, not pitched, so
a search for pitch missed it a third time.

| Thing | Location | Detail |
|---|---|---|
| Ramp | `Synthwave_city/SM_bridge` at **(64970, 13520, 220)** | roll **-20 deg**, scale 14.3 |
| Arch | `Synthwave_city/SM_arch` at **(68970, 13460, 1240)** | roll -20 deg, scale 1.4 |
| Humanoid cactus | (67420, 3400, 2590) | `/Game/Fab/Humanoid_Cactus/` |

**Traced gradient: a clean, consistent 20 degrees** (+182 z per 500 y). Well inside
`MaxClimbAngleDegrees` 50, so no tuning needed.

**The onramp is at approximately (67000, 13600).** This matters: the ramp is a huge tilted slab
that passes *through* the ground, so most of its perimeter is a cliff, not an entrance. Tracing
across X at y=15000 showed ground at z=50 out to x=64000 and deck at z=759 from x=66000 - a
~700-unit wall. Approach from **lower Y at x ~ 66000-68000** and the deck rises out of the
ground cleanly; approach from anywhere else and it is a kerb taller than the car.

**The old ground code would have failed this ramp**, and that is the concrete case the v3 fix
was for: 20 degrees at cruise is ~70 units of rise per frame, which the old symmetric
`FInterpTo` could not keep up with, so the nose would bury and the blocking box would stop the
car at the foot.

A driveable `ADCCRover` is now placed at **(67000, 12200, 328)**, facing +Y at the approach, in
the `Playground` folder. (The car visible in Zoe's screenshot is a static prop, not an
`ADCCRover` - there was only ever one real rover in the level, at the spawn hub.)

### Still NOT verified

**None of the v3 behaviour has been driven.** It is compile-checked only, and was not deployed
at the time of writing. There is no input-injection tool in this MCP toolset, so a rover cannot
be driven from here at all - the ramp numbers above are measured from the geometry, but the
handling on it is reasoned.

---

## v4, 2026-09-02 — why it could not leave the arena, plus passengers

### The bug

`NotifyHit` asked `IsLargerThanRover(Other)`, which compared the **whole actor's** bounds. The
arena floor and walls are single large actors, so clipping a 20-unit kerb belonging to one was
judged identical to hitting a mountain: full bounce-back plus 0.55s of dead controls. Every lip
in the room read as a wall, and the rover was effectively fenced in.

**It now measures the obstruction's actual height at the contact point** (`MeasureObstacleTop`),
tracing down just *beyond* the hit surface - the nudge past the contact matters, or the probe
re-hits the same face and every step measures as zero.

- Height <= `MaxStepHeight` (260): a kerb. Handed to the step-over logic, speed untouched.
- Taller: a real wall. Bounce and crash recovery, as before.

### Hovering over things

- `RideHeight` **95 -> 140**, so it sits higher and small debris passes underneath.
- `ProbeGroundAhead` samples the ground `ForwardProbeDistance` (420) in front, so a step is
  climbed **before** the collision box reaches it. Reacting only on contact means the sweep has
  already stopped the car by the time anything lifts.
- Rises are **rate-limited** by `StepClimbSpeed` (900 cm/s) rather than snapped - hopping onto a
  ledge instantly looks like teleporting, and the ask was for it to crawl.
- `StepClimbPitchDegrees` (9) noses it up while climbing, so it reads as heaving itself over the
  lip rather than levitating flatly across it.

### Passengers

Allies board **automatically** when the driver gets in and they are within
`PassengerBoardRadius` (1400), up to `MaxPassengers` (4). Seating four AI companions by hand
every time is a chore, not a mechanic.

- Filter is `IsCrawler() && !IsPlayerControlled()`, which derives from `bIsAlly` - so the car
  cannot accidentally load up with Debt Eaters.
- **X** ejects them, fanned out to one side so four allies do not materialise inside each other.
- They also all get out when the driver does: a rover parked with four invisible allies still
  attached to it is a very hard bug to notice later.

Still not driven from here - compile-checked only.
