#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "Engine/StaticMesh.h"
#include "NiagaraSystem.h"
#include "RobotAssemblyConfig.generated.h"

UENUM(BlueprintType)
enum class EHighlightMode : uint8
{
	MaterialParameter	UMETA(DisplayName = "Material Parameter"),
	CustomDepthStencil	UMETA(DisplayName = "Custom Depth / Stencil")
};

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
};

UCLASS(BlueprintType)
class FORGEFX_API URobotAssemblyConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// All parts that make up the robot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|Assembly")
	TArray<FRobotPartSpec> Parts;

	// Optional global effect for detach (can be overridden per-part in BP)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Robot|VFX")
	TObjectPtr<UNiagaraSystem> DetachEffect = nullptr;

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
