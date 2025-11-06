#include "Components/RobotArmComponent.h"

URobotArmComponent::URobotArmComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URobotArmComponent::OnRegister()
{
	Super::OnRegister();
	if (Config)
	{
		bAttached = Config->bDefaultAttached;
	}
}

void URobotArmComponent::AttachToRobot()
{
	if (!bAttached)
	{
		bAttached = true;
		BP_OnAttached();
		OnAttachedChanged.Broadcast(true);
	}
}

void URobotArmComponent::DetachFromRobot()
{
	if (bAttached)
	{
		bAttached = false;
		BP_OnDetached();
		OnAttachedChanged.Broadcast(false);
	}
}
