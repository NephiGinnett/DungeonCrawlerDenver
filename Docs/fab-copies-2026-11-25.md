# Fab sample copies, 2026-11-25

Copied from `D:\Unreal Why Am I deciding where this goes` into this project's `Content`, preserving `/Game/...` paths. VaultCache was not copied. Robocopy finished with exits 1 and 3 (both success). Log: the temp copy log from that pass.

Landed as their own folders:

- `Content/DerelictCorridor` (plus its World Partition external actors and objects)
- `Content/ElderboomVillage`
- `Content/SciFi_Space_Station`
- `Content/SuperGrid`
- `Content/RPG_Magic_Staff_Pack`
- `Content/Characters/Echo`, `Paragon`, `UE4_Mannequin`, `UE5_Mannequins`, `UEFN_Mannequin`

Factory Environment and the Game Animation Sample share roots with folders this project already had. Their `Audio`, `Blueprints`, `Fx`, `Maps`, `Materials`, `Meshes`, `Sequences`, `Textures`, and the GAS `IsolatedExamples`, `Levels`, `MetaHumans`, `Misc`, `Movies`, `Widgets` were merged into those existing `Content` folders. Where a file name collided, the later copy won.

Not copied: VaultCache, Factory's ThirdPerson, and the GAS `Characters` and `Input` roots (those names already belonged to this game).

These packs were already in `Content` before that copy and are what the new games use first: `Free_Spells` (crystal Niagara), `RadioSoldier_Announcer` (wave lines). The spell catalogue, weapon pass, and room sculpting stay on the later Fab plan. Do not travel to the sample maps.
