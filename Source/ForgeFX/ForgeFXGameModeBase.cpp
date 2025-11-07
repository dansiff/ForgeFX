// Copyright Epic Games, Inc. All Rights Reserved.

#include "ForgeFXGameModeBase.h"
#include "Pawn/RobotDemoPawn.h"
#include "Player/RobotDemoPlayerController.h"

AForgeFXGameModeBase::AForgeFXGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = ARobotDemoPawn::StaticClass();
	PlayerControllerClass = ARobotDemoPlayerController::StaticClass();
}

