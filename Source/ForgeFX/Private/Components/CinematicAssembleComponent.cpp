#include "Components/CinematicAssembleComponent.h"
#include "Components/AssemblyBuilderComponent.h"
#include "Actors/RobotPartActor.h"
#include "Actors/RobotActor.h"
#include "EngineUtils.h"

UCinematicAssembleComponent::UCinematicAssembleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UAssemblyBuilderComponent* UCinematicAssembleComponent::GetAssembly() const
{
	if (ARobotActor* Owner = Cast<ARobotActor>(GetOwner()))
	{
		return Owner->FindComponentByClass<UAssemblyBuilderComponent>();
	}
	return nullptr;
}

void UCinematicAssembleComponent::BuildPartList()
{
	Parts.Reset();
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly || !Assembly->AssemblyConfig) return;
	for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts)
	{
		if (!Assembly->IsDetachable(Spec.PartName)) continue;
		ARobotPartActor* OutActor = nullptr;
		if (!Assembly->IsPartDetached(Spec.PartName))
		{
			Assembly->DetachPart(Spec.PartName, OutActor);
		}
		else
		{
			for (TActorIterator<ARobotPartActor> It(GetWorld()); It; ++It)
			{
				if (It->GetPartName()==Spec.PartName) { OutActor = *It; break; }
			}
		}
		if (!OutActor) continue;
		USceneComponent* Parent; FName Socket; FTransform SocketWorld = OutActor->GetActorTransform();
		if (Assembly->GetAttachParentAndSocket(Spec.PartName, Parent, Socket) && Parent) SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
		FCinePart CP; CP.Actor = OutActor; CP.TargetSocketWorld = SocketWorld; CP.ScatterDir = FVector(FMath::FRandRange(-1.f,1.f), FMath::FRandRange(-1.f,1.f), FMath::FRandRange(0.2f,1.f)).GetSafeNormal();
		Parts.Add(CP);
	}
}

void UCinematicAssembleComponent::TriggerAssemble(FVector NewLocation)
{
	if (Phase != ECinematicPhase::None) return; // ignore while active
	BuildPartList(); if (Parts.Num()==0) return;
	BaseTarget = NewLocation; Phase = ECinematicPhase::Scatter; Elapsed =0.f;
}

void UCinematicAssembleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Phase == ECinematicPhase::None) return;
	Elapsed += DeltaTime;
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly) return;
	if (Phase == ECinematicPhase::Scatter)
	{
		float A = FMath::Clamp(Elapsed / ScatterDuration,0.f,1.f);
		for (FCinePart& P : Parts) if (P.Actor) P.Actor->SetActorLocation(FMath::Lerp(P.TargetSocketWorld.GetLocation(), P.TargetSocketWorld.GetLocation() + P.ScatterDir * ScatterDistance, A));
		if (A >=1.f)
		{
			Phase = ECinematicPhase::Return; Elapsed =0.f; if (ARobotActor* Robot = Cast<ARobotActor>(GetOwner())) Robot->SetActorLocation(BaseTarget);
		}
	}
	else if (Phase == ECinematicPhase::Return)
	{
		float Araw = FMath::Clamp(Elapsed / ReturnDuration,0.f,1.f); float A = FMath::Pow(Araw, EasePower);
		for (FCinePart& P : Parts) if (P.Actor) P.Actor->SetActorLocation(FMath::Lerp(P.TargetSocketWorld.GetLocation() + P.ScatterDir * ScatterDistance, P.TargetSocketWorld.GetLocation() + (BaseTarget - GetOwner()->GetActorLocation()), A));
		if (Araw >=1.f)
		{
			for (FCinePart& P : Parts) if (P.Actor) Assembly->ReattachPart(P.Actor->GetPartName(), P.Actor);
			Parts.Reset(); Phase = ECinematicPhase::None; Elapsed =0.f;
		}
	}
}
