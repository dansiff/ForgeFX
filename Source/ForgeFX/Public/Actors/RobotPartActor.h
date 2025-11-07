#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "RobotPartActor.generated.h"

class UStaticMeshComponent;
class UHighlightComponent;

UCLASS()
class FORGEFX_API ARobotPartActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ARobotPartActor();

	// Setup this part when spawned from an assembly component
	void InitializePart(FName InPartName, UStaticMesh* InMesh, const TArray<UMaterialInterface*>& InMaterials);

	// Optional physics toggle
	UFUNCTION(BlueprintCallable, Category="Robot|Part") void EnablePhysics(bool bEnable);

	// IInteractable defaults (forward to highlight)
	virtual void OnHoverBegin_Implementation() override;
	virtual void OnHoverEnd_Implementation() override;
	virtual void OnInteract_Implementation() override {}

	// Accessors
	UFUNCTION(BlueprintPure, Category="Robot|Part") FName GetPartName() const { return PartName; }
	UFUNCTION(BlueprintPure, Category="Robot|Part") UStaticMeshComponent* GetMeshComponent() const { return Mesh; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UHighlightComponent> Highlight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Robot|Part") FName PartName;
};
