#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Components/RobotArmComponent.h"
#include "RobotActor.generated.h"

class UPrimitiveComponent; class UUserWidget; class UTextBlock; class ARobotPartActor; class UInteractionTraceComponent; class USphereComponent; class UHighlightComponent; class UAssemblyBuilderComponent; class UCinematicAssembleComponent; class UPartInteractionComponent; class URobotShowcaseComponent; class AOrbitCameraRig;

UENUM(BlueprintType)
enum class EDetachInteractMode : uint8 { HoldToDrag, ToggleToDrag, ClickToggleAttach };

UCLASS()
class FORGEFX_API ARobotActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ARobotActor();
	// Exec/debug helpers
	UFUNCTION(Exec) void DetachPartByName(const FString& PartName);
	UFUNCTION(Exec) void ReattachPartByName(const FString& PartName);
	UFUNCTION(Exec) void ListRobotParts();
	UFUNCTION(Exec) void TraceTest();
	UFUNCTION(Exec) void DumpAssemblyState();
	UFUNCTION(Exec) void EnforceHideForDetached();

	// Showcase control
	UFUNCTION(BlueprintCallable, Category="Showcase") void StartShowcase();
	UFUNCTION(BlueprintCallable, Category="Showcase") void StopShowcase();

	// Accessors
	UFUNCTION(BlueprintPure) FName GetHoveredPart() const { return HoveredPartName; }
	UFUNCTION(BlueprintPure) EDetachInteractMode GetDetachMode() const { return DetachMode; }
	UFUNCTION(BlueprintPure) bool IsDraggingPart() const { return bDraggingPart; }
	UFUNCTION(BlueprintPure) bool IsDraggingRobot() const { return bDragging; }
	UFUNCTION(BlueprintPure) bool IsShowcaseActive() const { return bShowcaseActive; }

	// Arm helpers
	UFUNCTION(BlueprintCallable, Category="Robot|Arm") void AttachArm();
	UFUNCTION(BlueprintCallable, Category="Robot|Arm") void DetachArm();
	UFUNCTION(BlueprintPure, Category="Robot|Arm") bool IsArmAttached() const { return bArmAttached; }

	// Interaction / actions
	UFUNCTION(BlueprintCallable, Category="Robot|Interact") void ForceDropHeldPart(bool bTrySnap=true);
	UFUNCTION(BlueprintCallable, Category="Robot|Scramble") void ScrambleParts();
	UFUNCTION(BlueprintCallable, Category="Robot|Interact") void TriggerCinematicAssemble(FVector NewLocation);
	UFUNCTION(BlueprintCallable, Category="Robot|Interact") bool ForceCrosshairDetach(bool bDrag=true);

	// Testing & automation helpers
	UFUNCTION(BlueprintPure, Category="Robot|Test") TArray<FName> GetDetachableParts() const;
	UFUNCTION(BlueprintPure, Category="Robot|Test") bool IsPartCurrentlyDetached(FName PartName) const;
	UFUNCTION(BlueprintCallable, Category="Robot|Test") bool DetachPartForTest(FName PartName);
	UFUNCTION(BlueprintCallable, Category="Robot|Test") bool ReattachPartForTest(FName PartName);
	UFUNCTION(BlueprintCallable, Category="Robot|Selection") void BatchDetachSelected();
	UFUNCTION(BlueprintCallable, Category="Robot|Selection") void BatchReattachSelected();
	UFUNCTION(BlueprintCallable, Category="Robot|Selection") void ClearSelection();
	UFUNCTION(BlueprintCallable, Category="Robot|Selection") void TogglePartSelection(FName PartName);

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
	UFUNCTION() void OnAssemblyPartDetached(FName PartName, ARobotPartActor* SpawnedActor);
	UFUNCTION() void OnAssemblyPartReattached(FName PartName);

	bool ComputeCursorWorldOnPlane(float PlaneZ, FVector& OutWorldPoint) const;
	void TryBindToPlayerTrace();
	void PollFallbackKeys(float DeltaSeconds);

private:
	void SetStatusMessage(const FString& Msg);
	void ShowPrompt(const FString& Msg, float DurationSeconds =2.f);
	void ClearPrompt();
	void SetCustomDepthAll(bool bEnable);
	void UpdateStatusText(bool bAttached);
	void SpawnDetachVFXIfConfigured();
	void UpdateReattachPreview();
	void UpdateSnapReadiness();
	void UpdateSocketInfoWidget(const FVector& SocketWorldLoc, const FName& SocketName, bool bReady);
	void ReattachAllDetached();
	void PositionSocketInfoWidget(const FVector2D& ScreenPos);
	void UpdateSnapMaterialParams();

	// Selection and UI enhancements
	UPROPERTY(Transient) TArray<FName> SelectedParts;
	UPROPERTY(EditAnywhere, Category="Robot|UI") TSubclassOf<UUserWidget> SocketInfoWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> SocketInfoWidget;
	FName LastPreviewSocketName = NAME_None;
	bool bSnapReady = false;

protected:
	// Components
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<URobotArmComponent> Arm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHighlightComponent> Highlight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAssemblyBuilderComponent> Assembly;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCinematicAssembleComponent> Cinematic;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPartInteractionComponent> PartInteraction;
	UPROPERTY(VisibleAnywhere) TObjectPtr<URobotShowcaseComponent> Showcase;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> PreviewSocketIndicator;

	// Input
	UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<UInputMappingContext> InputContext;
	UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<UInputAction> ToggleArmAction;
	UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<UInputAction> ScrambleAction;
	UPROPERTY(EditAnywhere, Category="Input") int32 MappingPriority =0;

	// UI
	UPROPERTY(EditAnywhere, Category="UI") TSubclassOf<UUserWidget> StatusWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> StatusWidget;
	UPROPERTY(EditAnywhere, Category="UI") TSubclassOf<UUserWidget> DebugWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> DebugWidget;
	UPROPERTY(EditAnywhere, Category="UI") TSubclassOf<UUserWidget> ShowcaseWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> ShowcaseWidget;

	// Config
	UPROPERTY(EditAnywhere, Category="Robot|Interact") EDetachInteractMode DetachMode = EDetachInteractMode::HoldToDrag;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float AttachPosTolerance =8.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float AttachAngleToleranceDeg =10.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") bool bAllowTorsoDrag = true;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") bool bAllowFreeAttach = true;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float FreeAttachMaxDistance =25.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float DragSmoothingSpeed =10.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartDragSmoothingSpeed =8.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float ArrowNudgeSpeed =150.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") bool bUseSkyrimGrab = true;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartGrabMinDistance =40.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartGrabMaxDistance =300.f;
	UPROPERTY(EditAnywhere, Category="Robot|Attach") float PartGrabDistanceStep =10.f;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") float PreviewSphereRadius =6.f;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") bool bShowSnapPreviewIndicator = false;
	UPROPERTY(EditAnywhere, Category="Robot|Scramble") int32 ScrambleIterations =1;
	UPROPERTY(EditAnywhere, Category="Robot|Scramble") bool bScramblePhysicsEnable = false;
	UPROPERTY(EditAnywhere, Category="Robot|Scramble") float ScrambleSocketSearchRadius =30.f;
	UPROPERTY(EditAnywhere, Category="Showcase") bool bShowcaseUseSpline = true;
	UPROPERTY(EditAnywhere, Category="Showcase") int32 ShowcaseSplinePoints =16;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseOrbitRadius =650.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseOrbitHeight =150.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseOrbitSpeedDeg =30.f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseDetachInterval =0.6f;
	UPROPERTY(EditAnywhere, Category="Showcase") float ShowcaseInitialDelay =0.5f;
	UPROPERTY(EditAnywhere, Category="Showcase") float DetachCooldownSeconds =0.2f;
	UPROPERTY(EditAnywhere, Category="Debug") bool bEnableRobotDebugLogs = false;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") UMaterialInterface* ReattachPreviewMaterial;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") float PreviewPulseSpeed =2.f;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") UMaterialInterface* SelectedPreviewMaterial;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") FLinearColor SnapReadyColor = FLinearColor::Green;
	UPROPERTY(EditAnywhere, Category="Robot|Preview") FLinearColor SnapNotReadyColor = FLinearColor::Red;
	UPROPERTY(Transient) TWeakObjectPtr<UStaticMeshComponent> CurrentPreviewComp;
	UPROPERTY(Transient) TMap<TWeakObjectPtr<UStaticMeshComponent>, UMaterialInterface*> PreviewOriginalMaterials;
	UPROPERTY(Transient) TMap<TWeakObjectPtr<UStaticMeshComponent>, UMaterialInstanceDynamic*> PreviewMIDs;
	float PreviewAccumTime =0.f;

	// State
	UPROPERTY(Transient) FName HoveredPartName = NAME_None;
	UPROPERTY(Transient) bool bDragging = false;
	UPROPERTY(Transient) bool bDraggingPart = false;
	UPROPERTY(Transient) bool bShowcaseActive = false;
	UPROPERTY(Transient) bool bArmAttached = true;
	float DragPlaneZ =0.f; FVector DragOffset = FVector::ZeroVector;
	UPROPERTY(Transient) TObjectPtr<ARobotPartActor> DraggedPartActor; UPROPERTY(Transient) FName DraggedPartName = NAME_None; UPROPERTY(Transient) float PartGrabDistance =100.f;
	TMap<FName,float> LastDetachTime;
	TArray<FName> ShowcaseOrder; int32 ShowcaseIndex = -1; TWeakObjectPtr<AOrbitCameraRig> ShowcaseRig; FTimerHandle ShowcaseTimer;

	// Prompt state
	float PromptExpiryTime =0.f; bool bPromptVisible=false; FString LastPrompt;
	// Trace binding
	bool bTraceBound = false; TWeakObjectPtr<UInteractionTraceComponent> CachedTrace;
};
