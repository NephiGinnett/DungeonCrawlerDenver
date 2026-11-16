# Repo Rally model list

The shift plays today on a concrete blockout. These meshes replace that blockout.
Nothing here is in the project yet. Do not point the game at a path until the
mesh imports and a truck sitting on it looks the right size in a test map.

Style (changed 2026-11-13, Zoe): **a futuristic vaporwave city**, the house style. The
city is now dressed from Synthwave_city / Cyberpunk_City / Sci_Fi_city, with neon lane
edges (`MI_neon_*`), palms, holograms and a dusk sun (`ADCCArcadeKart::DressCity`).
New meshes should match that: clean retro-future shapes, neon trim, pastel and chrome.
Invented names only. No real tow-company logos. (The old grunge brief below is kept
for the vehicle list and scale notes; ignore its "no neon" rule.)

## Look

Studio shots from this session, for whoever sources the meshes. They were not
opened and checked, so treat them as a starting point and discard one if it
misses the brief.

- `SourceArt/RepoRally/reference/wrecker.jpg` — the player truck
- `SourceArt/RepoRally/reference/beater.jpg` — a marked car
- `SourceArt/RepoRally/reference/gas-station.jpg` — a street station
- `SourceArt/RepoRally/reference/street.jpg` — the city, wide

## How to deliver them

- Static meshes. FBX, or GLB converted to FBX with `Tools/glb_to_fbx.py`
  (Blender is at `D:\Blender\blender.exe`). Unreal's mesh import does not read GLB.
- Pivot at the bottom center. The front of the vehicle points **+X**.
- Real-world meters in the file are fine. The game thinks in centimeters.
- One material per mesh if you can. Textures as PNG. No text baked into a
  texture: signs are drawn in code, so door panels and price boards stay blank.
- Put source files in `SourceArt/RepoRally/` and import to `/Game/RepoRally/`.
  Suggested asset names are below. Wiring them into the city is a follow-up.
  `ADCCArcadeArena::Prop` fits a mesh into a box and logs a warning (it does
  not crash) if the path is missing.

## Scale

The rover's collision box is 640 cm long, 360 cm wide, and 180 cm tall. Hero
trucks can be taller than that box. A wrecker should read about 7 m long,
2.6 m wide, and 2.8 m tall. Marked cars about 4.5 m long. Traffic can share
those sizes. A street block in the current city is 200 m, and the roads are
22 m wide, so a building mesh can be large.

## Vehicles

| Asset | What it is |
|---|---|
| `SM_Wrecker_Stock` | The crew truck. Cab, flatbed, a winch spool and a hook, reinforced bumper, mud, worn paint. |
| `SM_Wrecker_Runner` | Same truck, stripped. No bumper, lighter bed, the engine showing. The fast role. |
| `SM_Wrecker_Net` | A rack over the bed that throws a net. |
| `SM_Wrecker_Winch` | A heavier bumper and a bigger spool. The long-line role. |
| `SM_Wrecker_Tank` | Side tanks and jerry cans. The fuel role. |
| `SM_Wrecker_Rival` | The same truck in a darker dirty paint, with a light bar. Blank doors. This is Night Shift and the other crew. |
| `SM_Beater_Sedan` | A marked car. Mismatched panels, dents. |
| `SM_Beater_Hatch` | A second marked car, same condition, different shape. |
| `SM_Beater_Coupe` | A third. |
| `SM_Beater_Van` | A fourth. A panel van, not a clean commercial van. |
| `SM_Traffic_CommuterA` | Ordinary dirty traffic. |
| `SM_Traffic_CommuterB` | A second commuter, so a street does not repeat one silhouette. |
| `SM_Traffic_Box` | A box truck. |
| `SM_Traffic_Bus` | A city bus. |

One wrecker plus bolt-on parts (`SM_Part_NetRack`, `SM_Part_Winch`, `SM_Part_Tanks`,
`SM_Part_LightBar`) is a fine substitute for five whole trucks, if that is easier
to source. The roles only need to read at a glance.

The hook is aimed from a camera on the truck, then the marked car is reeled onto the
**rear bumper** and towed. The stock wrecker wants a visible hitch and hook on the back.
A winch on the bed is the winch role, not a second hitch.

## The city

| Asset | What it is |
|---|---|
| `SM_Gas_Canopy` | A rusted canopy on four posts. Tall enough to drive under. No prices. |
| `SM_Gas_Pump` | One pump. The game places two under a canopy. |
| `SM_Warehouse` | A low industrial block with a loading door. The city is mostly these. |
| `SM_Factory` | A brick factory face, dirtier and taller than the warehouse. |
| `SM_Smokestack` | A stack. The blockout already plants a few. |
| `SM_Fence_Segment` | Chainlink, one bay wide, so it can repeat. |
| `SM_JerseyBarrier` | Concrete barrier. |
| `SM_Dumpster` | |
| `SM_Container` | A shipping container, rusted, no carrier logo. |
| `SM_Streetlight` | A sodium streetlight. The bulb color can be warm; the mesh stays dark metal. |
| `SM_Billboard` | An empty frame. Copy is added in game. |
| `SM_Overpass` | One deck segment, so a road can cross another. |
| `SM_Net` | The net that lands on the road. Flat, wide, orange-brown, not a spider web. |
| `SM_Barrel` | An oil drum. |
| `SM_Pallet` | A loaded pallet. |

## People

Rhee, Jan, and Diaz already use Quinn and Manny. Leave them. A hard hat and a
high-visibility vest would fit the yard. Do not bring back the party hat.

The old synthwave cars under `/Game/Synthwave_city/` are the wrong look for this
shift. They stay in the project for Neon Mile. Repo Rally should not wear them.
