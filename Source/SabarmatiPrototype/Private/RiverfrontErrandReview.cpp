#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontPedestrian.h"
#include "RiverfrontWorld.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ARiverfrontPlayerController::RunErrandReview()
{
    TWeakObjectPtr<ARiverfrontPedestrian> Printmaker;
    for (TActorIterator<ARiverfrontPedestrian> It(GetWorld()); It; ++It)
        if (It->IsVisitingPrintmaker()) { Printmaker=*It; break; }
    TWeakObjectPtr<ARiverfrontWorld> District;
    if (TActorIterator<ARiverfrontWorld> It(GetWorld()); It) District=*It;
    auto Previous=MakeShared<FVector>(Printmaker.IsValid()?Printmaker->GetActorLocation():FVector::ZeroVector);
    auto Travel=MakeShared<double>(0.);
    auto CheckedPrint=MakeShared<bool>(false);
    const double Started=FPlatformTime::Seconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [Printmaker,District,Previous,Travel,CheckedPrint,Started](float)
        {
            const auto Fail=[](const TCHAR* Reason)
            {
                UE_LOG(LogTemp, Error, TEXT("Sabarmati errand review failed: %s"),Reason);
                FPlatformMisc::RequestExitWithStatus(false,1); return false;
            };
            if (!Printmaker.IsValid() || !District.IsValid() || !Printmaker->HasTwoShopErrand())
                return Fail(TEXT("checked two-shop route unavailable"));
            *Travel+=FVector::Dist2D(*Previous,Printmaker->GetActorLocation());
            *Previous=Printmaker->GetActorLocation();
            if (Printmaker->PrintCounterVisits()>0 && !*CheckedPrint)
            {
                if (FVector::Dist(Printmaker->GetActorLocation(),District->GetShopVisit(false).Last())>10.)
                    return Fail(TEXT("print counter arrival"));
                *CheckedPrint=true;
            }
            if (Printmaker->TeaCounterVisits()>0)
            {
                if (!*CheckedPrint || FVector::Dist(Printmaker->GetActorLocation(),District->GetShopVisit(true).Last())>10.
                    || Printmaker->ShopVisits()<2 || *Travel<1000.) return Fail(TEXT("tea counter arrival or route distance"));
                UE_LOG(LogTemp, Display, TEXT("Sabarmati errand review: reached both counters on foot; travelled %.1f m."),*Travel*.01);
                FPlatformMisc::RequestExitWithStatus(false,0); return false;
            }
            if (FPlatformTime::Seconds()-Started>110.) return Fail(TEXT("timed out before both counter visits"));
            return true;
        }));
}
#endif
