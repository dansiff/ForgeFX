// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ForgeFX : ModuleRules
{
	public ForgeFX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core runtime modules
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		// Gameplay & features
		PrivateDependencyModuleNames.AddRange(new string[] { "InputCore", "EnhancedInput", "UMG", "Niagara" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AutomationController", "FunctionalTesting" });
		}
	}
}
