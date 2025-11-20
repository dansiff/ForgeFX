#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Actors/RobotActor.h"

static UWorld* GetAutomationWorld3(){ if (!GEngine) return nullptr; for (const FWorldContext& Ctx : GEngine->GetWorldContexts()){ if (Ctx.WorldType==EWorldType::PIE && Ctx.World()) return Ctx.World(); } return nullptr; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRobotSelectionBatchTest, "RobotTests.SelectionBatchDetach", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRobotSelectionBatchTest::RunTest(const FString& Parameters)
{
	UWorld* World = GetAutomationWorld3(); if (!World){ AddWarning(TEXT("No PIE world active")); return true; }
	ARobotActor* Robot = World->SpawnActor<ARobotActor>(); TestNotNull(TEXT("Robot spawned"), Robot); if (!Robot) return false;
	TArray<FName> Parts = Robot->GetDetachableParts(); if (Parts.Num() <2){ AddWarning(TEXT("Need at least2 detachable parts for selection batch test")); return true; }
	// Select first two
	Robot->TogglePartSelection(Parts[0]); Robot->TogglePartSelection(Parts[1]);
	Robot->BatchDetachSelected();
	TestTrue(TEXT("First part detached"), Robot->IsPartCurrentlyDetached(Parts[0]));
	TestTrue(TEXT("Second part detached"), Robot->IsPartCurrentlyDetached(Parts[1]));
	// Reattach for cleanup
	Robot->ReattachPartForTest(Parts[0]); Robot->ReattachPartForTest(Parts[1]);
	TestFalse(TEXT("First part reattached"), Robot->IsPartCurrentlyDetached(Parts[0]));
	TestFalse(TEXT("Second part reattached"), Robot->IsPartCurrentlyDetached(Parts[1]));
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
