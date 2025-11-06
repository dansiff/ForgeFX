#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RobotArmConfig.h"
#include "RobotArmComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArmAttachedChanged, bool, bAttached);

/**
 * Component managing a robot arm attachment state.
 * Pure logic, no visuals. Blueprint-callable for UI/Blueprint glue.
 */
UCLASS(BlueprintType, ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API URobotArmComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URobotArmComponent();

	virtual void OnRegister() override;

	UFUNCTION(BlueprintCallable, Category="Robot|Arm") void AttachToRobot();
	UFUNCTION(BlueprintCallable, Category="Robot|Arm") void DetachFromRobot();
	UFUNCTION(BlueprintPure, Category="Robot|Arm") bool IsAttached() const { return bAttached; }

	UFUNCTION(BlueprintImplementableEvent, Category="Robot|Arm") void BP_OnAttached();
	UFUNCTION(BlueprintImplementableEvent, Category="Robot|Arm") void BP_OnDetached();

	// Broadcast whenever the logical attached state changes
	UPROPERTY(BlueprintAssignable, Category="Robot|Arm")
	FOnArmAttachedChanged OnAttachedChanged;

	// Optional data-driven configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Arm")
	TObjectPtr<URobotArmConfig> Config;

private:
	UPROPERTY(EditAnywhere, Category="Robot|Arm") bool bAttached = true;
};
