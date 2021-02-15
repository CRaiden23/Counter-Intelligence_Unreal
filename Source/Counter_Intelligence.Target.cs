

using UnrealBuildTool;
using System.Collections.Generic;

public class Counter_IntelligenceTarget : TargetRules
{
	public Counter_IntelligenceTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		ExtraModuleNames.AddRange( new string[] { "Counter_Intelligence" } );
	}
}
