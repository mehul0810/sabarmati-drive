#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverfrontTraffic.generated.h"

class UBoxComponent;
class USpotLightComponent;
class ARiverfrontWorld;
class USplineComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class ARiverfrontCharacter;
class ARiverfrontCar;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontRoadAgent : public AActor
{
    GENERATED_BODY()
public:
    ARiverfrontRoadAgent();
    bool Assemble(int32 Style, int32 Variant);
    bool MoveOnRoad(const FVector& Position, const FRotator& Heading, double Travel, bool Near, FHitResult* OutHit=nullptr);
    void UpdateLights(bool Night, bool Stopping, bool Project);
    void AnimateWheels(double Travel, double Steering);
    void SetManualDriver(const ARiverfrontCharacter* Driver);
    void SetDriverVisible(bool bVisible);
    bool IsNearAccess(const ARiverfrontCharacter& Character) const;
    FVector AccessPoint(const FVector& From) const;
    const TCHAR* GetVehicleLabel() const;
    FVector GetDriveHull() const { return FVector(HalfLength, HalfWidth, (RoofHeight - 12.) * .5); }
    double GetRideHeight() const { return GetDriveHull().Z + 12.; }
    double GetAccessX() const;
    double HalfLength=200., HalfWidth=85., TyreRadius=32.;
    double Wheelbase=240., Track=77.;
    float GetTopSpeed() const;
    bool IsSport() const { return BodyStyle==9; }
    FVector SeatPoint() const;
    bool bFemaleOccupant=false;
    int32 WheelCount=4;
    bool IsEmergency=false;
private:
    UPROPERTY() TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> DriverFigure;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Wheels;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Beacons;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeadLens;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TailLens;
    UPROPERTY() TObjectPtr<USpotLightComponent> DippedBeam;
    int32 BodyStyle=0;
    double RoofHeight=150.;
    int32 LightState=-1;
    int32 FrontWheels=0;
    float Spin=0;
};

struct FSabarmatiLane
{
    int32 Route=0;
    int32 ClearingJunction=INDEX_NONE;
    double At=0, Speed=0, Cruise=800, Offset=0;
    bool WasStopped=false;
    bool bParked=false;
#if !UE_BUILD_SHIPPING
    double LastDiagnosticAt=-100.;
    bool bRecordedPass=false;
    bool bRecordedPlayerYield=false;
#endif
};

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontTraffic : public AActor
{
    GENERATED_BODY()
public:
    ARiverfrontTraffic();
    void Start(bool MappedCity, const FVector& StreetStart);
    virtual void Tick(float Dt) override;
    bool WalkSignal() const;
    bool BankTrafficHasGreen() const { return Phase()==0; }
    int32 ActiveCount() const { return Agents.Num(); }
    int32 GetStops() const { return Stops; }
    int32 GetTurns() const { return Turns; }
    double GetTravelMetres() const { return TravelCm/100.; }
    FVector GetCrossing() const { return PedestrianCrossing; }
    void ForEachParkedCar(const ARiverfrontCharacter& Character, TFunctionRef<void(ARiverfrontCar&)> Visit) const;
    void ForEachEnterableVehicle(const ARiverfrontCharacter& Character, TFunctionRef<void(ARiverfrontRoadAgent&)> Visit) const;
    FVector RoadVelocity(const ARiverfrontRoadAgent* Agent) const;
    bool RemoveCrashedAgent(ARiverfrontRoadAgent* Agent);
    bool EnterVehicle(ARiverfrontCharacter& Character, ARiverfrontRoadAgent& Target);
#if !UE_BUILD_SHIPPING
    bool CheckPlayerPathPrediction(ARiverfrontCar& Car, FString& Details) const;
#endif
private:
    UPROPERTY() TArray<TObjectPtr<USplineComponent>> Routes;
    UPROPERTY() TArray<TObjectPtr<ARiverfrontRoadAgent>> Agents;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Signals;
    TArray<FSabarmatiLane> Drivers;
    TArray<FVector> Junctions;
    TArray<TWeakObjectPtr<ARiverfrontCar>> Parked;
    FVector PedestrianCrossing=FVector(-5350,4090,0);
    float Elapsed=0;
    double TravelCm=0;
    int32 Stops=0, Turns=0, LastPhase=-1;
    TWeakObjectPtr<ARiverfrontWorld> District;
    bool bCity=false;
    bool bGrandCity=false;
    bool JunctionReserved(int32 Junction) const;
    void AddDistrictRoute(const TArray<FVector>& Nodes, bool Closed=true);
    void Lane(const TArray<FVector>& Nodes, bool Reverse, bool bClosed=true);
    bool SpawnVehicle(int32 Route, double Distance, int32 Style, int32 Variant, bool ParkAtKerb=false);
    FTransform RoadPose(const USplineComponent& Route, double Distance, double Offset,
        double Radius, double Wheelbase, bool bSampleRoad) const;
    double PlayerPathGap(const ARiverfrontRoadAgent& Agent, const FSabarmatiLane& Driver,
        const USplineComponent& Route, const ARiverfrontCar& PlayerCar) const;
    void SetUpSignals();
    void UpdateJunctionClearance(FSabarmatiLane& Driver, const ARiverfrontRoadAgent& Agent, const FVector& Heading, int32 Signal);
    int32 Phase() const;
    float StopDistance(const FVector& At, const FVector& Heading, double Length, bool& Obstacle, double& PassingOffset,
        int32 Index, TConstArrayView<FVector> CrossingWalkers, FString* Diagnostic=nullptr);
};
