#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PostProcessHelper.generated.h"

class UPostProcessComponent;
class UMaterialInterface;

UCLASS()
class FORGEFX_API APostProcessHelper : public AActor
{
	GENERATED_BODY()
public:
	APostProcessHelper();

protected:
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UPostProcessComponent> PostProcess;

	// Assign your PP outline material (Domain: Post Process) here from the editor
	UPROPERTY(EditAnywhere, Category="PostProcess") TObjectPtr<UMaterialInterface> OutlineMaterial;

	// Intensity multiplier to tune effect strength if your material supports a scalar parameter
	UPROPERTY(EditAnywhere, Category="PostProcess") float OutlineIntensity =1.0f;

	virtual void OnConstruction(const FTransform& Transform) override;
};