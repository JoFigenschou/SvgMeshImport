using UnrealBuildTool;
using System.IO;

public class SvgMeshImporterEditor : ModuleRules
{
	public SvgMeshImporterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorStyle",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"Projects",
			"SvgMeshImporterRuntime",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"DesktopPlatform"
		});
	}
}
