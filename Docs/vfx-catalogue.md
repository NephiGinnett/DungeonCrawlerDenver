# VFX catalogue

Generated 2026-11-25 from the six effect folders. Loop and size are read from the asset name.
The Niagara files embed both burst and loop module names, so the binary cannot be trusted for which one actually plays.
Cast auras are switched off after 1.4 s in code, impacts after 1.8 s, so a looping system cannot sit on a crawler.
Sizes were checked on the pose bench, 2026-11-25, shots in `Saved/TestShots/spell_fx2/` (scale 1, spawned on Zoe). A fire aura, a lightning buff and Level Up fill the room. A shockwave and the magic slash are about two body-heights, above the head. `NS_Explosion_Small` is a loose sparkle, not a fireball. Several systems were still compiling shaders on that pass (the Free Spells explosion among them), so those sizes are still a guess. There is no scale field on the skill row. Scales live in `DCCSpellBook.cpp` and are applied when the system is spawned. Ordinary casts sit near 0.3. The level-20 shows stay large.

Spline systems need a spline. They are listed and not used as a cast or an impact.
Projectile systems need something to fly. They are listed and not used as a point puff.
There is no point-spawn ice system. Frost is spline-only, so ice spells use the air aura and a shockwave.

Weapon hits (about 25 m, then nothing): fire uses `NS_Explosion_Small`, piercing uses `NS_Free_Magic_Slash`, blunt and fists use `NS_Impact_Concrete`.
Deaths that skip the ragdoll (Debt Eater, bone-maker monsters) use `NS_Explosion_Small` at a small scale.
The book-skill rows in `Tools/gen_loot_content.py` (Club, Axe, and the rest) are checks. They have no cast. A swing uses the hit puff.

## What each spell wears

| Spell | Cast | Impact | Scales (cast, impact) |
|---|---|---|---|
| StaticBurst | Aura_Lightning | Buff_Lightning | 0.28, 0.22 |
| BallLightning | Aura_Lightning | Explosion | 0.40, 0.80 |
| LienEnforcement | Circle | Shockwave | 0.40, 0.45 |
| MarkAndCollect | Aura_Soul | Shockwave | 0.30, 0.50 |
| FinalNotice | Circle | Shockwave | 0.40, 0.45 |
| HostileTakeover | Buff_Air | Shockwave | 0.32, 0.55 |
| TotalRepossession | Circle | Explosion | 1.00, 1.15 |
| SecondWind | Aura_Healing | Buff_Healing | 0.32, 0.30 |
| HoldTheLine | Circle | Shockwave | 0.45, 0.55 |
| AvalancheWarning | Aura_Air | Shockwave | 0.35, 0.65 |
| Whiteout | Aura_Air | Shockwave | 1.05, 1.20 |
| FeedbackLoop | Aura_Lightning | Buff_Lightning | 0.32, 0.28 |
| RadioSilence | Aura_Notes | Shockwave | 0.35, 0.60 |
| Interference | Aura_Lightning | Buff_Lightning | 0.30, 0.28 |
| StationIdentification | Aura_Notes | Explosion | 1.15, 1.25 |
| Timeout | Aura_Healing | Buff_Healing | 0.32, 0.30 |
| PenaltyFlag | Buff_Air | Shockwave | 0.28, 0.45 |
| TwoMinuteDrill | Aura_Healing | LevelUp | 0.35, 0.55 |
| HalftimeSpeech | LevelUp | Buff_Healing | 1.15, 1.10 |
| HotLap | Aura_Fire | Buff_Fire | 0.32, 0.28 |
| PhotoFinish | Buff_Air | Shockwave | 0.25, 0.40 |
| Slipstream | Aura_Air | Shockwave | 0.32, 0.55 |
| VictoryLap | Aura_Fire | Explosion | 1.05, 1.10 |
| Lockdown | Circle | Explosion | 1.00, 1.10 |
| CleanUpAisleFive | Aura_Air | Shockwave | 0.28, 0.50 |
| CustomerService | Aura_Healing | Buff_Healing | 0.32, 0.30 |
| GrandReopening | Buff_Fire | Explosion | 1.10, 1.20 |
| FireFingers | Aura_Fire | Buff_Fire | 0.35, 0.32 |
| FirstAid | Aura_Healing | Buff_Healing | 0.30, 0.28 |
| SoulCollector | Aura_Soul | Absorption_Purple | 0.35, 0.50 |
| FieldDressing | Aura_Healing | Buff_Healing | 0.28, 0.26 |
| HypeTheCrowd | Aura_Notes | Buff_Notes | 0.35, 0.32 |
| SummonInterns | Circle | LevelUp | 0.45, 0.45 |
| SummonRegulars | Circle | LevelUp | 0.50, 0.50 |

The level-20 shows (Total Repossession, Whiteout, Station Identification, Halftime Speech, Victory Lap, Lockdown, Grand Reopening) and Ball Lightning get the bigger scales.
The mesh spell show from 2026-11-22 still plays on top of these. This table is the Niagara.

## Every system

| Path | Look | Plays | Size |
|---|---|---|---|
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Air.NS_Free_Spells_Aura_Air` | wind aura | loops | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Fire.NS_Free_Spells_Aura_Fire` | fire aura | loops | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Healing.NS_Free_Spells_Aura_Healing` | green heal aura | loops | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Lightning.NS_Free_Spells_Aura_Lightning` | lightning aura | loops | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Mana.NS_Free_Spells_Aura_Mana` | blue mana aura | loops | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Notes.NS_Free_Spells_Aura_Notes` | music-note aura | loops | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Soul.NS_Free_Spells_Aura_Soul` | soul aura | loops | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Air.NS_Free_Spells_Buff_Air` | air buff burst | loops until stopped | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Fire.NS_Free_Spells_Buff_Fire` | fire buff burst | loops until stopped | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Healing.NS_Free_Spells_Buff_Healing` | heal buff burst | loops until stopped | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Lightning.NS_Free_Spells_Buff_Lightning` | lightning buff burst | loops until stopped | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Mana.NS_Free_Spells_Buff_Mana` | mana buff burst | loops until stopped | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Notes.NS_Free_Spells_Buff_Notes` | note buff burst | loops until stopped | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Soul.NS_Free_Spells_Buff_Soul` | soul buff burst | loops until stopped | body-sized |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle.NS_Free_Spells_Circle` | magic circle on the ground | loops until stopped | a few metres |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion.NS_Free_Spells_Explosion` | spell explosion | plays once | several metres at scale 1 |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_LevelUp.NS_Free_Spells_LevelUp` | level-up column | plays once | a few metres |
| `/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave.NS_Free_Spells_Shockwave` | ground shockwave | plays once | a few metres |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Area1.NS_Free_Magic_Area1` | magic area | unknown until shot | unmeasured |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Area2.NS_Free_Magic_Area2` | magic area, second | unknown until shot | unmeasured |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Attack1.NS_Free_Magic_Attack1` | magic attack | unknown until shot | unmeasured |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Attack2.NS_Free_Magic_Attack2` | magic attack, second | unknown until shot | unmeasured |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Attack_Line.NS_Free_Magic_Attack_Line` | line attack | unknown until shot | unmeasured |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Aura.NS_Free_Magic_Aura` | generic magic aura | loops | body-sized |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Buff.NS_Free_Magic_Buff` | generic magic buff | loops until stopped | body-sized |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Circle1.NS_Free_Magic_Circle1` | magic circle | loops until stopped | a few metres |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Circle2.NS_Free_Magic_Circle2` | magic circle, second | loops until stopped | a few metres |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Hit1.NS_Free_Magic_Hit1` | magic hit | plays once | small |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Hit2.NS_Free_Magic_Hit2` | magic hit, second | plays once | small |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Projectile1.NS_Free_Magic_Projectile1` | projectile, needs a flight | trail | not a point puff |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Projectile1_Base.NS_Free_Magic_Projectile1_Base` | projectile base | trail | not a point puff |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Projectile2.NS_Free_Magic_Projectile2` | projectile, second | trail | not a point puff |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Projectile2_Base.NS_Free_Magic_Projectile2_Base` | projectile base, second | trail | not a point puff |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Slash.NS_Free_Magic_Slash` | slash | plays once | about a weapon swing |
| `/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Slash2.NS_Free_Magic_Slash2` | slash, second | plays once | about a weapon swing |
| `/Game/Basic_VFX/Niagara/NS_Basic_1.NS_Basic_1` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_10.NS_Basic_10` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_11.NS_Basic_11` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_12.NS_Basic_12` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_13.NS_Basic_13` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_14.NS_Basic_14` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_15.NS_Basic_15` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_2.NS_Basic_2` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_3.NS_Basic_3` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_4.NS_Basic_4` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_5.NS_Basic_5` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_6.NS_Basic_6` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_7.NS_Basic_7` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_8.NS_Basic_8` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/Basic_VFX/Niagara/NS_Basic_9.NS_Basic_9` | numbered demo, look unknown | unknown until shot | unmeasured |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Black.NS_Absorption_Black` | black pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Blue.NS_Absorption_Blue` | blue pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Green.NS_Absorption_Green` | green pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Orange.NS_Absorption_Orange` | orange pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Pink.NS_Absorption_Pink` | pink pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Purple.NS_Absorption_Purple` | purple pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Red.NS_Absorption_Red` | red pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Teal.NS_Absorption_Teal` | teal pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_White.NS_Absorption_White` | white pull | loops until stopped | body-sized |
| `/Game/AbsorbtionFX/FX/NS_Absorption_Yellow.NS_Absorption_Yellow` | yellow pull | loops until stopped | body-sized |
| `/Game/_SplineVFX/NS/NS_Spline_ElectricLightning.NS_Spline_ElectricLightning` | lightning along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_EnergyLoop.NS_Spline_EnergyLoop` | energy loop along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_Fire.NS_Spline_Fire` | fire along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_Frost.NS_Spline_Frost` | frost along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_Holy.NS_Spline_Holy` | holy light along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_Magic.NS_Spline_Magic` | magic along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_MiasmaBoil.NS_Spline_MiasmaBoil` | poison boil along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_ParticleFollow.NS_Spline_ParticleFollow` | particles along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_WaterSplash.NS_Spline_WaterSplash` | water along a spline | spline, not a point puff | follows the spline |
| `/Game/_SplineVFX/NS/NS_Spline_Wind.NS_Spline_Wind` | wind along a spline | spline, not a point puff | follows the spline |
| `/Game/NiagaraExamples/FX_Explosions/NS_Dirt_Explosion.NS_Dirt_Explosion` | dirt blast | plays once | several metres |
| `/Game/NiagaraExamples/FX_Explosions/NS_Dirt_Explosion_Medium.NS_Dirt_Explosion_Medium` | medium dirt blast | plays once | a few metres |
| `/Game/NiagaraExamples/FX_Explosions/NS_Dirt_Explosion_Small.NS_Dirt_Explosion_Small` | small dirt blast | plays once | about 2 m |
| `/Game/NiagaraExamples/FX_Explosions/NS_Explosion.NS_Explosion` | explosion | plays once | several metres |
| `/Game/NiagaraExamples/FX_Explosions/NS_Explosion_Medium.NS_Explosion_Medium` | medium explosion | plays once | a few metres |
| `/Game/NiagaraExamples/FX_Explosions/NS_Explosion_Small.NS_Explosion_Small` | small explosion | plays once | about 2 m at scale 1 |
| `/Game/NiagaraExamples/FX_Footstep/NS_Footstep_Bubbles.NS_Footstep_Bubbles` | Footstep Bubbles | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Footstep/NS_Footstep_Fire.NS_Footstep_Fire` | Footstep Fire | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Footstep/NS_Footstep_Gravel.NS_Footstep_Gravel` | Footstep Gravel | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Footstep/NS_Footstep_LW.NS_Footstep_LW` | Footstep LW | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Markers/NS_Marker_Location.NS_Marker_Location` | Marker Location | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Markers/NS_Marker_Target.NS_Marker_Target` | Marker Target | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_Boundary.NS_Boundary` | Boundary | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_Boundary_Box.NS_Boundary_Box` | Boundary Box | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_Boundary_Cylinder.NS_Boundary_Cylinder` | Boundary Cylinder | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_Boundary_Sphere.NS_Boundary_Sphere` | Boundary Sphere | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_Bubble_Burst.NS_Bubble_Burst` | bubble burst | plays once | small |
| `/Game/NiagaraExamples/FX_Misc/NS_Fire.NS_Fire` | campfire-style fire | loops | body-sized |
| `/Game/NiagaraExamples/FX_Misc/NS_FireworkBurst.NS_FireworkBurst` | firework | plays once | a few metres |
| `/Game/NiagaraExamples/FX_Misc/NS_HitDissolve.NS_HitDissolve` | hit dissolve | plays once | body-sized, wants a mesh |
| `/Game/NiagaraExamples/FX_Misc/NS_SkeletalMeshBones_Burst.NS_SkeletalMeshBones_Burst` | SkeletalMeshBones Burst | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_SkeletalMeshBones_Loop.NS_SkeletalMeshBones_Loop` | SkeletalMeshBones Loop | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_SkeletalMeshTris_Burst.NS_SkeletalMeshTris_Burst` | SkeletalMeshTris Burst | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Misc/NS_SkeletalMeshTris_Loop.NS_SkeletalMeshTris_Loop` | SkeletalMeshTris Loop | loops | unmeasured |
| `/Game/NiagaraExamples/FX_NDC/NS_NDC_Footsteps.NS_NDC_Footsteps` | NDC Footsteps | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_NDC/NS_NDC_Footsteps_Bubbles.NS_NDC_Footsteps_Bubbles` | NDC Footsteps Bubbles | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_NDC/NS_NDC_Footsteps_Fire.NS_NDC_Footsteps_Fire` | NDC Footsteps Fire | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_NDC/NS_NDC_Impacts.NS_NDC_Impacts` | NDC Impacts | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_PickUp/NS_Pickup_Idle.NS_Pickup_Idle` | Pickup Idle | loops | unmeasured |
| `/Game/NiagaraExamples/FX_PickUp/NS_Pickup_Spawn.NS_Pickup_Spawn` | Pickup Spawn | loops | unmeasured |
| `/Game/NiagaraExamples/FX_PickUp/NS_Pickup_Success.NS_Pickup_Success` | Pickup Success | loops | unmeasured |
| `/Game/NiagaraExamples/FX_PickUp/NS_Pickup_Timeout.NS_Pickup_Timeout` | Pickup Timeout | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Player/NS_Player_Buff_Looping.NS_Player_Buff_Looping` | Player Buff Looping | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Player/NS_Player_DeBuff_Looping.NS_Player_DeBuff_Looping` | Player DeBuff Looping | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Player/NS_Player_Electricity_Looping.NS_Player_Electricity_Looping` | Player Electricity Looping | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Player/NS_Player_Teleport_In.NS_Player_Teleport_In` | Player Teleport In | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Player/NS_Player_Teleport_Out.NS_Player_Teleport_Out` | Player Teleport Out | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Player/NS_Weapon_Buff_Looping.NS_Weapon_Buff_Looping` | Weapon Buff Looping | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Player/NS_Weapon_DeBuff_Looping.NS_Weapon_DeBuff_Looping` | Weapon DeBuff Looping | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Ribbons/NS_TeslaCoil.NS_TeslaCoil` | TeslaCoil | loops | unmeasured |
| `/Game/NiagaraExamples/FX_SkeletalMesh/NS_Dino_Tri_Loop.NS_Dino_Tri_Loop` | Dino Tri Loop | loops | unmeasured |
| `/Game/NiagaraExamples/FX_SkeletalMesh/NS_Dino_Tris_Color_Burst.NS_Dino_Tris_Color_Burst` | Dino Tris Color Burst | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Smoke/NS_Chimney_Smoke.NS_Chimney_Smoke` | Chimney Smoke | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Smoke/NS_Smoke_Plume.NS_Smoke_Plume` | Smoke Plume | loops | unmeasured |
| `/Game/NiagaraExamples/FX_Sparks/NS_Spark_Burst.NS_Spark_Burst` | spark burst | plays once | small |
| `/Game/NiagaraExamples/FX_Sparks/NS_Spark_Continuous.NS_Spark_Continuous` | continuous sparks | loops | small |
| `/Game/NiagaraExamples/FX_Sparks/NS_Spark_Impact_Looping.NS_Spark_Impact_Looping` | looping spark impact | loops | small |
| `/Game/NiagaraExamples/FX_Weapons/Impacts/NS_Impact_Concrete.NS_Impact_Concrete` | blunt impact | plays once | about a metre |
| `/Game/NiagaraExamples/FX_Weapons/Impacts/NS_Impact_Glass.NS_Impact_Glass` | glass impact | plays once | about a metre |
| `/Game/NiagaraExamples/FX_Weapons/Impacts/NS_Impact_Metal.NS_Impact_Metal` | metal impact | plays once | about a metre |
| `/Game/NiagaraExamples/FX_Weapons/Impacts/NS_Impact_Wood.NS_Impact_Wood` | wood impact | plays once | about a metre |
| `/Game/NiagaraExamples/FX_Weapons/MuzzleFlashes/NS_MuzzleFlash.NS_MuzzleFlash` | MuzzleFlash | plays once | unmeasured |
| `/Game/NiagaraExamples/FX_Weapons/Trails/NS_BulletTracer.NS_BulletTracer` | BulletTracer | see the name | unmeasured |
| `/Game/NiagaraExamples/FX_Weapons/Trails/NS_RocketTrail.NS_RocketTrail` | RocketTrail | see the name | unmeasured |
| `/Game/NiagaraExamples/FX_Weapons/Trails/NS_SimpleRibbonTrail.NS_SimpleRibbonTrail` | SimpleRibbonTrail | see the name | unmeasured |
| `/Game/NiagaraExamples/Materials/Reference/NS_SubUV_Utilities.NS_SubUV_Utilities` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil.NS_ExplosionRoil` | ExplosionRoil | plays once | unmeasured |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Alpha.NS_ExplosionRoil_BakedAtlas_Alpha` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Emissive.NS_ExplosionRoil_BakedAtlas_Emissive` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Normals.NS_ExplosionRoil_BakedAtlas_Normals` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Occlusion.NS_ExplosionRoil_BakedAtlas_Occlusion` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/NS_TestSprite.NS_TestSprite` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight.NS_SmokePuffLight` | SmokePuffLight | see the name | unmeasured |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Emissive.NS_SmokePuffLight_BakedAtlas_Emissive` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Normals.NS_SmokePuffLight_BakedAtlas_Normals` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Occlusion.NS_SmokePuffLight_BakedAtlas_Occlusion` | sprite-tool, not a spell | not used | n/a |
| `/Game/NiagaraExamples/Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Smoke_2048_RGB.NS_SmokePuffLight_BakedAtlas_Smoke_2048_RGB` | sprite-tool, not a spell | not used | n/a |

137 Niagara systems.

`DCC.ShowFx <path> [delay]` spawns one 3 m in front of the camera and labels it. A path without the `.Asset` suffix is filled in.
