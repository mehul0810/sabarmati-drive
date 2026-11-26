#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Perception/AIPerceptionTypes.h"
#include "RiverfrontResidentAppearance.h"
#include "RiverfrontPedestrian.generated.h"

class ARiverfrontWorld;
class USplineComponent;
class UMaterialInterface;
class UAnimSequence;
class UBlendSpace1D;
class USkeletalMesh;
class USkeletalMeshComponent;
class UWorld;
class UAIPerceptionComponent;

/** A lightweight ambient walker on a checked, pedestrian-only circuit. */
UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontPedestrian : public AActor
{
    GENERATED_BODY()

public:
    ARiverfrontPedestrian();
    ERiverfrontGender GetAvatarGender() const { return AvatarGender; }
    bool IsCrossingStreet() const { return bAtCrosswalk && bSteppingOff; }
    int32 ShopVisits() const {return CounterVisits;}
    int32 CrossingsCompleted() const { return Crossings; }
    bool IsFamilyMember() const { return FamilyRoute != nullptr; }
    bool IsInternationalVisitor() const { return bSightseeing || bVisitingPrintmaker; }
    bool IsVisitingPrintmaker() const { return bVisitingPrintmaker; }
    int32 FamilyKerbWaits() const { return KerbWaits; }
    static int32 SpawnRiverfrontCrowd(UWorld& World);
    static ARiverfrontPedestrian* SpawnStreetWalker(UWorld& World, const FVector& A, const FVector& B, int32 Variant);
    void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;
#if !UE_BUILD_SHIPPING
    bool IsReactingToPlayer() const { return ReactionLeft > 0.; }
    int32 PlayerReactions() const { return ReactionCount; }
    bool HasPlayerAwareness() const { return Awareness != nullptr; }
    bool HasSeenPlayer() const { return bPlayerVisible; }
    double HeardPlayerAt() const { return LastPlayerFootstep; }
    int32 PrintCounterVisits() const { return PrintVisits; }
    int32 TeaCounterVisits() const { return TeaVisits; }
    bool HasTwoShopErrand() const { return bTwoShopErrand; }
#endif

protected:
    void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> Mesh;

    void Configure(USkeletalMesh* Model, UAnimSequence* Walk, const FVector& StartTurn, const FVector& EndTurn,
        double Radius, double StartFraction, double SpeedCmPerSecond, int32 Variant, bool bKurta);
    void SetGarmentColor(FName SlotName, FColor Color, UMaterialInterface* Parent = nullptr);
    ERiverfrontGender AvatarGender = ERiverfrontGender::Male;
    void UpdateRouteTransform();
    void EnablePlayerAwareness();
    bool TickPlayerAwareness(float Delta);
    UFUNCTION() void OnPlayerPerception(AActor* Actor, FAIStimulus Stimulus);
    UPROPERTY(Transient) TObjectPtr<UAIPerceptionComponent> Awareness;
    double LastPlayerFootstep = -100.;
    double AttentionTime = 0., ReactionLeft = 0.;
    int32 ReactionCount = 0;
    bool bPlayerVisible = false, bNeedsSpace = false, bResumeMoving = false;

    FVector RouteStart = FVector::ZeroVector;
    FVector RouteEnd = FVector::ZeroVector;
    double MeshYawToForward=90.;
    double TurnRadius = 0.0;
    double RouteLength = 0.0;
    double Distance = 0.0;
    double Speed = 0.0;
    float WalkingPlayRate = 1.f;
    float GaitFraction = 1.f, GaitSampleSpeed = 0.f;
    double RestAt=0., RestLeft=0.;
    int32 Crossings=0;
    bool bAtCrosswalk=false, bSteppingOff=false, bReturning=false;
    UPROPERTY() TObjectPtr<UAnimSequence> Gait;
    UPROPERTY() TObjectPtr<UAnimSequence> IdlePose;
    UPROPERTY() TObjectPtr<UBlendSpace1D> GaitBlend;
    TArray<FVector> Errand;
    int32 ErrandStep=0,ErrandDirection=1,CounterVisits=0,PrintVisits=0,TeaVisits=0;
    bool bTwoShopErrand=false;
    static bool SpawnFamilyOuting(ARiverfrontWorld& District, USkeletalMesh& FirstModel, UAnimSequence& FirstWalk,
        USkeletalMesh& SecondModel, UAnimSequence& SecondWalk);
    void TickFamily(float Delta);
    void DressCloseResident(FLinearColor ComplexionTint, FLinearColor FibreTint, bool bSweptHair);
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
    void UpdateGait(float Delta);
};
