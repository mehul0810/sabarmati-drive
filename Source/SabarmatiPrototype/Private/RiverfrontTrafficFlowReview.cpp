#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontCar.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontTraffic.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

namespace
{
struct FTrafficFlowEvidence
{
    double Started=FPlatformTime::Seconds();
    TSet<FName> TwoWheelApproaches,LongApproaches;
    bool bTwoWheelPassed=false,bLongPassed=false;
};
}

bool ARiverfrontTraffic::CheckPlayerPathPrediction(ARiverfrontCar& Car,FString& Details) const
{
    auto* CarBody=Cast<UPrimitiveComponent>(Car.GetRootComponent());
    if (bCity || !CarBody) {Details=TEXT("riverfront player collision unavailable");return false;}
    for (int32 I=0;I<Agents.Num();++I)
    {
        auto* Agent=Agents[I].Get();
        if (Drivers[I].bParked || Agent->WheelCount!=2) continue;
        const auto* Route=Routes[Drivers[I].Route].Get();
        const double Length=Route->GetSplineLength();
        for (double At=0.;At+1500.<Length;At+=200.)
        {
            const FVector Point=Route->GetLocationAtDistanceAlongSpline(At,ESplineCoordinateSpace::World);
            if (FVector::DistSquared2D(Point,FVector(3150.,4090.,0.))>FMath::Square(1600.)) continue;
            const FTransform Start=RoadPose(*Route,At,0.,Agent->TyreRadius,Agent->Wheelbase,false);
            const FTransform End=RoadPose(*Route,At+950.,0.,Agent->TyreRadius,Agent->Wheelbase,false);
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(Start.Rotator().Yaw,End.Rotator().Yaw))<20.) continue;

            const FTransform OriginalAgent=Agent->GetActorTransform(),OriginalCar=Car.GetActorTransform();
            const bool bSimulating=CarBody->IsSimulatingPhysics();
            const FVector OriginalVelocity=bSimulating?CarBody->GetPhysicsLinearVelocity():FVector::ZeroVector;
            Agent->SetActorLocationAndRotation(Start.GetLocation(),Start.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
            FVector CarPoint=End.GetLocation()+FVector(0.,0.,68.-Agent->TyreRadius-6.);
            Car.SetActorLocationAndRotation(CarPoint,End.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
            if (bSimulating) CarBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
            FSabarmatiLane Sample=Drivers[I];Sample.At=At;Sample.Offset=0.;Sample.Speed=800.;
            const double OnPath=PlayerPathGap(*Agent,Sample,*Route,Car);
            const FVector Side=End.GetRotation().GetRightVector().GetSafeNormal2D();
            CarPoint+=Side*750.;
            Car.SetActorLocationAndRotation(CarPoint,End.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
            if (bSimulating) CarBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
            const double Away=PlayerPathGap(*Agent,Sample,*Route,Car);
            if (bSimulating) CarBody->SetPhysicsLinearVelocity(-Side*650.);
            const double Crossing=PlayerPathGap(*Agent,Sample,*Route,Car);
            if (bSimulating) CarBody->SetPhysicsLinearVelocity(Side*650.);
            const double Departing=PlayerPathGap(*Agent,Sample,*Route,Car);
            Car.SetActorTransform(OriginalCar,false,nullptr,ETeleportType::TeleportPhysics);
            if (bSimulating) CarBody->SetPhysicsLinearVelocity(OriginalVelocity);
            Agent->SetActorTransform(OriginalAgent,false,nullptr,ETeleportType::TeleportPhysics);
            Details=FString::Printf(TEXT("agent=%s physics=%d on_path_gap_cm=%.1f adjacent_gap_cm=%.1f crossing_gap_cm=%.1f departing_gap_cm=%.1f"),
                *Agent->GetName(),bSimulating,OnPath,Away,Crossing,Departing);
            return bSimulating && OnPath<1.e7 && Away>=1.e7 && Crossing<1.e7 && Departing>=1.e7;
        }
    }
    Details=TEXT("no two-wheeler route with a testable junction turn");
    return false;
}

void ARiverfrontPlayerController::RunTrafficFlowReview()
{
    const auto Fail=[](const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("Sabarmati traffic flow review failed: %s"),Why);
        FPlatformMisc::RequestExitWithStatus(false,1);return false;
    };
    if (FParse::Param(FCommandLine::Get(),TEXT("SabarmatiTrafficYieldCheck")))
    {
        const auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
        auto* Traffic=Mode?Mode->GetTraffic():nullptr;
        auto* Car=Mode?Mode->GetCar():nullptr;
        FString Details;
        if (!Traffic || !Car || !Traffic->CheckPlayerPathPrediction(*Car,Details))
        {
            UE_LOG(LogTemp,Error,TEXT("Sabarmati traffic prediction review: %s"),*Details);
            Fail(TEXT("player path prediction"));return;
        }
        UE_LOG(LogTemp,Display,TEXT("Sabarmati traffic prediction review passed: %s"),*Details);
    }
    int32 LongShapes=0;
    for (TActorIterator<ARiverfrontRoadAgent> It(GetWorld());It;++It)
    {
        if (It->HalfLength<300.) continue;
        auto* Collision=Cast<UPrimitiveComponent>(It->GetRootComponent());
        if (!Collision) {Fail(TEXT("long vehicle collision component"));return;}
        const FVector At=It->GetActorLocation();
        const double Ground=At.Z-It->TyreRadius-6.;
        const FVector Side=It->GetActorRightVector().GetSafeNormal2D();
        FHitResult Person,Car;
        const FVector PersonEnd(At.X,At.Y,Ground+92.);
        const FVector CarEnd(At.X,At.Y,Ground+68.);
        const bool bPerson=Collision->SweepComponent(Person,PersonEnd+Side*(It->HalfWidth+500.),PersonEnd,
            FQuat::Identity,FCollisionShape::MakeCapsule(33.f,92.f));
        const bool bCar=Collision->SweepComponent(Car,CarEnd+Side*(It->HalfWidth+500.),CarEnd,
            FQuat::Identity,FCollisionShape::MakeBox(FVector(216.,95.,50.)));
        if (!bPerson || !bCar) {Fail(TEXT("long vehicle no longer blocks a pedestrian or car"));return;}
        ++LongShapes;
    }
    if (!LongShapes) {Fail(TEXT("no long vehicle available"));return;}
    UE_LOG(LogTemp,Display,TEXT("Sabarmati traffic flow review: %d long vehicles retain pedestrian and car collision."),LongShapes);
    auto State=MakeShared<FTrafficFlowEvidence>();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,State,Fail](float)
    {
        for (TActorIterator<ARiverfrontRoadAgent> It(GetWorld());It;++It)
        {
            const FVector P=It->GetActorLocation();
            if (FMath::Abs(P.X-3150.)>500.) continue;
            const double Heading=It->GetActorForwardVector().Y;
            if (It->WheelCount==2 && Heading<-.5 && P.Y>3650. && P.Y<4150.)
                State->TwoWheelApproaches.Add(It->GetFName());
            if (State->TwoWheelApproaches.Contains(It->GetFName()) && P.Y<3500. && !State->bTwoWheelPassed)
            {
                State->bTwoWheelPassed=true;
                UE_LOG(LogTemp,Display,TEXT("Sabarmati traffic flow review: %s crossed the graded road-to-bridge join."),*It->GetName());
            }
            if (It->HalfLength>=300. && Heading>.5 && P.Y>2000. && P.Y<2500.)
                State->LongApproaches.Add(It->GetFName());
            if (State->LongApproaches.Contains(It->GetFName()) && P.Y>2700. && !State->bLongPassed)
            {
                State->bLongPassed=true;
                UE_LOG(LogTemp,Display,TEXT("Sabarmati traffic flow review: %s crossed the bridge grade change."),*It->GetName());
            }
        }
        if (State->bTwoWheelPassed && State->bLongPassed)
        {
            UE_LOG(LogTemp,Display,TEXT("Sabarmati traffic flow review complete: two-wheeler and long vehicle traversed their bridge sections."));
            FPlatformMisc::RequestExitWithStatus(false,0);return false;
        }
        if (FPlatformTime::Seconds()-State->Started>130.) return Fail(TEXT("traffic did not complete both bridge traversals"));
        return true;
    }));
}
#endif
