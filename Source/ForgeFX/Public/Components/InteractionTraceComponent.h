#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Interactable.h"
#include "InteractionTraceComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoverComponentChanged, UPrimitiveComponent*, HitComponent, AActor*, HitActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractPressedSig, UPrimitiveComponent*, HitComponent, AActor*, HitActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractReleasedSig);

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
	void InteractPressed();
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void InteractReleased();

	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnHoverComponentChanged OnHoverComponentChanged;
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractPressedSig OnInteractPressed;
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractReleasedSig OnInteractReleased;

private:
	TWeakInterfacePtr<IInteractable> CurrentHover;
	TWeakObjectPtr<UPrimitiveComponent> CurrentHitComp;
	void UpdateHover(const TWeakInterfacePtr<IInteractable>& NewHover, UPrimitiveComponent* NewComp, AActor* HitActor);
	bool bInteractHeld = false;
};
