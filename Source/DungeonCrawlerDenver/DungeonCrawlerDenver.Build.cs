using UnrealBuildTool;

public class DungeonCrawlerDenver : ModuleRules
{
	public DungeonCrawlerDenver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Each .cpp compiles on its own (2026-11-16). Many files keep private helpers in anonymous namespaces with the
		// same names (Rate, ToPcm, Wave, FCastSpeaker...); unity builds glued them into one file whenever a new .cpp
		// reshuffled the batches, and the build broke in code nobody had touched.
		bUseUnity = false;

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
			"RenderCore", // GWhiteTexture for the HUD's solid triangles (2026-11-14)
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
			// FPlatformApplicationMisc::ClipboardCopy, for the party menu's "Copy invite".
			"ApplicationCore",
			// The gear bench lists every animation on a skeleton.
			"AssetRegistry",
			"HTTP",
			"Json",
			"JsonUtilities",
			"DeveloperSettings",
			// Spray tags (Arcade/DCCSpray): read the JPG or PNG a player picks for their tag.
			"ImageWrapper"
		});

		// Spray tags: the Windows picture picker (GetOpenFileNameW).
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("comdlg32.lib");
		}

		// DCC.NpcRetarget (Arcade/DCCNpcRetarget.cpp, editor only): builds IK Rigs and a retargeter in code and batch
		// retargets the mannequin clips onto the NPC Forge skeleton (2026-11-14).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AssetTools", "IKRig", "IKRigEditor" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"DungeonCrawlerDenver"
		});
	}
}
