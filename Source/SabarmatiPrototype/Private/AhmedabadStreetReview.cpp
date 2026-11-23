#include "RiverfrontPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontWorld.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ARiverfrontPlayerController::RunNeighbourhoodReview()
{
    TActorIterator<ARiverfrontWorld> District(GetWorld());
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    if (!District || !District->IsCityMode() || !Walker || !Mode || !Mode->GetCar())
    {
        UE_LOG(LogTemp,Error,TEXT("Street review requires the offline city, a walker and parked car."));
        FPlatformMisc::RequestExitWithStatus(false,1);return;
    }
    const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("StreetReviews"),FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    if (!IFileManager::Get().MakeDirectory(*Folder,true)) {FPlatformMisc::RequestExitWithStatus(false,1);return;}
    struct FVisit
    {
        int32 Stop=0,Stage=0,Count=0;
        double Since=0,Last=0,Walked=0,Driven=0,TravelMs=0;
        FVector From=FVector::ZeroVector;
        FString Rows=TEXT("place,plots_within_150m,walk_m,measured_frames,p95_wall_ms,street_travel_call_ms\n");
        TArray<double> Frames;
    };
    auto State=MakeShared<FVisit>();State->Since=FPlatformTime::Seconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,State,Folder,City=TWeakObjectPtr<ARiverfrontWorld>(*District),Who=TWeakObjectPtr<ARiverfrontCharacter>(Walker),
            Vehicle=TWeakObjectPtr<ARiverfrontCar>(Mode->GetCar())](float)
        {
            const double Now=FPlatformTime::Seconds(),Age=Now-State->Since;
            const TCHAR* Stops[]={TEXT("ashram"),TEXT("cg"),TEXT("sg-service"),TEXT("city-landing"),TEXT("ashram")};
            auto Fail=[&](const TCHAR* Why)
            {
                UE_LOG(LogTemp,Error,TEXT("Street review failed: stop %d stage %d: %s"),State->Stop,State->Stage,Why);
                FPlatformMisc::RequestExitWithStatus(false,1);return false;
            };
            if (!City.IsValid() || !Who.IsValid() || !Vehicle.IsValid() || Age>35.) return Fail(TEXT("actor or timeout"));
            const auto Key=[&](FKey K,EInputEvent E){InputKey(FInputKeyEventArgs(GetWorld()->GetGameViewport()->Viewport,
                FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
            const auto Tap=[&](FKey K){Key(K,IE_Pressed);Key(K,IE_Released);};
            const auto Next=[&](int32 N){State->Stage=N;State->Since=Now;};
            const auto Shot=[&](const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,Name+TEXT(".png")),false,false);};
            const FRiverSite* Target=Destination();
            if (State->Stage==0)
            {
                if (Age<.14) return true;
                if (!Target || Target->Id!=FName(Stops[State->Stop]))
                {
                    if (++State->Count>16) return Fail(TEXT("missing named destination"));
                    Tap(EKeys::N);State->Since=Now;return true;
                }
                State->Count=0;
                Tap(EKeys::V);State->TravelMs=(FPlatformTime::Seconds()-Now)*1000.;Next(1);
            }
            else if (State->Stage==1)
            {
                if (Age<3.0) return true;
                if (!Target || FVector::Dist2D(GetPawn()->GetActorLocation(),Target->Point)>600.) return Fail(TEXT("street travel"));
                State->From=GetPawn()->GetActorLocation();
                if (State->Stop==3)
                {
                    const FVector Door=Vehicle->GetActorTransform().TransformPosition(FVector(11,-169,35));
                    if (!Who->TeleportTo(Door,Vehicle->GetActorRotation())) return Fail(TEXT("access to parked car"));
                    Tap(EKeys::E);Next(4);return true;
                }
                if (State->Stop==4)
                {
                    if (GetPawn()!=Vehicle.Get()) return Fail(TEXT("travel with possessed car"));
                    Key(EKeys::W,IE_Pressed);Next(5);return true;
                }
                if (City->CityBuildingsNear(Target->Point,15000.)<4) return Fail(TEXT("missing street plots"));
                SetControlRotation(FRotator(-3,Target->StreetHeading.GetValue()+13.,0));
                State->Frames.Reset();State->Last=Now;Next(2);
            }
            else if (State->Stage==2)
            {
                if (!Target || Who->GetCharacterMovement()->IsFalling()) return Fail(TEXT("unsupported walking lane"));
                Who->AddMovementInput(FRotator(0,Target->StreetHeading.GetValue(),0).Vector(),1.f,true);
                State->Frames.Add((Now-State->Last)*1000.);State->Last=Now;
                State->Walked=FVector::Dist2D(Who->GetActorLocation(),State->From)/100.;
                if (State->Walked<8. || State->Frames.Num()<120) return true;
                Shot(Target->Id.ToString());
                State->Frames.Sort();
                const double P95=State->Frames[FMath::CeilToInt(.95*State->Frames.Num())-1];
                State->Rows+=FString::Printf(TEXT("%s,%d,%.2f,%d,%.3f,%.3f\n"),*Target->Id.ToString(),
                    City->CityBuildingsNear(Target->Point,15000.),State->Walked,State->Frames.Num(),P95,State->TravelMs);
                Next(3);
            }
            else if (State->Stage==3 && Age>.65) { ++State->Stop;Next(0); }
            else if (State->Stage==4 && Age>.65)
            {
                if (GetPawn()!=Vehicle.Get()) return Fail(TEXT("enter from street stop"));
                ++State->Stop;Next(0);
            }
            else if (State->Stage==5)
            {
                State->Driven=FVector::Dist2D(Vehicle->GetActorLocation(),State->From)/100.;
                if (State->Driven<22.) return true;
                if (!Vehicle->HasFourContacts()) return Fail(TEXT("car lost road contact"));
                Key(EKeys::W,IE_Released);Key(EKeys::SpaceBar,IE_Pressed);Shot(TEXT("ashram-driving"));Next(6);
            }
            else if (State->Stage==6 && Vehicle->GetSpeedKph()<.8)
            {
                Key(EKeys::SpaceBar,IE_Released);Tap(EKeys::E);Next(7);
            }
            else if (State->Stage==7 && Age>.7)
            {
                if (GetPawn()!=Who.Get()) return Fail(TEXT("street exit"));
                if (!FFileHelper::SaveStringToFile(State->Rows,*FPaths::Combine(Folder,TEXT("visits.csv"))) ||
                    !FFileHelper::SaveStringToFile(FString::Printf(TEXT("drive_m=%.2f\ncontacts=4\nexit=1\n"),State->Driven),*FPaths::Combine(Folder,TEXT("drive.txt"))))
                    return Fail(TEXT("evidence output"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati street review complete: %s"),*Folder);
                FPlatformMisc::RequestExit(false);return false;
            }
            return true;
        }));
}
#endif
