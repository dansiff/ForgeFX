#include "Actors/PostProcessHelper.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInterface.h"

APostProcessHelper::APostProcessHelper()
{
	PrimaryActorTick.bCanEverTick = false;
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	SetRootComponent(PostProcess);
	PostProcess->bUnbound = true; // infinite extent
}

void APostProcessHelper::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PostProcess->Settings.WeightedBlendables.Array.Reset();
	if (OutlineMaterial)
	{
		FWeightedBlendable WB; WB.Weight = OutlineIntensity; WB.Object = OutlineMaterial; 
		PostProcess->Settings.WeightedBlendables.Array.Add(WB);
	}
}
