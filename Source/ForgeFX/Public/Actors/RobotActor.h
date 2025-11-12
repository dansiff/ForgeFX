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
class UInteractionTraceComponent;
class USphereComponent;

UENUM(BlueprintType)
enum class EDetachInteractMode : uint8
{
	HoldToDrag UMETA(DisplayName="Hold To Drag"),
	ToggleToDrag UMETA(DisplayName="Toggle To Drag"),
	ClickToggleAttach UMETA(DisplayName="Click Toggle Attach")
};

UCLASS()
class FORGEFX_API ARobotActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ARobotActor();

	// Debug console helpers
	UFUNCTION(Exec) void DetachPartByName(const FString& PartName);
	UFUNCTION(Exec) void ReattachPartByName(const FString& PartName);
	UFUNCTION(Exec) void ListRobotParts();
	UFUNCTION(Exec) void TraceTest();
	UFUNCTION(Exec) void StartShowcase();
	UFUNCTION(Exec) void DumpAssemblyState();
	UFUNCTION(Exec) void EnforceHideForDetached();

	UFUNCTION(BlueprintPure, Category="Robot|Debug") FName GetHoveredPart() const { return HoveredPartName; }
	UFUNCTION(BlueprintPure, Category="Robot|Debug") EDetachInteractMode GetDetachMode() const { return DetachMode; }
	UFUNCTION(BlueprintPure, Category="Robot|Debug") bool IsDraggingPart() const { return bDraggingPart; }
	UFUNCTION(BlueprintPure, Category="Robot|Debug") bool IsDraggingRobot() const { return bDragging; }

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION() void OnArmAttachedChanged(bool bNowAttached);
	UFUNCTION() void OnHighlightedChanged(bool bNowHighlighted);
	UFUNCTION() void ToggleArm();
	UFUNCTION() void OnHoverComponentChanged(UPrimitiveComponent* HitComponent, AActor* HitActor);
	UFUNCTION() void OnInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor);
	UFUNCTION() void OnInteractReleased();

	void SpawnDetachVFXIfConfigured();
	virtual void OnHoverBegin_Implementation() override { if (Highlight) Highlight->SetHighlighted(true); }
	virtual void OnHoverEnd_Implementation() override { if (Highlight) Highlight->SetHighlighted(false); }
	virtual void OnInteract_Implementation() override { ToggleArm(); }

	bool ComputeCursorWorldOnPlane(float PlaneZ, FVector& OutWorldPoint) const;
	bool TrySnapDraggedPartToSocket();
	bool TryFreeAttachDraggedPart();
	UFUNCTION() void ScrambleParts();

	void TryBindToPlayerTrace();
	void PollFallbackKeys(float DeltaSeconds);
	bool TryDetachUnderCrosshair(bool bDrag, FName* OutPartName = nullptr);

	private:
		void ShowcaseStep();
		void EndShowcase();

	private:
	FTimerHandle ShowcaseTimer;
	int32 ShowcaseIndex = -1;
	TArray<FName> ShowcaseOrder;
	TWeakObjectPtr<class AOrbitCameraRig> ShowcaseRig;

	protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot") TObjectPtr<URobotArmComponent> Arm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot") TObjectPtr<UHighlightComponent> Highlight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot") TObjectPtr<UAssemblyBuilderComponent> Assembly;

	UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<UInputMappingContext> InputContext;
	UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<UInputAction> ToggleArmAction;
	UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<UInputAction> ScrambleAction;
	UPROPERTY(EditAnywhere, Category="Input") int32 MappingPriority =0;

	UPROPERTY(EditAnywhere, Category="UI") TSubclassOf<UUserWidget> StatusWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> StatusWidget;
	UPROPERTY(EditAnywhere, Category="UI") TSubclassOf<UUserWidget> DebugWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> DebugWidget;
	UPROPERTY(EditAnywhere, Category="UI") TSubclassOf<UUserWidget> ShowcaseWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> ShowcaseWidget;

	UPROPERTY(EditAnywhere, Category="Robot|Interact") EDetachInteractMode DetachMode = EDetachInteractMode::HoldToDrag;

	bool bDragging = false;
	float DragPlaneZ =0.f;
	FVector DragOffset = FVector::ZeroVector;
	UPROPERTY(Transient) TObjectPtr<ARobotPartActor> DraggedPartActor;
	UPROPERTY(Transient) FName DraggedPartName;
	bool bDraggingPart = false;
	float PartDragPlaneZ =0.f;
	FVector PartDragOffset = FVector::ZeroVector;
	// Smoothing
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float AttachPosTolerance =8.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float AttachAngleToleranceDeg =10.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") bool bAllowTorsoDrag = true;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") bool bAllowFreeAttach = true;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float FreeAttachMaxDistance =25.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float DragSmoothingSpeed =10.f; // slower drag for robot move
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartDragSmoothingSpeed =8.f; // slower drag for parts
	// Arrow-key nudge while dragging
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float ArrowNudgeSpeed =150.f; // cm/s
	FVector DragArrowAccum = FVector::ZeroVector;

	// Skyrim-style grab
	UPROPERTY(EditAnywhere, Category="Robot|Attach") bool bUseSkyrimGrab = true;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartGrabDistance =100.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartGrabMinDistance =40.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartGrabMaxDistance =300.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartGrabDistanceStep =10.f;

	UPROPERTY(EditAnywhere, Category="Robot|Scramble") int32 ScrambleIterations =1;
	UPROPERTY(EditAnywhere, Category="Robot|Scramble") bool bScramblePhysicsEnable = false;
	UPROPERTY(EditAnywhere, Category="Robot|Scramble") float ScrambleSocketSearchRadius =30.f;

	UPROPERTY(Transient) FName HoveredPartName = NAME_None;
	bool bTraceBound = false;
	TWeakObjectPtr<UInteractionTraceComponent> CachedTrace;

	// Tap vs hold timing and detach cooldown
	float InteractPressTime =0.f;
	bool bInteractCurrentlyHeld = false;
	UPROPERTY(EditAnywhere, Category="Robot|Interact") float TapThreshold =0.20f;
	UPROPERTY(EditAnywhere, Category="Robot|Interact") float DetachCooldownSeconds =0.20f;
	TMap<FName,float> LastDetachTime;

	// Preview indicator component for snap target
	UPROPERTY(VisibleAnywhere, Category="Robot|Preview") TObjectPtr<USphereComponent> PreviewSocketIndicator;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") float PreviewSphereRadius =6.f;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") FLinearColor PreviewSnapColorGood = FLinearColor::Green;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") FLinearColor PreviewSnapColorBad = FLinearColor::Red;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") float MouseWheelDistanceStep =40.f; // cm per wheel notch toward/away camera
	// Allow disabling the visual indicator entirely
	UPROPERTY(EditAnywhere, Category="Robot|Preview") bool bShowSnapPreviewIndicator = false;

	UPROPERTY(EditAnywhere, Category="Showcase") bool bShowcaseUseSpline = true;
	UPROPERTY(EditAnywhere, Category="Showcase") int32 ShowcaseSplinePoints =16;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseOrbitRadius =650.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseOrbitHeight =150.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseOrbitSpeedDeg =30.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseDetachInterval =0.6f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseInitialDelay =0.5f;

	// Quiet noisy logs unless enabled
	UPROPERTY(EditAnywhere, Category="Debug") bool bEnableRobotDebugLogs = false;

	UFUNCTION(BlueprintCallable, Category="Robot|Interact") void ForceDropHeldPart(bool bTrySnap=true);
};
