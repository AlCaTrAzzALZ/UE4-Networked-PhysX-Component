// Copyright (C) James Baxter 2017. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class NTGameTarget : TargetRules
{
	public NTGameTarget(TargetInfo Target)
	{
		Type = TargetType.Game;
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