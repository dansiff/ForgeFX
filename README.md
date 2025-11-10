# ForgeFX Robot Assessment

Quick start (for reviewers)
- Engine: Unreal Engine5.6
- Open `ForgeFX.uproject` in the editor.
- Load `MainDemo` level.
- Press Play.
- Controls: WASD move, Mouse look, Space/Ctrl up/down, Shift boost, E press/hold interact (detach/drag), Y toggle arm, P scramble parts.

Enhanced Input (professional mapping)
Define these actions:
- `IA_Move` (Axis2D): W(+Y), S(-Y), A(-X), D(+X)
- `IA_Look` (Axis2D): Mouse X(+X), Mouse Y(+Y or -Y to invert)
- `IA_UpDown` (Axis1D): Space(+1), LeftCtrl(-1)
- `IA_Boost` (Trigger): LeftShift (Started/Completed)
- `IA_Interact` (Trigger): E (Started/Completed) – press begins drag/detach; release finalizes snap/free-attach
- `IA_InteractAlt` (Trigger): RightMouseButton (Started/Completed) – attempt reattach (snap original, else free attach)
- `IA_ToggleArm` (Trigger): Y (Started)
- `IA_Scramble` (Trigger): P (Started)

Apply mapping context:
- Set `IMC_Demo` on PlayerController (`DefaultInputContext`) or Pawn (`InputContext`).
- Assign the C++ properties: in placed `ARobotDemoPawn` and `ARobotActor` instances.

Interaction summary
- Hover part (crosshair) + E Started: detach (if detachable) and begin drag.
- E Completed: attempt socket snap; if fail and free-attach enabled, attach to nearest valid target.
- Y: simple arm attach/detach toggle.
- P: scramble all detachable parts (random reattachment or nearest socket within radius).

Full-body modular detachment demo
- Any part flagged bDetachable in `DA_RobotAssembly` can be pulled off with E.
- Drag the detached part; release near its original socket to snap/reattach.
- If release is not close enough and Free Attach is enabled (`bAllowFreeAttach` on `ARobotActor`), part will attach to the nearest socket or component origin (supports experimentation: eyes to arm, jaw to torso side, etc.).
- Batch enable/disable detachment at runtime: `Assembly.SetDetachEnabledForAll(true/false)` or `SetDetachEnabledForParts([...], true)`.

How to observe each requirement
1) Camera movement – WASD + mouse + vertical flight.
2) Highlight whole robot when hovering torso – aim at torso.
3) Drag torso – hold E on torso (if enabled).
4) Highlight arm when hovering – aim at arm parts.
5) Detach arm (and any detachable part) – press E while aiming at part.
6) Re-attach – drag near socket or free-attach elsewhere.
7) Status text – add `WBP_ArmStatus` widget.
8) Unit tests – run automation tests (`ForgeFX.Robot.*`).




Implementation Summary
- C++ modules and components implement data-driven assembly, highlighting, part promotion/demotion, and free-attach logic.

Detachable Part System (extended)
- `FRobotPartSpec` includes detachable settings per part (eyes, jaw, hands, legs, feet, thigh, shin, hip, extra torsos).
- `UAssemblyBuilderComponent` builds components; detaches to `ARobotPartActor`; supports free-attach anywhere.
- `ARobotActor` handles drag, snap-to-socket, or free-attach fallback.

Editor steps to make every piece detachable
1) Open `DA_RobotAssembly`.
2) For each static mesh piece (Eyes, Jaw, Hand_Left, Hand_Right, Upperarm_Left, Upperarm_Right, Lowerarm_Left, Lowerarm_Right, Thigh_Left, Thigh_Right, Shin_Left, Shin_Right, Foot_Left, Foot_Right, Hip, Torso_A, Torso_B, etc.):
 - Set `bDetachable = true`.
 - Optionally set `bSimulatePhysicsWhenDetached = true` if you want it to fall when released.
 - Ensure parent references and sockets exist (add sockets in mesh editor where needed).
3) Save asset; rebuild robot (reopen level or recompile actor / call BuildAssembly).
4) In the placed `ARobotActor` instance (or BP_Robot):
 - Enable Free Attach if you want arbitrary reattachment (`bAllowFreeAttach = true`).
 - Adjust `AttachPosTolerance` (snap range to original socket) and `FreeAttachMaxDistance` (range for nearest attach anywhere).

Free-attach behavior
- Release the dragged part outside original socket tolerance but inside free-attach range -> attaches to nearest socket/origin of any component.
- Overrides stored so future detaches reattach to the newly chosen parent/socket unless manually changed.

Optional debug (socket indicator)
Add debug draw snippet to `ARobotActor::Tick` (see detachable section below) to visualize socket proximity (green = snap-ready, red = far).

Runtime API (examples)
```
// Globally enable detachment
Assembly->SetDetachEnabledForAll(true);
// Temporarily disable detachment for eyes
Assembly->SetDetachEnabledForParts({FName("Eye_Left"), FName("Eye_Right")}, false);
// Free attach custom logic: after custom drag end
USceneComponent* Parent; FName Socket; float Dist;
if (Assembly->FindNearestAttachTarget(DraggedLoc, Parent, Socket, Dist) && Dist <30.f) {
 Assembly->AttachDetachedPartTo(PartName, PartActor, Parent, Socket);
}
```

Scramble feature
- `ScrambleParts()` detaches all detachable components and reattaches them to random or nearest sockets (iterations configurable; optional physics).

Troubleshooting
- Missing Detach fields: close editor, full C++ rebuild, reopen asset.
- Snap fails: increase `AttachPosTolerance` or ensure socket names match.
- Free attach not working: confirm `bAllowFreeAttach` true and part released within `FreeAttachMaxDistance`.
- Highlight missing: Material must have `HighlightAmount` scalar; set mode to Material Parameter.

Testing additions (recommended)
- Add functional test: detach random part, move near wrong socket (should free-attach) then near original socket (should snap back).

---
All parts are now configurable for detachment and arbitrary reattachment, enabling a flexible demonstration of modular assembly.

# ForgeFX Robot Assessment (Updated Interaction)

Left Mouse (E action) – detach / drag (based on DetachMode).
Right Mouse (Alt action) – attempt reattach (snap original, else free attach).

Add `IA_InteractAlt` (Trigger) mapped to RightMouseButton and assign to `InteractAltAction` on the pawn.

Rest of README content remains below.
