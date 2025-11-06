// Simple functional test to validate visibility toggles for arm parts in PIE.
// Uses Assembly + Arm configs and runs quickly in editor.

#include "FunctionalTest.h"
#include "Actors/RobotActor.h"
#include "Components/AssemblyBuilderComponent.h"
#include "Components/RobotArmComponent.h"
#include "RobotVisibilityFunctionalTest.generated.h"

UCLASS()
class FORGEFX_API ARobotVisibilityFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()
public:
	ARobotVisibilityFunctionalTest();

protected:
	virtual void StartTest() override;

	// Spawned robot under test
	UPROPERTY(Transient)
	TObjectPtr<ARobotActor> Robot;
};
