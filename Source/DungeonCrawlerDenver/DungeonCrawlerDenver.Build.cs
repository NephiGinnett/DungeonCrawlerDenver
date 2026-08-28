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
			"Niagara",
			// FGameplayTag/FGameplayTagContainer for item tags (Liquid, Alcohol,
			// Flammable, Organic, Craftable...) - see Items/DCCConsumableTypes.h.
			// Distinct from the editor-only "GameplayTagsToolset" MCP plugin in
			// the .uproject: this is the always-available runtime module every
			// UE5 project ships, and nothing here previously depended on it
			// directly (only transitively through AIModule/StateTree, which
			// does not guarantee the public headers are visible).
			"GameplayTags"
		});

		// The AI narrator talks to a local model over plain HTTP (Ollama, LM Studio).
		// Private because nothing outside this module should be making requests -
		// the narrator is the only thing here that touches the network, and keeping
		// the dependency private is what stops that quietly changing.
		//
		// Json/JsonUtilities are for building the request and parsing the reply.
		PrivateDependencyModuleNames.AddRange(new string[] {
			// DCCWhoAmI reads the machine's own address so the host can read it
			// out to the party. Private: nothing in the public headers needs it.
			"Sockets",
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
