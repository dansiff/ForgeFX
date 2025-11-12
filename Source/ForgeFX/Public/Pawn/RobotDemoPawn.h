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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float BaseMaxSpeed =1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float BoostMultiplier =2.0f;
	bool bBoosting = false;

	// Raw key polling fallback (six-direction)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") bool bEnableRawKeyMovementFallback = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float RawMoveSpeed =1200.f; // used if axis mapping absent
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float RawVerticalSpeed =1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement") float RawAccelerationBlend =10.f; // smoothing factor
	FVector PendingRawInput = FVector::ZeroVector;

	// Crosshair UI (optional)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI") TSubclassOf<UUserWidget> CrosshairWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> CrosshairWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input") int32 MappingPriority =0;
};
