// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ForgeFX : ModuleRules
{
	public ForgeFX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "EnhancedInput", "Niagara" });
		PrivateDependencyModuleNames.AddRange(new string[] { "InputCore", "UMG", "Slate", "SlateCore" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AutomationController", "FunctionalTesting" });
		}
	}
}
