#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/RobotArmComponent.h"
#include "Components/HighlightComponent.h"
#include "Components/AssemblyBuilderComponent.h"
#include "Interfaces/Interactable.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "RobotActor.generated.h"

/**
 * Base actor composing the robot pieces; intended to be subclassed in Blueprint.
 * Owns Arm, Highlight, and Assembly components and wires their interactions.
 */
UCLASS()
class FORGEFX_API ARobotActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ARobotActor();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnArmAttachedChanged(bool bNowAttached);

	UFUNCTION()
	void OnHighlightedChanged(bool bNowHighlighted);

	UFUNCTION()
	void ToggleArm();

	void SpawnDetachVFXIfConfigured();

	// IInteractable - default behavior forwards to components
	virtual void OnHoverBegin_Implementation() override { if (Highlight) Highlight->SetHighlighted(true); }
	virtual void OnHoverEnd_Implementation() override { if (Highlight) Highlight->SetHighlighted(false); }
	virtual void OnInteract_Implementation() override { ToggleArm(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot")
	TObjectPtr<URobotArmComponent> Arm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot")
	TObjectPtr<UHighlightComponent> Highlight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot")
	TObjectPtr<UAssemblyBuilderComponent> Assembly;

	// Optional Enhanced Input setup from C++ (can be left null to prefer BP)
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputMappingContext> InputContext;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> ToggleArmAction;

	UPROPERTY(EditAnywhere, Category="Input")
	int32 InputPriority =0;
};
