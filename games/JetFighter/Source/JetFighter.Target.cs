using UnrealBuildTool;
public class JetFighterTarget : TargetRules
{
    public JetFighterTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("JetFighter");
    }
}
