#include "Actors/RobotActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/InteractionTraceComponent.h"
#include "Actors/RobotPartActor.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/OrbitCameraRig.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

namespace { static const FName Part_Torso(TEXT("Torso")); }

static FVector ComputeSafeOrbitCenter(const ARobotActor* Robot)
{
	return Robot ? Robot->GetActorLocation() : FVector::ZeroVector;
}

ARobotActor::ARobotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Arm = CreateDefaultSubobject<URobotArmComponent>(TEXT("RobotArm"));
	Highlight = CreateDefaultSubobject<UHighlightComponent>(TEXT("Highlight"));
	Assembly = CreateDefaultSubobject<UAssemblyBuilderComponent>(TEXT("Assembly"));
	PreviewSocketIndicator = CreateDefaultSubobject<USphereComponent>(TEXT("PreviewSocket"));
	PreviewSocketIndicator->InitSphereRadius(PreviewSphereRadius);
	PreviewSocketIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewSocketIndicator->SetVisibility(false, true);
	PreviewSocketIndicator->SetHiddenInGame(true);
	PreviewSocketIndicator->SetupAttachment(Root);
}

void ARobotActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Assembly) Assembly->BuildAssembly();
}

void ARobotActor::BeginPlay()
{
	Super::BeginPlay();
	if (Assembly)
	{
		if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("RobotActor: AssemblyConfig=%s"), *GetNameSafe(Assembly->AssemblyConfig));
		Assembly->RebuildAssembly();
	}
	if (Arm) Arm->OnAttachedChanged.AddDynamic(this, &ARobotActor::OnArmAttachedChanged);
	if (Highlight) Highlight->OnHighlightedChanged.AddDynamic(this, &ARobotActor::OnHighlightedChanged);

	TryBindToPlayerTrace();

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		APawn* P = PC->GetPawn();
		if (P)
		{
			if (UEnhancedInputComponent* PawnEIC = Cast<UEnhancedInputComponent>(P->InputComponent))
			{
				if (ToggleArmAction) { PawnEIC->BindAction(ToggleArmAction, ETriggerEvent::Started, this, &ARobotActor::ToggleArm); if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("RobotActor: Bound ToggleArm on Pawn EIC")); }
				if (ScrambleAction) { PawnEIC->BindAction(ScrambleAction, ETriggerEvent::Started, this, &ARobotActor::ScrambleParts); if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("RobotActor: Bound Scramble on Pawn EIC")); }
			}
		}
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (InputContext) { Subsys->AddMappingContext(InputContext, MappingPriority); }
			}
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
	if (!bTraceBound) TryBindToPlayerTrace();
	PollFallbackKeys(DeltaSeconds);
	if (bDragging && bAllowTorsoDrag)
	{
		FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld))
		{
			FVector Target = CursorWorld + DragOffset;
			SetActorLocation(FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, DragSmoothingSpeed));
		}
	}
	if (bDraggingPart && DraggedPartActor)
	{
		// Skyrim-style: maintain grab distance along camera forward
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		FVector ViewLoc; FRotator ViewRot;
		if (PC) PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		const FVector Fwd = ViewRot.Vector();
		const float TargetDist = FMath::Clamp(PartGrabDistance, PartGrabMinDistance, PartGrabMaxDistance);
		const FVector DesiredLoc = ViewLoc + Fwd * TargetDist;
		DraggedPartActor->SetActorLocation(FMath::VInterpTo(DraggedPartActor->GetActorLocation(), DesiredLoc, DeltaSeconds, PartDragSmoothingSpeed));
	}

	// Visual snap preview indicator
	if (bShowSnapPreviewIndicator && bDraggingPart && DraggedPartActor && Assembly)
	{
		USceneComponent* Parent; FName Socket;
		if (Assembly->GetAttachParentAndSocket(DraggedPartName, Parent, Socket) && Parent)
		{
			const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
			const float Dist = FVector::Dist(DraggedPartActor->GetActorLocation(), SocketWorld.GetLocation());
			const bool bSnapSoon = (Dist <= AttachPosTolerance);
			PreviewSocketIndicator->SetWorldLocation(SocketWorld.GetLocation());
			PreviewSocketIndicator->SetVisibility(true, true);
			PreviewSocketIndicator->SetHiddenInGame(false);
			PreviewSocketIndicator->SetSphereRadius(PreviewSphereRadius);
			PreviewSocketIndicator->ShapeColor = bSnapSoon ? FColor::Green : FColor::Red;
		}
		else { PreviewSocketIndicator->SetVisibility(false, true); PreviewSocketIndicator->SetHiddenInGame(true); }
	}
	else { PreviewSocketIndicator->SetVisibility(false, true); PreviewSocketIndicator->SetHiddenInGame(true); }
}

bool ARobotActor::ComputeCursorWorldOnPlane(float PlaneZ, FVector& OutWorldPoint) const
{
	static FVector LastGood = FVector::ZeroVector;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		float MX, MY; if (!PC->GetMousePosition(MX, MY)) { OutWorldPoint = LastGood; return false; }
		FVector WorldOrigin, WorldDir; if (!PC->DeprojectScreenPositionToWorld(MX, MY, WorldOrigin, WorldDir)) { OutWorldPoint = LastGood; return false; }
		if (FMath::IsNearlyZero(WorldDir.Z,1e-4f)) { OutWorldPoint = LastGood; return false; }
		const float T = (PlaneZ - WorldOrigin.Z) / WorldDir.Z;
		if (!FMath::IsFinite(T)) { OutWorldPoint = LastGood; return false; }
		OutWorldPoint = WorldOrigin + T * WorldDir; LastGood = OutWorldPoint; return true;
	}
	OutWorldPoint = LastGood; return false;
}

bool ARobotActor::TrySnapDraggedPartToSocket()
{
	if (!Assembly || !DraggedPartActor || DraggedPartName.IsNone()) return false;
	USceneComponent* Parent; FName Socket;
	if (!Assembly->GetAttachParentAndSocket(DraggedPartName, Parent, Socket)) return false;
	if (!Parent) return false;
	const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
	const FVector PartLoc = DraggedPartActor->GetActorLocation();
	if (FVector::Dist(PartLoc, SocketWorld.GetLocation()) > AttachPosTolerance) return false;
	const float AngleDiff = DraggedPartActor->GetActorQuat().AngularDistance(SocketWorld.GetRotation()) *180.f / PI;
	if (AngleDiff > AttachAngleToleranceDeg) return false;
	Assembly->ReattachPart(DraggedPartName, DraggedPartActor);
	if (UStaticMeshComponent* Comp = Assembly->GetPartByName(DraggedPartName)) { Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
	bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; return true;
}

bool ARobotActor::TryFreeAttachDraggedPart()
{
	if (!bAllowFreeAttach || !Assembly || !DraggedPartActor || DraggedPartName.IsNone()) return false;
	USceneComponent* Parent = nullptr; FName Socket = NAME_None; float Dist=0.f; // fixed NAME_None
	if (!Assembly->FindNearestAttachTarget(DraggedPartActor->GetActorLocation(), Parent, Socket, Dist, DraggedPartName)) return false;
	const float MaxD = (FreeAttachMaxDistance >0.f) ? FreeAttachMaxDistance : AttachPosTolerance;
	if (Dist > MaxD) return false;
	Assembly->AttachDetachedPartTo(DraggedPartName, DraggedPartActor, Parent, Socket);
	if (UStaticMeshComponent* Comp = Assembly->GetPartByName(DraggedPartName)) { Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
	bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; return true;
}

void ARobotActor::OnHoverComponentChanged(UPrimitiveComponent* HitComponent, AActor* HitActor)
{
	if (!Assembly) return;
	Assembly->ClearHoverOverride();
	static TWeakObjectPtr<UPrimitiveComponent> LastHoverOutlineComp; if (LastHoverOutlineComp.IsValid()) LastHoverOutlineComp->SetRenderCustomDepth(false); LastHoverOutlineComp.Reset();
	FName PartName = NAME_None;
	if (HitComponent && Assembly->FindPartNameByComponent(HitComponent, PartName))
	{
		HoveredPartName = PartName; TArray<FName> One{ PartName }; Assembly->ApplyHighlightScalarToParts(One,1.f);
		if (UStaticMeshComponent* Comp = Assembly->GetPartByName(PartName)) { Comp->SetRenderCustomDepth(true); LastHoverOutlineComp = Comp; Assembly->ApplyHoverOverride(Comp); }
	}
	else if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		HoveredPartName = PartActor->GetPartName(); TArray<FName> One{ HoveredPartName }; Assembly->ApplyHighlightScalarToParts(One,1.f);
		if (UStaticMeshComponent* Comp = PartActor->GetMeshComponent()) { Comp->SetRenderCustomDepth(true); LastHoverOutlineComp = Comp; Assembly->ApplyHoverOverride(Comp); }
	}
	else
	{
		HoveredPartName = NAME_None; Assembly->ApplyHighlightScalarAll(0.f);
	}
}

void ARobotActor::OnInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor)
{
	// torso drag speed dampening
	if (bDragging && DetachMode == EDetachInteractMode::HoldToDrag) DragSmoothingSpeed =6.f;
	if (DetachMode == EDetachInteractMode::ClickToggleAttach)
	{
		if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
		{
			DraggedPartActor = PartActor; DraggedPartName = PartActor->GetPartName();
			if (!TrySnapDraggedPartToSocket()) { TryFreeAttachDraggedPart(); }
			DraggedPartActor = nullptr; DraggedPartName = NAME_None; bDraggingPart = false; return;
		}
		if (Assembly && HitComponent)
		{
			FName Part; if (Assembly->FindPartNameByComponent(HitComponent, Part) && !Assembly->IsPartDetached(Part))
			{
				ARobotPartActor* NewActor=nullptr;
				if (Assembly->DetachPart(Part, NewActor))
				{
					if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("Detached (ClickToggleAttach) %s"), *Part.ToString());
				}
				else if (bEnableRobotDebugLogs)
				{
					UE_LOG(LogTemp, Warning, TEXT("Detach failed %s"), *Part.ToString());
				}
			}
		}
		return;
	}
	// Drag modes
	if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		DraggedPartActor = PartActor; DraggedPartName = PartActor->GetPartName(); bDraggingPart = true; 
		// Initialize Skyrim grab distance from current viewpoint
		APlayerController* PC = GetWorld()->GetFirstPlayerController(); FVector ViewLoc; FRotator ViewRot; if (PC) PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		PartGrabDistance = FMath::Clamp(FVector::Distance(ViewLoc, PartActor->GetActorLocation()), PartGrabMinDistance, PartGrabMaxDistance);
		return;
	}
	if (!Assembly) { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("No Assembly in OnInteractPressed")); return; }
	FName Part;
	if (HitComponent && Assembly->FindPartNameByComponent(HitComponent, Part))
	{
		if (!Assembly->IsPartDetached(Part))
		{
			ARobotPartActor* NewPartActor = nullptr; if (Assembly->DetachPart(Part, NewPartActor) && NewPartActor)
			{
				DraggedPartActor = NewPartActor; DraggedPartName = Part; 
				APlayerController* PC = GetWorld()->GetFirstPlayerController(); FVector ViewLoc; FRotator ViewRot; if (PC) PC->GetPlayerViewPoint(ViewLoc, ViewRot);
				PartGrabDistance = FMath::Clamp(FVector::Distance(ViewLoc, NewPartActor->GetActorLocation()), PartGrabMinDistance, PartGrabMaxDistance);
				bDraggingPart = true; return;
			}
			else if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("Detach failed %s"), *Part.ToString());
		}
	}
	// Whole robot drag
	if (bAllowTorsoDrag && HitComponent && Assembly->FindPartNameByComponent(HitComponent, Part) && Part == Part_Torso)
	{
		if (DetachMode == EDetachInteractMode::HoldToDrag)
		{
			bDragging = true; DragPlaneZ = GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) DragOffset = GetActorLocation() - CursorWorld; return;
		}
		else if (DetachMode == EDetachInteractMode::ToggleToDrag)
		{
			bDragging = !bDragging; if (bDragging) { DragPlaneZ = GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) DragOffset = GetActorLocation() - CursorWorld; }
			if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("Torso drag toggled to %s"), bDragging?TEXT("ON"):TEXT("OFF")); return;
		}
	}
	if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Verbose, TEXT("OnInteractPressed: no actionable target"));
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
		return;
	}
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
			if (Assembly->IsPartDetached(PName)) continue;
			ARobotPartActor* TempActor=nullptr;
			if (Assembly->DetachPart(PName, TempActor) && TempActor)
			{
				if (bScramblePhysicsEnable) TempActor->EnablePhysics(true);
				USceneComponent* NewParent = nullptr; FName Socket = NAME_None; float Dist=0.f;
				if (ScrambleSocketSearchRadius >0.f && Assembly->FindNearestAttachTarget(TempActor->GetActorLocation(), NewParent, Socket, Dist) && Dist <= ScrambleSocketSearchRadius)
				{
					Assembly->AttachDetachedPartTo(PName, TempActor, NewParent, Socket);
				}
				else
				{
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
	if (!Assembly) { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("No Assembly")); return; }
	ARobotPartActor* Out=nullptr; if (Assembly->DetachPart(FName(*PartName), Out)) { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("Detached %s"), *PartName); } else { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("Detach failed for %s"), *PartName); }
}

void ARobotActor::ReattachPartByName(const FString& PartName)
{
	if (!Assembly) { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("No Assembly")); return; }
	if (!Assembly->IsPartDetached(FName(*PartName))) { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("Part %s not marked detached"), *PartName); return; }
	if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("Use in-game drag or alt-press to reattach; console reattach requires actor ref."));
}

void ARobotActor::ListRobotParts()
{
	if (!Assembly || !Assembly->AssemblyConfig) { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Warning, TEXT("No Assembly/Config")); return; }
	for (const FRobotPartSpec& S : Assembly->AssemblyConfig->Parts) { if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("Part: %s Parent:%s Socket:%s"), *S.PartName.ToString(), *S.ParentPartName.ToString(), *S.ParentSocketName.ToString()); }
}

void ARobotActor::TryBindToPlayerTrace()
{
	if (bTraceBound) return;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		APawn* P = PC->GetPawn(); if (!P) return;
		UInteractionTraceComponent* Trace = P->FindComponentByClass<UInteractionTraceComponent>();
		if (Trace && !Trace->OnInteractPressed.IsAlreadyBound(this, &ARobotActor::OnInteractPressed))
		{
			Trace->OnHoverComponentChanged.AddDynamic(this, &ARobotActor::OnHoverComponentChanged);
			Trace->OnInteractPressed.AddDynamic(this, &ARobotActor::OnInteractPressed);
			Trace->OnInteractAltPressed.AddDynamic(this, &ARobotActor::OnInteractPressed);
			Trace->OnInteractReleased.AddDynamic(this, &ARobotActor::OnInteractReleased);
			CachedTrace = Trace;
			bTraceBound = true;
			if (bEnableRobotDebugLogs) UE_LOG(LogTemp, Log, TEXT("RobotActor: Bound to InteractionTraceComponent."));
		}
	}
}

bool ARobotActor::TryDetachUnderCrosshair(bool bDrag, FName* OutPartName)
{
	if (!Assembly) return false;
	APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!PC) return false;
	float MX, MY; if (!PC->GetMousePosition(MX, MY)) return false;
	FVector Origin, Dir; if (!PC->DeprojectScreenPositionToWorld(MX, MY, Origin, Dir)) return false;
	FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Dir *2000.f, ECC_Visibility);
	if (Hit.GetActor() != this) return false; UPrimitiveComponent* Comp = Hit.GetComponent(); if (!Comp) return false;
	FName Part; if (!Assembly->FindPartNameByComponent(Comp, Part) || Assembly->IsPartDetached(Part)) return false;
	float Now = GetWorld()->GetTimeSeconds(); if (float* Last = LastDetachTime.Find(Part)) { if (Now - *Last < DetachCooldownSeconds) return false; *Last = Now; } else { LastDetachTime.Add(Part, Now); }
	ARobotPartActor* NewActor=nullptr; if (!Assembly->DetachPart(Part, NewActor) || !NewActor) return false;
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (OutPartName) *OutPartName = Part;
	if (bDrag)
	{
		DraggedPartActor = NewActor; DraggedPartName = Part; bDraggingPart = true; 
		APlayerController* PC2 = GetWorld()->GetFirstPlayerController(); FVector ViewLoc; FRotator ViewRot; if (PC2) PC2->GetPlayerViewPoint(ViewLoc, ViewRot);
		PartGrabDistance = FMath::Clamp(FVector::Distance(ViewLoc, NewActor->GetActorLocation()), PartGrabMinDistance, PartGrabMaxDistance);
	}
	return true;
}

void ARobotActor::PollFallbackKeys(float DeltaSeconds)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!PC) return;
	if (PC->WasInputKeyJustPressed(EKeys::K)) { StartShowcase(); }
	if (ShowcaseIndex >=0)
	{
		if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Escape)) { EndShowcase(); }
	}
	if (PC->WasInputKeyJustPressed(EKeys::Y)) { ToggleArm(); }
	if (PC->WasInputKeyJustPressed(EKeys::P)) { ScrambleParts(); }

	// Manual orbit control during showcase
	if (ShowcaseRig.IsValid())
	{
		float Step =120.f * DeltaSeconds; // deg/sec
		if (PC->IsInputKeyDown(EKeys::Comma)) { ShowcaseRig->StepManual(-Step); }
		if (PC->IsInputKeyDown(EKeys::Period)) { ShowcaseRig->StepManual(+Step); }
	}

	// Mouse wheel adjusts grab distance when dragging part
	if (bDraggingPart)
	{
		float Wheel = PC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
		if (!FMath::IsNearlyZero(Wheel))
		{
			PartGrabDistance = FMath::Clamp(PartGrabDistance + Wheel * PartGrabDistanceStep, PartGrabMinDistance, PartGrabMaxDistance);
		}
	}

	// Removed E-key vertical mapping from pawn; Q is used for up
}

void ARobotActor::StartShowcase()
{
	if (!Assembly || !Assembly->AssemblyConfig) return;
	EndShowcase();
	ShowcaseOrder.Reset();
	for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts)
	{
		if (Spec.bDetachable && Spec.PartName != Part_Torso)
		{
			ShowcaseOrder.Add(Spec.PartName);
		}
	}
	if (ShowcaseOrder.Num()==0)
	{
		return; // nothing to show
	}
	ShowcaseOrder.Sort([](const FName& A, const FName& B){ return A.LexicalLess(B); });
	ShowcaseOrder.Add(Part_Torso);

	UWorld* W = GetWorld(); if (!W) return;
	const FVector Center = ComputeSafeOrbitCenter(this);
	const FVector SpawnLoc = Center + FVector(ShowcaseOrbitRadius,0.f,ShowcaseOrbitHeight);
	AOrbitCameraRig* Rig = W->SpawnActor<AOrbitCameraRig>(AOrbitCameraRig::StaticClass(), SpawnLoc, FRotator::ZeroRotator);
	if (Rig)
	{
		Rig->Setup(this, ShowcaseOrbitRadius, ShowcaseOrbitHeight, ShowcaseOrbitSpeedDeg);
		Rig->bAutoOrbit = false; // manual only unless toggled in details
		if (bShowcaseUseSpline) Rig->UseCircularSplinePath(ShowcaseSplinePoints);
		ShowcaseRig = Rig;
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			FViewTargetTransitionParams Blend; Blend.BlendTime =0.5f; PC->SetViewTarget(Rig, Blend);
		}
	}
	ShowcaseIndex =0; GetWorldTimerManager().SetTimer(ShowcaseTimer, this, &ARobotActor::ShowcaseStep, FMath::Max(ShowcaseDetachInterval,0.25f), true, ShowcaseInitialDelay);
}

void ARobotActor::ShowcaseStep()
{
	if (!Assembly || ShowcaseIndex <0) { EndShowcase(); return; }
	if (ShowcaseIndex < ShowcaseOrder.Num())
	{
		const FName Part = ShowcaseOrder[ShowcaseIndex++];
		if (!Assembly->IsPartDetached(Part))
		{
			ARobotPartActor* OutActor=nullptr;
			if (Assembly->DetachPart(Part, OutActor) && OutActor)
			{
				const FVector Dir = (OutActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				OutActor->SetActorLocation(OutActor->GetActorLocation() + Dir *35.f);
				if (Assembly->AssemblyConfig->DetachSound) UGameplayStatics::PlaySoundAtLocation(this, Assembly->AssemblyConfig->DetachSound, OutActor->GetActorLocation());
				if (Assembly->AssemblyConfig->DetachEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Assembly->AssemblyConfig->DetachEffect, OutActor->GetActorLocation());
			}
		}
		return;
	}
	// pause one tick before reattach, then reattach all
	if (ShowcaseIndex == ShowcaseOrder.Num())
	{
		ShowcaseIndex++;
		return;
	}
	if (ShowcaseIndex > ShowcaseOrder.Num())
	{
		for (int32 i=0;i<ShowcaseOrder.Num();++i)
		{
			const FName Part = ShowcaseOrder[i];
			if (Assembly->IsPartDetached(Part))
			{
				USceneComponent* Parent; FName Socket; if (Assembly->GetAttachParentAndSocket(Part, Parent, Socket))
				{
					for (TActorIterator<ARobotPartActor> It(GetWorld()); It; ++It)
					{
						if (It->GetPartName()==Part) { Assembly->ReattachPart(Part, *It); break; }
					}
					if (Assembly->AssemblyConfig->AttachSound && Parent)
					{
						UGameplayStatics::PlaySoundAtLocation(this, Assembly->AssemblyConfig->AttachSound, Parent->GetComponentLocation());
					}
				}
			}
		}
		EndShowcase();
	}
}

void ARobotActor::EndShowcase()
{
	GetWorldTimerManager().ClearTimer(ShowcaseTimer);
	ShowcaseIndex = -1; ShowcaseOrder.Reset();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FViewTargetTransitionParams Blend; Blend.BlendTime =0.6f; PC->SetViewTarget(this, Blend);
	}
	if (ShowcaseRig.IsValid()) { ShowcaseRig->Destroy(); ShowcaseRig.Reset(); }
	if (ShowcaseWidget) { ShowcaseWidget->RemoveFromParent(); ShowcaseWidget = nullptr; }
}

void ARobotActor::TraceTest()
{
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		APawn* P = PC->GetPawn(); if (!P) { UE_LOG(LogTemp, Warning, TEXT("TraceTest: No Pawn")); return; }
		if (UInteractionTraceComponent* Trace = P->FindComponentByClass<UInteractionTraceComponent>())
		{
			const FVector Start = P->GetActorLocation();
			const FVector End = Start + P->GetActorForwardVector() *500.f;
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false,2.f,0,2.f);
		}
	}
}
