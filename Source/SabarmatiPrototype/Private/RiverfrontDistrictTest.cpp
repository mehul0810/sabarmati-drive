#include "RiverfrontPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontWorld.h"
#include "UnrealClient.h"

namespace
{
struct FWalkTrial
{
    FString Name;
    TArray<FVector> Points;
};
}
void ARiverfrontPlayerController::RunDistrictReview()
{
    TActorIterator<ARiverfrontWorld> District(GetWorld());
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    if (!District || !Walker || District->GetShopPath().Num()<3)
    {
        UE_LOG(LogTemp,Error,TEXT("Sabarmati district review needs the riverfront, the shop and a walker."));
        FPlatformMisc::RequestExitWithStatus(false,1);
        return;
    }
    TArray<FWalkTrial> Routes;
    Routes.Add({TEXT("tea-house"),District->GetShopPath()});
    for (const double X : {-64400.,58600.})
        for (const double Y : {2350.,-24270.})
            Routes.Add({FString::Printf(TEXT("%s-%s"),X<0?TEXT("sardar"):TEXT("ellis"),Y>0?TEXT("west"):TEXT("east")),
                {FVector(X-1420,Y,13),FVector(X-300,Y,13),FVector(X+420,Y,13),FVector(X+1420,Y,13)}});
    const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("DistrictReviews"),FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*Folder,true);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,Routes=MoveTemp(Routes),Folder,Who=TWeakObjectPtr<ARiverfrontCharacter>(Walker),Route=0,Leg=0,
         Start=0.,Previous=FVector::ZeroVector,Distance=0.,Report=FString(),Good=true,Shot=false](float) mutable
        {
            if (!Who.IsValid() || GetPawn()!=Who.Get())
            {
                UE_LOG(LogTemp,Error,TEXT("Sabarmati district review lost the walker."));
                FPlatformMisc::RequestExitWithStatus(false,1);
                return false;
            }
            const FWalkTrial& Trial=Routes[Route];
            auto* Movement=Who->GetCharacterMovement();
            const double Now=FPlatformTime::Seconds();
            if (Leg==0)
            {
                Movement->StopMovementImmediately();
                const FRotator Look=(Trial.Points[1]-Trial.Points[0]).Rotation();
                Good=Who->TeleportTo(Trial.Points[0],Look);
                SetControlRotation(Look);
                Previous=Who->GetActorLocation();
                Distance=0.; Start=Now; Leg=1; Shot=false;
            }
            const FVector Here=Who->GetActorLocation();
            Distance+=FVector::Dist2D(Here,Previous); Previous=Here;
            if (Now-Start>.65 && Movement->IsFalling()) Good=false;
            const FVector To=Trial.Points[Leg]-Here;
            if (To.Size2D()<34 && ++Leg==Trial.Points.Num())
            {
                if (!Good)
                {
                    UE_LOG(LogTemp,Error,TEXT("Sabarmati district review lost floor in %s."),*Trial.Name);
                    FPlatformMisc::RequestExitWithStatus(false,1);
                    return false;
                }
                Report+=FString::Printf(TEXT("%s,%.2f,grounded\n"),*Trial.Name,Distance/100.);
                UE_LOG(LogTemp,Display,TEXT("Sabarmati district walk %s: %.2f m, grounded."),*Trial.Name,Distance/100.);
                if (++Route==Routes.Num())
                {
                    if (!FFileHelper::SaveStringToFile(Report,*FPaths::Combine(Folder,TEXT("walks.csv"))))
                    {
                        FPlatformMisc::RequestExitWithStatus(false,1);
                        return false;
                    }
                    UE_LOG(LogTemp,Display,TEXT("Sabarmati district access complete: %s"),*Folder);
                    FPlatformMisc::RequestExit(false);
                    return false;
                }
                Leg=0;
                return true;
            }
            if (!Good || Now-Start>27.)
            {
                UE_LOG(LogTemp,Error,TEXT("Sabarmati district review blocked in %s at leg %d, position %s."),*Trial.Name,Leg,*Here.ToCompactString());
                FPlatformMisc::RequestExitWithStatus(false,1);
                return false;
            }
            const FVector Dir=(Trial.Points[Leg]-Here).GetSafeNormal2D();
            SetControlRotation(FRotator(-3,Dir.Rotation().Yaw,0));
            Who->AddMovementInput(Dir);
            if (!Shot && Leg>=2 && Distance>(Route==0?365.:1220.))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,Trial.Name+TEXT(".png")),false,false);
                Shot=true;
            }
            return true;
        }));
}
#endif
