using UnrealBuildTool;
public class JetFighterEditorTarget : TargetRules
{
    public JetFighterEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("JetFighter");
    }
}
