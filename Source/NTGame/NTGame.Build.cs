using UnrealBuildTool;

public class NTGame : ModuleRules
{
	public NTGame(TargetInfo Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
                "NTGame/Public",
                "NTGame/Classes",
                "NTGame/Private",
            }
        );
        
	    PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "OnlineSubSystem", "OnlineSubsystemUtils" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "UMG" });
		
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.Add("UnrealEd");
        }
	}
}