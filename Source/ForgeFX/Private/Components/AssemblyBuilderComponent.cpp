#include "Components/AssemblyBuilderComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Actors/RobotPartActor.h"
#include "Kismet/KismetSystemLibrary.h"

UAssemblyBuilderComponent::UAssemblyBuilderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAssemblyBuilderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!AssemblyConfig || AssemblyConfig->HighlightMode != EHighlightMode::MaterialParameter) return;
	const FName Param = AssemblyConfig->HighlightScalarParam;
	for (auto& Pair : DynamicMIDs)
	{
		UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(Pair.Key.Get()); if (!Comp) continue;
		float* Curr = CurrentHighlight.Find(Comp); float* Tgt = TargetHighlight.Find(Comp);
		const float CurrV = Curr ? *Curr :0.f; const float TgtV = Tgt ? *Tgt :0.f;
		const float NewV = FMath::FInterpTo(CurrV, TgtV, DeltaTime, HighlightInterpSpeed);
		CurrentHighlight.Add(Comp, NewV);
		for (UMaterialInstanceDynamic* MID : Pair.Value.MIDs) if (MID) MID->SetScalarParameterValue(Param, NewV);
	}
}

void UAssemblyBuilderComponent::EnsureDynamicMIDs(UStaticMeshComponent* Comp)
{
	if (!Comp) return;
	if (DynamicMIDs.Contains(Comp)) return;
	FDynamicMIDArray Arr;
	const int32 NumMats = Comp->GetNumMaterials();
	for (int32 i=0; i<NumMats; ++i)
	{
		UMaterialInterface* Mat = Comp->GetMaterial(i);
		if (Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
			Comp->SetMaterial(i, MID);
			Arr.MIDs.Add(MID);
		}
	}
	DynamicMIDs.Add(Comp, MoveTemp(Arr));
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
	CurrentHoverComp.Reset();
	SavedMaterials.Empty();
}

void UAssemblyBuilderComponent::ApplyHighlightScalar(float Value)
{
	if (!AssemblyConfig || AssemblyConfig->HighlightMode != EHighlightMode::MaterialParameter) return;
	for (auto& Pair : DynamicMIDs)
	{
		UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(Pair.Key.Get()); if (!Comp) continue;
		TargetHighlight.Add(Comp, Value);
	}
}

void UAssemblyBuilderComponent::ApplyHighlightScalarAll(float Value)
{
	// Alias to ApplyHighlightScalar for smooth interpolation
	ApplyHighlightScalar(Value);
}

void UAssemblyBuilderComponent::ApplyHighlightScalarToParts(const TArray<FName>& PartNames, float Value)
{
	if (!AssemblyConfig || AssemblyConfig->HighlightMode != EHighlightMode::MaterialParameter) return;
	TSet<FName> Set(PartNames);
	for (auto& Pair : DynamicMIDs)
	{
		UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(Pair.Key.Get()); if (!Comp) continue;
		FName PartName = NAME_None; if (FName* N = ComponentToName.Find(Comp)) PartName = *N;
		TargetHighlight.Add(Comp, Set.Contains(PartName) ? Value :0.f);
	}
}

void UAssemblyBuilderComponent::BuildAssembly()
{
	ClearAssembly(); if (!AssemblyConfig) return;
	for (const FRobotPartSpec& Spec : AssemblyConfig->Parts)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(GetOwner());
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->RegisterComponent();
		Comp->SetStaticMesh(Spec.Mesh.LoadSynchronous());
		Comp->SetRelativeTransform(Spec.RelativeTransform);
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
		Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		if (AssemblyConfig->HighlightMode == EHighlightMode::CustomDepthStencil)
		{
			Comp->SetRenderCustomDepth(true);
			Comp->SetCustomDepthStencilValue(AssemblyConfig->CustomDepthStencilValue);
			Comp->MarkRenderStateDirty();
		}
		EnsureDynamicMIDs(Comp);
		CurrentHighlight.Add(Comp,0.f); TargetHighlight.Add(Comp,0.f);
		USceneComponent* Parent = nullptr; if (Spec.ParentPartName.IsNone()) Parent = GetOwner()->GetRootComponent(); else if (TObjectPtr<UStaticMeshComponent>* Found = NameToComponent.Find(Spec.ParentPartName)) Parent = Found->Get(); if (!Parent) Parent = GetOwner()->GetRootComponent();
		Comp->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, Spec.ParentSocketName);
		NameToComponent.Add(Spec.PartName, Comp); ComponentToName.Add(Comp, Spec.PartName); PartAffectsHighlight.Add(Spec.PartName, Spec.bAffectsHighlight);
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
	TSubclassOf<ARobotPartActor> ClassToSpawn = Spec.DetachedActorClass ? Spec.DetachedActorClass : TSubclassOf<ARobotPartActor>(ARobotPartActor::StaticClass());
	OutActor = World->SpawnActor<ARobotPartActor>(ClassToSpawn, Comp->GetComponentTransform()); if (!OutActor) return false;
	OutActor->InitializePart(PartName, Mesh, Materials);
	OutActor->GetMeshComponent()->SetCollisionProfileName(Spec.DetachedCollisionProfile);
	OutActor->EnablePhysics(Spec.bSimulatePhysicsWhenDetached);
	// Hide original definitively and disable collision + highlight
	Comp->SetHiddenInGame(true);
	Comp->SetVisibility(false, true);
	Comp->SetRenderInMainPass(false);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (AssemblyConfig && AssemblyConfig->HighlightMode == EHighlightMode::CustomDepthStencil)
	{
		Comp->SetRenderCustomDepth(false);
	}
	Comp->MarkRenderStateDirty();
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
	if (Parent == Comp) { Parent = GetOwner()->GetRootComponent(); } // avoid self-attach
	// Restore visual, collision, and highlight state
	Comp->SetHiddenInGame(false);
	Comp->SetVisibility(true, true);
	Comp->SetRenderInMainPass(true);
	Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (AssemblyConfig && AssemblyConfig->HighlightMode == EHighlightMode::CustomDepthStencil)
	{
		Comp->SetRenderCustomDepth(true);
	}
	Comp->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetIncludingScale, Spec.ParentSocketName);
	Comp->MarkRenderStateDirty();
	PartActor->Destroy(); DetachedParts.Remove(PartName); return true;
}

bool UAssemblyBuilderComponent::AttachDetachedPartTo(FName PartName, ARobotPartActor* PartActor, USceneComponent* NewParent, FName SocketName)
{
	if (!PartActor || !NewParent) return false;
	UStaticMeshComponent* Comp = GetPartByName(PartName);
	if (!Comp) return false;
	if (NewParent == Comp) { NewParent = GetOwner()->GetRootComponent(); SocketName = NAME_None; } // avoid self-attach
	Comp->SetHiddenInGame(false);
	Comp->SetVisibility(true, true);
	Comp->SetRenderInMainPass(true);
	Comp->AttachToComponent(NewParent, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
	Comp->MarkRenderStateDirty();
	PartActor->Destroy();
	DetachedParts.Remove(PartName);
	ParentOverride.Add(PartName, NewParent);
	SocketOverride.Add(PartName, SocketName);
	return true;
}

bool UAssemblyBuilderComponent::FindNearestAttachTarget(const FVector& AtWorldLocation, USceneComponent*& OutParent, FName& OutSocket, float& OutDistance, FName ExcludePartName) const
{
	OutParent = nullptr; OutSocket = NAME_None; OutDistance = TNumericLimits<float>::Max();
	for (const auto& Pair : NameToComponent)
	{
		const FName ThisPart = Pair.Key;
		if (ExcludePartName != NAME_None && ThisPart == ExcludePartName) continue; // avoid self
		UStaticMeshComponent* Comp = Pair.Value.Get(); if (!Comp) continue;
		{
			const float D = FVector::Dist(AtWorldLocation, Comp->GetComponentLocation());
			if (D < OutDistance) { OutDistance = D; OutParent = Comp; OutSocket = NAME_None; }
		}
		TArray<FName> Sockets = Comp->GetAllSocketNames();
		for (const FName S : Sockets)
		{
			const FTransform T = Comp->GetSocketTransform(S, RTS_World);
			const float D = FVector::Dist(AtWorldLocation, T.GetLocation());
			if (D < OutDistance) { OutDistance = D; OutParent = Comp; OutSocket = S; }
		}
	}
	return OutParent != nullptr;
}

void UAssemblyBuilderComponent::GetAllAttachTargets(TArray<USceneComponent*>& OutTargets) const
{
	OutTargets.Reset();
	for (const auto& Pair : NameToComponent)
	{
		if (UStaticMeshComponent* Comp = Pair.Value.Get()) { OutTargets.Add(Comp); }
	}
}

bool UAssemblyBuilderComponent::IsDetachable(FName PartName) const
{
	return IsDetachableNow(PartName);
}

void UAssemblyBuilderComponent::RebuildAssembly()
{
	UE_LOG(LogTemp, Log, TEXT("AssemblyBuilder: RebuildAssembly invoked"));
	BuildAssembly();
	for (const auto& Pair : NameToComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("AssemblyBuilder: Registered Part %s Component %s"), *Pair.Key.ToString(), *GetNameSafe(Pair.Value.Get()));
	}
	if (AssemblyConfig)
	{
		UE_LOG(LogTemp, Log, TEXT("AssemblyBuilder: DataAsset Parts=%d"), AssemblyConfig->Parts.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AssemblyBuilder: No AssemblyConfig assigned"));
	}
}

bool UAssemblyBuilderComponent::IsDetachEnabled(FName PartName) const
{
	return IsDetachableNow(PartName);
}

void UAssemblyBuilderComponent::ApplyHoverOverride(UPrimitiveComponent* HoveredComp)
{
	if (!bUseHoverHighlightMaterial || !HoveredComp || !HoverHighlightMaterial) return;
	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(HoveredComp); if (!MeshComp) return;
	if (CurrentHoverComp.Get() == MeshComp) return;
	ClearHoverOverride();

	TArray<TObjectPtr<UMaterialInterface>> Originals;
	Originals.Reserve(MeshComp->GetNumMaterials());
	for (int32 i=0;i<MeshComp->GetNumMaterials();++i) Originals.Add(MeshComp->GetMaterial(i));
	SavedMaterials.Add(MeshComp, Originals);

	for (int32 i=0;i<MeshComp->GetNumMaterials();++i) MeshComp->SetMaterial(i, HoverHighlightMaterial);
	CurrentHoverComp = MeshComp;
}

void UAssemblyBuilderComponent::ClearHoverOverride()
{
	if (!CurrentHoverComp.IsValid()) return;
	UStaticMeshComponent* MeshComp = CurrentHoverComp.Get();
	if (TArray<TObjectPtr<UMaterialInterface>>* Originals = SavedMaterials.Find(MeshComp))
	{
		for (int32 i=0;i<MeshComp->GetNumMaterials() && i<Originals->Num(); ++i)
		{
			MeshComp->SetMaterial(i, (*Originals)[i]);
		}
	}
	CurrentHoverComp.Reset();
	SavedMaterials.Remove(MeshComp);
}
