#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PartInteractionComponent.generated.h"
class UAssemblyBuilderComponent; class ARobotPartActor; class UPrimitiveComponent;

UCLASS(ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API UPartInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPartInteractionComponent();

	// Drag state accessors
	UFUNCTION(BlueprintPure, Category="PartInteraction") bool IsDraggingPart() const { return bDraggingPart; }
	UFUNCTION(BlueprintCallable, Category="PartInteraction") void ForceDropHeldPart(bool bTrySnap);

	// Handle an interact press from RobotActor
	bool HandleInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor, bool bAllowFreeAttach, float AttachPosTolerance, float AttachAngleToleranceDeg, float PartGrabMinDistance, float PartGrabMaxDistance);
	void HandleInteractReleased(bool bHoldToDragMode, bool bAllowFreeAttach);

	// Per-frame update
	void TickPartDrag(float DeltaSeconds, float PartDragSmoothingSpeed, float InPartGrabDistance, float PartGrabMinDistance, float PartGrabMaxDistance);

	// Adjust distance (mouse wheel)
	void AdjustGrabDistance(float Delta, float PartGrabMinDistance, float PartGrabMaxDistance);

	float GetPartGrabDistance() const { return PartGrabDistance; }
	void SetPartGrabDistance(float V) { PartGrabDistance = V; }

	UFUNCTION(BlueprintPure, Category="PartInteraction") FName GetDraggedPartName() const { return bDraggingPart ? DraggedPartName : NAME_None; }
	UFUNCTION(BlueprintPure, Category="PartInteraction") ARobotPartActor* GetDraggedPartActor() const { return bDraggingPart ? DraggedPartActor : nullptr; }

private:
	UAssemblyBuilderComponent* GetAssembly() const;
	bool TrySnapDragged(float AttachPosTolerance, float AttachAngleToleranceDeg);
	bool TryFreeAttachDragged(bool bAllowFreeAttach, float FreeAttachMaxDistance, float AttachPosTolerance);

private:
	UPROPERTY(Transient) TObjectPtr<ARobotPartActor> DraggedPartActor;
	UPROPERTY(Transient) FName DraggedPartName;
	bool bDraggingPart = false;
	float PartGrabDistance =100.f;
};