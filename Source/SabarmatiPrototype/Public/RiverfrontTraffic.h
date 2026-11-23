#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverfrontTraffic.generated.h"

class UBoxComponent;
class USplineComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class ARiverfrontCar;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontRoadAgent : public AActor
{
    GENERATED_BODY()
public:
    ARiverfrontRoadAgent();
    bool Assemble(int32 Style, int32 Variant);
    bool MoveOnRoad(const FVector& Position, const FRotator& Heading, double Travel, bool Near);
    double HalfLength=200., HalfWidth=85., TyreRadius=32.;
    bool IsEmergency=false;
private:
    UPROPERTY() TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Wheels;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Beacons;
    float Spin=0;
};

struct FSabarmatiLane
{
    int32 Route=0;
    double At=0, Speed=0, Cruise=800, Offset=0;
    bool WasStopped=false;
};

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontTraffic : public AActor
{
    GENERATED_BODY()
public:
    ARiverfrontTraffic();
    void Start(bool MappedCity);
    virtual void Tick(float Dt) override;
    bool WalkSignal() const;
    int32 ActiveCount() const { return Agents.Num(); }
    const TArray<TPair<FVector,FVector>>& MapSegments() const { return MapLines; }
    int32 GetStops() const { return Stops; }
    int32 GetTurns() const { return Turns; }
    double GetTravelMetres() const { return TravelCm/100.; }
    FVector GetCrossing() const { return PedestrianCrossing; }
private:
    UPROPERTY() TArray<TObjectPtr<USplineComponent>> Routes;
    UPROPERTY() TArray<TObjectPtr<ARiverfrontRoadAgent>> Agents;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Signals;
    TArray<FSabarmatiLane> Drivers;
    TArray<FVector> Junctions;
    TArray<TPair<FVector,FVector>> MapLines;
    TArray<TWeakObjectPtr<ARiverfrontCar>> Parked;
    FVector PedestrianCrossing=FVector(-5350,4090,0);
    float Elapsed=0;
    double TravelCm=0;
    int32 Stops=0, Turns=0, LastPhase=-1;
    bool bCity=false;
    void Lane(const TArray<FVector>& Nodes, bool Reverse);
    void SetUpSignals();
    int32 Phase() const;
    float StopDistance(const FVector& At, const FVector& Heading, double Length, bool& Obstacle, int32 Index);
};
