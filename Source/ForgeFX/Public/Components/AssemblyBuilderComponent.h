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

	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void ApplyHighlightScalar(float Value);
	UFUNCTION(BlueprintCallable, Category="Robot|Assembly") void ApplyHighlightScalarAll(float Value);
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Assembly") TObjectPtr<URobotAssemblyConfig> AssemblyConfig;

protected:
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

	void EnsureDynamicMIDs(UStaticMeshComponent* Comp);
};
