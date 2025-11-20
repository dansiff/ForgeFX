# ForgeFX Robot Assessment

## TL;DR (Reviewer Quick Scan)
Play the modular robot demo in UE5.6. Detach, drag, snap, and free-attach any configured part. Multi-select parts (Ctrl+Click) then batch detach (D) or batch reattach (Shift+R). Mouse wheel adjusts drag distance, ESC cancels. R reattaches all. Arm toggles (Y). Scramble randomizes assembly (P). Tests under Automation: search `RobotTests`.

## Quick Start
1. Open `ForgeFX.uproject` (UE5.6).
2. Load `RobotDemoMap` level.
3. Press Play.
4. Controls:
 - Movement: WASD + Mouse look, Space Up / Ctrl Down, Shift boost
 - Interact (Detach/Drag/Snap): E (press / release depending on DetachMode)
 - Multi-select part: Ctrl + Left Click on hovered part
 - Batch detach selected: D
 - Batch reattach selected: Shift + R
 - Reattach all detached parts: R
 - Toggle arm: Y
 - Scramble assembly: P
 - Adjust part drag distance: Mouse Wheel
 - Cancel active drag: ESC
 - Attempt quick reattach (alternative flow if bound): Right Mouse (optional IA_InteractAlt)

## Interaction Modes & Features
- Detach Mode (`DetachMode`): HoldToDrag / ToggleToDrag / ClickToggleAttach
- Hover highlighting: torso hover outlines whole robot; single part hover isolates part.
- Part detachment: Press E while hovering a detachable component.
- Dragging: Detached part maintains forward distance (Skyrim-style) adjustable via wheel.
- Socket snap: Release or click while drag; position + angle within tolerances -> snap.
- Free attach: If enabled (`bAllowFreeAttach`), release outside snap tolerance but inside range -> attach to nearest socket.
- Snap readiness feedback: Dynamic preview material pulses and changes color (green ready / red not ready) + socket info widget displays socket name & state.
- Multi-selection: Ctrl+Click accumulates parts. Use D or Shift+R for batch operations.
- Batch operations:
 - D: Detach all selected (ignores already detached)
 - Shift+R: Reattach selected (only those detached)
 - R: Reattach all detached (ignores selection)
- Arm control: Toggle separate arm assembly visibility & state with Y.
- Scramble: Random detach/reattach pass for detachable parts (configurable iterations, optional physics).

## UI / Materials Setup
1. Status Widget (`StatusWidgetClass`): Should contain a `TextBlock` named `StatusText` for prompts/status.
2. Socket Info Widget (`SocketInfoWidgetClass`): Needs `SocketNameText` and `SnapStateText` `TextBlock` widgets. Appears near the target socket while dragging.
3. Preview Materials:
 - `ReattachPreviewMaterial`: Must expose scalar `Pulse` and vector `SnapColor` parameters.
 - `SelectedPreviewMaterial`: Optional distinct look for selected parts.
4. Custom Depth / Outline: Ensure meshes allow render custom depth if using outline highlighting.

## Configuration Asset Workflow
Open `DA_RobotAssembly`:
- Mark each part `bDetachable = true` for full modularity.
- Provide parent part name & socket (add sockets in Mesh Editor if needed).
- Optional physics: enable on detach via part spec.
After edits: Rebuild assembly (Play, or re-place actor, or call BuildAssembly).

## Key Tunables (on `ARobotActor`)
- `AttachPosTolerance`: Snap distance to original socket.
- `AttachAngleToleranceDeg`: Angular snap tolerance.
- `FreeAttachMaxDistance`: Search radius for free attach.
- `PartGrabMinDistance` / `PartGrabMaxDistance` / `PartGrabDistanceStep`: Drag distance behavior.
- `ScrambleIterations` / `ScrambleSocketSearchRadius`: Scramble behavior.
- `PreviewPulseSpeed`, `SnapReadyColor`, `SnapNotReadyColor`: Visual feedback timing/colors.

## Runtime API Examples
```cpp
// Enable all detachable parts
Assembly->SetDetachEnabledForAll(true);
// Disable specific parts
Assembly->SetDetachEnabledForParts({ FName("Eye_Left"), FName("Eye_Right") }, false);
// Programmatically detach for scripted sequence
ARobotPartActor* OutActor = nullptr; Assembly->DetachPart(FName("Hand_Left"), OutActor);
// Force free attach search
USceneComponent* Parent; FName Socket; float Dist;
if (Assembly->FindNearestAttachTarget(OutActor->GetActorLocation(), Parent, Socket, Dist) && Dist <30.f)
{
 Assembly->AttachDetachedPartTo(OutActor->GetPartName(), OutActor, Parent, Socket);
}
```

## Batch Operations
```cpp
// C++ detach selected list
RobotActor->BatchDetachSelected();
// C++ reattach selected list
RobotActor->BatchReattachSelected();
// Clear selection without altering part states
RobotActor->ClearSelection();
```

## Automation / Tests
Run in Automation Tab (PIE active) → search `RobotTests`:
- `RobotTests.ArmAttachment` – verifies arm attach/detach cycle.
- `RobotTests.DetachReattachCycle` – detaches all detachable parts then reattaches.
- `RobotTests.SelectionBatchDetach` – multi-selection batch detach & reattach.
(Extendable: add tests for snap tolerance failure then success, multi-attach sequences.)

## Troubleshooting Guide
| Issue | Fix |
|-------|-----|
| Part won’t detach | Confirm `bDetachable` true and not already detached. |
| Snap never triggers | Increase `AttachPosTolerance` / `AttachAngleToleranceDeg`; verify socket exists. |
| Free attach not working | Ensure `bAllowFreeAttach` true and within `FreeAttachMaxDistance`. |
| Preview no color change | Assign `ReattachPreviewMaterial` and ensure `SnapColor` parameter exists. |
| Status widget blank | Verify `StatusText` exists and added to viewport. |
| Automation tests skip | Must have PIE world running (Play in Editor first). |

## Design Rationale (Concise)
- Separation of concerns: `UAssemblyBuilderComponent` handles structural mapping; `ARobotActor` handles interaction + UI; `ARobotPartActor` isolates detached behavior (physics, highlight).
- Extensibility: Multi-select and batch operations layered without altering core detach logic.
- Deterministic snap + flexible free attach for creative recombination demos.

## Extended Summary
The project demonstrates a fully modular robot assembly: every static mesh component can be detached, dragged with smooth interpolation, reattached to its original socket, or free-attached to any compatible socket based on proximity. Visual feedback (pulse, color, widget) communicates snap readiness. Multi-selection enables batch operations for rapid state changes. Automated tests provide regression coverage for core arm and part lifecycle behaviors. Scrambling showcases dynamic reconfiguration and validates robustness under repeated detach/reattach cycles.

---
End of README.
