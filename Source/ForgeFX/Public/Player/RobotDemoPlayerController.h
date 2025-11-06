#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "RobotDemoPlayerController.generated.h"

/**
 * Adds a default Enhanced Input mapping context at BeginPlay so the demo works out-of-the-box.
 */
UCLASS()
class FORGEFX_API ARobotDemoPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;

	// Assign an IMC asset in the editor to auto-add it at runtime
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultInputContext;

	// Priority when adding the mapping context
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 MappingPriority =0;
};
