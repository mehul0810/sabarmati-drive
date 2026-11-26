#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontTraffic.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
struct FTrafficFlowEvidence
{
    double Started=FPlatformTime::Seconds();
    TSet<FName> TwoWheelApproaches,LongApproaches;
    bool bTwoWheelPassed=false,bLongPassed=false;
};
}

void ARiverfrontPlayerController::RunTrafficFlowReview()
{
    const auto Fail=[](const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("Sabarmati traffic flow review failed: %s"),Why);
        FPlatformMisc::RequestExitWithStatus(false,1);return false;
    };
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
