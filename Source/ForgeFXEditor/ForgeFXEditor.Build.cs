using UnrealBuildTool;

public class ForgeFXEditor : ModuleRules
{
	public ForgeFXEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"FunctionalTesting",
			"Slate","SlateCore","UMG",
			"ForgeFX"
		});
	}
}
