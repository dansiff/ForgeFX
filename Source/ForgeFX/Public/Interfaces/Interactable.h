#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(BlueprintType)
class FORGEFX_API UInteractable : public UInterface
{
	GENERATED_BODY()
};

class FORGEFX_API IInteractable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")	void OnHoverBegin();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")	void OnHoverEnd();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")	void OnInteract();
};
