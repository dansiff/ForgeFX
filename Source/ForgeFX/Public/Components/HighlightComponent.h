#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HighlightComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHighlightedChanged, bool, bHighlighted);

/**
 * Holds highlight state and exposes Blueprint events for VFX hookup.
 */
UCLASS(BlueprintType, ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API UHighlightComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHighlightComponent();

	UFUNCTION(BlueprintCallable, Category="Highlight") void SetHighlighted(bool bInHighlighted);
	UFUNCTION(BlueprintPure, Category="Highlight") bool IsHighlighted() const { return bHighlighted; }

	// Event hooks for Blueprint to drive materials and Niagara
	UFUNCTION(BlueprintImplementableEvent, Category="Highlight") void BP_OnHighlighted();
	UFUNCTION(BlueprintImplementableEvent, Category="Highlight") void BP_OnUnhighlighted();

	// Multicast to allow C++/BP listeners to react to highlight changes
	UPROPERTY(BlueprintAssignable, Category="Highlight")
	FOnHighlightedChanged OnHighlightedChanged;

private:
	UPROPERTY(VisibleAnywhere, Category="Highlight") bool bHighlighted = false;
};
