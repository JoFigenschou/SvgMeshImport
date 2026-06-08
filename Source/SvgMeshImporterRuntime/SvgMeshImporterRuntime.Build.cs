using UnrealBuildTool;
using System.IO;

public class SvgMeshImporterRuntime : ModuleRules
{
	public SvgMeshImporterRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProceduralMeshComponent"
		});

		string PluginDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "../.."));
		string ThirdParty = Path.Combine(PluginDir, "ThirdParty");
		string ClipperSrc = Path.Combine(ThirdParty, "Clipper2", "CPP", "Clipper2Lib", "src");

		PublicIncludePaths.Add(Path.Combine(ThirdParty, "nanosvg", "src"));
		PublicIncludePaths.Add(ThirdParty);
		PublicIncludePaths.Add(Path.Combine(ThirdParty, "Clipper2", "CPP", "Clipper2Lib", "include"));
		PrivateIncludePaths.Add(Path.Combine(ThirdParty, "earcut"));
		PrivateIncludePaths.Add(ClipperSrc);

		PublicDefinitions.Add("SVG_MESH_IMPORTER_CLIPPER_SCALE=1000.0");

		string[] Clipper2SourceFiles = new string[]
		{
			Path.Combine(ClipperSrc, "clipper.engine.cpp"),
			Path.Combine(ClipperSrc, "clipper.offset.cpp"),
			Path.Combine(ClipperSrc, "clipper.rectclip.cpp")
		};

		foreach (string ClipperSource in Clipper2SourceFiles)
		{
			if (!File.Exists(ClipperSource))
			{
				throw new BuildException("SvgMeshImporter: missing Clipper2 source " + ClipperSource);
			}
		}

		// Clipper2SourceFiles are compiled in Private/ThirdParty/Clipper2Impl.cpp (single translation unit).

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
