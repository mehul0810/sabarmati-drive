#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontCar.h"
#include "RiverfrontWorld.h"
#include "EngineUtils.h"
#include "Components/SpotLightComponent.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "PipelineFileCache.h"
#include "PipelineStateCache.h"
#include "RenderTimer.h"
#include "ShaderPipelineCache.h"
#include "UnrealClient.h"

namespace
{
struct FBenchmarkView
{
    const TCHAR* Name;
    FVector Position;
    FRotator Rotation;
    float FieldOfView;
};

const FBenchmarkView Views[] = {
    {TEXT("atal-landing"), FVector(-3650, 2355, 94), FRotator(7, -128, 0), 80.f},
    {TEXT("shopfront"), FVector(-5450, 3790, 161), FRotator(4, 50, 0), 70.f},
    {TEXT("parked-hatch"), FVector(-3750, 3670, 145), FRotator(-7, 147, 0), 61.f},
    {TEXT("tea-room"), FVector(-3505, 5890, 158), FRotator(-5, 118, 0), 77.f},
    {TEXT("garden-walk"), FVector(-3050, 4170, 165), FRotator(-2, -151, 0), 76.f},
    {TEXT("service-lane"), FVector(9890, 6405, 162), FRotator(6, 141, 0), 76.f},
    {TEXT("road-surface"), FVector(-80310, 4065, 126), FRotator(-13, 15, 0), 70.f}
};

class FRiverfrontBenchmark
{
public:
    explicit FRiverfrontBenchmark(ARiverfrontPlayerController& InPlayer) : Player(InPlayer) {}

    bool Start(int32 GraphicsPreset)
    {
        UWorld* World = Player.GetWorld();
        const auto* Mode = World->GetAuthGameMode<ARiverfrontGameMode>();
        if (!Mode || Mode->IsCityMode() || !Mode->GetCar())
        {
            return Fail(TEXT("The benchmark requires the riverfront district and a parked hatchback."));
        }
        Car = Mode->GetCar();
        Camera = World->SpawnActor<ACameraActor>();
        if (!Camera.IsValid())
        {
            return Fail(TEXT("Could not create the benchmark camera."));
        }
        const FString ID = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT("-")
            + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
        Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RenderingBenchmarks"), ID));
        if (!IFileManager::Get().MakeDirectory(*Directory, true))
        {
            return Fail(TEXT("Could not create the results directory."));
        }
        TActorIterator<ARiverfrontWorld> District{World};
        if (!District)
        {
            return Fail(TEXT("Missing lighting state."));
        }
        const TCHAR* TimeName = FParse::Param(FCommandLine::Get(), TEXT("SabarmatiLightingReference"))
            ? TEXT("reference") : District->TimeName();
        // The standard run uses native pixels. An opted-in preset uses the same
        // scene scale as the Graphics menu while keeping the drawable and HUD fixed.
        for (const TPair<const TCHAR*,int32>& Setting : {
            TPair<const TCHAR*,int32>{TEXT("r.SecondaryScreenPercentage.GameViewport"), 100},
            {TEXT("r.DynamicRes.OperationMode"), 0}, {TEXT("r.VSync"), 0}, {TEXT("t.IdleWhenNotForeground"), 0}})
        {
            if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(Setting.Key))
            {
                Variable->Set(Setting.Value, ECVF_SetByCode);
            }
        }
        if (GraphicsPreset == INDEX_NONE)
        {
            if (auto* Scale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
                Scale->Set(100, ECVF_SetByCode);
        }
        auto* Viewport = World->GetGameViewport()->Viewport;
        const FIntPoint Size = Viewport->GetSizeXY();
        FString Info = FString::Printf(TEXT("format=3\ntime=%s\nviewport=%dx%d\nwindowed=%d\nengine=%s\ngpu=%s\n"),
            TimeName, Size.X, Size.Y, Viewport->GetWindowMode() == EWindowMode::Windowed,
            *FEngineVersion::Current().ToString(), *FPlatformMisc::GetPrimaryGPUBrand());
        Info += FString::Printf(TEXT("frame_smoothing=%d\n"), GEngine && GEngine->IsAllowedFramerateSmoothing());
        Info += FString::Printf(TEXT("graphics_preset=%d\n"), GraphicsPreset);
        Info += FString::Printf(TEXT("pso_precache_supported=%d\npso_precache_enabled=%d\npso_file_cache_supported=%d\n")
            TEXT("pso_file_cache_enabled=%d\npso_precompiles_outstanding=%u\n"),
            GRHISupportsPSOPrecaching, PipelineStateCache::IsPSOPrecachingEnabled(),
            GRHISupportsPipelineFileCache, FPipelineFileCacheManager::IsPipelineFileCacheEnabled(),
            FShaderPipelineCache::NumPrecompilesRemaining());
        for (const TCHAR* Key : {TEXT("r.ScreenPercentage"), TEXT("r.SecondaryScreenPercentage.GameViewport"), TEXT("r.AntiAliasingMethod"), TEXT("r.VSync"),
            TEXT("r.DynamicRes.OperationMode"), TEXT("r.ReflectionMethod"), TEXT("r.DynamicGlobalIlluminationMethod"),
            TEXT("sg.ShadowQuality"), TEXT("sg.PostProcessQuality"), TEXT("sg.ReflectionQuality"), TEXT("t.MaxFPS"), TEXT("t.IdleWhenNotForeground")})
        {
            if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Key))
            {
                Info += FString::Printf(TEXT("%s=%s\n"), Key, *Var->GetString());
            }
        }
        if (!Write(TEXT("settings.txt"), Info))
        {
            return Fail(TEXT("Could not save benchmark settings."));
        }
        UE_LOG(LogTemp, Display, TEXT("Sabarmati benchmark started: %s"), *Directory);
        return BeginView();
    }

    bool Tick()
    {
        const double Now = FPlatformTime::Seconds();
        if (bTestingTime)
        {
            return CheckTimeInput(Now);
        }
        if (Now - Started > 40.0)
        {
            return Fail(TEXT("The current view exceeded its time limit."));
        }
        if (bWaitingForScreenshot)
        {
            if (IFileManager::Get().FileSize(*ScreenshotPath) <= 0)
            {
                return true;
            }
            bWaitingForScreenshot = false;
            if (++ViewIndex <= UE_ARRAY_COUNT(Views))
            {
                return BeginView();
            }
            if (!Write(TEXT("frames.csv"), Frames))
            {
                return Fail(TEXT("Could not write the frame timings."));
            }
            bTestingTime = true;
            return StartTimeInput(Now);
        }
        const bool Driving = ViewIndex == UE_ARRAY_COUNT(Views);
        if (!Driving && Now - Started < 3.0)
        {
            Last = Now;
            return true;
        }
        const double FrameMs = (Now - Last) * 1000.;
        Last = Now;
        Frames += FString::Printf(TEXT("%s,%d,%.3f,%.3f,%.3f,%.3f\n"), Name(), Sample++, FrameMs,
            FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
            FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
        if (Driving)
        {
            if (!Car.IsValid() || Player.GetPawn() != Car.Get())
            {
                return Fail(TEXT("Lost possession during the drive."));
            }
            const FVector At = Car->GetActorLocation();
            const FCollisionQueryParams Query(SCENE_QUERY_STAT(BenchmarkRoad), false, Car.Get());
            FHitResult Floor;
            const bool Grounded = Player.GetWorld()->LineTraceSingleByChannel(Floor, At, At-FVector(0,0,140), ECC_Visibility, Query)
                && Floor.ImpactNormal.Z > .65 && At.Z-Floor.ImpactPoint.Z < 100.;
            if (!Grounded)
            {
                return Fail(TEXT("The car lost road support during the drive."));
            }
            const double Meters=FVector::Dist2D(DriveOrigin, At)/100.;
            if (Meters < 45.) return true;
            Key(EKeys::W, IE_Released);
            if (Car->GetSpeedKph()<5 || Sample<30 || Now-Started<2.5)
            {
                return Fail(TEXT("The chase run was too short or the car stopped."));
            }
            const FString Drive=FString::Printf(TEXT("distance_m=%.2f\nspeed_kph=%.2f\nroad_contact=1\ncontact_samples=%d\nduration_s=%.3f\n"),
                Meters, Car->GetSpeedKph(), Sample, Now-Started);
            if (!Write(TEXT("drive.txt"), Drive))
            {
                return Fail(TEXT("Could not save driving evidence."));
            }
        }
        else if (Sample < 180)
        {
            return true;
        }
        ScreenshotPath = FPaths::Combine(Directory, FString(Name()) + TEXT(".png"));
        FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
        bWaitingForScreenshot = true;
        return true;
    }

private:
    const TCHAR* Name() const { return ViewIndex < UE_ARRAY_COUNT(Views) ? Views[ViewIndex].Name : TEXT("driving"); }

    bool Write(const TCHAR* File, const FString& Contents) const
    {
        return FFileHelper::SaveStringToFile(Contents, *FPaths::Combine(Directory, File), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool Fail(const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("Sabarmati benchmark failed: %s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return false;
    }

    void Key(FKey Button, EInputEvent Event)
    {
        Player.InputKey(FInputKeyEventArgs(Player.GetWorld()->GetGameViewport()->Viewport,
            FInputDeviceId::CreateFromInternalId(0), Button, Event, FPlatformTime::Cycles64()));
    }

    bool StartTimeInput(double Now)
    {
        TActorIterator<ARiverfrontWorld> World{Player.GetWorld()};
        if (!World)
        {
            return Fail(TEXT("District disappeared during the input check."));
        }
        const FString Prior = World->TimeName();
        ExpectedTime = Prior == TEXT("golden") ? TEXT("night") : Prior == TEXT("day") ? TEXT("golden") : TEXT("day");
        Started = Now;
        Key(EKeys::T, IE_Pressed);
        return true;
    }

    bool CheckTimeInput(double Now)
    {
        if (Now - Started < .15)
        {
            return true;
        }
        if (bBetweenKeyPresses)
        {
            bBetweenKeyPresses = false;
            return StartTimeInput(Now);
        }
        TActorIterator<ARiverfrontWorld> World{Player.GetWorld()};
        if (!World || ExpectedTime != World->TimeName())
        {
            if (Now - Started < 2.)
            {
                return true;
            }
            return Fail(TEXT("The bound T key did not advance district lighting."));
        }
        Key(EKeys::T, IE_Released);
        TInlineComponentArray<USpotLightComponent*> Lamps(Car.Get());
        int32 Visible = 0;
        for (const auto* Light : Lamps)
        {
            Visible += Light->IsVisible();
        }
        if (Visible != (World->IsNight() ? 2 : 0))
        {
            return Fail(TEXT("Incorrect occupied-car headlight state."));
        }
        InputChecks += FString::Printf(TEXT("%s: %d vehicle beams\n"), *ExpectedTime, Visible);
        if (++Checked < 3)
        {
            bBetweenKeyPresses = true;
            Started = Now;
            return true;
        }
        if (!Write(TEXT("input.txt"), InputChecks))
        {
            return Fail(TEXT("Could not write the input check."));
        }
        UE_LOG(LogTemp, Display, TEXT("Sabarmati benchmark complete: %s"), *Directory);
        FPlatformMisc::RequestExit(false);
        return false;
    }

    bool BeginView()
    {
        Sample = 0;
        Started = Last = FPlatformTime::Seconds();
        const bool Driving = ViewIndex == UE_ARRAY_COUNT(Views);
        if (AHUD* HUD = Player.GetHUD())
        {
            HUD->bShowHUD = Driving;
        }
        if (Driving)
        {
            ARiverfrontCharacter* Walker = Cast<ARiverfrontCharacter>(Player.GetPawn());
            if (!Walker || !Car.IsValid())
            {
                return Fail(TEXT("Character or hatchback unavailable."));
            }
            const FVector Door = Car->GetActorTransform().TransformPosition(FVector(12, -174, 37));
            if (!Walker->TeleportTo(Door, Car->GetActorRotation()) || !Car->IsNearDoor(*Walker))
            {
                return Fail(TEXT("The parked hatchback door is obstructed."));
            }
            Car->Enter(*Walker);
            if (Player.GetPawn() != Car.Get())
            {
                return Fail(TEXT("Entering the hatchback failed."));
            }
            Player.SetViewTarget(Car.Get());
            DriveOrigin = Car->GetActorLocation();
            Key(EKeys::W, IE_Pressed);
        }
        else
        {
            const FBenchmarkView& View = Views[ViewIndex];
            Camera->SetActorLocationAndRotation(View.Position, View.Rotation);
            Camera->GetCameraComponent()->SetFieldOfView(View.FieldOfView);
            Player.SetViewTarget(Camera.Get());
        }
        return true;
    }

    ARiverfrontPlayerController& Player;
    TWeakObjectPtr<ARiverfrontCar> Car;
    TWeakObjectPtr<ACameraActor> Camera;
    FString Frames = TEXT("view,frame,wall_ms,game_ms,render_ms,gpu_ms\n");
    FString Directory, ScreenshotPath;
    FVector DriveOrigin = FVector::ZeroVector;
    double Started = 0., Last = 0.;
    FString ExpectedTime, InputChecks;
    int32 ViewIndex = 0, Sample = 0, Checked = 0;
    bool bTestingTime = false, bBetweenKeyPresses = false;
    bool bWaitingForScreenshot = false;
};
}

void ARiverfrontPlayerController::RunRenderingBenchmark()
{
    if (!HasLocalProfile() && !CreateLocalProfile(TEXT("Render Review")).IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Sabarmati benchmark could not create its isolated QA profile."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    ResumeDrive();
    TSharedRef<FRiverfrontBenchmark> Run = MakeShared<FRiverfrontBenchmark>(*this);
    if (Run->Start(ReviewGraphicsPreset))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [Run](float)
        {
            return Run->Tick();
        }));
    }
}
#endif
