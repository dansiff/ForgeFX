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

static FVector ComputeDesiredDragLoc(UWorld* World, float Distance)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr; if (!PC) return FVector::ZeroVector;
	int32 SizeX=0, SizeY=0; PC->GetViewportSize(SizeX, SizeY);
	float MidX = SizeX *0.5f; float MidY = SizeY *0.5f; FVector Origin, Dir;
	if (!PC->DeprojectScreenPositionToWorld(MidX, MidY, Origin, Dir)) return FVector::ZeroVector;
	return Origin + Dir.GetSafeNormal() * Distance;
}

bool UPartInteractionComponent::HandleInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor, bool bAllowFreeAttach, float AttachPosTolerance, float AttachAngleToleranceDeg, float PartGrabMinDistance, float PartGrabMaxDistance)
{
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly) return false;
	// Click on detached actor: attempt snap/free attach
	if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		DraggedPartActor = PartActor; DraggedPartName = PartActor->GetPartName(); bDraggingPart = true;
		GroupChildActors.Reset(); GroupChildNames.Reset(); GroupChildOffsets.Reset(); GroupChildRotOffsets.Reset();
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
			const bool bGroup = (GetWorld()->GetFirstPlayerController() && GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::LeftShift));
			if (bGroup)
			{
				TArray<ARobotPartActor*> ChildActors;
				if (Assembly->DetachPartWithChildren(PartName, NewActor, ChildActors) && NewActor)
				{
					DraggedPartActor = NewActor; DraggedPartName = PartName; bDraggingPart = true;
					GroupChildActors = ChildActors;
					GroupChildNames.Reset(); GroupChildOffsets.Reset(); GroupChildRotOffsets.Reset();
					// capture offsets relative to root at drag start
					for (ARobotPartActor* Child : GroupChildActors)
					{
						if (!Child) continue; GroupChildNames.Add(Child->GetPartName());
						GroupChildOffsets.Add(Child->GetActorLocation() - NewActor->GetActorLocation());
						GroupChildRotOffsets.Add(Child->GetActorQuat() * NewActor->GetActorQuat().Inverse());
					}
					if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
					{
						FVector ViewLoc; FRotator ViewRot; PC->GetPlayerViewPoint(ViewLoc, ViewRot);
						PartGrabDistance = FMath::Clamp(FVector::Distance(ViewLoc, NewActor->GetActorLocation()), PartGrabMinDistance, PartGrabMaxDistance);
					}
					return true;
				}
			}
			else if (Assembly->DetachPart(PartName, NewActor) && NewActor)
			{
				DraggedPartActor = NewActor; DraggedPartName = PartName; bDraggingPart = true;
				GroupChildActors.Reset(); GroupChildNames.Reset(); GroupChildOffsets.Reset(); GroupChildRotOffsets.Reset();
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
		if (!TrySnapDragged(8.f,10.f)) // use nominal tolerances
		{
			TryFreeAttachDragged(bAllowFreeAttach,25.f,8.f);
		}
	}
	ClearDragState();
}

bool UPartInteractionComponent::TrySnapDragged(float AttachPosTolerance, float AttachAngleToleranceDeg)
{
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly || !DraggedPartActor || DraggedPartName.IsNone()) return false;
	USceneComponent* Parent; FName Socket; if (!Assembly->GetAttachParentAndSocket(DraggedPartName, Parent, Socket) || !Parent) return false;
	const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
	const float Dist = FVector::Dist(DraggedPartActor->GetActorLocation(), SocketWorld.GetLocation()); if (Dist > AttachPosTolerance) return false;
	const float AngleDiff = DraggedPartActor->GetActorQuat().AngularDistance(SocketWorld.GetRotation()) *180.f / PI; if (AngleDiff > AttachAngleToleranceDeg) return false;
	// Group reattach if we have children
	if (GroupChildActors.Num() >0)
	{
		TArray<FName> Names; Names.Add(DraggedPartName); Names.Append(GroupChildNames);
		TArray<ARobotPartActor*> Actors; Actors.Add(DraggedPartActor); Actors.Append(GroupChildActors);
		Assembly->ReattachPartsGroup(Names, Actors);
		ClearDragState();
		return true;
	}
	Assembly->ReattachPart(DraggedPartName, DraggedPartActor); ClearDragState(); return true;
}

bool UPartInteractionComponent::TryFreeAttachDragged(bool bAllowFreeAttach, float FreeAttachMaxDistance, float AttachPosTolerance)
{
	if (!bAllowFreeAttach || !DraggedPartActor) return false;
	UAssemblyBuilderComponent* Assembly = GetAssembly(); if (!Assembly) return false;
	USceneComponent* Parent = nullptr; FName Socket = NAME_None; float Dist=0.f;
	if (!Assembly->FindNearestAttachTarget(DraggedPartActor->GetActorLocation(), Parent, Socket, Dist, DraggedPartName)) return false;
	const float MaxD = (FreeAttachMaxDistance >0.f) ? FreeAttachMaxDistance : AttachPosTolerance; if (Dist > MaxD) return false;
	Assembly->AttachDetachedPartTo(DraggedPartName, DraggedPartActor, Parent, Socket);
	// attach children near their nearest targets as well
	for (int32 i=0;i<GroupChildActors.Num();++i)
	{
		ARobotPartActor* Child = GroupChildActors[i]; if (!Child) continue;
		USceneComponent* CParent=nullptr; FName CSocket=NAME_None; float CDist=0.f;
		const FName ChildName = GroupChildNames.IsValidIndex(i) ? GroupChildNames[i] : NAME_None;
		if (Assembly->FindNearestAttachTarget(Child->GetActorLocation(), CParent, CSocket, CDist, ChildName))
		{
			Assembly->AttachDetachedPartTo(ChildName, Child, CParent, CSocket);
		}
	}
	ClearDragState();
	return true;
}

void UPartInteractionComponent::TickPartDrag(float DeltaSeconds, float PartDragSmoothingSpeed, float InPartGrabDistance, float PartGrabMinDistance, float PartGrabMaxDistance)
{
	if (!bDraggingPart || !DraggedPartActor) return;
	PartGrabDistance = FMath::Clamp(PartGrabDistance, PartGrabMinDistance, PartGrabMaxDistance);
	const FVector Desired = ComputeDesiredDragLoc(GetWorld(), PartGrabDistance);
	FVector RootLoc = DraggedPartActor->GetActorLocation();
	FVector FlatDesired = FVector(Desired.X, Desired.Y, RootLoc.Z);
	// Orient root toward camera ray (optional small rotation for visual)
	DraggedPartActor->SetActorLocation(FMath::VInterpTo(RootLoc, FlatDesired, DeltaSeconds, PartDragSmoothingSpeed));
	for (int32 i=0;i<GroupChildActors.Num();++i)
	{
		ARobotPartActor* Child = GroupChildActors[i]; if (!Child) continue;
		const FVector Offset = GroupChildOffsets.IsValidIndex(i) ? GroupChildOffsets[i] : FVector::ZeroVector;
		const FQuat RotOffset = GroupChildRotOffsets.IsValidIndex(i) ? GroupChildRotOffsets[i] : FQuat::Identity;
		const FVector Target = FlatDesired + Offset;
		Child->SetActorLocation(FMath::VInterpTo(Child->GetActorLocation(), Target, DeltaSeconds, PartDragSmoothingSpeed));
		Child->SetActorRotation((RotOffset * DraggedPartActor->GetActorQuat()).Rotator());
	}
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
	ClearDragState();
}

void UPartInteractionComponent::ClearDragState()
{
	bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None;
	GroupChildActors.Reset(); GroupChildNames.Reset(); GroupChildOffsets.Reset(); GroupChildRotOffsets.Reset();
}
