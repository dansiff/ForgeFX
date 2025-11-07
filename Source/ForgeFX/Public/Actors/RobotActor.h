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

class UPrimitiveComponent;
class UUserWidget;
class ARobotPartActor;

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
	virtual void OnConstruction(const FTransform& Transform) override; // declare OnConstruction
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnArmAttachedChanged(bool bNowAttached);

	UFUNCTION()
	void OnHighlightedChanged(bool bNowHighlighted);

	UFUNCTION()
	void ToggleArm();

	// Component-aware interaction hooks
	UFUNCTION()
	void OnHoverComponentChanged(UPrimitiveComponent* HitComponent, AActor* HitActor);

	UFUNCTION()
	void OnInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor);

	UFUNCTION()
	void OnInteractReleased();

	void SpawnDetachVFXIfConfigured();

	// IInteractable - default behavior forwards to components
	virtual void OnHoverBegin_Implementation() override { if (Highlight) Highlight->SetHighlighted(true); }
	virtual void OnHoverEnd_Implementation() override { if (Highlight) Highlight->SetHighlighted(false); }
	virtual void OnInteract_Implementation() override { ToggleArm(); }

	// Drag helpers
	bool ComputeCursorWorldOnPlane(float PlaneZ, FVector& OutWorldPoint) const;
	bool TrySnapDraggedPartToSocket();

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

	// Mapping context priority (avoid name clash with AActor::InputPriority)
	UPROPERTY(EditAnywhere, Category="Input")
	int32 MappingPriority =0;

	// Optional status UI spawned at BeginPlay if set
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UUserWidget> StatusWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StatusWidget;

	// Drag state
	bool bDragging = false;
	float DragPlaneZ =0.f;
	FVector DragOffset = FVector::ZeroVector;

	// Part drag state (detached actors)
	UPROPERTY(Transient)
	TObjectPtr<ARobotPartActor> DraggedPartActor;

	UPROPERTY(Transient)
	FName DraggedPartName;

	bool bDraggingPart = false;
	float PartDragPlaneZ =0.f;
	FVector PartDragOffset = FVector::ZeroVector;

	// Snap tolerances
	UPROPERTY(EditAnywhere, Category="Robot|Attach")
	float AttachPosTolerance =8.f; // cm

	UPROPERTY(EditAnywhere, Category="Robot|Attach")
	float AttachAngleToleranceDeg =10.f;

	// Allow moving the whole robot by torso drag
	UPROPERTY(EditAnywhere, Category="Robot|Attach")
	bool bAllowTorsoDrag = true;
};
