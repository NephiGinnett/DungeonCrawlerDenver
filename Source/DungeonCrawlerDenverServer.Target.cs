using UnrealBuildTool;
using System.Collections.Generic;

// Dedicated server target. This is what gets cross-compiled to a Linux binary
// for the Hetzner box later. It exists now so the project never accidentally
// grows client-only assumptions that are painful to unpick at deploy time.
public class DungeonCrawlerDenverServerTarget : TargetRules
{
	public DungeonCrawlerDenverServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("DungeonCrawlerDenver");
	}
}
