// Copyright (C) James Baxter 2017. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class NTGameEditorTarget : TargetRules
{
	public NTGameEditorTarget(TargetInfo Target)
	{
		Type = TargetType.Editor;
	}

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutExtraModuleNames.AddRange( new string[] { "NTGame" } );
	}
}
