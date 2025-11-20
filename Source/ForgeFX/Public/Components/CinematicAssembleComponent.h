#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinematicAssembleComponent.generated.h"
class UAssemblyBuilderComponent; class ARobotPartActor;

UENUM(BlueprintType)
enum class ECinematicPhase : uint8 { None, Scatter, Return };

USTRUCT()
struct FCinePart
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<ARobotPartActor> Actor;
	UPROPERTY() FTransform TargetSocketWorld;
	UPROPERTY() FVector ScatterDir;
};

UCLASS(ClassGroup=(ForgeFX), meta=(BlueprintSpawnableComponent))
class FORGEFX_API UCinematicAssembleComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCinematicAssembleComponent();
	UFUNCTION(BlueprintCallable, Category="Cinematic") void TriggerAssemble(FVector NewLocation);
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	ECinematicPhase Phase = ECinematicPhase::None; float Elapsed =0.f; FVector BaseTarget = FVector::ZeroVector; TArray<FCinePart> Parts;
	UPROPERTY(EditAnywhere, Category="Cinematic") float ScatterDuration =0.75f;
	UPROPERTY(EditAnywhere, Category="Cinematic") float ReturnDuration =1.5f;
	UPROPERTY(EditAnywhere, Category="Cinematic") float ScatterDistance =300.f;
	UPROPERTY(EditAnywhere, Category="Cinematic") float EasePower =2.f;
	UAssemblyBuilderComponent* GetAssembly() const;
	void BuildPartList();
};