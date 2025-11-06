#include "Components/AssemblyBuilderComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

UAssemblyBuilderComponent::UAssemblyBuilderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAssemblyBuilderComponent::ClearAssembly()
{
	for (auto& Pair : NameToComponent)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	NameToComponent.Empty();
	PartAffectsHighlight.Empty();
	DynamicMIDs.Empty();
}

void UAssemblyBuilderComponent::BuildAssembly()
{
	ClearAssembly();
	if (!AssemblyConfig)
	{
		return;
	}

	for (const FRobotPartSpec& Spec : AssemblyConfig->Parts)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(GetOwner());
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->RegisterComponent();
		Comp->SetStaticMesh(Spec.Mesh.LoadSynchronous());
		Comp->SetRelativeTransform(Spec.RelativeTransform);

		// CustomDepth policy
		if (AssemblyConfig->HighlightMode == EHighlightMode::CustomDepthStencil)
		{
			Comp->SetRenderCustomDepth(true);
			Comp->SetCustomDepthStencilValue(AssemblyConfig->CustomDepthStencilValue);
			Comp->MarkRenderStateDirty();
		}

		USceneComponent* Parent = nullptr;
		if (Spec.ParentPartName.IsNone())
		{
			Parent = GetOwner()->GetRootComponent();
		}
		else if (UStaticMeshComponent** Found = NameToComponent.Find(Spec.ParentPartName))
		{
			Parent = *Found;
		}
		if (!Parent)
		{
			Parent = GetOwner()->GetRootComponent();
		}

		Comp->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, Spec.ParentSocketName);
		NameToComponent.Add(Spec.PartName, Comp);
		PartAffectsHighlight.Add(Spec.PartName, Spec.bAffectsHighlight);
	}
}

UStaticMeshComponent* UAssemblyBuilderComponent::GetPartByName(FName PartName) const
{
	if (auto* Found = NameToComponent.Find(PartName))
	{
		return *Found;
	}
	return nullptr;
}

void UAssemblyBuilderComponent::EnsureDynamicMIDs(UStaticMeshComponent* Comp)
{
	if (!Comp) return;
	if (DynamicMIDs.Contains(Comp)) return;

	FDynamicMIDArray Array;
	const int32 MatCount = Comp->GetNumMaterials();
	Array.MIDs.Reserve(MatCount);
	for (int32 Index =0; Index < MatCount; ++Index)
	{
		UMaterialInterface* Mat = Comp->GetMaterial(Index);
		UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamicFromMaterial(Index, Mat);
		Array.MIDs.Add(MID);
	}
	DynamicMIDs.Add(Comp, Array);
}

void UAssemblyBuilderComponent::ApplyHighlightScalar(float Value)
{
	if (!AssemblyConfig) return;
	if (AssemblyConfig->HighlightMode != EHighlightMode::MaterialParameter) return;
	const FName Param = AssemblyConfig->HighlightScalarParam;
	for (auto& Pair : NameToComponent)
	{
		const bool bDo = PartAffectsHighlight.FindRef(Pair.Key);
		if (!bDo) continue;
		UStaticMeshComponent* Comp = Pair.Value;
		EnsureDynamicMIDs(Comp);
		if (FDynamicMIDArray* Array = DynamicMIDs.Find(Comp))
		{
			for (UMaterialInstanceDynamic* MID : Array->MIDs)
			{
				if (MID)
				{
					MID->SetScalarParameterValue(Param, Value);
				}
			}
		}
	}
}

bool UAssemblyBuilderComponent::SetPartVisibility(FName PartName, bool bVisible)
{
	if (UStaticMeshComponent* Comp = GetPartByName(PartName))
	{
		Comp->SetVisibility(bVisible, true);
		return true;
	}
	return false;
}

bool UAssemblyBuilderComponent::GetPartWorldLocation(FName PartName, FVector& OutLocation) const
{
	if (const UStaticMeshComponent* Comp = GetPartByName(PartName))
	{
		OutLocation = Comp->GetComponentLocation();
		return true;
	}
	return false;
}
