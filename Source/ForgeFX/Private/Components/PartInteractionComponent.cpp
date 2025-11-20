#include "Components/PartInteractionComponent.h"
#include "Components/AssemblyBuilderComponent.h"
#include "Actors/RobotPartActor.h"
#include "Actors/RobotActor.h"
#include "GameFramework/PlayerController.h"

UPartInteractionComponent::UPartInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // manual tick from owner
}

UAssemblyBuilderComponent* UPartInteractionComponent::GetAssembly() const
{
	if (ARobotActor* Owner = Cast<ARobotActor>(GetOwner()))
	{
		return Owner->FindComponentByClass<UAssemblyBuilderComponent>();
	}
	return nullptr;
}

bool UPartInteractionComponent::HandleInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor, bool bAllowFreeAttach, float AttachPosTolerance, float AttachAngleToleranceDeg, float PartGrabMinDistance, float PartGrabMaxDistance)
{
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly) return false;
	// Click on detached actor: attempt snap/free attach
	if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		DraggedPartActor = PartActor; DraggedPartName = PartActor->GetPartName(); bDraggingPart = true;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector ViewLoc; FRotator ViewRot; PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			PartGrabDistance = FMath::Clamp(FVector::Distance(ViewLoc, PartActor->GetActorLocation()), PartGrabMinDistance, PartGrabMaxDistance);
		}
		return true;
	}
	// Click on attached component -> detach
	if (HitComponent)
	{
		FName PartName; if (Assembly->FindPartNameByComponent(HitComponent, PartName) && !Assembly->IsPartDetached(PartName))
		{
			ARobotPartActor* NewActor=nullptr;
			if (Assembly->DetachPart(PartName, NewActor) && NewActor)
			{
				DraggedPartActor = NewActor; DraggedPartName = PartName; bDraggingPart = true;
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					FVector ViewLoc; FRotator ViewRot; PC->GetPlayerViewPoint(ViewLoc, ViewRot);
					PartGrabDistance = FMath::Clamp(FVector::Distance(ViewLoc, NewActor->GetActorLocation()), PartGrabMinDistance, PartGrabMaxDistance);
				}
				return true;
			}
		}
	}
	return false;
}

void UPartInteractionComponent::HandleInteractReleased(bool bHoldToDragMode, bool bAllowFreeAttach)
{
	if (!bHoldToDragMode) return; // only end drag on release if hold mode
	if (bDraggingPart && DraggedPartActor)
	{
		if (!TrySnapDragged(8.f,10.f)) // use nominal tolerances; owner passes explicit during ForceDrop
		{
			TryFreeAttachDragged(bAllowFreeAttach,25.f,8.f);
		}
	}
	bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None;
}

bool UPartInteractionComponent::TrySnapDragged(float AttachPosTolerance, float AttachAngleToleranceDeg)
{
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly || !DraggedPartActor || DraggedPartName.IsNone()) return false;
	USceneComponent* Parent; FName Socket; if (!Assembly->GetAttachParentAndSocket(DraggedPartName, Parent, Socket) || !Parent) return false;
	const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
	const float Dist = FVector::Dist(DraggedPartActor->GetActorLocation(), SocketWorld.GetLocation()); if (Dist > AttachPosTolerance) return false;
	const float AngleDiff = DraggedPartActor->GetActorQuat().AngularDistance(SocketWorld.GetRotation()) *180.f / PI; if (AngleDiff > AttachAngleToleranceDeg) return false;
	Assembly->ReattachPart(DraggedPartName, DraggedPartActor); bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; return true;
}

bool UPartInteractionComponent::TryFreeAttachDragged(bool bAllowFreeAttach, float FreeAttachMaxDistance, float AttachPosTolerance)
{
	if (!bAllowFreeAttach || !DraggedPartActor) return false;
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly) return false;
	USceneComponent* Parent = nullptr; FName Socket = NAME_None; float Dist=0.f;
	if (!Assembly->FindNearestAttachTarget(DraggedPartActor->GetActorLocation(), Parent, Socket, Dist, DraggedPartName)) return false;
	const float MaxD = (FreeAttachMaxDistance >0.f) ? FreeAttachMaxDistance : AttachPosTolerance; if (Dist > MaxD) return false;
	Assembly->AttachDetachedPartTo(DraggedPartName, DraggedPartActor, Parent, Socket); bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; return true;
}

void UPartInteractionComponent::TickPartDrag(float DeltaSeconds, float PartDragSmoothingSpeed, float InPartGrabDistance, float PartGrabMinDistance, float PartGrabMaxDistance)
{
	if (!bDraggingPart || !DraggedPartActor) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!PC) return;
	int32 SizeX=0, SizeY=0; PC->GetViewportSize(SizeX, SizeY);
	float MidX = SizeX *0.5f; float MidY = SizeY *0.5f; FVector Origin, Dir;
	if (!PC->DeprojectScreenPositionToWorld(MidX, MidY, Origin, Dir)) return;
	PartGrabDistance = FMath::Clamp(PartGrabDistance, PartGrabMinDistance, PartGrabMaxDistance);
	FVector Desired = Origin + Dir.GetSafeNormal() * PartGrabDistance;
	DraggedPartActor->SetActorLocation(FMath::VInterpTo(DraggedPartActor->GetActorLocation(), Desired, DeltaSeconds, PartDragSmoothingSpeed));
}

void UPartInteractionComponent::AdjustGrabDistance(float Delta, float PartGrabMinDistance, float PartGrabMaxDistance)
{
	PartGrabDistance = FMath::Clamp(PartGrabDistance + Delta, PartGrabMinDistance, PartGrabMaxDistance);
}

void UPartInteractionComponent::ForceDropHeldPart(bool bTrySnap)
{
	if (!bDraggingPart || !DraggedPartActor) return;
	if (bTrySnap)
	{
		if (!TrySnapDragged(8.f,10.f))
		{
			TryFreeAttachDragged(true,25.f,8.f);
		}
	}
	else
	{
		bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None;
	}
}
