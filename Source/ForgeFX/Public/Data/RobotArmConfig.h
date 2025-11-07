#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NiagaraSystem.h"
#include "RobotArmConfig.generated.h"

UCLASS(BlueprintType)
class FORGEFX_API URobotArmConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Arm")
	bool bDefaultAttached = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Arm")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Arm")
	TArray<FName> ArmPartNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|VFX")
	FName DetachEffectPartName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|VFX")
	TObjectPtr<UNiagaraSystem> DetachEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Materials")
	FName HighlightScalarParam = TEXT("HighlightAmount");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Input")
	FName ToggleInputActionName = TEXT("IA_ToggleArm");
};
