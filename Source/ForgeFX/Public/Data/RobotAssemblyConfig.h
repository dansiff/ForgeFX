#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "RobotAssemblyConfig.generated.h"

UENUM(BlueprintType)
enum class EHighlightMode : uint8
{
	MaterialParameter	UMETA(DisplayName = "Material Parameter"),
	CustomDepthStencil	UMETA(DisplayName = "Custom Depth / Stencil")
};

// Forward declare custom detachable part actor class
class ARobotPartActor;

USTRUCT(BlueprintType)
struct FORGEFX_API FRobotPartSpec
{
	GENERATED_BODY()

	// Unique name of this part (e.g., Torso, Head, Upperarm_Left)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PartName;

	// Parent part name (None or empty attaches to actor root)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ParentPartName;

	// Optional socket on parent to attach to (e.g., S_Upperarm_L)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ParentSocketName;

	// Mesh for this part (static for this assessment)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> Mesh;

	// Additional local offset/rotation/scale relative to the parent/socket
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform RelativeTransform = FTransform::Identity;

	// If true, this part will respond to highlight state changes
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAffectsHighlight = true;

	// Detach / modular settings
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Detach")
	bool bDetachable = true;

	// If simulate physics when this part is detached
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Detach", meta=(EditCondition="bDetachable"))
	bool bSimulatePhysicsWhenDetached = false;

	// Collision profile name to use when detached
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Detach", meta=(EditCondition="bDetachable"))
	FName DetachedCollisionProfile = TEXT("PhysicsActor");

	// Optional override actor class when detached (must be subclass of ARobotPartActor); if null default class used
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Detach", meta=(EditCondition="bDetachable"))
	TSubclassOf<ARobotPartActor> DetachedActorClass;
};

UCLASS(BlueprintType)
class FORGEFX_API URobotAssemblyConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	// All parts that make up the robot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Assembly")
	TArray<FRobotPartSpec> Parts;

	// Global effects and audio
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|VFX")
	TObjectPtr<UNiagaraSystem> DetachEffect = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|VFX")
	TObjectPtr<UNiagaraSystem> AttachEffect = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Audio")
	TObjectPtr<USoundBase> DetachSound = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Audio")
	TObjectPtr<USoundBase> AttachSound = nullptr;

	// Highlight policy for this project
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Highlight")
	EHighlightMode HighlightMode = EHighlightMode::MaterialParameter;

	// If using MaterialParameter mode, this is the parameter to set on MIDs
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Highlight", meta=(EditCondition="HighlightMode==EHighlightMode::MaterialParameter"))
	FName HighlightScalarParam = TEXT("HighlightAmount");

	// If using CustomDepthStencil mode
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Highlight", meta=(EditCondition="HighlightMode==EHighlightMode::CustomDepthStencil"))
	int32 CustomDepthStencilValue =252; //0-255
};
