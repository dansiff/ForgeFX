#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "NiagaraSystem.h"
#include "RobotArmConfig.generated.h"

/**
 * Data-driven configuration for robot arm behavior and visuals.
 * Designers can create instances and assign per-robot tuning without code changes.
 */
UCLASS(BlueprintType)
class FORGEFX_API URobotArmConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// Logic
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Arm")
	bool bDefaultAttached = true;

	// Optional: name of the socket to use when visually attaching in BP
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Arm")
	FName AttachSocketName = NAME_None;

	// The logical set of assembly part names that make up this arm (e.g., Upperarm_Left, Lowerarm_Left, Hand_Left)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Arm")
	TArray<FName> ArmPartNames;

	// When detaching, spawn VFX at this part if provided; otherwise at the actor location
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|VFX")
	FName DetachEffectPartName = NAME_None;

	// VFX
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|VFX")
	TObjectPtr<UNiagaraSystem> DetachEffect = nullptr;

	// Materials
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Materials")
	FName HighlightScalarParam = TEXT("HighlightAmount");

	// Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Input")
	FName ToggleInputActionName = TEXT("IA_ToggleArm");
};
