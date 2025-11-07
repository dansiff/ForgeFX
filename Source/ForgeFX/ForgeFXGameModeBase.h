// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ForgeFXGameModeBase.generated.h"

UCLASS()
class FORGEFX_API AForgeFXGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AForgeFXGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
