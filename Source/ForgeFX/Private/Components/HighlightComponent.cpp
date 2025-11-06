#include "Components/HighlightComponent.h"

UHighlightComponent::UHighlightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHighlightComponent::SetHighlighted(bool bInHighlighted)
{
	if (bHighlighted == bInHighlighted)
	{
		return;
	}
	bHighlighted = bInHighlighted;
	OnHighlightedChanged.Broadcast(bHighlighted);
	if (bHighlighted)
	{
		BP_OnHighlighted();
	}
	else
	{
		BP_OnUnhighlighted();
	}
}
