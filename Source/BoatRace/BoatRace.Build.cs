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
			// Crisp scalable HUD text (Roboto through the Slate font cache).
			"Slate",
			"SlateCore",
			// Computer rivals are AI controllers (BoatRivalController).
			"AIModule",
			// The moving water surface is built from a procedural mesh.
			"ProceduralMeshComponent",
			// Finding races on the same Wi-Fi by UDP broadcast (LanDiscovery).
			"Sockets",
			"Networking"
		});
	}
}
