#include "Arcade/DCCGateThemes.h"

#include "Characters/DCCEnemyCharacter_Corporate.h"
#include "Characters/DCCEnemyCharacter_DeadAir.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Characters/DCCEnemyCharacter_NuclearFamily.h"
#include "Characters/DCCEnemyCharacter_Oathgate.h"
#include "Characters/DCCEnemyCharacter_SurveyDrone.h"
#include "Characters/DCCEnemyCharacter_Wildlife.h"

namespace DCCGateThemes
{
	const TArray<FTheme>& All()
	{
		static const TArray<FTheme> Themes = []
		{
			TArray<FTheme> List;

			// 0. The old look, kept: a server hall under the city.
			List.Add({
				TEXT("THE DATA CENTER"), TEXT("Cold aisles, warm servers, and something living in the cable trays."),
				TEXT("/Game/Ice_Station/Materials/MI_floor_tile"), TEXT("/Game/Ice_Station/Materials/MI_floor_tile4"),
				TEXT("/Game/Ice_Station/Materials/MI_floor_tile2"),
				TEXT("/Game/Ice_Station/Materials/MI_trim_01_interior"), TEXT("/Game/Ice_Station/Materials/MI_trim_01_interior2"),
				FLinearColor(0.35f, 0.85f, 1.0f), FLinearColor(0.7f, 0.9f, 1.0f), 1.0f, FLinearColor(0.02f, 0.025f, 0.04f),
				TEXT("/Game/Synthwave_city/Material/MI_neon_1"),
				nullptr, TEXT("/Game/Ice_Station/Materials/MI_trim_2_metal"),
				{ TEXT("/Game/Ice_Station/Meshes/Computer/SM_computer_01"), TEXT("/Game/Ice_Station/Meshes/Computer/SM_computer_02"),
				  TEXT("/Game/Ice_Station/Meshes/Crates/SM_crate_01_group"), TEXT("/Game/Ice_Station/Meshes/Citerns/SM_citern") },
				{ TEXT("/Game/Ice_Station/Meshes/interior/SM_generator"), TEXT("/Game/Ice_Station/Meshes/Hologram/SM_planet_hologram"),
				  TEXT("/Game/Ice_Station/Meshes/Computer/SM_computer_circular") },
				TEXT("/Game/Ice_Station/FX/NS_Smoke.NS_Smoke"),
				TEXT("live cable"), nullptr, FLinearColor(0.3f, 0.7f, 1.0f), EDCCDamageType::Electric, false,
				{ ADCCEnemyCharacter_SurveyDrone::StaticClass(), ADCCEnemyCharacter_Intern::StaticClass(),
				  ADCCEnemyCharacter_FinePrint::StaticClass(), ADCCEnemyCharacter_Arbiter::StaticClass() },
			});

			// 1. Denver's storm drains, which the gate has been using as a basement.
			List.Add({
				TEXT("THE STORM DRAIN"), TEXT("Everything the city flushed. Some of it learned to walk."),
				TEXT("/Game/Modular_Sewer_Kit/Materials/MI_concrete_02"), TEXT("/Game/Modular_Sewer_Kit/Materials/MI_concrete_03"),
				TEXT("/Game/Modular_Sewer_Kit/Materials/MI_concrete_01"),
				TEXT("/Game/Modular_Sewer_Kit/Materials/MI_rust_modules"), TEXT("/Game/Modular_Sewer_Kit/Materials/MI_concrete_01"),
				FLinearColor(0.45f, 1.0f, 0.35f), FLinearColor(0.55f, 0.9f, 0.5f), 0.9f, FLinearColor(0.015f, 0.025f, 0.015f),
				nullptr,
				TEXT("/Game/Modular_Sewer_Kit/Meshes/Modules/Walls/SM_pillar"), nullptr,
				{ TEXT("/Game/Modular_Sewer_Kit/Meshes/Pipes/SM_pipe_01"), TEXT("/Game/Modular_Sewer_Kit/Meshes/Pipes/SM_pipe_02"),
				  TEXT("/Game/Modular_Sewer_Kit/Meshes/Pipes/SM_pipes_07"), TEXT("/Game/Modular_Sewer_Kit/Meshes/Details/SM_detail_01"),
				  TEXT("/Game/Modular_Sewer_Kit/Meshes/Details/SM_detail_03"), TEXT("/Game/Modular_Sewer_Kit/Meshes/Lamp/SM_lamp_green") },
				{ TEXT("/Game/Modular_Sewer_Kit/Meshes/Pipes/SM_pipe_03"), TEXT("/Game/Modular_Sewer_Kit/Meshes/Rust_modules/SM_fence_01") },
				TEXT("/Game/Modular_Sewer_Kit/FX/NS_smoke_01.NS_smoke_01"),
				TEXT("sludge"), TEXT("/Game/Modular_Sewer_Kit/Materials/Water/MI_water_02"), FLinearColor(0.35f, 0.75f, 0.15f),
				EDCCDamageType::Poison, true,
				{ ADCCEnemyCharacter_BreakRoomMold::StaticClass(), ADCCEnemyCharacter_Thornback::StaticClass(),
				  ADCCEnemyCharacter_UnclaimedProperty::StaticClass(), ADCCEnemyCharacter_DebtEater::StaticClass() },
			});

			// 2. A forge that has been running since before Denver.
			List.Add({
				TEXT("THE FOUNDRY"), TEXT("The fires never went out. The workers never clocked out either."),
				TEXT("/Game/Legendary_Forge/Materials/MI_ground"), TEXT("/Game/Legendary_Forge/Materials/MI_marble"),
				TEXT("/Game/Legendary_Forge/Materials/MI_ground"),
				TEXT("/Game/Legendary_Forge/Materials/MI_bricks"), TEXT("/Game/Legendary_Forge/Materials/MI_bricks"),
				FLinearColor(1.0f, 0.5f, 0.15f), FLinearColor(1.0f, 0.6f, 0.3f), 1.0f, FLinearColor(0.03f, 0.015f, 0.01f),
				nullptr,
				TEXT("/Game/Legendary_Forge/Meshes/Kit/SM_Pillar_02"), nullptr,
				{ TEXT("/Game/Legendary_Forge/Meshes/Barrel/SM_barrel_01"), TEXT("/Game/Legendary_Forge/Meshes/Barrel/SM_barrel_02"),
				  TEXT("/Game/Legendary_Forge/Meshes/Brasero/SM_brasero_02"), TEXT("/Game/Legendary_Forge/Meshes/Bellows/SM_bellows"),
				  TEXT("/Game/Legendary_Forge/Meshes/Grinding_wheel/SM_grinding_wheel"),
				  TEXT("/Game/Legendary_Forge/Meshes/Rocks/Small_rocks/SM_rock_assembly_01") },
				{ TEXT("/Game/Legendary_Forge/Meshes/Forge/SM_forge_02"), TEXT("/Game/Legendary_Forge/Meshes/Anvil_02/SM_anvil_02") },
				TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire.NS_fire"),
				TEXT("slag"), TEXT("/Game/Legendary_Forge/Materials/MI_lava"), FLinearColor(1.0f, 0.35f, 0.05f),
				EDCCDamageType::Fire, false,
				{ ADCCEnemyCharacter_SlagDog::StaticClass(), ADCCEnemy_NuclearDad::StaticClass(),
				  ADCCEnemyCharacter_EscalationSpecialist::StaticClass(), ADCCEnemyCharacter_Thornback::StaticClass() },
			});

			// 3. A tomb, fully paid for, in a city that cannot afford to die.
			List.Add({
				TEXT("THE MAUSOLEUM"), TEXT("Prepaid burial plots. The tenants want their deposit back."),
				TEXT("/Game/Egyptian_Temple_kit/Materials/MI_Temple_floor"), TEXT("/Game/Sun_Temple_Kit/Materials/MI_Temple_tiled_gold"),
				TEXT("/Game/Egyptian_Temple_kit/Materials/MI_Temple_floor"),
				TEXT("/Game/Egyptian_Temple_kit/Materials/MI_Temple_wall_block"), TEXT("/Game/Egyptian_Temple_kit/Materials/MI_Temple_trim"),
				FLinearColor(1.0f, 0.8f, 0.35f), FLinearColor(1.0f, 0.85f, 0.6f), 1.1f, FLinearColor(0.03f, 0.025f, 0.015f),
				nullptr,
				TEXT("/Game/Egyptian_Temple_kit/Meshes/Modules/Pillar/SM_pillar_03"), nullptr,
				{ TEXT("/Game/Egyptian_Temple_kit/Meshes/Modules/Vases/SM_vase_01"), TEXT("/Game/Egyptian_Temple_kit/Meshes/Modules/Vases/SM_vase_03"),
				  TEXT("/Game/Egyptian_Temple_kit/Meshes/Modules/Vases/SM_vase_05"), TEXT("/Game/Egyptian_Temple_kit/Meshes/Details/SM_detail_01"),
				  TEXT("/Game/Sun_Temple_Kit/Meshes/Modules/Pillar/SM_small_pillar_01") },
				{ TEXT("/Game/Sun_Temple_Kit/Meshes/Modules/Altar/SM_central_altar"), TEXT("/Game/Sun_Temple_Kit/Meshes/Modules/Throne/SM_Throne_02") },
				TEXT("/Game/Sun_Temple_Kit/FX/NS_light_particules_01.NS_light_particules_01"),
				TEXT("curse glyph"), nullptr, FLinearColor(0.9f, 0.6f, 1.0f), EDCCDamageType::Necrotic, true,
				{ ADCCEnemyCharacter_Notary::StaticClass(), ADCCEnemyCharacter_UnclaimedProperty::StaticClass(),
				  ADCCEnemyCharacter_Beholder::StaticClass(), ADCCEnemyCharacter_ShellCompany::StaticClass() },
			});

			// 4. A nightclub that closed in 1987 and kept the lights on.
			List.Add({
				TEXT("THE AFTER HOURS"), TEXT("Last call was decades ago. Nobody told the dance floor."),
				TEXT("/Game/Synthwave_city/Material/MI_synthwave_floor"), TEXT("/Game/Synthwave_city/Material/MI_synthwave_floor1"),
				TEXT("/Game/Synthwave_city/Material/MI_floor"),
				TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall"), TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall2"),
				FLinearColor(1.0f, 0.3f, 0.8f), FLinearColor(0.9f, 0.4f, 1.0f), 1.15f, FLinearColor(0.025f, 0.01f, 0.04f),
				TEXT("/Game/Synthwave_city/Material/MI_neon_3"),
				TEXT("/Game/Synthwave_city/Meshes/Column/SM_circular_column"), nullptr,
				{ TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_02"), TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_05"),
				  TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_02"), TEXT("/Game/Cyberpunk_City/Meshes/Crates/SM_crate_03"),
				  TEXT("/Game/NY_Subway/Meshes/SM_RSG_Subway_Vending_Machine_Snacks") },
				{ TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_01"), TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_03") },
				nullptr,
				TEXT("laser floor"), nullptr, FLinearColor(1.0f, 0.15f, 0.55f), EDCCDamageType::Electric, false,
				{ ADCCEnemyCharacter_ShellCompany::StaticClass(), ADCCEnemyCharacter_EscalationSpecialist::StaticClass(),
				  ADCCEnemyCharacter_Intern::StaticClass(), ADCCEnemyCharacter_SurveyDrone::StaticClass() },
			});

			// 5. Ruins the jungle took back, somewhere a gate should not reach.
			List.Add({
				TEXT("THE OVERGROWTH"), TEXT("A temple the jungle ate. It is still chewing."),
				TEXT("/Game/Jungle_Ruins_Kit/Materials/MI_floor"), TEXT("/Game/Jungle_Ruins_Kit/Materials/MI_Pillar"),
				TEXT("/Game/Jungle_Ruins_Kit/Materials/MI_floor"),
				TEXT("/Game/Jungle_Ruins_Kit/Materials/MI_big_rock"), TEXT("/Game/Jungle_Ruins_Kit/Materials/MI_Pillar"),
				FLinearColor(0.6f, 1.0f, 0.4f), FLinearColor(0.85f, 1.0f, 0.65f), 1.1f, FLinearColor(0.015f, 0.025f, 0.012f),
				nullptr,
				TEXT("/Game/Jungle_Ruins_Kit/Meshes/Column/SM_column_01"), nullptr,
				{ TEXT("/Game/Jungle_Ruins_Kit/Meshes/Plants/SM_plant_03"), TEXT("/Game/Jungle_Ruins_Kit/Meshes/Plants/SM_plant_07"),
				  TEXT("/Game/Jungle_Ruins_Kit/Meshes/Plants/SM_plant_12"), TEXT("/Game/Jungle_Ruins_Kit/Meshes/Stone/SM_stone_group_01"),
				  TEXT("/Game/Jungle_Ruins_Kit/Meshes/Stone/SM_stone_group_02") },
				{ TEXT("/Game/Jungle_Ruins_Kit/Meshes/Tree/SM_tree_01"), TEXT("/Game/Jungle_Ruins_Kit/Meshes/Column/SM_column_03") },
				TEXT("/Game/Jungle_Ruins_Kit/Fx/Flying_leaves/NS_flowers_flying.NS_flowers_flying"),
				TEXT("thornbed"), nullptr, FLinearColor(0.45f, 0.6f, 0.1f), EDCCDamageType::Physical, true,
				{ ADCCEnemyCharacter_Thornback::StaticClass(), ADCCEnemyCharacter_Beholder::StaticClass(),
				  ADCCEnemyCharacter_BreakRoomMold::StaticClass() },
			});

			// 6. Something grew in the gate itself.
			List.Add({
				TEXT("THE HIVE"), TEXT("The gate is alive in here. Mind the floor. It minds you."),
				TEXT("/Game/Alien_Cave_biome/Materials/MI_Alien_biomass_01"), TEXT("/Game/Alien_Cave_biome/Materials/MI_Alien_biomass_02"),
				TEXT("/Game/Alien_Cave_biome/Materials/MI_rock"),
				TEXT("/Game/Alien_Cave_biome/Materials/MI_rock"), TEXT("/Game/Alien_Cave_biome/Materials/MI_rock_02"),
				FLinearColor(0.4f, 1.0f, 0.85f), FLinearColor(0.6f, 0.4f, 1.0f), 0.85f, FLinearColor(0.02f, 0.01f, 0.03f),
				nullptr,
				TEXT("/Game/Alien_Cave_biome/Meshes/Alien_organism/SM_alien_organism_07"), nullptr,
				{ TEXT("/Game/Alien_Cave_biome/Meshes/Alien_organism/SM_alien_organism_01"), TEXT("/Game/Alien_Cave_biome/Meshes/Alien_organism/SM_alien_organism_04"),
				  TEXT("/Game/Alien_Cave_biome/Meshes/Egg/SM_egg"), TEXT("/Game/Alien_Cave_biome/Meshes/Plants/SM_plant_group_01"),
				  TEXT("/Game/Alien_Cave_biome/Meshes/Plants/SM_plant_group_02") },
				{ TEXT("/Game/Alien_Cave_biome/Meshes/Alien_organism/SM_alien_organism_10"), TEXT("/Game/Alien_Cave_biome/Meshes/rocks/SM_rock_01") },
				TEXT("/Game/Alien_Cave_biome/FX/NS_spores.NS_spores"),
				TEXT("spore bed"), nullptr, FLinearColor(0.3f, 1.0f, 0.75f), EDCCDamageType::Poison, true,
				{ ADCCEnemyCharacter_Beholder::StaticClass(), ADCCEnemyCharacter_BreakRoomMold::StaticClass(),
				  ADCCEnemyCharacter_Thornback::StaticClass(), ADCCEnemyCharacter_SlagDog::StaticClass() },
			});
			return List;
		}();
		return Themes;
	}

	int32 IndexFor(int32 Seed)
	{
		// A fixed theme on request (-GateTheme=N), for testing and trailers.
		int32 Forced = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("GateTheme="), Forced) && All().IsValidIndex(Forced))
		{
			return Forced;
		}
		return FRandomStream(Seed * 7919 + 3).RandRange(0, All().Num() - 1);
	}

	const FTheme& For(int32 Seed)
	{
		return All()[IndexFor(Seed)];
	}
}
