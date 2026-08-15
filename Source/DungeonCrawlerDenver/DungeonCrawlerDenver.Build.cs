using UnrealBuildTool;

public class DungeonCrawlerDenver : ModuleRules
{
	public DungeonCrawlerDenver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"NetCore",
			// Skill cast and impact effects. Niagara is enabled by default in UE5,
			// and every VFX in the installed Fab packs is an NS_ Niagara system, so
			// this is the format to target rather than legacy Cascade.
			"Niagara"
		});

		// The AI narrator talks to a local model over plain HTTP (Ollama, LM Studio).
		// Private because nothing outside this module should be making requests -
		// the narrator is the only thing here that touches the network, and keeping
		// the dependency private is what stops that quietly changing.
		//
		// Json/JsonUtilities are for building the request and parsing the reply.
		PrivateDependencyModuleNames.AddRange(new string[] {
			"HTTP",
			"Json",
			"JsonUtilities",
			"DeveloperSettings"
		});

		PublicIncludePaths.AddRange(new string[] {
			"DungeonCrawlerDenver"
		});
	}
}
