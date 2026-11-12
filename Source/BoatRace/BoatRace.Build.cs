using UnrealBuildTool;

public class BoatRace : ModuleRules
{
	public BoatRace(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			// The moving water surface is built from a procedural mesh.
			"ProceduralMeshComponent"
		});
	}
}
