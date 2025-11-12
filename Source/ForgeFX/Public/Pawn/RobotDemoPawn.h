#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "RobotDemoPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UFloatingPawnMovement;
class UInputMappingContext;
class UInputAction;
class UInteractionTraceComponent;
class UUserWidget;
class ARobotActor; // forward

UCLASS()
class FORGEFX_API ARobotDemoPawn : public APawn
{
	GENERATED_BODY()

public:
	ARobotDemoPawn();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override; // added

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void InteractPress(const FInputActionValue& Value);
	void InteractRelease(const FInputActionValue& Value);
	void InteractAltPress(const FInputActionValue& Value);
	void UpDown(const FInputActionValue& Value);
	void BoostOn(const FInputActionValue& Value);
	void BoostOff(const FInputActionValue& Value);
	void PollRawMovementKeys(float DeltaSeconds); // fallback

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UFloatingPawnMovement> Movement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UInteractionTraceComponent> Interaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputMappingContext> InputContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> InteractAltAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> UpDownAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> BoostAction;

	// Speed settings
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float BaseMaxSpeed =800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float BoostMultiplier =1.5f;
	bool bBoosting = false;

	// Raw key polling fallback (six-direction)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") bool bEnableRawKeyMovementFallback = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float RawMoveSpeed =800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float RawVerticalSpeed =600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float RawAccelerationBlend =6.f; // smoothing factor (lower = smoother)
	FVector PendingRawInput = FVector::ZeroVector;

	// Raw interact fallback (only if no Enhanced Input action set)
	UPROPERTY(EditAnywhere, Category="Input") bool bEnableRawInteractFallback = true;

	// Crosshair UI (optional)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI") TSubclassOf<UUserWidget> CrosshairWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> CrosshairWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") int32 MappingPriority =0;

	// Optional: disable verbose movement logs if needed
	UPROPERTY(EditAnywhere, Category="Debug") bool bLogMovementAxis = false;
};
