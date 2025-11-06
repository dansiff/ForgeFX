#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RobotAssemblyConfig.h"
#include "Components/StaticMeshComponent.h"
#include "AssemblyBuilderComponent.generated.h"

class UMaterialInstanceDynamic;

USTRUCT()
struct FORGEFX_API FDynamicMIDArray
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
};

/**
 * Spawns and attaches mesh components according to a URobotAssemblyConfig.
 * Designed for modular assembly from provided assets.
 */
UCLASS(ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API UAssemblyBuilderComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAssemblyBuilderComponent();

	UFUNCTION(BlueprintCallable, Category="Robot|Assembly")
	void BuildAssembly();

	UFUNCTION(BlueprintCallable, Category="Robot|Assembly")
	void ClearAssembly();

	// Apply highlight scalar to all parts that opt-in (see FRobotPartSpec::bAffectsHighlight)
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly")
	void ApplyHighlightScalar(float Value);

	// Toggle visibility of a single part by logical name
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly")
	bool SetPartVisibility(FName PartName, bool bVisible);

	// Query world-space location of a part (for VFX spawn, etc.)
	UFUNCTION(BlueprintPure, Category="Robot|Assembly")
	bool GetPartWorldLocation(FName PartName, FVector& OutLocation) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Assembly")
	TObjectPtr<URobotAssemblyConfig> AssemblyConfig;

	// Return named spawned component for BP access
	UFUNCTION(BlueprintPure, Category="Robot|Assembly")
	UStaticMeshComponent* GetPartByName(FName PartName) const;

private:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMeshComponent>> NameToComponent;

	// Cache whether a part participates in highlight operations
	UPROPERTY(Transient)
	TMap<FName, bool> PartAffectsHighlight;

	// Cache dynamic material instances for quick parameter updates
	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMeshComponent>, FDynamicMIDArray> DynamicMIDs;

	void EnsureDynamicMIDs(UStaticMeshComponent* Comp);
};
