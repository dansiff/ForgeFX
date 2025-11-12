#include "Components/InteractionTraceComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UInteractionTraceComponent::UInteractionTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractionTraceComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInteractionTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner) return;
	UCameraComponent* Cam = Owner->FindComponentByClass<UCameraComponent>();
	const FVector Start = Cam ? Cam->GetComponentLocation() : Owner->GetActorLocation();
	const FVector Dir = Cam ? Cam->GetForwardVector() : Owner->GetActorForwardVector();
	const FVector End = Start + Dir * TraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), false);
	Params.AddIgnoredActor(Owner);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// Optional debug
	if (bDrawDebugTrace)
	{
		DrawDebugLine(GetWorld(), Start, End, Hit.bBlockingHit ? FColor::Green : FColor::Red, false,0.f,0,1.f);
	}
	if (bLogTraceHits && Hit.GetActor())
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("Trace Hit Actor=%s Comp=%s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()));
	}

	TWeakInterfacePtr<IInteractable> NewHover;
	UPrimitiveComponent* NewComp = nullptr;
	AActor* HitActor = Hit.GetActor();
	if (HitActor)
	{
		NewComp = Hit.GetComponent();
		if (HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			NewHover = TWeakInterfacePtr<IInteractable>(HitActor);
		}
	}
	UpdateHover(NewHover, NewComp, HitActor);
}

void UInteractionTraceComponent::UpdateHover(const TWeakInterfacePtr<IInteractable>& NewHover, UPrimitiveComponent* NewComp, AActor* HitActor)
{
	const bool bSameObj = (CurrentHover.GetObject() == NewHover.GetObject());
	const bool bSameComp = (CurrentHitComp.Get() == NewComp);
	if (bSameObj && bSameComp) return;

	if (CurrentHover.IsValid())
	{
		IInteractable::Execute_OnHoverEnd(CurrentHover.GetObject());
	}
	CurrentHover = NewHover;
	CurrentHitComp = NewComp;
	if (CurrentHover.IsValid())
	{
		IInteractable::Execute_OnHoverBegin(CurrentHover.GetObject());
	}
	OnHoverComponentChanged.Broadcast(NewComp, HitActor);
}

void UInteractionTraceComponent::InteractPressed()
{
	bInteractHeld = true;
	if (CurrentHover.IsValid())
	{
		OnInteractPressed.Broadcast(CurrentHitComp.Get(), Cast<AActor>(CurrentHover.GetObject()));
		IInteractable::Execute_OnInteract(CurrentHover.GetObject());
	}
}

void UInteractionTraceComponent::InteractAltPressed()
{
	if (CurrentHover.IsValid())
	{
		OnInteractAltPressed.Broadcast(CurrentHitComp.Get(), Cast<AActor>(CurrentHover.GetObject()));
	}
}

void UInteractionTraceComponent::InteractReleased()
{
	bInteractHeld = false;
	OnInteractReleased.Broadcast();
}
