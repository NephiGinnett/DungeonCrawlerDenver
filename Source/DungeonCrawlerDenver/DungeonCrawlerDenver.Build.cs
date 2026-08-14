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

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"DungeonCrawlerDenver"
		});
	}
}
