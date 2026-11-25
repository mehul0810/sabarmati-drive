#include "RiverfrontPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontWorld.h"
#include "RiverfrontPedestrian.h"
#include "RiverfrontTraffic.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "UnrealClient.h"

namespace
{
class FSystemsReview
{
public:
    explicit FSystemsReview(ARiverfrontPlayerController& Player) : PC(Player) {}
    bool Start()
    {
        const auto* Mode=PC.GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
        TActorIterator<ARiverfrontWorld> World(PC.GetWorld());
        if (!World || !Mode || Mode->IsCityMode()) return Fail(TEXT("riverfront world"));
        District=*World;Car=Mode->GetCar();Traffic=Mode->GetTraffic();Walker=Cast<ARiverfrontCharacter>(PC.GetPawn());
        if (!Car.IsValid() || !Traffic.IsValid() || !Walker.IsValid() || Traffic->ActiveCount()<12 || District->GetShopPath().Num()<3)
            return Fail(TEXT("playable actors or traffic network"));
        FParse::Value(FCommandLine::Get(),TEXT("SabarmatiEndurance="),Minutes);
        Minutes=FMath::Clamp(Minutes,0,60);
        Folder=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SystemsReviews"),FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        if (!IFileManager::Get().MakeDirectory(*Folder,true)) return Fail(TEXT("output directory"));
        auto& Console=IConsoleManager::Get();
        for (const auto& V:TArray<TPair<FString,int32>>{{TEXT("r.ScreenPercentage"),100},{TEXT("r.SecondaryScreenPercentage.GameViewport"),100},
            {TEXT("r.VSync"),0},{TEXT("r.DynamicRes.OperationMode"),0},{TEXT("t.IdleWhenNotForeground"),0}})
            if (auto* C=Console.FindConsoleVariable(*V.Key)) C->Set(V.Value,ECVF_SetByCode);
        auto* Window=PC.GetWorld()->GetGameViewport()->Viewport;
        FString Config=FString::Printf(TEXT("gpu=%s\nviewport=%dx%d\nwindowed=%d\n"),*FPlatformMisc::GetPrimaryGPUBrand(),
            Window->GetSizeXY().X,Window->GetSizeXY().Y,Window->GetWindowMode()==EWindowMode::Windowed);
        Config += FString::Printf(TEXT("frame_smoothing=%d\n"), GEngine && GEngine->IsAllowedFramerateSmoothing());
        for (const TCHAR* Key:{TEXT("r.ScreenPercentage"),TEXT("r.SecondaryScreenPercentage.GameViewport"),TEXT("r.AntiAliasingMethod"),TEXT("r.VSync"),TEXT("r.DynamicRes.OperationMode"),TEXT("t.MaxFPS"),TEXT("t.IdleWhenNotForeground")})
            if (auto* C=Console.FindConsoleVariable(Key)) Config+=FString::Printf(TEXT("%s=%s\n"),Key,*C->GetString());
        if (!Write(TEXT("settings.txt"),Config)) return Fail(TEXT("settings file"));
        const FVector Street=FRiverfrontFirstMorningWorld::StreetPoint();
        int32 Local=0, Assembled=0;
        TSet<FName> Bodies;
        for (TActorIterator<ARiverfrontRoadAgent> It(PC.GetWorld());It;++It)
        {
            const TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
            for (const auto* Part:Parts)
            {
                const UStaticMesh* Mesh=Part->GetStaticMesh();
                if (!Mesh || !Mesh->GetName().StartsWith(TEXT("SM_Traffic"))) continue;
                if (Mesh->GetNumLODs()!=3 || Part->GetNumMaterials()<4) return Fail(TEXT("traffic body render data"));
                Bodies.Add(Mesh->GetFName());
                ++Assembled;
                if (FVector::DistSquared2D(Street,It->GetActorLocation())<FMath::Square(4700.)) ++Local;
                break;
            }
        }
        const FString Fleet=FString::Printf(TEXT("road_agents=%d\nbodies_loaded=%d\nbody_styles=%d\nwithin_47m_of_kesar=%d\n"),
            Traffic->ActiveCount(),Assembled,Bodies.Num(),Local);
        UE_LOG(LogTemp,Display,TEXT("Sabarmati fleet review: %s"),*Fleet);
        if (Assembled!=Traffic->ActiveCount() || Bodies.Num()!=12 || Local<7 || !Write(TEXT("fleet.txt"),Fleet))
            return Fail(TEXT("traffic mesh variety or starting street coverage"));
        if (!Walker->TeleportTo(FVector(Street.X-540.,4790.,99.),FRotator(0.,-19.,0.))) return Fail(TEXT("Kesar street viewpoint"));
        PC.SetControlRotation(FRotator(-1.,-19.,0.));
        StartTime=StageTime=FPlatformTime::Seconds();UE_LOG(LogTemp,Display,TEXT("Sabarmati systems review: %s"),*Folder);
        return true;
    }
    bool Tick()
    {
        if (!Walker.IsValid() || !Car.IsValid() || !Traffic.IsValid() || !District.IsValid()) return Fail(TEXT("lost scene actor"));
        if (Pending.IsValid()) { Key(Pending,IE_Released); Pending=FKey(); }
        const double Now=FPlatformTime::Seconds(), Age=Now-StageTime;
        if (Stage==99) return Endurance(Now);
        if (Age>90.) return Fail(*FString::Printf(TEXT("timeout in stage %d"),Stage));
        if (Age<.57) return true;
        switch (Stage)
        {
        case -1:
            Shot(TEXT("kesar-traffic"));
            break;
        case 0:
            if (!Walker->TeleportTo(Car->GetActorTransform().TransformPosition(FVector(12,-174,37)),Car->GetActorRotation()) || !Car->IsNearDoor(*Walker.Get()))
                return Fail(TEXT("reach hatchback door"));
            Tap(EKeys::O); break;
        case 1:
            if (!Car->IsDoorOpen()) return Fail(TEXT("front door did not articulate"));
            Shot(TEXT("door")); Tap(EKeys::E); break;
        case 2:
            if (PC.GetPawn()!=Car.Get()) return Fail(TEXT("enter through E"));
            Tap(EKeys::C); break;
        case 3:
            if (Car->GetViewMode()!=1) return Fail(TEXT("cockpit camera"));
            Shot(TEXT("cockpit"));Tap(EKeys::Z);break;
        case 4:
            if (Car->GetSignal()!=-1) return Fail(TEXT("left indicator"));
            Tap(EKeys::B);break;
        case 5:
            if (Car->GetSignal()!=2) return Fail(TEXT("hazard switch"));
            Tap(EKeys::H);TapWeather();break;
        case 6:
            if (FCString::Strcmp(District->WeatherName(),TEXT("Haze"))!=0) return Fail(TEXT("haze"));
            Tap(EKeys::P);break;
        case 7:
            if (Age<2.8) return true;
            if (FCString::Strcmp(District->WeatherName(),TEXT("Monsoon"))!=0 || District->GetWetness()<.1f) return Fail(TEXT("rain and wet road"));
            Tap(EKeys::C);Shot(TEXT("monsoon")); break;
        case 8:
            if (Car->GetViewMode()!=2) return Fail(TEXT("bonnet camera"));
            Tap(EKeys::C);break;
        case 9:
            if (Car->GetViewMode()!=0) return Fail(TEXT("return to chase camera"));
            Tap(EKeys::G);break;
        case 10:
            if (PC.NavigationText().IsEmpty()) return Fail(TEXT("time trial guidance"));
            // Measure acceleration and braking on a clear stretch, then run the
            // traffic-aware four-gate drive separately below.
            {
                bool Clear=false;
                for (double X:{-34500.,-30000.,-25000.,7000.,28500.})
                {
                    const FVector Start(X,4090.,72.);
                    const FCollisionQueryParams Query(SCENE_QUERY_STAT(BrakeTestLane),false,Car.Get());
                    if (PC.GetWorld()->SweepTestByChannel(Start,Start+FVector(11000.,0,0),FQuat::Identity,
                        ECC_Pawn,FCollisionShape::MakeBox(FVector(215.,97.,45.)),Query)) continue;
                    if (Car->TeleportTo(Start,FRotator::ZeroRotator)) { Clear=true;break; }
                }
                if (!Clear) return Fail(TEXT("clear approach for braking distance"));
            }
            Origin=Car->GetActorLocation();Key(EKeys::W,IE_Pressed);break;
        case 11:
            if (FVector::Dist2D(Car->GetActorLocation(),Origin)<5200 && Age<14.) return true;
            if (Car->GetGear()<2 || !Car->HasFourContacts() || Car->GetSpeedKph()<12)
                return Fail(*FString::Printf(TEXT("acceleration: %.1f m, %.1f km/h, gear %d, four contacts %d, %.0f RPM"),
                    FVector::Dist2D(Car->GetActorLocation(),Origin)*.01,Car->GetSpeedKph(),Car->GetGear(),
                    Car->HasFourContacts(),Car->GetEngineRpm()));
            Shot(TEXT("driving-instruments"));
            if (!Write(TEXT("driving-instruments.csv"), FString::Printf(
                TEXT("speed_kmh,gear,engine_rpm,fuel_percent\n%.2f,%d,%.0f,%.2f\n"),
                Car->GetSpeedKph(),Car->GetGear(),Car->GetEngineRpm(),100.f*Car->GetFuel())))
                return Fail(TEXT("driving instrument readings"));
            Key(EKeys::W,IE_Released);Key(EKeys::SpaceBar,IE_Pressed);break;
        case 12:
            if (Car->GetSpeedKph()>1.f && Age<8.) return true;
            if (Car->GetSpeedKph()>1.f) return Fail(TEXT("braking"));
            Key(EKeys::SpaceBar,IE_Released);Key(EKeys::S,IE_Pressed);break;
        case 13:
            if (Age<1.25) return true;
            if (Car->GetGear()!=-1) return Fail(TEXT("reverse gear"));
            Key(EKeys::S,IE_Released);Tap(EKeys::R);break;
        case 14:
            if (Car->GetSpeedKph()>1.f) return Fail(TEXT("parked reset"));
            Tap(EKeys::E);break;
        case 15:
            if (PC.GetPawn()!=Walker.Get()) return Fail(TEXT("exit through E"));
            if (!Walker->TeleportTo(District->GetTeaCounter(),FRotator(0,90,0))) return Fail(TEXT("tea room floor"));
            PC.SetControlRotation(FRotator(-5,90,0));Tap(EKeys::N);break;
        case 16:
            if (!PC.PlacePrompt().Contains(TEXT("chai")) || !PC.Destination()) return Fail(TEXT("contextual shop action or waypoint"));
            Tap(EKeys::E);break;
        case 17:
            if (PC.Discovered()<1) return Fail(TEXT("discovery persistence"));
            Shot(TEXT("tea-visit"));Tap(EKeys::P);break;
        case 18:
            if (FCString::Strcmp(District->WeatherName(),TEXT("Clear"))!=0) return Fail(TEXT("weather cycle"));
            if (!Walker->TeleportTo(FVector(-5170,3210,99),FRotator(0,90,0))) return Fail(TEXT("crosswalk viewpoint"));
            PC.SetControlRotation(FRotator(0,90,0));break;
        case 19:
        {
            int32 Crossed=0,Shopped=0;
            for (TActorIterator<ARiverfrontPedestrian> P(PC.GetWorld());P;++P)
            { Crossed+=P->CrossingsCompleted(); Shopped+=P->ShopVisits(); }
            if (!Shopped) return Fail(TEXT("no customer reached either counter"));
            if (!Crossed && Age<58) return true;
            if (!Crossed || Traffic->GetTravelMetres()<100. || !Traffic->GetStops() || !Traffic->GetTurns()) return Fail(TEXT("traffic stops, turns, or completed pedestrian crossing"));
            Shot(TEXT("crossing"));
            const FString Result=FString::Printf(TEXT("doors=1\ncockpit=1\nbonnet=1\nhazards=1\nfour_wheels=1\nbrake=1\nreverse=1\nwet_roads=1\nshop=1\nvisits=%d\ntraffic=%d\nstops=%d\nturn_samples=%d\ntraffic_metres=%.1f\ncrossings=%d\nshopper_visits=%d\n"),
                PC.Discovered(),Traffic->ActiveCount(),Traffic->GetStops(),Traffic->GetTurns(),Traffic->GetTravelMetres(),Crossed,Shopped);
            if (!Write(TEXT("systems.txt"),Result)) return Fail(TEXT("write review evidence"));
            break;
        }
        case 20:
        {
            const FVector Door=Car->GetActorTransform().TransformPosition(FVector(12,-174,37));
            if (!Walker->TeleportTo(Door,Car->GetActorRotation())) return Fail(TEXT("return to parked hatch"));
            Tap(EKeys::E); break;
        }
        case 21:
            if (PC.GetPawn()!=Car.Get()) return Fail(TEXT("challenge driver"));
            Tap(EKeys::G); break;
        case 22:
            if (!PC.NavigationText().Contains(TEXT("Gate"))) return Fail(TEXT("first trial gate"));
            Key(EKeys::W,IE_Pressed); break;
        case 23:
        {
            const FVector Here=Car->GetActorLocation();
            const double Remaining=19400.-Here.X;
            const double Velocity=Car->GetSpeedKph()/.036;
            if (Remaining>165.)
            {
                if (Age>84.)
                {
                    for (TActorIterator<ARiverfrontRoadAgent> It(PC.GetWorld());It;++It)
                        if (FVector::DistSquared2D(It->GetActorLocation(),Here)<FMath::Square(1000.))
                            UE_LOG(LogTemp,Display,TEXT("Trial traffic nearby: %s heading %.0f"),
                                *It->GetActorLocation().ToCompactString(),It->GetActorRotation().Yaw);
                    Shot(TEXT("trial-blocked"));
                    return Fail(*FString::Printf(TEXT("trial drive stalled at %.0f, %.0f, yaw %.1f; %s"),
                        Here.X,Here.Y,Car->GetActorRotation().Yaw,*PC.NavigationText()));
                }
                // Follow the northbound left-hand lane, including its signal and
                // traffic queue. Oncoming vehicles use the other side of the bank road.
                constexpr double LaneY=4090.-222.;
                const double Aim=FMath::RadiansToDegrees(FMath::Atan2(LaneY-Here.Y,FMath::Max(620.,Velocity*.91)));
                const double Error=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Aim);
                TrialSteer(Error>2.4?1:Error< -2.4?-1:0);
                const double Limit=FMath::Abs(Here.X-3150.)<1000.?590.:1280.;
                double SafeSpeed=FMath::Clamp(FMath::Sqrt(700.*(Remaining-165.)),190.,Limit);
                bool bHold=false;
                const double ToStopLine=3150.-845.-Here.X;
                bool bYieldAtBridge=!Traffic->BankTrafficHasGreen();
                for (TActorIterator<ARiverfrontRoadAgent> It(PC.GetWorld());It;++It)
                {
                    const FVector Route=It->GetActorForwardVector();
                    const FVector Other=It->GetActorLocation();
                    const double ProjectedHalfWidth=FMath::Abs(Route.Y)*It->HalfLength+FMath::Abs(Route.X)*It->HalfWidth;
                    const double ProjectedHalfLength=FMath::Abs(Route.X)*It->HalfLength+FMath::Abs(Route.Y)*It->HalfWidth;
                    const double Gap=Other.X-Here.X-ProjectedHalfLength-212.;
                    const double FutureY=FMath::Lerp(Here.Y,LaneY,FMath::Clamp(Gap/900.,0.,1.));
                    // Look across the bridge approach too. A turning driver can
                    // cross the trial before their present footprint touches it.
                    if (ToStopLine>=0. && ToStopLine<2400. && FMath::Abs(Other.X-3150.)<770.)
                    {
                        const FVector Motion=Traffic->RoadVelocity(*It);
                        const double LaterY=Other.Y+Motion.Y*2.8;
                        const double Margin=ProjectedHalfWidth+138.;
                        if (FMath::Abs(Route.Y)>.2 && FMath::Min(Other.Y,LaterY)<LaneY+Margin
                            && FMath::Max(Other.Y,LaterY)>LaneY-Margin)
                            bYieldAtBridge=true;
                    }
                    if (Other.X>Here.X && Gap<2300. && FMath::Abs(Other.Y-FutureY)<ProjectedHalfWidth+97.)
                    {
                        SafeSpeed=FMath::Min(SafeSpeed,FMath::Sqrt(710.*FMath::Max(0.,Gap-145.)));
                        bHold|=Gap<170.;
                    }
                }
                if (bYieldAtBridge && ToStopLine>=0. && ToStopLine<2400.)
                {
                    SafeSpeed=FMath::Min(SafeSpeed,FMath::Sqrt(620.*FMath::Max(0.,ToStopLine-55.)));
                    bHold|=ToStopLine<75.;
                }
                const bool bBrake=bHold || Velocity>SafeSpeed+(bTrialBraking?-50.:65.);
                if (bBrake!=bTrialBraking)
                {
                    Key(bBrake?EKeys::W:EKeys::SpaceBar,IE_Released);
                    Key(bBrake?EKeys::SpaceBar:EKeys::W,IE_Pressed);
                    bTrialBraking=bBrake;
                }
                return true;
            }
            TrialSteer(0);Key(EKeys::W,IE_Released);Key(EKeys::SpaceBar,IE_Pressed);break;
        }
        case 24:
            if (Car->GetSpeedKph()>.5f) return true;
            if (!PC.ActivityText().Contains(TEXT("finished"))) return Fail(*FString::Printf(TEXT("finish line X %.0f; %s"),
                Car->GetActorLocation().X,*PC.NavigationText()));
            Key(EKeys::SpaceBar,IE_Released);
            Shot(TEXT("finish-line")); break;
        case 25:
            if (!Car->TeleportTo(FVector(-14100,5380,72),FRotator::ZeroRotator)) return Fail(TEXT("forecourt access"));
            break;
        case 26:
            if (!PC.PlacePrompt().Contains(TEXT("Refuel")) || Car->GetFuel()>=1.f) return Fail(TEXT("fuel consumed and service pump available"));
            Tap(EKeys::F); break;
        case 27:
        {
            float Record=0.f;
            if (Car->GetFuel()<.99999f || Car->GetCondition()<.99999f || !PC.VerifyReviewProfile(Record))
                return Fail(TEXT("fuel, repair and reload local river run record"));
            if (!Write(TEXT("exploration.txt"),FString::Printf(TEXT("gates=4\nriver_run_seconds=%.2f\nrefuelled=1\nrepaired=1\nreloaded_visits=2\n"),Record)))
                return Fail(TEXT("write exploration results"));
            Tap(EKeys::R); break;
        }
        case 28: Tap(EKeys::E);break;
        case 29:
            if (PC.GetPawn()!=Walker.Get()) return Fail(TEXT("release service vehicle"));
            if (!Minutes) return Finish();
            Stage=99; EnduranceStart=LastFrame=Now;LapStart=0.;Leg=-1;
            if (!Write(TEXT("endurance.csv"),TEXT("elapsed_s,minute,leg,wall_ms,game_ms,render_ms,gpu_ms,focused\n"))) return Fail(TEXT("open endurance file"));
            return true;
        default: return Fail(TEXT("unknown review stage"));
        }
        ++Stage; StageTime=Now;return true;
    }
private:
    void TrialSteer(int32 Direction)
    {
        if (Direction==TrialDirection) return;
        if (TrialDirection!=0) Key(TrialDirection<0?EKeys::A:EKeys::D,IE_Released);
        TrialDirection=Direction;
        if (TrialDirection!=0) Key(TrialDirection<0?EKeys::A:EKeys::D,IE_Pressed);
    }
    void Key(FKey K,EInputEvent E)
    {
        PC.InputKey(FInputKeyEventArgs(PC.GetWorld()->GetGameViewport()->Viewport,
            FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));
    }
    void Tap(FKey K) { Key(K,IE_Pressed);Pending=K; }
    void TapWeather() { Key(EKeys::H,IE_Released);Tap(EKeys::P); }
    void Shot(const TCHAR* Name) {FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,FString(Name)+TEXT(".png")),false,false);}
    bool Write(const TCHAR* Name,const FString& Data) const
    {return FFileHelper::SaveStringToFile(Data,*FPaths::Combine(Folder,Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
    bool Fail(const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("Sabarmati systems review failed in stage %d: %s"),Stage,Why);
        FPlatformMisc::RequestExitWithStatus(false,1);return false;
    }
    bool Finish()
    {
        UE_LOG(LogTemp,Display,TEXT("Sabarmati systems review complete: %s"),*Folder);FPlatformMisc::RequestExit(false);return false;
    }
    bool Endurance(double Now)
    {
        const double Age=Now-EnduranceStart;
        if (Age>=Minutes*60.)
        {
            if (!Flush() || !Write(TEXT("endurance.txt"),FString::Printf(TEXT("duration_s=%.3f\nframes=%d\nloops=%d\nwalk_metres=%.2f\ndrive_metres=%.2f\n"),Age,Count,Laps,WalkCm/100.,DriveCm/100.)))
                return Fail(TEXT("write endurance summary"));
            if (WalkCm<1000. || DriveCm<3500. || Laps<1) return Fail(TEXT("insufficient endurance traversal"));
            return Finish();
        }
        const double Wall=(Now-LastFrame)*1000.;LastFrame=Now;
        Buffer.Appendf(TEXT("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%d\n"),Age,int32(Age/60),Leg,Wall,
            FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()), FApp::HasFocus());
        if (++Count%450==0 && !Flush()) return Fail(TEXT("flush endurance frames"));
        if (Leg<0)
        {
            StartWalk(FVector(-65550,2360,13),FRotator::ZeroRotator);
            Leg=0;LapStart=Now;Previous=Walker->GetActorLocation();++Laps;
        }
        else if (Leg==0 || Leg==1)
        {
            const FVector Direction=Leg==0?FVector(1,0,0):FVector(0,-1,0);
            Walker->AddMovementInput(Direction,1.f,true);
            const FVector Here=Walker->GetActorLocation();WalkCm+=FVector::Dist2D(Here,Previous);Previous=Here;
            if (Now-LapStart>(Leg==0?12.:16.))
            {
                if (Leg==0)
                {
                    StartWalk(FVector(-5350,2110,120),FRotator(0,-90,0));Leg=1;LapStart=Now;Previous=Walker->GetActorLocation();
                }
                else
                {
                    const FVector Door=Car->GetActorTransform().TransformPosition(FVector(12,-174,37));
                    if (!Walker->TeleportTo(Door,Car->GetActorRotation())) return Fail(TEXT("repeat door placement"));
                    Car->Enter(*Walker.Get());
                    if (PC.GetPawn()!=Car.Get()) return Fail(TEXT("repeat vehicle entry"));
                    PC.SetViewTarget(Car.Get());Key(EKeys::W,IE_Pressed);
                    Leg=2;LapStart=Now;Previous=Car->GetActorLocation();
                }
            }
        }
        else if (Leg==2)
        {
            const FVector Here=Car->GetActorLocation();DriveCm+=FVector::Dist2D(Here,Previous);Previous=Here;
            if (Now-LapStart>14.) { Key(EKeys::W,IE_Released);Tap(EKeys::R);Leg=3;LapStart=Now; }
        }
        else if (Leg==3 && Now-LapStart>.75)
        {
            if (Car->GetSpeedKph()>.8) return Fail(TEXT("endurance reset"));
            Tap(EKeys::E);Leg=4;LapStart=Now;
        }
        else if (Leg==4 && Now-LapStart>.8)
        {
            if (PC.GetPawn()!=Walker.Get()) return Fail(TEXT("endurance exit"));
            Leg=-1;
        }
        return true;
    }
    void StartWalk(FVector Point,FRotator Heading)
    {
        Walker->GetCharacterMovement()->StopMovementImmediately();
        Walker->TeleportTo(Point,Heading);PC.SetViewTarget(Walker.Get());PC.SetControlRotation(Heading);
    }
    bool Flush()
    {
        if (Buffer.IsEmpty()) return true;
        const bool OK=FFileHelper::SaveStringToFile(Buffer,*FPaths::Combine(Folder,TEXT("endurance.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
        Buffer.Reset();return OK;
    }
    ARiverfrontPlayerController& PC;
    TWeakObjectPtr<ARiverfrontWorld> District;TWeakObjectPtr<ARiverfrontCar> Car;
    TWeakObjectPtr<ARiverfrontCharacter> Walker;TWeakObjectPtr<ARiverfrontTraffic> Traffic;
    FString Folder,Buffer;FVector Origin,Previous;FKey Pending;
    double StartTime=0,StageTime=0,EnduranceStart=0,LastFrame=0,LapStart=0,WalkCm=0,DriveCm=0;
    bool bTrialBraking=false;
    int32 Minutes=0,Stage=-1,Leg=0,Laps=0,Count=0,TrialDirection=0;
};
}
void ARiverfrontPlayerController::RunSystemsReview()
{
    TSharedRef<FSystemsReview> Review=MakeShared<FSystemsReview>(*this);
    if (Review->Start()) FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[Review](float){return Review->Tick();}));
}
#endif
