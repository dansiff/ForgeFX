#include "Components/RobotShowcaseComponent.h"
#include "Components/AssemblyBuilderComponent.h"
#include "Actors/RobotActor.h"
#include "Actors/OrbitCameraRig.h"
#include "Actors/RobotPartActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

URobotShowcaseComponent::URobotShowcaseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UAssemblyBuilderComponent* URobotShowcaseComponent::GetAssembly() const
{
	if (ARobotActor* Owner = Cast<ARobotActor>(GetOwner()))
	{
		return Owner->FindComponentByClass<UAssemblyBuilderComponent>();
	}
	return nullptr;
}

void URobotShowcaseComponent::StartShowcase()
{
	if (bActive) return;
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly || !Assembly->AssemblyConfig) return;
	Order.Reset();
	for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts)
	{
		if (Spec.bDetachable && Spec.PartName != FName("Torso")) Order.Add(Spec.PartName);
	}
	if (Order.Num()==0) return;
	Order.Sort([](const FName& A, const FName& B){ return A.LexicalLess(B); });
	Order.Add(FName("Torso"));
	Elapsed =0.f; Index =0; bActive = true;
	// spawn camera rig
	UWorld* W = GetWorld(); if (W)
	{
		FVector Center = GetOwner()->GetActorLocation(); FVector SpawnLoc = Center + FVector(OrbitRadius,0.f,OrbitHeight);
		Rig = W->SpawnActor<AOrbitCameraRig>(AOrbitCameraRig::StaticClass(), SpawnLoc, FRotator::ZeroRotator);
		if (Rig.IsValid())
		{
			Rig->Setup(Cast<ARobotActor>(GetOwner()), OrbitRadius, OrbitHeight, OrbitSpeedDeg); Rig->bAutoOrbit = false; if (bUseSpline) Rig->UseCircularSplinePath(SplinePoints);
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				FViewTargetTransitionParams Blend; Blend.BlendTime =0.35f; PC->SetViewTarget(Rig.Get(), Blend); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
			}
		}
	}
}

void URobotShowcaseComponent::StopShowcase()
{
	if (!bActive) return;
	bActive = false; Index = -1; Order.Reset();
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FViewTargetTransitionParams Blend; Blend.BlendTime =0.35f; PC->SetViewTarget(GetOwner(), Blend); PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
	}
	if (Rig.IsValid()) { Rig->Destroy(); Rig.Reset(); }
}

void URobotShowcaseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bActive) return;
	Elapsed += DeltaTime;
	if (Elapsed < InitialDelay) return;
	float Interval = DetachInterval; float SegmentTime = (Elapsed - InitialDelay);
	int32 TargetIndex = FMath::FloorToInt(SegmentTime / Interval);
	if (TargetIndex == Index && TargetIndex < Order.Num())
	{
		// detach next part once per index
		UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly) return;
		FName Part = Order[Index];
		if (!Assembly->IsPartDetached(Part))
		{
			ARobotPartActor* OutActor=nullptr; if (Assembly->DetachPart(Part, OutActor) && OutActor)
			{
				FVector Dir = (OutActor->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal(); OutActor->SetActorLocation(OutActor->GetActorLocation() + Dir *35.f);
				if (Assembly->AssemblyConfig->DetachSound) UGameplayStatics::PlaySoundAtLocation(this, Assembly->AssemblyConfig->DetachSound, OutActor->GetActorLocation());
				if (Assembly->AssemblyConfig->DetachEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Assembly->AssemblyConfig->DetachEffect, OutActor->GetActorLocation());
			}
		}
		Index++;
		if (Index > Order.Num())
		{
			// reattach pass
			for (int32 i=0;i<Order.Num();++i)
			{
				FName P = Order[i]; UAssemblyBuilderComponent* A = GetAssembly(); if (!A) break; if (A->IsPartDetached(P))
				{
					USceneComponent* Parent; FName Socket; if (A->GetAttachParentAndSocket(P, Parent, Socket))
					{
						for (TActorIterator<ARobotPartActor> It(GetWorld()); It; ++It)
						{
							if (It->GetPartName()==P) { A->ReattachPart(P, *It); break; }
						}
						if (A->AssemblyConfig->AttachSound && Parent) UGameplayStatics::PlaySoundAtLocation(this, A->AssemblyConfig->AttachSound, Parent->GetComponentLocation());
					}
				}
			}
			StopShowcase();
		}
	}
}
