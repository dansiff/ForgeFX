#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Actors/RobotActor.h"

// Helper: PIE world fetch
static UWorld* GetAutomationWorld()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (Ctx.WorldType == EWorldType::PIE && Ctx.World()) return Ctx.World();
	}
	return nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmConnectionTest, "RobotTests.ArmAttachment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArmConnectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = GetAutomationWorld();
	if (!World)
	{
		AddWarning(TEXT("No PIE world active; start Play In Editor before running test."));
		return true; // environment issue, not a failure of logic
	}

	// Spawn robot
	ARobotActor* Robot = World->SpawnActor<ARobotActor>();
	TestNotNull(TEXT("Robot spawned"), Robot);
	if (!Robot) return false;

	// Ensure arm initially attached
	TestTrue(TEXT("Arm initially attached"), Robot->IsArmAttached());

	Robot->DetachArm();
	TestFalse(TEXT("Arm should be detached after DetachArm"), Robot->IsArmAttached());

	Robot->AttachArm();
	TestTrue(TEXT("Arm should be attached after AttachArm"), Robot->IsArmAttached());

	// Repeat to catch hidden state issues
	Robot->DetachArm();
	TestFalse(TEXT("Arm should detach consistently"), Robot->IsArmAttached());
	Robot->AttachArm();
	TestTrue(TEXT("Arm should reattach consistently"), Robot->IsArmAttached());

	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
