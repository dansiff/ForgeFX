#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Interactable.h"
#include "InteractionTraceComponent.generated.h"

/**
 * Simple camera-forward trace that calls IInteractable on hit.
 * Intended for PIE demo without needing a custom controller.
 */
UCLASS(ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API UInteractionTraceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UInteractionTraceComponent();

	// Max trace distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	float TraceDistance =500.f;

	// Line trace every frame to drive hover (kept cheap)
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void BeginPlay() override;

	// Call to perform interact on current target (bound to input in BP or code)
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void Interact();

private:
	TWeakInterfacePtr<IInteractable> CurrentHover;
	void UpdateHover(const TWeakInterfacePtr<IInteractable>& NewHover);
};
