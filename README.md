# ForgeFX Robot Assessment

## Overview
A Unreal Engine5.6 project demonstrating a modular robot arm that can attach/detach, interact highlighting, simple UI status, and automation tests. Code favors C++ for core logic and uses Blueprints for visuals and bindings.

## Setup
- Engine: Unreal Engine5.6
- IDE: Visual Studio2022
- Clone repo and open `ForgeFX.uproject`.
- Plugins enabled: Enhanced Input, Niagara, UMG (built-in). Starter Content disabled.

## Run
- Open `MinimalDemo` map (to be added) or default level.
- WASD to move, Mouse to look, `E` to interact/hover; `ToggleArm` action detaches/attaches.

## Implementation Summary
- C++: `URobotArmComponent` (attach/detach), `UHighlightComponent` (state only), `UAssemblyBuilderComponent` (spawns and attaches parts from data), `ARobotActor` (composition), `UInteractionTraceComponent` (hover/interact traces), `ARobotDemoPawn` (movement + input), `ARobotDemoPlayerController` (auto IMC add), data assets (`URobotArmConfig`, `URobotAssemblyConfig`), automation tests (`RobotAutomationTests.cpp`, `RobotVisibilityFunctionalTest`).
- Blueprints: material highlight or custom depth (configurable), UI text binding, simple input mapping.
- Niagara: subtle spark on detach.

## Editor setup (quick)
1) Create `DA_RobotAssembly` (URobotAssemblyConfig) and fill parts with meshes and sockets. Optionally switch HighlightMode.
2) Create `DA_RobotArm_Left` (URobotArmConfig) with ArmPartNames and optional Niagara.
3) Create `BP_Robot` (child of ARobotActor). Assign configs to `Assembly` and `RobotArm`.
4) Enhanced Input assets: `IMC_Demo`, `IA_Move(Vector2)`, `IA_Look(Vector2)`, `IA_Interact(Trigger)`, `IA_ToggleArm(Trigger)`.
5) Assign `IMC_Demo` to either `ARobotDemoPawn.InputContext` or `ARobotDemoPlayerController.DefaultInputContext`. Assign actions to the pawn and robot actor.
6) Place `BP_Robot` and press Play (default game mode uses the demo pawn/controller).

## Testing
- From Editor: Session Frontend > Automation (unit-like) + Functional Tests (PIE) -> `RobotVisibilityFunctionalTest`.
- From command line:
 - `RunTests ForgeFX.Robot.AttachTest`
 - `RunTests ForgeFX.Robot.HighlightHoverTest`
 - `RunTests ForgeFX.Robot.DetachMustFlipState`

## Notes
- Editor tasks like creating data assets, Blueprints, materials, Enhanced Input assets, or PP outlines are done in-editor. All core logic and extensibility are in C++.
