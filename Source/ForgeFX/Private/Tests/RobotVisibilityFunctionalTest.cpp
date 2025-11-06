#include "Tests/RobotVisibilityFunctionalTest.h"
#include "Engine/World.h"

ARobotVisibilityFunctionalTest::ARobotVisibilityFunctionalTest()
{
	TimeLimit =5.f;
	bIsEnabled = true;
}

void ARobotVisibilityFunctionalTest::StartTest()
{
	Super::StartTest();

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishTest(EFunctionalTestResult::Failed, TEXT("World invalid"));
		return;
	}

	Robot = World->SpawnActor<ARobotActor>(ARobotActor::StaticClass());
	if (!Robot)
	{
		FinishTest(EFunctionalTestResult::Failed, TEXT("Failed to spawn robot"));
		return;
	}

	URobotArmComponent* Arm = Robot->FindComponentByClass<URobotArmComponent>();
	UAssemblyBuilderComponent* Assembly = Robot->FindComponentByClass<UAssemblyBuilderComponent>();
	if (!Arm || !Assembly || !Arm->Config)
	{
		FinishTest(EFunctionalTestResult::Failed, TEXT("Missing components or config"));
		return;
	}

	// Detach and assert all arm parts are hidden
	Arm->DetachFromRobot();
	for (const FName N : Arm->Config->ArmPartNames)
	{
		bool bVisible = true;
		if (UStaticMeshComponent* Comp = Assembly->GetPartByName(N))
		{
			bVisible = Comp->IsVisible();
		}
		if (bVisible)
		{
			FinishTest(EFunctionalTestResult::Failed, FString::Printf(TEXT("Part %s still visible after detach"), *N.ToString()));
			return;
		}
	}

	// Reattach and assert visible
	Arm->AttachToRobot();
	for (const FName N : Arm->Config->ArmPartNames)
	{
		bool bVisible = false;
		if (UStaticMeshComponent* Comp = Assembly->GetPartByName(N))
		{
			bVisible = Comp->IsVisible();
		}
		if (!bVisible)
		{
			FinishTest(EFunctionalTestResult::Failed, FString::Printf(TEXT("Part %s not visible after attach"), *N.ToString()));
			return;
		}
	}

	FinishTest(EFunctionalTestResult::Succeeded, TEXT("Visibility toggles validated"));
}
