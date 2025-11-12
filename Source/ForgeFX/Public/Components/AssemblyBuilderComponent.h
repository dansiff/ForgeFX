#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RobotAssemblyConfig.h"
#include "Components/StaticMeshComponent.h"
#include "AssemblyBuilderComponent.generated.h"

class UMaterialInstanceDynamic;
class ARobotPartActor;

USTRUCT()
struct FORGEFX_API FDynamicMIDArray
{
	GENERATED_BODY()
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
};

UCLASS(ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API UAssemblyBuilderComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAssemblyBuilderComponent();

	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void BuildAssembly();
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void ClearAssembly();

	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void ApplyHighlightScalar(float Value); // all
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void ApplyHighlightScalarAll(float Value); // alias
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void ApplyHighlightScalarToParts(const TArray<FName>& PartNames, float Value);

	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") bool SetPartVisibility(FName PartName, bool bVisible);
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool GetPartWorldLocation(FName PartName, FVector& OutLocation) const;
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") UStaticMeshComponent* GetPartByName(FName PartName) const;
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool FindPartNameByComponent(const UPrimitiveComponent* Comp, FName& OutName) const;

	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") bool DetachPart(FName PartName, ARobotPartActor*& OutActor);
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") bool ReattachPart(FName PartName, ARobotPartActor* PartActor);
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool IsPartDetached(FName PartName) const { return DetachedParts.Contains(PartName); }
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool GetPartSpec(FName PartName, FRobotPartSpec& OutSpec) const;
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool GetAttachParentAndSocket(FName PartName, USceneComponent*& OutParent, FName& OutSocket) const;
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void SetDetachEnabledForParts(const TArray<FName>& PartNames, bool bEnabled);
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void SetDetachEnabledForAll(bool bEnabled);
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool IsDetachEnabled(FName PartName) const;
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") bool AttachDetachedPartTo(FName PartName, ARobotPartActor* PartActor, USceneComponent* NewParent, FName SocketName);
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool FindNearestAttachTarget(const FVector& AtWorldLocation, USceneComponent*& OutParent, FName& OutSocket, float& OutDistance) const;
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") void GetAllAttachTargets(TArray<USceneComponent*>& OutTargets) const;
	UFUNCTION(BlueprintPure, Category="Robot|Assembly") bool IsDetachable(FName PartName) const;
	UFUNCTION(Exec) void RebuildAssembly();

	// Optional: strong hover override via material swap
	UPROPERTY(EditAnywhere, Category="Robot|Highlight") bool bUseHoverHighlightMaterial = false;
	UPROPERTY(EditAnywhere, Category="Robot|Highlight") TObjectPtr<UMaterialInterface> HoverHighlightMaterial;
	UFUNCTION(BlueprintCallable, Category="Robot|Highlight") void ApplyHoverOverride(UPrimitiveComponent* HoveredComp);
	UFUNCTION(BlueprintCallable, Category="Robot|Highlight") void ClearHoverOverride();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Assembly") TObjectPtr<URobotAssemblyConfig> AssemblyConfig;

	// Highlight smoothing speed (units per second toward target value)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Highlight") float HighlightInterpSpeed =12.f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	bool IsDetachableNow(FName PartName) const;

private:
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UStaticMeshComponent>> NameToComponent;
	UPROPERTY(Transient) TMap<TObjectPtr<UStaticMeshComponent>, FName> ComponentToName;
	UPROPERTY(Transient) TMap<FName, bool> PartAffectsHighlight;
	UPROPERTY(Transient) TMap<TObjectPtr<UStaticMeshComponent>, FDynamicMIDArray> DynamicMIDs;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<ARobotPartActor>> DetachedParts;
	UPROPERTY(Transient) TMap<FName, bool> DetachEnabledOverride;
	UPROPERTY(Transient) TMap<FName, TWeakObjectPtr<USceneComponent>> ParentOverride;
	UPROPERTY(Transient) TMap<FName, FName> SocketOverride;
	// Highlight interpolation maps
	TMap<TWeakObjectPtr<UStaticMeshComponent>, float> CurrentHighlight;
	TMap<TWeakObjectPtr<UStaticMeshComponent>, float> TargetHighlight;

	// Hover material override original cache
	TWeakObjectPtr<UStaticMeshComponent> CurrentHoverComp;
	TMap<TWeakObjectPtr<UStaticMeshComponent>, TArray<TObjectPtr<UMaterialInterface>>> SavedMaterials;

	void EnsureDynamicMIDs(UStaticMeshComponent* Comp);
};
