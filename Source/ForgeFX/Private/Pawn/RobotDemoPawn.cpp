#include "Pawn/RobotDemoPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/InteractionTraceComponent.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"

ARobotDemoPawn::ARobotDemoPawn()
{
	PrimaryActorTick.bCanEverTick = true; // enable tick for raw fallback & smoothing
	AutoPossessPlayer = EAutoReceiveInput::Player0; // ensure possession in PIE

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength =0.f;
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = BaseMaxSpeed;
	Movement->Acceleration =4096.f;
	Movement->Deceleration =4096.f;

	Interaction = CreateDefaultSubobject<UInteractionTraceComponent>(TEXT("Interaction"));
}

void ARobotDemoPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// Force keyboard focus to the game so WASD works without clicking
		FInputModeGameOnly Mode; PC->SetInputMode(Mode);
		PC->bShowMouseCursor = false;
	}

	if (CrosshairWidgetClass)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			CrosshairWidget = CreateWidget<UUserWidget>(PC, CrosshairWidgetClass);
			if (CrosshairWidget)
			{
				CrosshairWidget->AddToViewport();
				CrosshairWidget->SetIsFocusable(false);
			}
		}
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
			{
				if (InputContext)
				{
					Subsys->AddMappingContext(InputContext, MappingPriority);
				}
			}
		}
	}
}

void ARobotDemoPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARobotDemoPawn::Move);
		if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARobotDemoPawn::Look);
		if (UpDownAction) EIC->BindAction(UpDownAction, ETriggerEvent::Triggered, this, &ARobotDemoPawn::UpDown);
		if (BoostAction) { EIC->BindAction(BoostAction, ETriggerEvent::Started, this, &ARobotDemoPawn::BoostOn); EIC->BindAction(BoostAction, ETriggerEvent::Completed, this, &ARobotDemoPawn::BoostOff); }
		if (InteractAction) { EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ARobotDemoPawn::InteractPress); EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &ARobotDemoPawn::InteractRelease); }
		if (InteractAltAction) { EIC->BindAction(InteractAltAction, ETriggerEvent::Started, this, &ARobotDemoPawn::InteractAltPress); }
	}
}

void ARobotDemoPawn::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (bLogMovementAxis) UE_LOG(LogTemp, VeryVerbose, TEXT("Move axis (%f,%f)"), Axis.X, Axis.Y);
	if (!Axis.IsNearlyZero())
	{
		// Use control rotation for movement direction (flying style)
		FRotator ControlRot = GetControlRotation(); ControlRot.Pitch =0.f;
		const FVector Fwd = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);
		AddMovementInput(Fwd, Axis.Y);
		AddMovementInput(Right, Axis.X);
	}
}

void ARobotDemoPawn::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ARobotDemoPawn::BoostOn(const FInputActionValue&)
{
	bBoosting = true; Movement->MaxSpeed = FMath::Max(BaseMaxSpeed * BoostMultiplier,2400.f);
}

void ARobotDemoPawn::BoostOff(const FInputActionValue&)
{
	bBoosting = false; Movement->MaxSpeed = BaseMaxSpeed;
}

void ARobotDemoPawn::InteractPress(const FInputActionValue& Value)
{
	if (Interaction) Interaction->InteractPressed();
}

void ARobotDemoPawn::InteractRelease(const FInputActionValue& Value)
{
	if (Interaction) Interaction->InteractReleased();
}

void ARobotDemoPawn::InteractAltPress(const FInputActionValue& Value)
{
	if (Interaction) Interaction->InteractAltPressed();
}

// Add missing UpDown implementation
void ARobotDemoPawn::UpDown(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (bLogMovementAxis && !FMath::IsNearlyZero(Axis)) UE_LOG(LogTemp, VeryVerbose, TEXT("UpDown axis %f"), Axis);
	if (FMath::Abs(Axis) > KINDA_SMALL_NUMBER)
	{
		AddMovementInput(FVector::UpVector, Axis);
	}
}

void ARobotDemoPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PollRawMovementKeys(DeltaSeconds);
}

void ARobotDemoPawn::PollRawMovementKeys(float DeltaSeconds)
{
	if (!bEnableRawKeyMovementFallback) return;
	APlayerController* PC = Cast<APlayerController>(Controller); if (!PC) return;

	// Gather intent: WASD for horizontal plane, Space/Ctrl for vertical
	float Fwd = (PC->IsInputKeyDown(EKeys::W)?1.f:0.f) - (PC->IsInputKeyDown(EKeys::S)?1.f:0.f);
	float Right = (PC->IsInputKeyDown(EKeys::D)?1.f:0.f) - (PC->IsInputKeyDown(EKeys::A)?1.f:0.f);
	float Up = (PC->IsInputKeyDown(EKeys::SpaceBar)?1.f:0.f) - (PC->IsInputKeyDown(EKeys::LeftControl)?1.f:0.f);

	FRotator ViewRot = GetControlRotation(); ViewRot.Pitch =0.f; // level for planar move
	const FVector FwdVec = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::X);
	const FVector RightVec = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Y);
	FVector Desired = FwdVec * Fwd + RightVec * Right + FVector::UpVector * Up;
	Desired = Desired.GetClampedToMaxSize(1.f);

	PendingRawInput = FMath::VInterpTo(PendingRawInput, Desired, DeltaSeconds, RawAccelerationBlend);
	if (!PendingRawInput.IsNearlyZero())
	{
		const float HorzSpeed = bBoosting ? BaseMaxSpeed * BoostMultiplier : BaseMaxSpeed;
		const float VertSpeed = bBoosting ? RawVerticalSpeed * BoostMultiplier : RawVerticalSpeed;
		const FVector Velocity = (FwdVec * PendingRawInput.X + RightVec * PendingRawInput.Y) * HorzSpeed + FVector::UpVector * PendingRawInput.Z * VertSpeed;
		AddMovementInput(Velocity.GetSafeNormal(), Velocity.Size());
	}

	if (Controller)
	{
		APawn::bUseControllerRotationYaw = true;
	}
}
