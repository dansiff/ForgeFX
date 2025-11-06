#include "Components/InteractionTraceComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UInteractionTraceComponent::UInteractionTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // lightweight line trace
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

	// Try to find a camera on owner; if none, use actor forward
	UCameraComponent* Cam = Owner->FindComponentByClass<UCameraComponent>();
	const FVector Start = Cam ? Cam->GetComponentLocation() : Owner->GetActorLocation();
	const FVector Dir = Cam ? Cam->GetForwardVector() : Owner->GetActorForwardVector();
	const FVector End = Start + Dir * TraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(Owner);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	TWeakInterfacePtr<IInteractable> NewHover;
	if (AActor* HitActor = Hit.GetActor())
	{
		if (HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			NewHover = TWeakInterfacePtr<IInteractable>(HitActor);
		}
	}
	UpdateHover(NewHover);
}

void UInteractionTraceComponent::UpdateHover(const TWeakInterfacePtr<IInteractable>& NewHover)
{
	if (CurrentHover.GetObject() == NewHover.GetObject()) return;

	if (CurrentHover.IsValid())
	{
		IInteractable::Execute_OnHoverEnd(CurrentHover.GetObject());
	}
	CurrentHover = NewHover;
	if (CurrentHover.IsValid())
	{
		IInteractable::Execute_OnHoverBegin(CurrentHover.GetObject());
	}
}

void UInteractionTraceComponent::Interact()
{
	if (CurrentHover.IsValid())
	{
		IInteractable::Execute_OnInteract(CurrentHover.GetObject());
	}
}
