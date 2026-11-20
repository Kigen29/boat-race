using UnrealBuildTool;
using System.Collections.Generic;

public class BoatRaceEditorTarget : TargetRules
{
	public BoatRaceEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BoatRace");
	}
}
