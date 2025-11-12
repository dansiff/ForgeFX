#include "Actors/OrbitCameraRig.h"
#include "Camera/CameraComponent.h"
#include "Components/SplineComponent.h"

AOrbitCameraRig::AOrbitCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	Spline->SetupAttachment(Root);
}

void AOrbitCameraRig::BeginPlay()
{
	Super::BeginPlay();
	Camera->FieldOfView = FOV;
}

void AOrbitCameraRig::Setup(AActor* InTarget, float InRadius, float InHeight, float InSpeed)
{
	Target = InTarget; Radius = InRadius; Height = InHeight; SpeedDegPerSec = InSpeed; PathT =0.f; AngleDeg =0.f;
}

void AOrbitCameraRig::UseCircularSplinePath(int32 NumPoints)
{
	bUseSpline = true; Spline->ClearSplinePoints();
	const FVector C = GetActorLocation();
	for (int32 i=0;i<NumPoints;++i)
	{
		const float A = (float)i / (float)NumPoints *2.f * PI;
		const FVector P = C + FVector(FMath::Cos(A)*Radius, FMath::Sin(A)*Radius, Height);
		Spline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
	}
	Spline->UpdateSpline();
}

void AOrbitCameraRig::StepManual(float DeltaDegrees)
{
	AngleDeg = FMath::Fmod(AngleDeg + DeltaDegrees,360.f);
}

void AOrbitCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AActor* T = Target.Get(); if (!T) return;
	const FVector Center = T->GetActorLocation();
	if (bUseSpline && Spline->GetNumberOfSplinePoints() >0)
	{
		if (bAutoOrbit)
		{
			const float Speed = SpeedDegPerSec/360.f; // cycles per second
			PathT = FMath::Fmod(PathT + Speed * DeltaSeconds,1.f);
		}
		const float Ln = Spline->GetSplineLength();
		const FVector Pos = Spline->GetLocationAtDistanceAlongSpline(PathT * Ln, ESplineCoordinateSpace::World);
		SetActorLocation(Pos);
		SetActorRotation((Center - GetActorLocation()).Rotation());
	}
	else
	{
		float DeltaDeg = bAutoOrbit ? (SpeedDegPerSec * DeltaSeconds) :0.f;
		if (bAutoOrbit && bEaseInOutSpeed)
		{
			const float Tsin = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(AngleDeg)));
			DeltaDeg *= FMath::Lerp(0.6f,1.2f, Tsin);
		}
		AngleDeg = FMath::Fmod(AngleDeg + DeltaDeg,360.f);
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Offset = FVector(FMath::Cos(Rad), FMath::Sin(Rad),0.f) * Radius + FVector(0,0,Height);
		SetActorLocation(Center + Offset);
		SetActorRotation((Center - GetActorLocation()).Rotation());
	}
}
