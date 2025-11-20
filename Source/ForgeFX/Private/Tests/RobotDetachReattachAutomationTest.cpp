#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Actors/RobotActor.h"
#include "Components/AssemblyBuilderComponent.h"

static UWorld* GetAutomationWorld2(){ if (!GEngine) return nullptr; for (const FWorldContext& Ctx : GEngine->GetWorldContexts()){ if (Ctx.WorldType==EWorldType::PIE && Ctx.World()) return Ctx.World(); } return nullptr; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRobotDetachReattachTest, "RobotTests.DetachReattachCycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRobotDetachReattachTest::RunTest(const FString& Parameters)
{
	UWorld* World = GetAutomationWorld2(); if (!World){ AddWarning(TEXT("No PIE world active")); return true; }
	ARobotActor* Robot = World->SpawnActor<ARobotActor>(); TestNotNull(TEXT("Robot spawned"), Robot); if (!Robot) return false;
	TArray<FName> Parts = Robot->GetDetachableParts(); if (Parts.Num()==0){ AddWarning(TEXT("No detachable parts")); return true; }
	// Detach all
	for (FName P : Parts){ const bool bDetached = Robot->DetachPartForTest(P); TestTrue(*FString::Printf(TEXT("Detached %s"), *P.ToString()), bDetached); }
	// Validate each now detached
	for (FName P : Parts){ TestTrue(*FString::Printf(TEXT("State detached %s"), *P.ToString()), Robot->IsPartCurrentlyDetached(P)); }
	// Reattach all
	for (FName P : Parts){ const bool bReattached = Robot->ReattachPartForTest(P); TestTrue(*FString::Printf(TEXT("Reattached %s"), *P.ToString()), bReattached); }
	// Validate attached
	for (FName P : Parts){ TestFalse(*FString::Printf(TEXT("Should no longer be detached %s"), *P.ToString()), Robot->IsPartCurrentlyDetached(P)); }
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
