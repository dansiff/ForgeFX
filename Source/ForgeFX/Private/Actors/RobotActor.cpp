#include "Actors/RobotActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/PlayerController.h"

ARobotActor::ARobotActor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arm = CreateDefaultSubobject<URobotArmComponent>(TEXT("RobotArm"));
	Highlight = CreateDefaultSubobject<UHighlightComponent>(TEXT("Highlight"));
	Assembly = CreateDefaultSubobject<UAssemblyBuilderComponent>(TEXT("Assembly"));
}

void ARobotActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Assembly)
	{
		Assembly->BuildAssembly();
	}
}

void ARobotActor::BeginPlay()
{
	Super::BeginPlay();

	// Build assembly at runtime if an asset is assigned
	if (Assembly) { Assembly->BuildAssembly(); }

	// Wire delegates
	if (Arm) { Arm->OnAttachedChanged.AddDynamic(this, &ARobotActor::OnArmAttachedChanged); }
	if (Highlight) { Highlight->OnHighlightedChanged.AddDynamic(this, &ARobotActor::OnHighlightedChanged); }

	// Optional Enhanced Input binding if provided and we are possessed
	if (ToggleArmAction)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				if (InputContext)
				{
					Subsys->AddMappingContext(InputContext, InputPriority);
				}
			}
			if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
			{
				EIC->BindAction(ToggleArmAction, ETriggerEvent::Started, this, &ARobotActor::ToggleArm);
			}
		}
	}
}

void ARobotActor::OnArmAttachedChanged(bool bNowAttached)
{
	// Minimal policy: toggle visibility of configured arm parts via BP using Assembly
	// Designer can also replace with physics detach or different visuals.
	if (!Arm || !Assembly || !Arm->Config) return;

	for (const FName PartName : Arm->Config->ArmPartNames)
	{
		Assembly->SetPartVisibility(PartName, bNowAttached);
	}

	if (!bNowAttached)
	{
		SpawnDetachVFXIfConfigured();
	}
}

void ARobotActor::SpawnDetachVFXIfConfigured()
{
	if (!Arm || !Assembly || !Arm->Config || !Arm->Config->DetachEffect) return;

	FVector Loc = GetActorLocation();
	if (!Arm->Config->DetachEffectPartName.IsNone())
	{
		FVector PartLoc;
		if (Assembly->GetPartWorldLocation(Arm->Config->DetachEffectPartName, PartLoc))
		{
			Loc = PartLoc;
		}
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Arm->Config->DetachEffect, Loc);
}

void ARobotActor::OnHighlightedChanged(bool bNowHighlighted)
{
	// Drive material highlight through assembly (value0..1)
	if (Assembly)
	{
		Assembly->ApplyHighlightScalar(bNowHighlighted ?1.f :0.f);
	}
}

void ARobotActor::ToggleArm()
{
	if (!Arm) return;
	if (Arm->IsAttached()) Arm->DetachFromRobot();
	else Arm->AttachToRobot();
}
