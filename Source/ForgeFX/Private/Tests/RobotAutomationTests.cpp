// Copyright
// Minimal automation tests for ForgeFX robot assessment.
// This test validates attach/detach logic without relying on visuals.
// It ensures logic correctness even if mesh assets change.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/RobotArmComponent.h"
#include "Components/HighlightComponent.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

static AActor* SpawnTestActor(UWorld* World)
{
	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transient;
	return World->SpawnActor<AActor>(Params);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttachToggleTest, "ForgeFX.Robot.AttachTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAttachToggleTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("World valid"), World);
	AActor* Actor = SpawnTestActor(World);
	URobotArmComponent* Arm = NewObject<URobotArmComponent>(Actor);
	Actor->AddInstanceComponent(Arm);
	Arm->RegisterComponent();

	TestTrue(TEXT("Initially attached"), Arm->IsAttached());
	Arm->DetachFromRobot();
	TestFalse(TEXT("Detached after call"), Arm->IsAttached());
	Arm->AttachToRobot();
	TestTrue(TEXT("Attached after call"), Arm->IsAttached());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHighlightHoverTest, "ForgeFX.Robot.HighlightHoverTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHighlightHoverTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AActor* Actor = SpawnTestActor(World);
	UHighlightComponent* Highlight = NewObject<UHighlightComponent>(Actor);
	Actor->AddInstanceComponent(Highlight);
	Highlight->RegisterComponent();

	TestFalse(TEXT("Default not highlighted"), Highlight->IsHighlighted());
	Highlight->SetHighlighted(true);
	TestTrue(TEXT("Highlighted on hover"), Highlight->IsHighlighted());
	Highlight->SetHighlighted(false);
	TestFalse(TEXT("Not highlighted when leaving hover"), Highlight->IsHighlighted());
	return true;
}

// Negative test: ensure DetachFromRobot() flips state from true->false.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDetachMustFlipStateTest, "ForgeFX.Robot.DetachMustFlipState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDetachMustFlipStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AActor* Actor = SpawnTestActor(World);
	URobotArmComponent* Arm = NewObject<URobotArmComponent>(Actor);
	Actor->AddInstanceComponent(Arm);
	Arm->RegisterComponent();

	TestTrue(TEXT("Precondition: starts attached"), Arm->IsAttached());
	Arm->DetachFromRobot();
	if (Arm->IsAttached())
	{
		AddError(TEXT("DetachFromRobot did not flip state to false"));
		return false;
	}
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
