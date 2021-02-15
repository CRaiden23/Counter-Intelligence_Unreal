

using UnrealBuildTool;
using System.Collections.Generic;

public class Counter_IntelligenceEditorTarget : TargetRules
{
	public Counter_IntelligenceEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		ExtraModuleNames.AddRange( new string[] { "Counter_Intelligence" } );
	}
}
