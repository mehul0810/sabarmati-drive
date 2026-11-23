#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverfrontPedestrian.generated.h"

class ARiverfrontWorld;
class USplineComponent;
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
    bool IsFamilyMember() const { return FamilyRoute != nullptr; }
    bool IsInternationalVisitor() const { return bSightseeing || bVisitingPrintmaker; }
    bool IsVisitingPrintmaker() const { return bVisitingPrintmaker; }
    int32 FamilyKerbWaits() const { return KerbWaits; }
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
    double MeshYawToForward=90.;
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
    static bool SpawnFamilyOuting(ARiverfrontWorld& District, USkeletalMesh& FirstModel, UAnimSequence& FirstWalk,
        USkeletalMesh& SecondModel, UAnimSequence& SecondWalk);
    void TickFamily(float Delta);
    void DressCloseResident(FColor Complexion, FLinearColor FibreTint, bool bSweptHair);
    void EquipVisitorCamera();
    void EquipPrintPortfolio();
    void LookTowards(const FVector& Point, float Delta);
    void TickErrand(float Delta);
    UPROPERTY(Transient) TObjectPtr<USplineComponent> FamilyRoute;
    TArray<TWeakObjectPtr<ARiverfrontPedestrian>> Family;
    int32 FamilyFrom = 0;
    int32 FamilyTo = 1;
    int32 KerbWaits = 0;
    double FamilyPosition = 0.0;
    bool bFamilyDeparted = false;
    bool bWaitedAtKerb = false;
    bool bMoving = true;
    bool bSightseeing = false;
    bool bVisitingPrintmaker = false;
    void SetMoving(bool Moving);
};
