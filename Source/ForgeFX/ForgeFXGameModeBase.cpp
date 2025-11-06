// Copyright Epic Games, Inc. All Rights Reserved.

#include "ForgeFXGameModeBase.h"
#include "Pawn/RobotDemoPawn.h"
#include "Player/RobotDemoPlayerController.h"

AForgeFXGameModeBase::AForgeFXGameModeBase()
{
	// Use native classes directly
	DefaultPawnClass = ARobotDemoPawn::StaticClass();
	PlayerControllerClass = ARobotDemoPlayerController::StaticClass();
}

