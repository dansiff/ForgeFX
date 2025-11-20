#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RobotShowcaseComponent.generated.h"
class UAssemblyBuilderComponent; class AOrbitCameraRig; class ARobotPartActor; class ARobotActor;

UCLASS(ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API URobotShowcaseComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URobotShowcaseComponent();
	UFUNCTION(BlueprintCallable, Category="Showcase") void StartShowcase();
	UFUNCTION(BlueprintCallable, Category="Showcase") void StopShowcase();
	UFUNCTION(BlueprintPure, Category="Showcase") bool IsShowcaseActive() const { return bActive; }
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	void StepShowcase();
	UAssemblyBuilderComponent* GetAssembly() const;
private:
	bool bActive = false; int32 Index = -1; TArray<FName> Order; float Elapsed =0.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float DetachInterval =0.6f;
	UPROPERTY(EditAnywhere, Category="Showcase") float InitialDelay =0.5f;
	UPROPERTY(EditAnywhere, Category="Showcase") float OrbitRadius =650.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float OrbitHeight =150.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float OrbitSpeedDeg =30.f;
	UPROPERTY(EditAnywhere, Category="Showcase") int32 SplinePoints =16;
	UPROPERTY(EditAnywhere, Category="Showcase") bool bUseSpline = true;
	TWeakObjectPtr<AOrbitCameraRig> Rig;
};