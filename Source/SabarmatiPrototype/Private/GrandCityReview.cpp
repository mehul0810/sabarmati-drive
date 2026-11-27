#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "GrandCityLayout.h"
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontWorld.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
bool FailGrandCityReview(const TCHAR* Reason)
{
    UE_LOG(LogTemp,Error,TEXT("Grand City review failed: %s."),Reason);
    FPlatformMisc::RequestExitWithStatus(false,1);
    return false;
}

struct FGrandCityReviewRun
{
    FVector Spawn=FVector::ZeroVector;
    double Started=0., LastFrame=0.;
    double StageStarted=0.;
    int32 Probes=0;
    int32 Stage=0;
    TArray<double> FrameMs;
};
}

void ARiverfrontPlayerController::RunGrandCityReview()
{
    using namespace GrandCityLayout;
    const auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    auto* Car=Mode ? Mode->GetCar() : nullptr;
    if (!Mode || !Mode->IsGrandCity() || !Walker || !IsValid(Car) || GetPlaces().Num()<6 ||
        !Mode->GetDistrict() || Mode->GetDistrict()->GetTeaCounter().IsNearlyZero())
    {
        FailGrandCityReview(TEXT("map, spawn car and destinations"));
        return;
    }

    const auto Run=MakeShared<FGrandCityReviewRun>();
    for (int32 Y : {MarketStreet,FoundryStreet})
        for (double Offset : {-340.,-170.,170.,340.})
            for (int32 X=WestAbutment-600;X<=EastAbutment+600;X+=50)
            {
                const double Height=BridgeHeight(X);
                FHitResult Hit;
                if (!GetWorld()->LineTraceSingleByObjectType(Hit,FVector(X,Y+Offset,Height+90.),
                    FVector(X,Y+Offset,Height-70.),FCollisionObjectQueryParams(ECC_WorldStatic))
                    || !Hit.GetComponent() || !Hit.GetComponent()->ComponentHasTag(TEXT("CityRoad"))
                    || FMath::Abs(Hit.ImpactPoint.Z-Height)>4. || Hit.ImpactNormal.Z<.98)
                {
                    UE_LOG(LogTemp,Error,TEXT("Grand City crossing probe: x=%d y=%.1f expected=%.1f hit=%.1f"),
                        X,Y+Offset,Height,Hit.ImpactPoint.Z);
                    FailGrandCityReview(TEXT("crossing load surface"));
                    return;
                }
                ++Run->Probes;
            }
    Run->Spawn=Walker->GetActorLocation();
    Run->Started=FPlatformTime::Seconds();
    int32 Seconds=8;
    FParse::Value(FCommandLine::Get(),TEXT("GrandCitySoakSeconds="),Seconds);
    Seconds=FMath::Clamp(Seconds,8,1200);
    const FString Evidence=FPaths::ProjectSavedDir()/TEXT("GrandCityReview");
    IFileManager::Get().MakeDirectory(*Evidence,true);
    SetControlRotation(FRotator(-7,180,0));
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,Run,Evidence,Seconds,Expected=TWeakObjectPtr<ARiverfrontCar>(Car)](float)
        {
            const double Now=FPlatformTime::Seconds();
            const double Age=Now-Run->Started;
            const auto Tap=[this](FKey Key)
            {
                if (!PlayerInput) return;
                const auto* View=GetWorld()->GetGameViewport();
                if (!View || !View->Viewport) return;
                for (EInputEvent Event : {IE_Pressed,IE_Released})
                {
                    InputKey(FInputKeyEventArgs(View->Viewport,FInputDeviceId::CreateFromInternalId(0),
                        Key,Event,FPlatformTime::Cycles64()));
                    ProcessPlayerInput(0.f,false);
                }
            };
            auto* Person=Cast<ARiverfrontCharacter>(GetPawn());
            auto* Parked=Expected.Get();
            if (!Parked) return FailGrandCityReview(TEXT("parked car disappeared"));
            if (Run->Stage==0)
            {
                if (Age<2.) return true;
                if (!Person) return FailGrandCityReview(TEXT("walk pawn"));
                Run->Spawn=Person->GetActorLocation();
                FScreenshotRequest::RequestScreenshot(Evidence/TEXT("market-street.png"),false,false);
                Run->StageStarted=Now;
                ++Run->Stage;
                return true;
            }
            if (Run->Stage==1)
            {
                if (!Person) return FailGrandCityReview(TEXT("walk pawn"));
                if (Run->Spawn.X-Person->GetActorLocation().X<220. && Now-Run->StageStarted<6.)
                {
                    Person->AddMovementInput(FVector(-1,0,0),1.f,true);
                    return true;
                }
                if (Run->Spawn.X-Person->GetActorLocation().X<200.)
                {
                    UE_LOG(LogTemp,Error,TEXT("Grand City walk: start %s end %s, mode %d, velocity %s."),
                        *Run->Spawn.ToCompactString(),*Person->GetActorLocation().ToCompactString(),
                        int32(Person->GetCharacterMovement()->MovementMode),*Person->GetVelocity().ToCompactString());
                    return FailGrandCityReview(TEXT("walk down the market footpath"));
                }
                const FVector Reach(Parked->GetActorLocation().X+10.,GrandCityLayout::WalkY,Run->Spawn.Z);
                if (!Person->TeleportTo(Reach,FRotator(0,-90,0))) return FailGrandCityReview(TEXT("approach parked door"));
                SetControlRotation((Parked->DoorInteractionPoint(Person->GetActorLocation())-
                    Person->GetActorLocation()).GetSafeNormal2D().Rotation());
                const auto Prompt=UpdateInteractionPrompt();
                if (!Prompt.Action.StartsWith(TEXT("Enter")))
                {
                    UE_LOG(LogTemp,Error,TEXT("Grand City parking: player %s car %s, near=%d, prompt '%s'."),
                        *Person->GetActorLocation().ToCompactString(),*Parked->GetActorLocation().ToCompactString(),
                        Parked->IsNearDoor(*Person),*Prompt.Action);
                    return FailGrandCityReview(TEXT("parked car prompt"));
                }
                Tap(EKeys::E);
                if (GetPawn()!=Parked) return FailGrandCityReview(TEXT("enter selected car"));
                Run->StageStarted=Now;
                ++Run->Stage;
                return true;
            }
            if (Run->Stage==2 && Now-Run->StageStarted>=1.5)
            {
                if (GetPawn()!=Parked) return FailGrandCityReview(TEXT("car possession"));
                FScreenshotRequest::RequestScreenshot(Evidence/TEXT("canal-district-driving.png"),false,false);
                ++Run->Stage;
                return true;
            }
            if (Run->Stage==3 && !FScreenshotRequest::IsScreenshotRequested())
            {
                Tap(EKeys::E);
                if (!Cast<ARiverfrontCharacter>(GetPawn())) return FailGrandCityReview(TEXT("safe exit from parked car"));
                ++Run->Stage;
            }
            if (Age>=5.)
            {
                if (Run->LastFrame>0.) Run->FrameMs.Add((Now-Run->LastFrame)*1000.);
                Run->LastFrame=Now;
            }
            if (Age>Seconds+35.) return FailGrandCityReview(TEXT("timed out at the market or car"));
            if (Age<Seconds || Run->Stage<4 || FScreenshotRequest::IsScreenshotRequested()) return true;
            if (Run->FrameMs.IsEmpty() || Run->Stage!=4) return FailGrandCityReview(TEXT("incomplete play sample"));
            Run->FrameMs.Sort();
            const auto Percentile=[&](double Fraction){return Run->FrameMs[int32((Run->FrameMs.Num()-1)*Fraction)];};
            UE_LOG(LogTemp,Display,TEXT("Grand City review passed: %d bridge probes, market walk, parked entry/exit, %.1fs, %d frames, median=%.2f ms p95=%.2f ms; %s."),
                Run->Probes,Age,Run->FrameMs.Num(),Percentile(.50),Percentile(.95),*Evidence);
            FPlatformMisc::RequestExitWithStatus(false,0);
            return false;
        }),0.f);
}
#endif
