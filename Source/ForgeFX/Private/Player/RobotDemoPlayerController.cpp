#include "Player/RobotDemoPlayerController.h"
#include "EnhancedInputSubsystems.h"

void ARobotDemoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!DefaultInputContext) return;

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			Subsys->AddMappingContext(DefaultInputContext, MappingPriority);
		}
	}
}
