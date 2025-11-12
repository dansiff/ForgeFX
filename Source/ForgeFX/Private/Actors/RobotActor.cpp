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

namespace { static const FName Part_Torso(TEXT("Torso")); }

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
		UE_LOG(LogTemp, Log, TEXT("RobotActor: AssemblyConfig=%s"), *GetNameSafe(Assembly->AssemblyConfig));
		Assembly->RebuildAssembly();
	}
	if (Arm) Arm->OnAttachedChanged.AddDynamic(this, &ARobotActor::OnArmAttachedChanged);
	if (Highlight) Highlight->OnHighlightedChanged.AddDynamic(this, &ARobotActor::OnHighlightedChanged);

	TryBindToPlayerTrace();

	// Prefer binding actions on the pawn's EnhancedInputComponent
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		APawn* P = PC->GetPawn();
		if (P)
		{
			if (UEnhancedInputComponent* PawnEIC = Cast<UEnhancedInputComponent>(P->InputComponent))
			{
				if (ToggleArmAction) { PawnEIC->BindAction(ToggleArmAction, ETriggerEvent::Started, this, &ARobotActor::ToggleArm); UE_LOG(LogTemp, Log, TEXT("RobotActor: Bound ToggleArm on Pawn EIC")); }
				if (ScrambleAction) { PawnEIC->BindAction(ScrambleAction, ETriggerEvent::Started, this, &ARobotActor::ScrambleParts); UE_LOG(LogTemp, Log, TEXT("RobotActor: Bound Scramble on Pawn EIC")); }
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
		FVector CursorWorld; if (ComputeCursorWorldOnPlane(PartDragPlaneZ, CursorWorld))
		{
			FVector Target = CursorWorld + PartDragOffset + DragArrowAccum;
			DraggedPartActor->SetActorLocation(FMath::VInterpTo(DraggedPartActor->GetActorLocation(), Target, DeltaSeconds, PartDragSmoothingSpeed));
		}
	}

	// Visual snap preview indicator in Tick after updating dragged part location
	if (bDraggingPart && DraggedPartActor && Assembly)
	{
		USceneComponent* Parent; FName Socket;
		if (Assembly->GetAttachParentAndSocket(DraggedPartName, Parent, Socket) && Parent)
		{
			const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
			const float Dist = FVector::Dist(DraggedPartActor->GetActorLocation(), SocketWorld.GetLocation());
			const bool bSnapSoon = (Dist <= AttachPosTolerance);
			PreviewSocketIndicator->SetWorldLocation(SocketWorld.GetLocation());
			PreviewSocketIndicator->SetVisibility(true, true);
			PreviewSocketIndicator->SetSphereRadius(PreviewSphereRadius);
			const FColor Col = bSnapSoon ? FColor::Green : FColor::Red;
			PreviewSocketIndicator->ShapeColor = Col;
		}
		else { PreviewSocketIndicator->SetVisibility(false, true); }
	}
	else { PreviewSocketIndicator->SetVisibility(false, true); }
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
	if (FVector::Dist(PartLoc, SocketWorld.GetLocation()) > AttachPosTolerance) return false;
	const float AngleDiff = DraggedPartActor->GetActorQuat().AngularDistance(SocketWorld.GetRotation()) *180.f / PI;
	if (AngleDiff > AttachAngleToleranceDeg) return false;
	Assembly->ReattachPart(DraggedPartName, DraggedPartActor);
	// Restore collision on component
	if (UStaticMeshComponent* Comp = Assembly->GetPartByName(DraggedPartName)) { Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
	bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; return true;
}

bool ARobotActor::TryFreeAttachDraggedPart()
{
	if (!bAllowFreeAttach || !Assembly || !DraggedPartActor || DraggedPartName.IsNone()) return false;
	USceneComponent* Parent = nullptr; FName Socket = NAME_None; float Dist=0.f;
	if (!Assembly->FindNearestAttachTarget(DraggedPartActor->GetActorLocation(), Parent, Socket, Dist)) return false;
	const float MaxD = (FreeAttachMaxDistance >0.f) ? FreeAttachMaxDistance : AttachPosTolerance;
	if (Dist > MaxD) return false;
	Assembly->AttachDetachedPartTo(DraggedPartName, DraggedPartActor, Parent, Socket);
	if (UStaticMeshComponent* Comp = Assembly->GetPartByName(DraggedPartName)) { Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
	bDraggingPart = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; return true;
}

void ARobotActor::OnHoverComponentChanged(UPrimitiveComponent* HitComponent, AActor* HitActor)
{
	if (!Assembly) return;
	if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		// Detached actors handle their own highlight; clear assembly highlight
		Assembly->ApplyHighlightScalarAll(0.f);
		HoveredPartName = PartActor->GetPartName();
		return;
	}

	FName Part;
	if (!HitComponent || !Assembly->FindPartNameByComponent(HitComponent, Part))
	{
		Assembly->ApplyHighlightScalarAll(0.f);
		HoveredPartName = NAME_None;
		return;
	}

	HoveredPartName = Part;
	// Torso => highlight all, otherwise highlight only the hovered part
	const bool bIsTorso = (Part == Part_Torso);
	Assembly->ApplyHighlightScalarAll(0.f);
	if (bIsTorso)
	{
		Assembly->ApplyHighlightScalarAll(1.f);
	}
	else
	{
		TArray<FName> One{ Part }; 
		Assembly->ApplyHighlightScalarToParts(One,1.f);
	}
}

void ARobotActor::OnInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor)
{
	UE_LOG(LogTemp, Log, TEXT("OnInteractPressed: HitComp=%s HitActor=%s DetachMode=%d"), *GetNameSafe(HitComponent), *GetNameSafe(HitActor), (int32)DetachMode);
	if (DetachMode == EDetachInteractMode::ClickToggleAttach)
	{
		if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
		{
			UE_LOG(LogTemp, Log, TEXT("ClickToggleAttach: attempting reattach for %s"), *PartActor->GetPartName().ToString());
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
					UE_LOG(LogTemp, Log, TEXT("Detached (ClickToggleAttach) %s"), *Part.ToString());
				}
				else
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
		UE_LOG(LogTemp, Log, TEXT("Begin drag existing detached part %s"), *PartActor->GetPartName().ToString());
		DraggedPartActor = PartActor; DraggedPartName = PartActor->GetPartName(); bDraggingPart = true; PartDragPlaneZ = PartActor->GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(PartDragPlaneZ, CursorWorld)) PartDragOffset = PartActor->GetActorLocation() - CursorWorld; return;
	}
	if (!Assembly) { UE_LOG(LogTemp, Warning, TEXT("No Assembly in OnInteractPressed")); return; }
	FName Part;
	if (HitComponent && Assembly->FindPartNameByComponent(HitComponent, Part))
	{
		if (!Assembly->IsPartDetached(Part))
		{
			ARobotPartActor* NewPartActor = nullptr; if (Assembly->DetachPart(Part, NewPartActor) && NewPartActor)
			{
				UE_LOG(LogTemp, Log, TEXT("Detached %s (drag mode)"), *Part.ToString());
				DraggedPartActor = NewPartActor; DraggedPartName = Part; PartDragPlaneZ = NewPartActor->GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(PartDragPlaneZ, CursorWorld)) PartDragOffset = NewPartActor->GetActorLocation() - CursorWorld; bDraggingPart = (DetachMode != EDetachInteractMode::ToggleToDrag) ? true : !bDraggingPart; return;
			}
			else UE_LOG(LogTemp, Warning, TEXT("Detach failed %s"), *Part.ToString());
		}
	}
	// Whole robot drag
	if (bAllowTorsoDrag && HitComponent && Assembly->FindPartNameByComponent(HitComponent, Part) && Part == Part_Torso)
	{
		UE_LOG(LogTemp, Log, TEXT("Torso drag engaged"));
		if (DetachMode == EDetachInteractMode::HoldToDrag)
		{
			bDragging = true; DragPlaneZ = GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) DragOffset = GetActorLocation() - CursorWorld; return;
		}
		else if (DetachMode == EDetachInteractMode::ToggleToDrag)
		{
			bDragging = !bDragging; if (bDragging) { DragPlaneZ = GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) DragOffset = GetActorLocation() - CursorWorld; }
			UE_LOG(LogTemp, Log, TEXT("Torso drag toggled to %s"), bDragging?TEXT("ON"):TEXT("OFF")); return;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("OnInteractPressed: no actionable target"));
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
	UE_LOG(LogTemp, Log, TEXT("ScrambleParts invoked"));
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
			UE_LOG(LogTemp, Log, TEXT("RobotActor: Bound to InteractionTraceComponent."));
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
	// Disable collision on hidden component
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UE_LOG(LogTemp, Log, TEXT("Direct detach under crosshair: %s"), *Part.ToString());
	if (OutPartName) *OutPartName = Part;
	if (bDrag)
	{
		DraggedPartActor = NewActor; DraggedPartName = Part; bDraggingPart = true; PartDragPlaneZ = NewActor->GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(PartDragPlaneZ, CursorWorld)) PartDragOffset = NewActor->GetActorLocation() - CursorWorld; DragArrowAccum = FVector::ZeroVector;
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
	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		InteractPressTime = GetWorld()->GetTimeSeconds(); bInteractCurrentlyHeld = true; bool bDragStart = (DetachMode != EDetachInteractMode::ClickToggleAttach); TryDetachUnderCrosshair(bDragStart);
	}
	if (bInteractCurrentlyHeld && PC->WasInputKeyJustReleased(EKeys::E))
	{
		float Held = GetWorld()->GetTimeSeconds() - InteractPressTime; bInteractCurrentlyHeld = false; bool bWasTap = Held <= TapThreshold;
		if (bWasTap && DetachMode == EDetachInteractMode::ClickToggleAttach)
		{
			float MX, MY; if (PC->GetMousePosition(MX, MY)) { FVector O,D; if (PC->DeprojectScreenPositionToWorld(MX, MY, O, D)) { FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit, O, O + D *2000.f, ECC_Visibility); if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(Hit.GetActor())) { FName PartName = PartActor->GetPartName(); Assembly->ReattachPart(PartName, PartActor); if (UStaticMeshComponent* Comp = Assembly->GetPartByName(PartName)) Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); UE_LOG(LogTemp, Log, TEXT("Direct tap reattach: %s"), *PartName.ToString()); } } }
		}
		else
		{
			if (bDraggingPart && DraggedPartActor)
			{
				if (!TrySnapDraggedPartToSocket()) { TryFreeAttachDraggedPart(); }
			}
			bDragging = false; DraggedPartActor = nullptr; DraggedPartName = NAME_None; bDraggingPart = false;
		}
	}

	// Arrow key nudging while dragging part
	if (bDraggingPart && DraggedPartActor)
	{
		FVector Nudge = FVector::ZeroVector;
		if (PC->IsInputKeyDown(EKeys::Up)) Nudge += FVector::ForwardVector;
		if (PC->IsInputKeyDown(EKeys::Down)) Nudge += -FVector::ForwardVector;
		if (PC->IsInputKeyDown(EKeys::Left)) Nudge += -FVector::RightVector;
		if (PC->IsInputKeyDown(EKeys::Right)) Nudge += FVector::RightVector;
		if (!Nudge.IsNearlyZero())
		{
			DragArrowAccum += Nudge * ArrowNudgeSpeed * DeltaSeconds;
		}
	}

	// Rotation while dragging
	if (bDraggingPart && DraggedPartActor)
	{
		float YawStep =0.f, PitchStep=0.f, RollStep=0.f;
		if (PC->IsInputKeyDown(EKeys::Q)) YawStep -=90.f * DeltaSeconds;
		if (PC->IsInputKeyDown(EKeys::E)) YawStep +=90.f * DeltaSeconds;
		if (PC->IsInputKeyDown(EKeys::PageUp)) PitchStep +=90.f * DeltaSeconds;
		if (PC->IsInputKeyDown(EKeys::PageDown)) PitchStep -=90.f * DeltaSeconds;
		if (PC->IsInputKeyDown(EKeys::Home)) RollStep +=90.f * DeltaSeconds;
		if (PC->IsInputKeyDown(EKeys::End)) RollStep -=90.f * DeltaSeconds;
		if (!FMath::IsNearlyZero(YawStep) || !FMath::IsNearlyZero(PitchStep) || !FMath::IsNearlyZero(RollStep))
		{
			FRotator R = DraggedPartActor->GetActorRotation(); R.Yaw += YawStep; R.Pitch += PitchStep; R.Roll += RollStep; DraggedPartActor->SetActorRotation(R);
		}
		// Mouse wheel: move along camera forward
		float Wheel=0.f; Wheel += PC->IsInputKeyDown(EKeys::MouseScrollUp) ? +1.f :0.f; Wheel += PC->IsInputKeyDown(EKeys::MouseScrollDown) ? -1.f :0.f;
		if (!FMath::IsNearlyZero(Wheel))
		{
			FVector CamLoc; FRotator CamRot; PC->GetPlayerViewPoint(CamLoc, CamRot);
			DragArrowAccum += CamRot.Vector() * (Wheel * MouseWheelDistanceStep);
		}
	}
}

void ARobotActor::StartShowcase()
{
	if (!Assembly || !Assembly->AssemblyConfig) return;
	ShowcaseOrder.Reset();
	for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts) if (Spec.bDetachable && Spec.PartName != Part_Torso) ShowcaseOrder.Add(Spec.PartName);
	ShowcaseOrder.Sort([](const FName& A, const FName& B){ return A.LexicalLess(B); });
	ShowcaseOrder.Add(Part_Torso);
	UWorld* W = GetWorld(); if (!W) return;
	AOrbitCameraRig* Rig = W->SpawnActor<AOrbitCameraRig>(AOrbitCameraRig::StaticClass(), GetActorLocation()+FVector(500,0,120), FRotator::ZeroRotator);
	if (Rig)
	{
		Rig->Setup(this,650.f,150.f,30.f);
		if (bShowcaseUseSpline) Rig->UseCircularSplinePath(ShowcaseSplinePoints);
		ShowcaseRig = Rig;
		if (APlayerController* PC = W->GetFirstPlayerController()) { FViewTargetTransitionParams Blend; Blend.BlendTime =0.6f; PC->SetViewTarget(Rig, Blend); }
	}
	ShowcaseIndex =0; GetWorldTimerManager().SetTimer(ShowcaseTimer, this, &ARobotActor::ShowcaseStep,0.6f, true,0.5f);
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
				// fan it slightly outward for visibility
				const FVector Dir = (OutActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				OutActor->SetActorLocation(OutActor->GetActorLocation() + Dir *35.f);
				if (Assembly->AssemblyConfig->DetachSound) UGameplayStatics::PlaySoundAtLocation(this, Assembly->AssemblyConfig->DetachSound, OutActor->GetActorLocation());
				if (Assembly->AssemblyConfig->DetachEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Assembly->AssemblyConfig->DetachEffect, OutActor->GetActorLocation());
			}
		}
		return;
	}
	// Reattach pass
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
				// find actor by scanning DetachedParts map via GetPartWorldLocation + nearest actor (simplified by calling TryFreeAttachDraggedPart path not available); call ReattachPart by finding actor from our map is private; we rely on AttachDetachedPartTo by finding parent.
				USceneComponent* Parent; FName Socket; if (Assembly->GetAttachParentAndSocket(Part, Parent, Socket))
				{
					// Find actor by name via brute scan of world actors of class ARobotPartActor
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
