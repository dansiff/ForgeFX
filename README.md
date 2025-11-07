# ForgeFX Robot Assessment

Quick start (for reviewers)
- Engine: Unreal Engine5.6
- Open `ForgeFX.uproject` in the editor.
- Load the level `MainDemo` 
- Press Play.
- Controls: WASD = move, Mouse = look, E = interact/drag, (optional) Y = ToggleArm.

How to observe each requirement
1) Camera movement
- Move the pawn around the robot using WASD + mouse look.
2) Highlight whole robot when hovering torso
- Aim at the torso. Entire robot highlights according to the configured highlight mode.
3) Drag the torso to move the robot
- Press and hold E while pointing at the torso to drag; release to stop.
4) Highlight the arm when hovering it
- Aim at any arm part (Upperarm/Lowerarm/Hand) to highlight only the arm.
5) Detach the arm with mouse
- Press E while aiming at an arm part (or use ToggleArm). A spark VFX plays if configured.
6) Re-attach the arm with mouse
- Press E again while aiming at the arm.
7) Status text shows Attached/Detached
- Add `WBP_ArmStatus` (text bound to `URobotArmComponent::IsAttached()`) to the viewport; the text updates to “Attached/Detached”.
8) Unit tests
- Editor: Window > Session Frontend > Automation > filter `ForgeFX.Robot.*` and run.
- Console: `Automation RunTests ForgeFX.Robot.AttachTest`, `Automation RunTests ForgeFX.Robot.HighlightHoverTest`, `Automation RunTests ForgeFX.Robot.DetachMustFlipState`.




Implementation Summary
- C++: `URobotArmComponent`, `UHighlightComponent`, `UAssemblyBuilderComponent` (build graph + part ops), `ARobotActor` (composition + component-aware hover/press handlers), `UInteractionTraceComponent` (component-aware tracing), `ARobotDemoPawn` (movement + input), `ARobotDemoPlayerController` (auto IMC add), data assets (`URobotArmConfig`, `URobotAssemblyConfig`), automation tests (`RobotAutomationTests.cpp`, `RobotVisibilityFunctionalTest`).
- Blueprints/Content: material highlight or custom depth (configurable), UI text binding, input assets.
- Niagara: subtle spark on detach.

Developer onboarding
- Prereqs: UE5.6, VS2022. Ensure Enhanced Input and Niagara are enabled (UMG is an engine module; no plugin entry in `.uproject`).
- Setup:
1) Create `DA_RobotAssembly` (URobotAssemblyConfig): list all `Robot_*` meshes with parent/sockets, choose highlight mode.
2) Create `DA_RobotArm_Left` (URobotArmConfig): `ArmPartNames=[Upperarm_Left, Lowerarm_Left, Hand_Left]`, optional `DetachEffect`/`DetachEffectPartName`.
3) Create `BP_Robot` (child of `ARobotActor`): assign configs to `Assembly` and `RobotArm`.
4) Enhanced Input: `IMC_Demo`, `IA_Move(Vector2)`, `IA_Look(Vector2)`, `IA_Interact(Trigger)`, `IA_ToggleArm(Trigger)`. Assign IMC on `ARobotDemoPlayerController` or `ARobotDemoPawn` and actions to the pawn/actor.
5) Create/rename level to `MainScene`, place `BP_Robot`, press Play.
6) Optional: `WBP_ArmStatus` bound to `URobotArmComponent::IsAttached()`.

Requirements mapping (ForgeFX Programmer’s Application)
-1 Camera movement: `ARobotDemoPawn` (WASD + Mouse) 
-2 Highlight whole robot over torso: torso -> `ApplyHighlightScalarAll(1)` 
-3 Drag robot via torso: press/hold on torso to drag 
-4 Highlight arm on hover: arm parts -> `ApplyHighlightScalarToParts(ArmPartNames,1)` 
-5 Detach arm by mouse: Interact on arm detaches; VFX optional 
-6 Reattach arm by mouse: Interact again 
-7 Status text: UMG widget bound to `IsAttached()` 
-8 Unit test: automation tests + functional test 

Optional enhancements / future work
- Physics-based arm with constraints; add breakable joints.
- IK targets and better hand placement for training scenarios.
- Networking: replicate arm state and interactions.
- Input rebinding UI + preset gamepad layouts.
- Accessibility: outline highlight via CustomDepth, colorblind-friendly palettes.
- Niagara pooling and param-driven effects.
- Persistence via `USaveGame` or backend.

# ForgeFX Modular Robot – Detachable Part System

## Overview
This document explains the detachable modular part system implemented in C++ for the ForgeFX robot assessment. It supports:
- Data-driven assembly of arbitrary robot graphs (multiple torsos, interconnectors, limbs).
- Runtime detachment of any configured part into its own actor for manipulation/physics.
- Reattachment via proximity snapping to original parent sockets.
- Batch enabling/disabling of detachment (Blueprint-exposed convenience functions).
- Visual debug indicators (socket location + color when within snap tolerance).

## Core Types
1. URobotAssemblyConfig (Data Asset)
- Array Parts (FRobotPartSpec): defines hierarchy, socket attachment, detachable settings.
- Highlight mode selection (MaterialParameter vs CustomDepthStencil).

2. FRobotPartSpec fields (extended)
- PartName: unique ID.
- ParentPartName / ParentSocketName: defines tree/graph linkage.
- Mesh / RelativeTransform.
- bAffectsHighlight: opt-in highlight.
- bDetachable: may become independent actor.
- bSimulatePhysicsWhenDetached: enable physics when promoted.
- DetachedCollisionProfile: profile applied to detached actor mesh.
- DetachedActorClass: optional subclass of ARobotPartActor for custom behavior.

3. UAssemblyBuilderComponent
- BuildAssembly(): spawns and attaches UStaticMeshComponents per spec.
- DetachPart(): promotes component -> ARobotPartActor.
- ReattachPart(): demotes actor -> component (snap to socket).
- Tracking maps for components, detached actors, and runtime detach overrides.
- Highlight helpers (all, subset, scalar setting).
- NEW Blueprint convenience:
 - SetDetachEnabledForParts(PartNames, bEnabled)
 - SetDetachEnabledForAll(bEnabled)
 - IsDetachEnabled(PartName)

4. ARobotPartActor
- Simple actor for detached pieces (mesh + highlight component).
- InitializePart() copies mesh & materials.
- EnablePhysics() toggles physics/collision.
- Implements IInteractable for hover highlight.

5. ARobotActor
- Owns UAssemblyBuilderComponent.
- InteractPressed logic:
 - If component and detachable -> DetachPart and start drag.
 - If detached actor -> drag and attempt snap.
- TrySnapDraggedPartToSocket(): checks tolerance and calls ReattachPart.
- Tolerances configurable: AttachPosTolerance (cm), AttachAngleToleranceDeg.
- Optional torso drag (bAllowTorsoDrag).

## Editor Setup
1. Create DA_RobotAssembly (URobotAssemblyConfig)
- Add entries for each static mesh part.
- Set ParentPartName, ParentSocketName (create sockets on meshes as needed).
- For detachable parts: bDetachable=true. Optionally enable physics.

2. Create DA_RobotArm_Left (URobotArmConfig) for arm grouping (still used for visibility toggles and VFX).

3. Assign DA_RobotAssembly to the Assembly component in BP_Robot (child of ARobotActor) or directly on placed ARobotActor.

4. Ensure sockets exist for reattachment (Static Mesh Editor > Sockets).

5. (Optional) Assign StatusWidgetClass on ARobotActor to show arm attached state.

## Detach / Reattach Flow
- Detach: User interacts (press E or mapped action) with hover target part -> UAssemblyBuilderComponent.DetachPart -> component hidden, actor spawned.
- Drag: Actor follows cursor plane using ARobotActor drag logic.
- Snap: While dragging, if actor is within AttachPosTolerance (distance) and rotation within AttachAngleToleranceDeg of socket transform -> reattached.
- Reattach: Actor destroyed; original component restored and reattached using FAttachmentTransformRules::SnapToTargetIncludingScale.

## Batch Detachment Control
Use Blueprints or C++:
- Disable detachment globally: SetDetachEnabledForAll(false)
- Enable only specific parts: SetDetachEnabledForParts(["Upperarm_Left","Hand_Left"], true)
- Query a part: IsDetachEnabled("Head")

Runtime hierarchy extension:
- Add new entries to DA_RobotAssembly (e.g., a second torso) and rebuild (editor or OnConstruction).
- Each new detachable part automatically supports promotion/demotion without code changes.

## Debug Visualization (Adding Socket Indicator)
To visualize sockets and snap state, you can add a call in ARobotActor::Tick during part drag:
```
if (bDraggingPart && DraggedPartActor)
{
 USceneComponent* Parent; FName Socket;
 if (Assembly->GetAttachParentAndSocket(DraggedPartName, Parent, Socket))
 {
 const FTransform SocketWorld = Parent->GetSocketTransform(Socket, RTS_World);
 const FVector Loc = SocketWorld.GetLocation();
 const bool bInRange = FVector::Dist(DraggedPartActor->GetActorLocation(), Loc) <= AttachPosTolerance;
 const FColor Col = bInRange ? FColor::Green : FColor::Red;
 DrawDebugSphere(GetWorld(), Loc,6.f,12, Col);
 DrawDebugLine(GetWorld(), Loc, DraggedPartActor->GetActorLocation(), Col);
 }
}
```
(Enable `#include "DrawDebugHelpers.h"`.)

## Performance Considerations
- Components remain lightweight; only detached parts become full actors.
- No per-frame tick on UAssemblyBuilderComponent.
- Highlight uses cached dynamic material instances.
- Detachment lookups are O(1) via TMaps.

## Extensibility Ideas
- Physics constraints between promoted parts for chain dynamics.
- Network replication: replicate attachment state; detached actors become replicated actors.
- Save/Load: store PartName + transform for detached actors, and rehydrate on load.
- Custom detach animations or particle FX (per-part override in spec).
- Multi-socket pairing / alternate reattachment targets.

## Testing Guidance
- Automation (logic): existing tests cover arm visibility and attach state; extend to test DetachPart/ReattachPart success.
- Functional (PIE): simulate detaching a part and assert actor spawn and subsequent snap.

## Minimal Usage Example (C++)
```
ARobotPartActor* NewActor = nullptr;
if (Assembly->DetachPart("Hand_Left", NewActor))
{
 // Move NewActor freely, then reattach:
 Assembly->ReattachPart("Hand_Left", NewActor);
}
```

## Blueprint Usage Example
- Call `DetachPart` from an interaction event, hold reference to actor for drag.
- On drag end, call `ReattachPart` if snap conditions met, else leave free.

## Troubleshooting
- Part not snapping: check socket name matches ParentSocketName and tolerances are large enough.
- Part doesn’t detach: verify bDetachable or runtime override (IsDetachEnabled) returns true.
- Materials not updating highlight: ensure HighlightMode is MaterialParameter and material has `HighlightAmount` scalar.

---
This detachable modular system ensures professional scalability, clear separation of concerns, and minimal Blueprint scripting burdens—content authors only manage assets and data entries while core behavior stays in C++.
