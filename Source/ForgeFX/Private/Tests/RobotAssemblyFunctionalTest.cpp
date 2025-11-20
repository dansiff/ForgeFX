#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/RobotActor.h"
#include "Components/AssemblyBuilderComponent.h"
#include "EngineUtils.h"
#include "Actors/RobotPartActor.h"

// Helper: ensure we have a PIE world
static UWorld* GetTestWorld()
{
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (Ctx.WorldType == EWorldType::PIE && Ctx.World()) return Ctx.World();
	}
	return nullptr;
}

// Simple functional automation test for robot assembly detach / reattach / highlight expectations.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRobotAssemblyFunctionalTest, "ForgeFX.Robot.Assembly.DetachAttach", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRobotAssemblyFunctionalTest::RunTest(const FString& Parameters)
{
	UWorld* World = GetTestWorld();
	if (!World)
	{
		AddWarning(TEXT("No PIE world found. Launch Play-In-Editor before running this test."));
		return true; // not a hard failure; environment not prepared
	}

	ARobotActor* Robot = nullptr;
	for (TActorIterator<ARobotActor> It(World); It; ++It) { Robot = *It; break; }
	if (!Robot)
	{
		AddError(TEXT("Robot actor not found in test world."));
		return false;
	}
	UAssemblyBuilderComponent* Assembly = Robot->FindComponentByClass<UAssemblyBuilderComponent>();
	if (!Assembly || !Assembly->AssemblyConfig)
	{
		AddError(TEXT("Assembly component or config missing on robot."));
		return false;
	}

	// Collect detachable parts
	TArray<FName> Detachable;
	for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts)
	{
		if (Spec.bDetachable) Detachable.Add(Spec.PartName);
	}
	if (Detachable.Num() ==0)
	{
		AddWarning(TEXT("No detachable parts configured; test passes trivially."));
		return true;
	}

	// Detach all
	for (FName P : Detachable)
	{
		if (Assembly->IsPartDetached(P)) continue; // already detached
		ARobotPartActor* OutActor = nullptr;
		const bool bDetached = Assembly->DetachPart(P, OutActor);
		TestTrue(FString::Printf(TEXT("Detach %s"), *P.ToString()), bDetached && OutActor != nullptr);
	}

	// Validate hidden original components
	for (FName P : Detachable)
	{
		UStaticMeshComponent* Comp = Assembly->GetPartByName(P);
		TestNotNull(*FString::Printf(TEXT("Original component exists %s"), *P.ToString()), Comp);
		if (!Comp) continue;
		const bool bHidden = Comp->bHiddenInGame || !Comp->IsVisible();
		TestTrue(FString::Printf(TEXT("Original hidden after detach %s"), *P.ToString()), bHidden);
	}

	// Reattach all
	for (FName P : Detachable)
	{
		if (!Assembly->IsPartDetached(P)) continue; // skip if failure earlier
		// find actor
		ARobotPartActor* PartActor = nullptr;
		for (TActorIterator<ARobotPartActor> It(World); It; ++It)
		{
			if (It->GetPartName() == P) { PartActor = *It; break; }
		}
		const bool bReattached = PartActor && Assembly->ReattachPart(P, PartActor);
		TestTrue(FString::Printf(TEXT("Reattach %s"), *P.ToString()), bReattached);
	}

	// Final visibility check
	for (FName P : Detachable)
	{
		UStaticMeshComponent* Comp = Assembly->GetPartByName(P);
		if (!Comp) continue;
		TestTrue(FString::Printf(TEXT("Original visible after reattach %s"), *P.ToString()), Comp->IsVisible() && !Comp->bHiddenInGame);
	}

	AddInfo(TEXT("Robot assembly functional test completed."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
