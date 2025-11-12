#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrbitCameraRig.generated.h"

class UCameraComponent; 
class USplineComponent;

UCLASS()
class FORGEFX_API AOrbitCameraRig : public AActor
{
	GENERATED_BODY()
public:
	AOrbitCameraRig();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit") float Radius =500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit") float Height =120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit") float SpeedDegPerSec =30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit") float FOV =70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit") bool bEaseInOutSpeed = true;
	// Disable continuous spin unless explicitly enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit") bool bAutoOrbit = false;

	UFUNCTION(BlueprintCallable, Category="Orbit") void Setup(AActor* InTarget, float InRadius, float InHeight, float InSpeed);
	UFUNCTION(BlueprintCallable, Category="Orbit") void UseCircularSplinePath(int32 NumPoints =8);
	// Manual stepping (positive = clockwise, negative = counter-clockwise)
	UFUNCTION(BlueprintCallable, Category="Orbit") void StepManual(float DeltaDegrees);

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USplineComponent> Spline;
	TWeakObjectPtr<AActor> Target;
	float AngleDeg =0.f; 
	float PathT =0.f; 
	bool bUseSpline = false;
};