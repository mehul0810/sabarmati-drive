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
    bool IsCrossingStreet() const { return bAtCrosswalk && bSteppingOff; }
    int32 ShopVisits() const {return CounterVisits;}
    int32 CrossingsCompleted() const { return Crossings; }
    static int32 SpawnRiverfrontCrowd(UWorld& World);
    static ARiverfrontPedestrian* SpawnStreetWalker(UWorld& World, const FVector& A, const FVector& B, int32 Variant);

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
    double RestAt=0., RestLeft=0.;
    int32 Crossings=0;
    bool bAtCrosswalk=false, bSteppingOff=false, bReturning=false;
    UPROPERTY() TObjectPtr<UAnimSequence> Gait;
    UPROPERTY() TObjectPtr<UAnimSequence> IdlePose;
    TArray<FVector> Errand;
    int32 ErrandStep=0,ErrandDirection=1,CounterVisits=0;
    void TickErrand(float Delta);
    void SetMoving(bool Moving);
};
