#include "Actors/RobotActor.h"
#include "Components/CinematicAssembleComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/AssemblyBuilderComponent.h"
#include "Components/HighlightComponent.h"
#include "Components/RobotArmComponent.h"
#include "Components/PartInteractionComponent.h"
#include "Components/RobotShowcaseComponent.h"
#include "Components/InteractionTraceComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextBlock.h"
#include "Actors/RobotPartActor.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

static const FName Part_Torso(TEXT("Torso"));

ARobotActor::ARobotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Arm = CreateDefaultSubobject<URobotArmComponent>(TEXT("RobotArm"));
	Highlight = CreateDefaultSubobject<UHighlightComponent>(TEXT("Highlight"));
	Assembly = CreateDefaultSubobject<UAssemblyBuilderComponent>(TEXT("Assembly"));
	Cinematic = CreateDefaultSubobject<UCinematicAssembleComponent>(TEXT("Cinematic"));
	PartInteraction = CreateDefaultSubobject<UPartInteractionComponent>(TEXT("PartInteraction"));
	Showcase = CreateDefaultSubobject<URobotShowcaseComponent>(TEXT("Showcase"));
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
}

void ARobotActor::StartShowcase() { if (Showcase) Showcase->StartShowcase(); }
void ARobotActor::StopShowcase() { if (Showcase) Showcase->StopShowcase(); }
void ARobotActor::ForceDropHeldPart(bool bTrySnap) { if (PartInteraction) PartInteraction->ForceDropHeldPart(bTrySnap); }
void ARobotActor::TriggerCinematicAssemble(FVector NewLocation) { if (Cinematic) Cinematic->TriggerAssemble(NewLocation); }

bool ARobotActor::ForceCrosshairDetach(bool /*bDrag*/)
{
	if (!Assembly) return false;
	APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!PC) return false;
	int32 SX=0,SY=0; PC->GetViewportSize(SX,SY); const float MX = SX*0.5f; const float MY = SY*0.5f;
	FVector Origin, Dir; if (!PC->DeprojectScreenPositionToWorld(MX, MY, Origin, Dir)) return false;
	FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Dir *2000.f, ECC_Visibility);
	if (Hit.GetActor() != this) return false; UPrimitiveComponent* Comp = Hit.GetComponent(); if (!Comp) return false;
	FName Part; if (!Assembly->FindPartNameByComponent(Comp, Part) || Assembly->IsPartDetached(Part)) return false;
	ARobotPartActor* NewActor=nullptr; if (!Assembly->DetachPart(Part, NewActor) || !NewActor) return false;
	UpdateStatusText(false); return true;
}

void ARobotActor::BeginPlay()
{
	Super::BeginPlay();
	if (Assembly)
	{
		Assembly->BuildAssembly();
		Assembly->OnRobotPartDetach.AddDynamic(this, &ARobotActor::OnAssemblyPartDetached);
		Assembly->OnRobotPartReattach.AddDynamic(this, &ARobotActor::OnAssemblyPartReattached);
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
				if (ToggleArmAction) PawnEIC->BindAction(ToggleArmAction, ETriggerEvent::Started, this, &ARobotActor::ToggleArm);
				if (ScrambleAction) PawnEIC->BindAction(ScrambleAction, ETriggerEvent::Started, this, &ARobotActor::ScrambleParts);
			}
		}
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (InputContext) Subsys->AddMappingContext(InputContext, MappingPriority);
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
	UpdateStatusText(true); // initial status
}

void ARobotActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bTraceBound) TryBindToPlayerTrace();
	PollFallbackKeys(DeltaSeconds);
	if (bDragging && bAllowTorsoDrag)
	{
		FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld))
			SetActorLocation(FMath::VInterpTo(GetActorLocation(), CursorWorld + DragOffset, DeltaSeconds, DragSmoothingSpeed));
	}
	if (PartInteraction)
	{
		PartInteraction->TickPartDrag(DeltaSeconds, PartDragSmoothingSpeed, PartInteraction->GetPartGrabDistance(), PartGrabMinDistance, PartGrabMaxDistance);
		UpdateReattachPreview();
		UpdateSnapReadiness();
	}
	// auto-clear prompt
	if (bPromptVisible && GetWorld())
	{
		if (GetWorld()->GetTimeSeconds() >= PromptExpiryTime) { ClearPrompt(); }
	}
}

bool ARobotActor::ComputeCursorWorldOnPlane(float PlaneZ, FVector& OutWorldPoint) const
{
	static FVector LastGood = FVector::ZeroVector;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		float MX, MY; if (!PC->GetMousePosition(MX, MY)) { OutWorldPoint = LastGood; return false; }
		FVector WorldOrigin, WorldDir; if (!PC->DeprojectScreenPositionToWorld(MX, MY, WorldOrigin, WorldDir)) { OutWorldPoint = LastGood; return false; }
		if (FMath::IsNearlyZero(WorldDir.Z,1e-4f)) { OutWorldPoint = LastGood; return false; }
		const float T = (PlaneZ - WorldOrigin.Z) / WorldDir.Z; if (!FMath::IsFinite(T)) { OutWorldPoint = LastGood; return false; }
		OutWorldPoint = WorldOrigin + T * WorldDir; LastGood = OutWorldPoint; return true;
	}
	OutWorldPoint = LastGood; return false;
}

void ARobotActor::TryBindToPlayerTrace()
{
	if (bTraceBound) return;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		APawn* P = PC->GetPawn(); if (!P) return;
		if (UInteractionTraceComponent* Trace = P->FindComponentByClass<UInteractionTraceComponent>())
		{
			Trace->OnHoverComponentChanged.AddDynamic(this, &ARobotActor::OnHoverComponentChanged);
			Trace->OnInteractPressed.AddDynamic(this, &ARobotActor::OnInteractPressed);
			Trace->OnInteractAltPressed.AddDynamic(this, &ARobotActor::OnInteractPressed);
			Trace->OnInteractReleased.AddDynamic(this, &ARobotActor::OnInteractReleased);
			CachedTrace = Trace; bTraceBound = true;
		}
	}
}

void ARobotActor::PollFallbackKeys(float DeltaSeconds)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!PC) return;
	const bool bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
	if (PC->WasInputKeyJustPressed(EKeys::R)) { if (bShift) BatchReattachSelected(); else ReattachAllDetached(); }
	if (PC->WasInputKeyJustPressed(EKeys::D)) { BatchDetachSelected(); }
	if (PC->WasInputKeyJustPressed(EKeys::Y)) ToggleArm();
	if (PC->WasInputKeyJustPressed(EKeys::P)) ScrambleParts();
	if (PC->WasInputKeyJustPressed(EKeys::Escape) && PartInteraction && PartInteraction->IsDraggingPart())
	{
		PartInteraction->ForceDropHeldPart(false); ShowPrompt(TEXT("Drag cancelled"),1.f); return;
	}
	if (PartInteraction && PartInteraction->IsDraggingPart())
	{
		float Wheel = PC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
		if (!FMath::IsNearlyZero(Wheel)) PartInteraction->AdjustGrabDistance(Wheel * PartGrabDistanceStep, PartGrabMinDistance, PartGrabMaxDistance);
	}
}

void ARobotActor::SetStatusMessage(const FString& Msg)
{
	if (!StatusWidget) return; if (UTextBlock* TB = Cast<UTextBlock>(StatusWidget->GetWidgetFromName(TEXT("StatusText")))) TB->SetText(FText::FromString(Msg));
}

void ARobotActor::SetCustomDepthAll(bool bEnable)
{
	if (!Assembly) return;
	TArray<USceneComponent*> Targets; Assembly->GetAllAttachTargets(Targets);
	for (USceneComponent* S : Targets) if (UStaticMeshComponent* C = Cast<UStaticMeshComponent>(S)) C->SetRenderCustomDepth(bEnable);
}

void ARobotActor::UpdateStatusText(bool bAttached)
{
	if (!StatusWidget) return;
	if (UTextBlock* TB = Cast<UTextBlock>(StatusWidget->GetWidgetFromName(TEXT("StatusText"))))
		TB->SetText(FText::FromString(bAttached?TEXT("Attached"):TEXT("Detached")));
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
		if (PartName == Part_Torso) SetCustomDepthAll(true);
		else
		{
			SetCustomDepthAll(false);
			if (UStaticMeshComponent* Comp = Assembly->GetPartByName(PartName)) { Comp->SetRenderCustomDepth(true); LastHoverOutlineComp = Comp; Assembly->ApplyHoverOverride(Comp); }
		}
	}
	else if (ARobotPartActor* PartActor = Cast<ARobotPartActor>(HitActor))
	{
		HoveredPartName = PartActor->GetPartName(); TArray<FName> One{ HoveredPartName }; Assembly->ApplyHighlightScalarToParts(One,1.f);
		SetCustomDepthAll(false);
		if (UStaticMeshComponent* Comp = PartActor->GetMeshComponent()) { Comp->SetRenderCustomDepth(true); LastHoverOutlineComp = Comp; Assembly->ApplyHoverOverride(Comp); }
	}
	else
	{
		HoveredPartName = NAME_None; Assembly->ApplyHighlightScalarAll(0.f); SetCustomDepthAll(false);
	}
}

// Helper to update reattach preview
void ARobotActor::UpdateReattachPreview()
{
	if (!Assembly || !PartInteraction) return;
	ARobotPartActor* Dragged = PartInteraction->GetDraggedPartActor();
	if (!Dragged)
	{
		// restore any previous preview materials
		for (auto& Pair : PreviewOriginalMaterials)
		{
			if (Pair.Key.IsValid()) Pair.Key->SetMaterial(0, Pair.Value);
		}
		PreviewOriginalMaterials.Empty(); PreviewMIDs.Empty(); CurrentPreviewComp.Reset();
		return;
	}
	FName DragName = PartInteraction->GetDraggedPartName();
	USceneComponent* Parent; FName Socket;
	if (Assembly->GetAttachParentAndSocket(DragName, Parent, Socket) && Parent)
	{
		if (UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(Parent))
		{
			if (CurrentPreviewComp.Get() != Comp)
			{
				// restore old
				for (auto& Pair : PreviewOriginalMaterials) if (Pair.Key.IsValid()) Pair.Key->SetMaterial(0, Pair.Value);
				PreviewOriginalMaterials.Empty(); PreviewMIDs.Empty();
				CurrentPreviewComp = Comp;
				if (ReattachPreviewMaterial)
				{
					PreviewOriginalMaterials.Add(Comp, Comp->GetMaterial(0));
					Comp->SetMaterial(0, ReattachPreviewMaterial);
					if (UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamic(0)) PreviewMIDs.Add(Comp, MID);
				}
				Comp->SetRenderCustomDepth(true);
			}
			// animate pulse
			PreviewAccumTime += GetWorld() ? GetWorld()->GetDeltaSeconds() :0.f;
			if (UMaterialInstanceDynamic* MID = PreviewMIDs.FindRef(Comp))
			{
				float Pulse =0.5f +0.5f * FMath::Sin(PreviewAccumTime * PreviewPulseSpeed * PI);
				MID->SetScalarParameterValue(TEXT("Pulse"), Pulse);
			}
		}
	}
}

// Snap readiness computation: distance + angle thresholds
void ARobotActor::UpdateSnapReadiness()
{
	bSnapReady = false;
	if (!Assembly || !PartInteraction || !PartInteraction->IsDraggingPart()) { UpdateSnapMaterialParams(); return; }
	FName DragName = PartInteraction->GetDraggedPartName(); if (DragName.IsNone()) { UpdateSnapMaterialParams(); return; }
	USceneComponent* Parent; FName Socket; if (!Assembly->GetAttachParentAndSocket(DragName, Parent, Socket) || !Parent) { UpdateSnapMaterialParams(); return; }
	if (ARobotPartActor* Dragged = PartInteraction->GetDraggedPartActor())
	{
		const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
		const float Dist = FVector::Dist(Dragged->GetActorLocation(), SocketWorld.GetLocation());
		const float AngleDiff = Dragged->GetActorQuat().AngularDistance(SocketWorld.GetRotation()) *180.f/PI;
		bSnapReady = (Dist <= AttachPosTolerance) && (AngleDiff <= AttachAngleToleranceDeg);
		UpdateSocketInfoWidget(SocketWorld.GetLocation(), Socket, bSnapReady);
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector2D Screen; PC->ProjectWorldLocationToScreen(SocketWorld.GetLocation(), Screen);
			PositionSocketInfoWidget(Screen + FVector2D(10.f, -30.f));
		}
	}
	UpdateSnapMaterialParams();
}

void ARobotActor::UpdateSnapMaterialParams()
{
	if (!CurrentPreviewComp.IsValid()) return;
	if (UMaterialInstanceDynamic* MID = PreviewMIDs.FindRef(CurrentPreviewComp))
	{
		MID->SetVectorParameterValue(TEXT("SnapColor"), bSnapReady? SnapReadyColor : SnapNotReadyColor);
	}
}

void ARobotActor::PositionSocketInfoWidget(const FVector2D& ScreenPos)
{
	if (!SocketInfoWidget) return;
	// Assumes widget has an exposed SetPositionInViewport in Blueprint or uses SetDesiredPositionInViewport
	SocketInfoWidget->SetDesiredSizeInViewport(FVector2D(150.f,60.f));
	SocketInfoWidget->SetPositionInViewport(ScreenPos, false);
}

void ARobotActor::BatchDetachSelected()
{
	if (!Assembly) return; int32 Count=0; for (FName P : SelectedParts) if (!Assembly->IsPartDetached(P)){ ARobotPartActor* A=nullptr; if (Assembly->DetachPart(P, A)&&A) ++Count; }
	ShowPrompt(FString::Printf(TEXT("Detached %d selected"), Count),1.5f);
}

void ARobotActor::BatchReattachSelected()
{
	if (!Assembly) return; int32 Count=0; for (FName P : SelectedParts){ if (Assembly->IsPartDetached(P)){ for (TActorIterator<ARobotPartActor> It(GetWorld()); It; ++It){ if (It->GetPartName()==P){ if (Assembly->ReattachPart(P, *It)) ++Count; break; } } } }
	ShowPrompt(FString::Printf(TEXT("Reattached %d selected"), Count),1.5f);
}

void ARobotActor::ClearSelection()
{
	// restore materials
	for (FName P : SelectedParts)
	{
		if (UStaticMeshComponent* Comp = Assembly? Assembly->GetPartByName(P):nullptr)
		{
			Comp->SetRenderCustomDepth(false);
			// Could restore material if we changed it
		}
	}
	SelectedParts.Empty(); ShowPrompt(TEXT("Selection cleared"),1.f);
}

// Extend TogglePartSelection to apply selection material
void ARobotActor::TogglePartSelection(FName PartName)
{
	if (PartName.IsNone() || !Assembly) return;
	if (SelectedParts.Contains(PartName))
	{
		SelectedParts.Remove(PartName); if (UStaticMeshComponent* Comp = Assembly->GetPartByName(PartName)) { Comp->SetRenderCustomDepth(false); }
		ShowPrompt(TEXT("Deselected part"),0.8f); UpdateSnapMaterialParams(); return;
	}
	SelectedParts.Add(PartName);
	if (UStaticMeshComponent* Comp = Assembly->GetPartByName(PartName))
	{
		Comp->SetRenderCustomDepth(true);
		if (SelectedPreviewMaterial) Comp->SetMaterial(0, SelectedPreviewMaterial);
	}
	ShowPrompt(TEXT("Selected part"),0.8f); UpdateSnapMaterialParams();
}

void ARobotActor::DumpAssemblyState(){ if (Assembly) Assembly->DumpState(); }

void ARobotActor::EnforceHideForDetached()
{
	if (!Assembly || !Assembly->AssemblyConfig) return;
	for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts)
		if (Assembly->IsPartDetached(Spec.PartName))
			if (UStaticMeshComponent* Comp = Assembly->GetPartByName(Spec.PartName))
				{ Comp->SetHiddenInGame(true); Comp->SetVisibility(false,true); Comp->SetRenderInMainPass(false); Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision); Comp->SetRenderCustomDepth(false); Comp->MarkRenderStateDirty(); }
}

void ARobotActor::OnAssemblyPartDetached(FName /*PartName*/, ARobotPartActor* /*SpawnedActor*/)
{
	UpdateStatusText(false);
}

void ARobotActor::OnAssemblyPartReattached(FName /*PartName*/)
{
	bool bAnyDetached=false; if (Assembly && Assembly->AssemblyConfig)
		for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts)
			if (Spec.bDetachable && Assembly->IsPartDetached(Spec.PartName)) { bAnyDetached=true; break; }
	UpdateStatusText(!bAnyDetached);
}

void ARobotActor::ShowPrompt(const FString& Msg, float DurationSeconds)
{
	LastPrompt = Msg; bPromptVisible = true; PromptExpiryTime = GetWorld() ? GetWorld()->GetTimeSeconds() + DurationSeconds :0.f;
	SetStatusMessage(Msg);
}

void ARobotActor::ClearPrompt()
{
	bPromptVisible = false; LastPrompt.Empty();
	SetStatusMessage(TEXT(""));
}

void ARobotActor::OnInteractPressed(UPrimitiveComponent* HitComponent, AActor* HitActor)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	bool bCtrl = PC && (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl));

	if (bCtrl && HoveredPartName != NAME_None)
	{
		TogglePartSelection(HoveredPartName);
		return;
	}
	if (PartInteraction && PartInteraction->IsDraggingPart())
	{
		PartInteraction->ForceDropHeldPart(true); ShowPrompt(bSnapReady?TEXT("Snapped!"):TEXT("Dropped"),1.0f); return;
	}
	FName TargetPart = HoveredPartName;
	if (TargetPart != NAME_None && Assembly && !Assembly->IsPartDetached(TargetPart))
	{
		ARobotPartActor* NewActor=nullptr; if (Assembly->DetachPart(TargetPart, NewActor) && NewActor)
		{
			ShowPrompt(TEXT("Detached; drag"),1.2f);
			if (PartInteraction) PartInteraction->HandleInteractPressed(nullptr, NewActor, bAllowFreeAttach, AttachPosTolerance, AttachAngleToleranceDeg, PartGrabMinDistance, PartGrabMaxDistance);
			return;
		}
	}
	if (PartInteraction && PartInteraction->HandleInteractPressed(HitComponent, HitActor, bAllowFreeAttach, AttachPosTolerance, AttachAngleToleranceDeg, PartGrabMinDistance, PartGrabMaxDistance)) { ShowPrompt(TEXT("Dragging part"),1.5f); return; }
	if (HitComponent && Assembly)
	{
		FName Part; if (Assembly->FindPartNameByComponent(HitComponent, Part) && Part == Part_Torso && bAllowTorsoDrag)
		{
			bDragging = !bDragging; if (bDragging){ DragPlaneZ=GetActorLocation().Z; FVector CursorWorld; if (ComputeCursorWorldOnPlane(DragPlaneZ, CursorWorld)) DragOffset = GetActorLocation()-CursorWorld; ShowPrompt(TEXT("Dragging robot"),1.5f);} else ShowPrompt(TEXT("Robot drag ended"),1.f); return;
		}
	}
}

void ARobotActor::AttachArm(){ if (Arm && !bArmAttached) Arm->AttachToRobot(); }
void ARobotActor::DetachArm(){ if (Arm && bArmAttached) Arm->DetachFromRobot(); }
void ARobotActor::OnArmAttachedChanged(bool bNowAttached)
{
	bArmAttached = bNowAttached;
	if (!Arm || !Assembly || !Arm->Config) return;
	for (const FName P : Arm->Config->ArmPartNames) Assembly->SetPartVisibility(P, bNowAttached);
	UpdateStatusText(bNowAttached);
	if (!bNowAttached) SpawnDetachVFXIfConfigured();
}

// --- Missing method implementations to resolve link errors ---

void ARobotActor::ScrambleParts()
{
	if (!Assembly || ScrambleIterations <=0 || !Assembly->AssemblyConfig) return;
	TArray<FName> PartNames; for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts) if (Assembly->IsDetachable(Spec.PartName)) PartNames.Add(Spec.PartName);
	if (PartNames.Num()==0) return;
	for (int32 Iter=0; Iter < ScrambleIterations; ++Iter)
	{
		for (FName PName : PartNames)
		{
			if (Assembly->IsPartDetached(PName)) continue;
			ARobotPartActor* TempActor=nullptr; if (Assembly->DetachPart(PName, TempActor) && TempActor)
			{
				USceneComponent* NewParent=nullptr; FName Socket=NAME_None; float Dist=0.f;
				if (ScrambleSocketSearchRadius >0.f && Assembly->FindNearestAttachTarget(TempActor->GetActorLocation(), NewParent, Socket, Dist) && Dist <= ScrambleSocketSearchRadius)
					Assembly->AttachDetachedPartTo(PName, TempActor, NewParent, Socket);
				else
				{
					TArray<USceneComponent*> Targets; Assembly->GetAllAttachTargets(Targets); if (Targets.Num()>0){ NewParent = Targets[FMath::RandRange(0, Targets.Num()-1)]; Assembly->AttachDetachedPartTo(PName, TempActor, NewParent, NAME_None);} }
			}
		}
	}
}

void ARobotActor::DetachPartByName(const FString& PartName)
{
	if (!Assembly) return; ARobotPartActor* Out=nullptr; if (Assembly->DetachPart(FName(*PartName), Out)) UpdateStatusText(false);
}

void ARobotActor::ReattachPartByName(const FString& /*PartName*/)
{
	// Intentionally minimal; prefer in-game drag/snap
}

void ARobotActor::ListRobotParts()
{
	if (!Assembly || !Assembly->AssemblyConfig) return;
	for (const FRobotPartSpec& S : Assembly->AssemblyConfig->Parts)
		UE_LOG(LogTemp, Log, TEXT("Part: %s Parent:%s Socket:%s"), *S.PartName.ToString(), *S.ParentPartName.ToString(), *S.ParentSocketName.ToString());
}

void ARobotActor::TraceTest()
{
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		APawn* P = PC->GetPawn(); if (!P) return;
		UInteractionTraceComponent* Trace = P->FindComponentByClass<UInteractionTraceComponent>(); if (!Trace) return;
		const FVector Start = P->GetActorLocation(); const FVector End = Start + P->GetActorForwardVector() *500.f;
		DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false,2.f,0,2.f);
	}
}

TArray<FName> ARobotActor::GetDetachableParts() const
{
	TArray<FName> Out; if (Assembly && Assembly->AssemblyConfig) for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts) if (Spec.bDetachable) Out.Add(Spec.PartName); return Out;
}

bool ARobotActor::IsPartCurrentlyDetached(FName PartName) const
{
	return Assembly ? Assembly->IsPartDetached(PartName) : false;
}

bool ARobotActor::DetachPartForTest(FName PartName)
{
	if (!Assembly || PartName.IsNone() || Assembly->IsPartDetached(PartName)) return false; ARobotPartActor* Out=nullptr; return Assembly->DetachPart(PartName, Out) && Out!=nullptr;
}

bool ARobotActor::ReattachPartForTest(FName PartName)
{
	if (!Assembly || PartName.IsNone() || !Assembly->IsPartDetached(PartName)) return false;
	for (TActorIterator<ARobotPartActor> It(GetWorld()); It; ++It) if (It->GetPartName()==PartName) return Assembly->ReattachPart(PartName, *It);
	return false;
}

void ARobotActor::OnHighlightedChanged(bool bNowHighlighted)
{
	if (Assembly) Assembly->ApplyHighlightScalar(bNowHighlighted ?1.f :0.f);
}

void ARobotActor::OnInteractReleased()
{
	if (PartInteraction && PartInteraction->IsDraggingPart())
	{
		PartInteraction->ForceDropHeldPart(true);
		ShowPrompt(TEXT("Released part"),1.f);
	}
	bDragging = false;
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

void ARobotActor::UpdateSocketInfoWidget(const FVector& SocketWorldLoc, const FName& SocketName, bool bReady)
{
	if (!SocketInfoWidget && SocketInfoWidgetClass)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			SocketInfoWidget = CreateWidget<UUserWidget>(PC, SocketInfoWidgetClass);
			if (SocketInfoWidget) SocketInfoWidget->AddToViewport(5);
		}
	}
	if (!SocketInfoWidget) return;
	LastPreviewSocketName = SocketName;
	if (UTextBlock* NameTB = Cast<UTextBlock>(SocketInfoWidget->GetWidgetFromName(TEXT("SocketNameText"))))
		NameTB->SetText(FText::FromName(SocketName));
	if (UTextBlock* StateTB = Cast<UTextBlock>(SocketInfoWidget->GetWidgetFromName(TEXT("SnapStateText"))))
		StateTB->SetText(FText::FromString(bReady?TEXT("Ready") : TEXT("Align / Move Closer")));
}

void ARobotActor::ReattachAllDetached()
{
	if (!Assembly || !Assembly->AssemblyConfig) return;
	int32 Count=0;
	for (const FRobotPartSpec& Spec : Assembly->AssemblyConfig->Parts)
	{
		if (Assembly->IsPartDetached(Spec.PartName))
		{
			for (TActorIterator<ARobotPartActor> It(GetWorld()); It; ++It)
			{
				if (It->GetPartName() == Spec.PartName)
				{
					if (Assembly->ReattachPart(Spec.PartName, *It)) ++Count;
					break;
				}
			}
		}
	}
	ShowPrompt(FString::Printf(TEXT("Reattached %d"), Count),1.5f);
}
// --- end missing methods ---

void ARobotActor::ToggleArm()
{
	if (!Arm) return;
	if (bArmAttached) Arm->DetachFromRobot(); else Arm->AttachToRobot();
}
