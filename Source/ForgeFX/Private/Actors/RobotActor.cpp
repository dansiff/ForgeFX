#include "Actors/RobotActor.h"
#include "Components/InteractionTraceComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Components/InteractionTraceComponent.h"
#include "Blueprint/UserWidget.h"
#include "Actors/RobotPartActor.h"

namespace { static const FName Part_Torso(TEXT("Torso")); }

ARobotActor::ARobotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Arm = CreateDefaultSubobject<URobotArmComponent>(TEXT("RobotArm"));
	Highlight = CreateDefaultSubobject<UHighlightComponent>(TEXT("Highlight"));
	Assembly = CreateDefaultSubobject<UAssemblyBuilderComponent>(TEXT("Assembly"));
}

void ARobotActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Assembly) Assembly->BuildAssembly();
}

void ARobotActor::BeginPlay()
{
	Super::BeginPlay();
	if (Assembly) Assembly->BuildAssembly();
	if (Arm) Arm->OnAttachedChanged.AddDynamic(this, &ARobotActor::OnArmAttachedChanged);
	if (Highlight) Highlight->OnHighlightedChanged.AddDynamic(this, &ARobotActor::OnHighlightedChanged);

	// Bind to the pawn’s interaction trace (not the robot)
	UInteractionTraceComponent* Trace = nullptr;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* P = PC->GetPawn())
		{
			Trace = P->FindComponentByClass<UInteractionTraceComponent>();
		}
	}
	if (Trace)
	{
		Trace->OnHoverComponentChanged.AddDynamic(this, &ARobotActor::OnHoverComponentChanged);
		Trace->OnInteractPressed.AddDynamic(this, &ARobotActor::OnInteractPressed);
		Trace->OnInteractAltPressed.AddDynamic(this, &ARobotActor::OnInteractPressed); // Alt press reuses same handler (reattach)
		Trace->OnInteractReleased.AddDynamic(this, &ARobotActor::OnInteractReleased);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (InputContext) Subsys->AddMappingContext(InputContext, MappingPriority);
		}
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			if (ToggleArmAction) EIC->BindAction(ToggleArmAction, ETriggerEvent::Started, this, &ARobotActor::ToggleArm);
			if (ScrambleAction) EIC->BindAction(ScrambleAction, ETriggerEvent::Started, this, &ARobotActor::ScrambleParts);
		}
	}

	if (StatusWidgetClass)
	{
		if (APlayerController* PC2 = GetWorld()->GetFirstPlayerController())
		{
			StatusWidget = CreateWidget<UUserWidget>(PC2, StatusWidgetClass);
			if (StatusWidget) StatusWidget->AddToViewport();
		}
	}

	if (DebugWidgetClass)
	{
		if (APlayerController* PC3 = GetWorld()->GetFirstPlayerController())
		{
			DebugWidget = CreateWidget<UUserWidget>(PC3, DebugWidgetClass);
			if (DebugWidget) DebugWidget->AddToViewport(10);
		}
	}
}

void ARobotActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDragging && bAllowTorsoDrag)
	{
		FVector CursorWorld;
		if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) SetActorLocation(CursorWorld + DragOffset);
	}
	if (bDraggingPart && DraggedPartActor)
	{
		FVector CursorWorld;
		if (ComputeCursorWorldOnPlane(PartDragPlaneZ, CursorWorld))
		{
			DraggedPartActor->SetActorLocation(CursorWorld + PartDragOffset);
			TrySnapDraggedPartToSocket();
		}
	}
}

bool ARobotActor::ComputeCursorWorldOnPlane(float PlaneZ, FVector& OutWorldPoint) const
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		float MX, MY; PC->GetMousePosition(MX, MY);
		FVector WorldOrigin, WorldDir;
		if (PC->DeprojectScreenPositionToWorld(MX, MY, WorldOrigin, WorldDir))
		{
			if (!FMath::IsNearlyZero(WorldDir.Z))
			{
				const float T = (PlaneZ - WorldOrigin.Z) / WorldDir.Z;
				OutWorldPoint = WorldOrigin + T * WorldDir;
				return true;
			}
		}
	}
	return false;
}

bool ARobotActor::TrySnapDraggedPartToSocket()
{
	if (!Assembly || !DraggedPartActor || DraggedPartName.IsNone()) return false;
	USceneComponent* Parent; FName Socket;
	if (!Assembly->GetAttachParentAndSocket(DraggedPartName, Parent, Socket)) return false;
	if (!Parent) return false;
	const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
	const FVector PartLoc = DraggedPartActor->GetActorLocation();
	const float Dist = FVector::Dist(PartLoc, SocketWorld.GetLocation());
	if (Dist > AttachPosTolerance) return false;
	const FQuat PartQ = DraggedPartActor->GetActorQuat();
	const FQuat SocketQ = SocketWorld.GetRotation();
	const float AngleDiff = PartQ.AngularDistance(SocketQ) *180.f / PI;
	if (AngleDiff > AttachAngleToleranceDeg) return false;
	Assembly->ReattachPart(DraggedPartName, DraggedPartActor);
	bDraggingPart = false;
	DraggedPartActor = nullptr;
	DraggedPartName = NAME_None;
	return true;
}

bool ARobotActor::TryFreeAttachDraggedPart()
{
	if (!bAllowFreeAttach || !Assembly || !DraggedPartActor || DraggedPartName.IsNone()) return false;
	USceneComponent* Parent = nullptr; FName Socket = NAME_None; float Dist=0.f;
	if (!Assembly->FindNearestAttachTarget(DraggedPartActor->GetActorLocation(), Parent, Socket, Dist)) return false;
	const float MaxD = (FreeAttachMaxDistance >0.f) ? FreeAttachMaxDistance : AttachPosTolerance;
	if (Dist > MaxD) return false;
	const bool bOk = Assembly->AttachDetachedPartTo(DraggedPartName, DraggedPartActor, Parent, Socket);
	if (bOk)
	{
		bDraggingPart = false;
		DraggedPartActor = nullptr;
		DraggedPartName = NAME_None;
	}
	return bOk;
}

void ARobotActor::OnHoverComponentChanged(UPrimitiveComponent* HitComponent, AActor* HitActor)
{
	if (!Assembly) return;
	if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		Assembly->ApplyHighlightScalarAll(0.f);
		return;
	}

	FName Part;
	if (!HitComponent || !Assembly->FindPartNameByComponent(HitComponent, Part))
	{
		Assembly->ApplyHighlightScalarAll(0.f);
		return;
	}

	const bool bIsTorso = (Part == Part_Torso);
	const bool bIsArm = (Arm && Arm->Config && Arm->Config->ArmPartNames.Contains(Part));
	if (bIsTorso) Assembly->ApplyHighlightScalarAll(1.f);
	else if (bIsArm)
	{
		Assembly->ApplyHighlightScalarAll(0.f);
		Assembly->ApplyHighlightScalarToParts(Arm->Config->ArmPartNames,1.f);
	}
	else Assembly->ApplyHighlightScalarAll(0.f);

	HoveredPartName = NAME_None;
	if (HitComponent && Assembly && Assembly->FindPartNameByComponent(HitComponent, HoveredPartName))
	{
		// keep HoveredPartName set
	}
	else
	{
		HoveredPartName = NAME_None;
	}
}

void ARobotActor::OnInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor)
{
	if (DetachMode == EDetachInteractMode::ClickToggleAttach)
	{
		// Toggle attach/detach without drag
		if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
		{
			// Actor currently detached -> try snap to original, else free attach
			DraggedPartActor = PartActor; DraggedPartName = PartActor->GetPartName();
			if (!TrySnapDraggedPartToSocket()) { TryFreeAttachDraggedPart(); }
			DraggedPartActor = nullptr; DraggedPartName = NAME_None; bDraggingPart = false;
			return;
		}
		// If component: detach and leave placed (no drag)
		if (Assembly && HitComponent)
		{
			FName Part; if (Assembly->FindPartNameByComponent(HitComponent, Part) && !Assembly->IsPartDetached(Part))
			{
				ARobotPartActor* NewActor=nullptr; if (Assembly->DetachPart(Part, NewActor)) { /* no drag */ }
			}
		}
		return;
	}

	// Other modes allow dragging
	if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		DraggedPartActor = PartActor; DraggedPartName = PartActor->GetPartName(); bDraggingPart = true;
		PartDragPlaneZ = PartActor->GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(PartDragPlaneZ, CursorWorld)) PartDragOffset = PartActor->GetActorLocation() - CursorWorld;
		return;
	}

	if (!Assembly) return;
	FName Part;
	if (HitComponent && Assembly->FindPartNameByComponent(HitComponent, Part))
	{
		if (!Assembly->IsPartDetached(Part))
		{
			ARobotPartActor* NewPartActor = nullptr; if (Assembly->DetachPart(Part, NewPartActor) && NewPartActor)
			{
				DraggedPartActor = NewPartActor; DraggedPartName = Part; PartDragPlaneZ = NewPartActor->GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(PartDragPlaneZ, CursorWorld)) PartDragOffset = NewPartActor->GetActorLocation() - CursorWorld;
				bDraggingPart = (DetachMode != EDetachInteractMode::ToggleToDrag) ? true : !bDraggingPart;
			}
			return;
		}
	}

	// Whole robot drag if enabled
	if (bAllowTorsoDrag && HitComponent && Assembly->FindPartNameByComponent(HitComponent, Part) && Part == FName("Torso"))
	{
		if (DetachMode == EDetachInteractMode::HoldToDrag)
		{
			bDragging = true; DragPlaneZ = GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) DragOffset = GetActorLocation() - CursorWorld;
		}
		else if (DetachMode == EDetachInteractMode::ToggleToDrag)
		{
			bDragging = !bDragging; if (bDragging) { DragPlaneZ = GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) DragOffset = GetActorLocation() - CursorWorld; }
		}
	}
}

void ARobotActor::OnInteractReleased()
{
	if (DetachMode == EDetachInteractMode::HoldToDrag)
	{
		if (bDraggingPart)
		{
			if (!TrySnapDraggedPartToSocket()) { TryFreeAttachDraggedPart(); }
		}
		bDragging = false; bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; return;
	}

	if (DetachMode == EDetachInteractMode::ToggleToDrag)
	{
		// Release does nothing; second press ends drag path (handled in Press)
		return;
	}

	// ClickToggleAttach handled entirely in Press
}

void ARobotActor::OnArmAttachedChanged(bool bNowAttached)
{
	if (!Arm || !Assembly || !Arm->Config) return;
	for (const FName PartName : Arm->Config->ArmPartNames) Assembly->SetPartVisibility(PartName, bNowAttached);
	if (!bNowAttached) SpawnDetachVFXIfConfigured();
}

void ARobotActor::SpawnDetachVFXIfConfigured()
{
	if (!Arm || !Assembly || !Arm->Config || !Arm->Config->DetachEffect) return;
	FVector Loc = GetActorLocation();
	if (!Arm->Config->DetachEffectPartName.IsNone())
	{
		FVector PartLoc; if (Assembly->GetPartWorldLocation(Arm->Config->DetachEffectPartName, PartLoc)) Loc = PartLoc;
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Arm->Config->DetachEffect, Loc);
}

void ARobotActor::OnHighlightedChanged(bool bNowHighlighted)
{
	if (Assembly) Assembly->ApplyHighlightScalar(bNowHighlighted ?1.f :0.f);
}

void ARobotActor::ToggleArm()
{
	if (!Arm) return;
	if (Arm->IsAttached()) Arm->DetachFromRobot(); else Arm->AttachToRobot();
}

void ARobotActor::ScrambleParts()
{
	if (!Assembly || ScrambleIterations <=0) return;
	TArray<USceneComponent*> Targets; Assembly->GetAllAttachTargets(Targets);
	if (Targets.Num()==0) return;

	// Collect detachable part names
	TArray<FName> PartNames;
	for (const auto& PairComp : Targets)
	{
		for (const FRobotPartSpec& MapSpec : Assembly->AssemblyConfig->Parts)
		{
			if (Assembly->GetPartByName(MapSpec.PartName) == PairComp)
			{
				if (Assembly->IsDetachable(MapSpec.PartName)) PartNames.AddUnique(MapSpec.PartName);
			}
		}
	}
	if (PartNames.Num()==0) return;

	for (int32 Iter=0; Iter < ScrambleIterations; ++Iter)
	{
		for (FName PName : PartNames)
		{
			if (Assembly->IsPartDetached(PName)) continue; // already detached
			ARobotPartActor* TempActor=nullptr;
			if (Assembly->DetachPart(PName, TempActor) && TempActor)
			{
				if (bScramblePhysicsEnable) TempActor->EnablePhysics(true);
				// Find random attach target (nearest optional logic)
				USceneComponent* NewParent = nullptr; FName Socket = NAME_None; float Dist=0.f;
				if (ScrambleSocketSearchRadius >0.f && Assembly->FindNearestAttachTarget(TempActor->GetActorLocation(), NewParent, Socket, Dist) && Dist <= ScrambleSocketSearchRadius)
				{
					Assembly->AttachDetachedPartTo(PName, TempActor, NewParent, Socket);
				}
				else
				{
					// Random target using FMath
					const int32 Index = (Targets.Num() >0) ? FMath::RandRange(0, Targets.Num()-1) :0;
					NewParent = Targets[Index];
					Assembly->AttachDetachedPartTo(PName, TempActor, NewParent, NAME_None);
				}
			}
		}
	}
}

void ARobotActor::DetachPartByName(const FString& PartName)
{
	if (!Assembly) { UE_LOG(LogTemp, Warning, TEXT("No Assembly")); return; }
	ARobotPartActor* Out=nullptr; if (Assembly->DetachPart(FName(*PartName), Out)) { UE_LOG(LogTemp, Log, TEXT("Detached %s"), *PartName); } else { UE_LOG(LogTemp, Warning, TEXT("Detach failed for %s"), *PartName); }
}

void ARobotActor::ReattachPartByName(const FString& PartName)
{
	if (!Assembly) { UE_LOG(LogTemp, Warning, TEXT("No Assembly")); return; }
	ARobotPartActor* Found=nullptr; // find by name in DetachedParts map
	// naive scan: try to spawn name then check; rely on internal map via IsPartDetached and Reattach
	if (!Assembly->IsPartDetached(FName(*PartName))) { UE_LOG(LogTemp, Warning, TEXT("Part %s not marked detached"), *PartName); return; }
	// We don't have direct accessor to actor; call FindNearestAttachTarget from current actor position is not applicable. We'll ask free attach by name is not supported; do normal Reattach via internal map by passing nullptr is not allowed. So skip.
	UE_LOG(LogTemp, Warning, TEXT("Use in-game drag or alt-press to reattach; console reattach requires actor ref."));
}

void ARobotActor::ListRobotParts()
{
	if (!Assembly || !Assembly->AssemblyConfig) { UE_LOG(LogTemp, Warning, TEXT("No Assembly/Config")); return; }
	for (const FRobotPartSpec& S : Assembly->AssemblyConfig->Parts) { UE_LOG(LogTemp, Log, TEXT("Part: %s Parent:%s Socket:%s"), *S.PartName.ToString(), *S.ParentPartName.ToString(), *S.ParentSocketName.ToString()); }
}

void ARobotActor::TraceTest()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		APawn* P = PC->GetPawn(); if (!P) { UE_LOG(LogTemp, Warning, TEXT("No Pawn")); return; }
		if (UInteractionTraceComponent* Trace = P->FindComponentByClass<UInteractionTraceComponent>())
		{
			const FVector Start = P->GetActorLocation(); const FVector End = Start + P->GetActorForwardVector() *500.f;
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false,2.f,0,2.f);
			UE_LOG(LogTemp, Log, TEXT("TraceTest drew line from %s to %s"), *Start.ToString(), *End.ToString());
		}
	}
}
