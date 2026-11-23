#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverfrontPedestrian.generated.h"

class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;
class UWorld;

/** A lightweight ambient walker on a checked, pedestrian-only circuit. */
UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontPedestrian : public AActor
{
    GENERATED_BODY()

public:
    ARiverfrontPedestrian();
    static int32 SpawnRiverfrontCrowd(UWorld& World);

protected:
    void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> Mesh;

    void Configure(USkeletalMesh* Model, UAnimSequence* Walk, const FVector& StartTurn, const FVector& EndTurn,
        double Radius, double StartFraction, double SpeedCmPerSecond, int32 Variant);
    void UpdateRouteTransform();

    FVector RouteStart = FVector::ZeroVector;
    FVector RouteEnd = FVector::ZeroVector;
    double TurnRadius = 0.0;
    double RouteLength = 0.0;
    double Distance = 0.0;
    double Speed = 0.0;
};
