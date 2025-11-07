#include "Components/AssemblyBuilderComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Actors/RobotPartActor.h"
#include "Kismet/KismetSystemLibrary.h"

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
	ComponentToName.Empty();
	PartAffectsHighlight.Empty();
	DynamicMIDs.Empty();
	DetachedParts.Empty();
	DetachEnabledOverride.Empty();
}

void UAssemblyBuilderComponent::BuildAssembly()
{
	ClearAssembly();
	if (!AssemblyConfig) return;

	for (const FRobotPartSpec& Spec : AssemblyConfig->Parts)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(GetOwner());
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->RegisterComponent();
		Comp->SetStaticMesh(Spec.Mesh.LoadSynchronous());
		Comp->SetRelativeTransform(Spec.RelativeTransform);

		if (AssemblyConfig->HighlightMode == EHighlightMode::CustomDepthStencil)
		{
			Comp->SetRenderCustomDepth(true);
			Comp->SetCustomDepthStencilValue(AssemblyConfig->CustomDepthStencilValue);
			Comp->MarkRenderStateDirty();
		}

		USceneComponent* Parent = nullptr;
		if (Spec.ParentPartName.IsNone()) Parent = GetOwner()->GetRootComponent();
		else if (TObjectPtr<UStaticMeshComponent>* Found = NameToComponent.Find(Spec.ParentPartName)) Parent = Found->Get();
		if (!Parent) Parent = GetOwner()->GetRootComponent();

		Comp->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, Spec.ParentSocketName);
		NameToComponent.Add(Spec.PartName, Comp);
		ComponentToName.Add(Comp, Spec.PartName);
		PartAffectsHighlight.Add(Spec.PartName, Spec.bAffectsHighlight);
	}
}

UStaticMeshComponent* UAssemblyBuilderComponent::GetPartByName(FName PartName) const
{
	if (const TObjectPtr<UStaticMeshComponent>* Found = NameToComponent.Find(PartName)) return Found->Get();
	return nullptr;
}

bool UAssemblyBuilderComponent::FindPartNameByComponent(const UPrimitiveComponent* Comp, FName& OutName) const
{
	if (const FName* Found = ComponentToName.Find(Cast<UStaticMeshComponent>(Comp))) { OutName = *Found; return true; }
	return false;
}

bool UAssemblyBuilderComponent::GetPartSpec(FName PartName, FRobotPartSpec& OutSpec) const
{
	if (!AssemblyConfig) return false;
	if (const FRobotPartSpec* Spec = AssemblyConfig->Parts.FindByPredicate([&](const FRobotPartSpec& S){ return S.PartName==PartName; })) { OutSpec = *Spec; return true; }
	return false;
}

bool UAssemblyBuilderComponent::GetAttachParentAndSocket(FName PartName, USceneComponent*& OutParent, FName& OutSocket) const
{
	OutParent = nullptr; OutSocket = NAME_None;
	FRobotPartSpec SpecTemp; if (!GetPartSpec(PartName, SpecTemp)) return false;
	if (SpecTemp.ParentPartName.IsNone()) OutParent = GetOwner()->GetRootComponent();
	else if (const TObjectPtr<UStaticMeshComponent>* Found = NameToComponent.Find(SpecTemp.ParentPartName)) OutParent = Found->Get();
	if (!OutParent) OutParent = GetOwner()->GetRootComponent();
	OutSocket = SpecTemp.ParentSocketName; return true;
}

void UAssemblyBuilderComponent::EnsureDynamicMIDs(UStaticMeshComponent* Comp)
{
	if (!Comp || DynamicMIDs.Contains(Comp)) return;
	FDynamicMIDArray Array; const int32 MatCount = Comp->GetNumMaterials(); Array.MIDs.Reserve(MatCount);
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
	if (!AssemblyConfig || AssemblyConfig->HighlightMode != EHighlightMode::MaterialParameter) return;
	const FName Param = AssemblyConfig->HighlightScalarParam;
	for (auto& Pair : NameToComponent)
	{
		if (!PartAffectsHighlight.FindRef(Pair.Key)) continue;
		UStaticMeshComponent* Comp = Pair.Value.Get(); EnsureDynamicMIDs(Comp);
		if (FDynamicMIDArray* Array = DynamicMIDs.Find(Comp)) for (UMaterialInstanceDynamic* MID : Array->MIDs) if (MID) MID->SetScalarParameterValue(Param, Value);
	}
}

void UAssemblyBuilderComponent::ApplyHighlightScalarAll(float Value)
{
	if (!AssemblyConfig) return; const FName Param = AssemblyConfig->HighlightScalarParam;
	for (auto& Pair : NameToComponent)
	{
		UStaticMeshComponent* Comp = Pair.Value.Get(); EnsureDynamicMIDs(Comp);
		if (FDynamicMIDArray* Array = DynamicMIDs.Find(Comp)) for (UMaterialInstanceDynamic* MID : Array->MIDs) if (MID) MID->SetScalarParameterValue(Param, Value);
	}
}

void UAssemblyBuilderComponent::ApplyHighlightScalarToParts(const TArray<FName>& PartNames, float Value)
{
	if (!AssemblyConfig) return; const FName Param = AssemblyConfig->HighlightScalarParam;
	for (const FName Part : PartNames)
	{
		if (UStaticMeshComponent* Comp = GetPartByName(Part))
		{
			EnsureDynamicMIDs(Comp);
			if (FDynamicMIDArray* Array = DynamicMIDs.Find(Comp)) for (UMaterialInstanceDynamic* MID : Array->MIDs) if (MID) MID->SetScalarParameterValue(Param, Value);
		}
	}
}

bool UAssemblyBuilderComponent::SetPartVisibility(FName PartName, bool bVisible)
{
	if (UStaticMeshComponent* Comp = GetPartByName(PartName)) { Comp->SetVisibility(bVisible, true); return true; }
	return false;
}

bool UAssemblyBuilderComponent::GetPartWorldLocation(FName PartName, FVector& OutLocation) const
{
	if (const UStaticMeshComponent* Comp = GetPartByName(PartName)) { OutLocation = Comp->GetComponentLocation(); return true; }
	return false;
}

bool UAssemblyBuilderComponent::IsDetachableNow(FName PartName) const
{
	if (const bool* Override = DetachEnabledOverride.Find(PartName)) return *Override;
	FRobotPartSpec Spec; if (GetPartSpec(PartName, Spec)) return Spec.bDetachable; return false;
}

void UAssemblyBuilderComponent::SetDetachEnabledForParts(const TArray<FName>& PartNames, bool bEnabled)
{
	for (const FName P : PartNames) { DetachEnabledOverride.Add(P, bEnabled); }
}

void UAssemblyBuilderComponent::SetDetachEnabledForAll(bool bEnabled)
{
	DetachEnabledOverride.Empty();
	if (!AssemblyConfig) return;
	for (const FRobotPartSpec& Spec : AssemblyConfig->Parts) { DetachEnabledOverride.Add(Spec.PartName, bEnabled); }
}

bool UAssemblyBuilderComponent::DetachPart(FName PartName, ARobotPartActor*& OutActor)
{
	OutActor = nullptr; if (!AssemblyConfig) return false;
	if (!IsDetachableNow(PartName)) return false;
	UStaticMeshComponent* Comp = GetPartByName(PartName); if (!Comp) return false;
	FRobotPartSpec Spec; if (!GetPartSpec(PartName, Spec)) return false;
	UWorld* World = GetWorld(); if (!World) return false;
	UStaticMesh* Mesh = Comp->GetStaticMesh();
	TArray<UMaterialInterface*> Materials; for (int32 i=0, n=Comp->GetNumMaterials(); i<n; ++i) Materials.Add(Comp->GetMaterial(i));
	TSubclassOf<ARobotPartActor> ClassToSpawn = Spec.DetachedActorClass ? Spec.DetachedActorClass : ARobotPartActor::StaticClass();
	OutActor = World->SpawnActor<ARobotPartActor>(ClassToSpawn, Comp->GetComponentTransform()); if (!OutActor) return false;
	OutActor->InitializePart(PartName, Mesh, Materials);
	OutActor->GetMeshComponent()->SetCollisionProfileName(Spec.DetachedCollisionProfile);
	OutActor->EnablePhysics(Spec.bSimulatePhysicsWhenDetached);
	Comp->SetVisibility(false, true); Comp->SetHiddenInGame(true);
	DetachedParts.Add(PartName, OutActor);
	return true;
}

bool UAssemblyBuilderComponent::ReattachPart(FName PartName, ARobotPartActor* PartActor)
{
	if (!PartActor) return false; UStaticMeshComponent* Comp = GetPartByName(PartName); if (!Comp) return false;
	FRobotPartSpec Spec; if (!GetPartSpec(PartName, Spec)) return false;
	USceneComponent* Parent = nullptr; if (Spec.ParentPartName.IsNone()) Parent = GetOwner()->GetRootComponent();
	else if (TObjectPtr<UStaticMeshComponent>* Found = NameToComponent.Find(Spec.ParentPartName)) Parent = Found->Get();
	if (!Parent) Parent = GetOwner()->GetRootComponent();
	Comp->SetHiddenInGame(false); Comp->SetVisibility(true, true);
	Comp->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetIncludingScale, Spec.ParentSocketName);
	PartActor->Destroy(); DetachedParts.Remove(PartName); return true;
}
