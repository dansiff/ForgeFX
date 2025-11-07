#include "Pawn/RobotDemoPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/InteractionTraceComponent.h"
#include "Blueprint/UserWidget.h"

ARobotDemoPawn::ARobotDemoPawn()
{
	PrimaryActorTick.bCanEverTick = false;

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

	if (CrosshairWidgetClass)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			CrosshairWidget = CreateWidget<UUserWidget>(PC, CrosshairWidgetClass);
			if (CrosshairWidget) CrosshairWidget->AddToViewport();
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
		if (InteractAction) EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ARobotDemoPawn::Interact);
		if (UpDownAction) EIC->BindAction(UpDownAction, ETriggerEvent::Triggered, this, &ARobotDemoPawn::UpDown);
		if (BoostAction)
		{
			EIC->BindAction(BoostAction, ETriggerEvent::Started, this, &ARobotDemoPawn::BoostOn);
			EIC->BindAction(BoostAction, ETriggerEvent::Completed, this, &ARobotDemoPawn::BoostOff);
		}
	}
}

void ARobotDemoPawn::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void ARobotDemoPawn::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ARobotDemoPawn::UpDown(const FInputActionValue& Value)
{
	AddMovementInput(FVector::UpVector, Value.Get<float>());
}

void ARobotDemoPawn::BoostOn(const FInputActionValue& Value)
{
	bBoosting = true;
	Movement->MaxSpeed = BaseMaxSpeed * BoostMultiplier;
}

void ARobotDemoPawn::BoostOff(const FInputActionValue& Value)
{
	bBoosting = false;
	Movement->MaxSpeed = BaseMaxSpeed;
}

void ARobotDemoPawn::Interact(const FInputActionValue& Value)
{
	if (Interaction)
	{
		Interaction->InteractPressed();
	}
}
