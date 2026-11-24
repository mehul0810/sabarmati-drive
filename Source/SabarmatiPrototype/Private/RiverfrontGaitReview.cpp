#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontCharacter.h"
#include "RiverfrontLifeProfile.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
struct FLegPose
{
    double Flex = 0.;
    double Clearance = 0.;
    FVector Sole = FVector::ZeroVector;
};

struct FGaitFrame
{
    double Seconds = 0.;
    FVector Centre = FVector::ZeroVector;
    double Speed = 0.;
    FLegPose Legs[2];
};

struct FGaitStrip
{
    int32 Mode = 0;
    double Started = 0.;
    bool bPressed = false;
    bool bPhotographed = false;
    bool bMetricsWritten = false;
    double LastCapture = -1.;
    int32 CaptureCount = 0;
    FString FrameTimes;
    double CaptureStart = 0.;
    bool bClockWasFixed = false;
    double OriginalDelta = 0.;
    TArray<FGaitFrame> Samples;
    TWeakObjectPtr<ACameraActor> Lens;
    FString Summary = TEXT("mode,side,frames,seconds,distance_cm,speed_cm_s,knee_min_deg,knee_max_deg,swings,plants,max_plant_drift_cm,min_sole_above_floor_cm,lateral_cm,pass\n");
};

bool ChooseStrip(UWorld& World,const AActor& Walker,FVector& Start)
{
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(GaitStrip),false,&Walker);
    const FCollisionObjectQueryParams Static(ECC_WorldStatic);
    // Check the full 34 m run and the width of the capsule. Planters, curbs,
    // benches and railings all appear along otherwise level stretches.
    for (const double X: {-2900.,-4100.,12300.,-25000.})
    for (const double Y: {3100.,3150.,3040.,3240.})
    {
        bool bClear=true;
        for (int32 Along=0;Along<=3400 && bClear;Along+=40)
        {
            const FVector Centre(X+Along,Y,2.);
            for (const double Width: {-32.,0.,32.})
            {
                const FVector P=Centre+FVector(0.,Width,0.);
                FHitResult Hit;
                if (!World.LineTraceSingleByObjectType(Hit,P+FVector(0.,0.,80.),P-FVector(0.,0.,15.),Static,Query)
                    || FMath::Abs(Hit.ImpactPoint.Z-2.)>1.5 || Hit.ImpactNormal.Z<.99)
                { bClear=false; break; }
            }
            const FVector Lens=Centre+FVector(0.,-445.,112.);
            for (const double Height: {10.,54.,155.})
                if (World.LineTraceTestByChannel(Lens,Centre+FVector(0.,0.,Height),ECC_Visibility,Query))
                { bClear=false; break; }
        }
        if (bClear) { Start=FVector(X,Y,100.); return true; }
    }
    return false;
}

bool MeasureLeg(const TArray<FGaitFrame>& Samples,int32 Side,bool bRun,FString& Report)
{
    if (Samples.Num()<2) return false;
    double LowKnee=180., HighKnee=0., FloorZ=DBL_MAX, Clearance=DBL_MAX;
    for (const auto& Frame:Samples)
    {
        LowKnee=FMath::Min(LowKnee,Frame.Legs[Side].Flex);
        HighKnee=FMath::Max(HighKnee,Frame.Legs[Side].Flex);
        FloorZ=FMath::Min(FloorZ,Frame.Legs[Side].Sole.Z);
        Clearance=FMath::Min(Clearance,Frame.Legs[Side].Clearance);
    }
    const double SwingAngle=bRun?70.:45.;
    int32 Swings=0, Plants=0, PlantFrames=0;
    bool bExtended=false;
    FVector PlantStart=FVector::ZeroVector;
    double PlantTravel=0., MaxDrift=0., PlantStartTime=0.;
    // Ignore partial contacts at the two recording boundaries. Define a contact by
    // sole height alone so a sliding foot cannot be counted as stable by definition.
    bool bWasAirborne=false;
    for (const auto& Frame:Samples)
    {
        const auto& Leg=Frame.Legs[Side];
        if (Leg.Flex<22.) bExtended=true;
        if (bExtended && Leg.Flex>SwingAngle) { ++Swings; bExtended=false; }
        if (Leg.Sole.Z<=FloorZ+2.5)
        {
            if (!bWasAirborne) continue;
            if (PlantFrames++==0)
            {
                PlantStart=Leg.Sole;
                PlantStartTime=Frame.Seconds;
                PlantTravel=0.;
            }
            PlantTravel=FMath::Max(PlantTravel,FVector::Dist2D(PlantStart,Leg.Sole));
        }
        else
        {
            if (PlantFrames>=2 && Frame.Seconds-PlantStartTime>=.045)
            {
                ++Plants;
                MaxDrift=FMath::Max(MaxDrift,PlantTravel);
            }
            bWasAirborne=true;
            PlantFrames=0;
        }
    }
    const double Elapsed=Samples.Last().Seconds-Samples[0].Seconds;
    const double Distance=FVector::Dist2D(Samples[0].Centre,Samples.Last().Centre);
    const double Speed=Distance/FMath::Max(.001,Elapsed);
    const double Expected=bRun?335.:145.;
    double LateralTravel=0.;
    for (const auto& Frame:Samples)
        LateralTravel=FMath::Max(LateralTravel,FMath::Abs(Frame.Centre.Y-Samples[0].Centre.Y));
    const bool bPass=Samples.Num()>=45 && Elapsed>=4.8 && LateralTravel<3. && FMath::Abs(Speed-Expected)<Expected*.05
        && HighKnee-LowKnee>=(bRun?65.:40.) && Swings>=3 && Plants>=3 && MaxDrift<=4. && FMath::Abs(Clearance)<=3.;
    Report+=FString::Printf(TEXT("%s,%s,%d,%.3f,%.2f,%.2f,%.2f,%.2f,%d,%d,%.2f,%.2f,%.2f,%d\n"),
        bRun?TEXT("run"):TEXT("walk"),Side==0?TEXT("left"):TEXT("right"),Samples.Num(),Elapsed,Distance,Speed,
        LowKnee,HighKnee,Swings,Plants,MaxDrift,Clearance,LateralTravel,bPass);
    return bPass;
}
}

void ARiverfrontPlayerController::RunGaitReview()
{
    const FString Id=ProfileSlot.RightChop(12);
    FGuid Guid;
    const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("GaitReviews"),Id));
    if (!ProfileSlot.StartsWith(TEXT("SabarmatiQA-")) || !FGuid::ParseExact(Id,EGuidFormats::Digits,Guid)
        || HasLocalProfile() || !IFileManager::Get().MakeDirectory(*Folder,true)
        || !CreateLocalProfile(TEXT("Gait Review")).IsEmpty())
    {
        UE_LOG(LogTemp,Error,TEXT("Gait review requires a new isolated QA profile and evidence directory."));
        FPlatformMisc::RequestExitWithStatus(false,1);
        return;
    }
    ResumeDrive();
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    if (!Walker || !Walker->GetMesh() || !Walker->GetMesh()->GetSkeletalMeshAsset())
    {
        UE_LOG(LogTemp,Error,TEXT("Gait review did not find the playable skeletal mesh."));
        FPlatformMisc::RequestExitWithStatus(false,1);
        return;
    }
    const bool bFemale = FParse::Param(FCommandLine::Get(),TEXT("SabarmatiGaitFemale"));
    if (bFemale)
    {
        const FText Error = RiverfrontLife::SetAvatarGender(ProfileSlot, ERiverfrontGender::Female);
        if (!Error.IsEmpty())
        {
            UE_LOG(LogTemp,Error,TEXT("Gait review could not select female avatar: %s"),*Error.ToString());
            FPlatformMisc::RequestExitWithStatus(false,1);
            return;
        }
        RefreshLifeAppearance();
        if (Walker->GetMesh()->GetSkeletalMeshAsset()->GetFName() != TEXT("SK_FemaleEveryday"))
        {
            UE_LOG(LogTemp,Error,TEXT("Gait review could not load female everyday mesh."));
            FPlatformMisc::RequestExitWithStatus(false,1);
            return;
        }
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("SabarmatiGaitKurta")))
    {
        Walker->ApplyWardrobeLook(TEXT("kurta"),NAME_None);
        if (Walker->GetMesh()->GetSkeletalMeshAsset()->GetFName() != (bFemale ? TEXT("SK_FemaleKurta") : TEXT("SK_FittedKurta")))
        {
            UE_LOG(LogTemp,Error,TEXT("Gait review could not switch to the fitted kurta."));
            FPlatformMisc::RequestExitWithStatus(false,1);
            return;
        }
    }
    auto State=MakeShared<FGaitStrip>();
    auto* Skin=Walker->GetMesh();
    const auto& Rig=Skin->GetSkeletalMeshAsset()->GetRefSkeleton();
    TArray<FVector> SoleOffsets,HeelOffsets,ToeOffsets;
    for (const FName Name: {FName(TEXT("Footl")),FName(TEXT("Footr"))})
    {
        const int32 Bone=Rig.FindBoneIndex(Name);
        if (Bone==INDEX_NONE)
        {
            UE_LOG(LogTemp,Error,TEXT("Missing gait review foot bone %s"),*Name.ToString());
            FPlatformMisc::RequestExitWithStatus(false,1); return;
        }
        // Ball of the leather shoe, expressed from the reference ankle in mesh centimetres.
        const FTransform Ref=Rig.GetBoneAbsoluteTransform(Bone);
        const double Inward=Ref.GetLocation().X>0.?-1.:1.;
        SoleOffsets.Add(Ref.InverseTransformVector(FVector(4.*Inward,10.5,-8.0)));
        HeelOffsets.Add(Ref.InverseTransformVector(FVector(4.*Inward,-3.5,-7.3)));
        ToeOffsets.Add(Ref.InverseTransformVector(FVector(4.*Inward,16.,-7.9)));
    }
    State->Lens=GetWorld()->SpawnActor<ACameraActor>();
    if (!State->Lens.IsValid()) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
    State->Lens->GetCameraComponent()->SetFieldOfView(48.);
    // Mount the side camera to the capsule so camera and mesh advance together
    // even when a screenshot blocks rendering. The player still moves through W and Shift.
    State->Lens->AttachToActor(Walker,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    State->Lens->SetActorRelativeLocation(FVector(0.,-445.,18.));
    State->Lens->SetActorRelativeRotation(FVector(0.,445.,-25.).Rotation());
    SetIgnoreLookInput(true);
    SetViewTarget(State->Lens.Get());
    if (GetHUD()) GetHUD()->bShowHUD=false;
    // The upper promenade has a clear side view between the bench and street-tree rows.
    FVector StripStart;
    if (!ChooseStrip(*GetWorld(),*Walker,StripStart))
    {
        UE_LOG(LogTemp,Error,TEXT("Gait review could not find an uninterrupted, visible, level promenade strip."));
        FPlatformMisc::RequestExitWithStatus(false,1); return;
    }
    UE_LOG(LogTemp,Display,TEXT("Gait review verified flat 34-metre strip: %s"),*StripStart.ToCompactString());
    auto ResetStrip=[this,Walker,StripStart]()
    {
        Walker->GetCharacterMovement()->StopMovementImmediately();
        const FRotator Direction=FRotator::ZeroRotator;
        SetControlRotation(Direction);
        return Walker->TeleportTo(StripStart,Direction);
    };
    if (!ResetStrip()) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
    UE_LOG(LogTemp,Display,TEXT("Sabarmati gait review started: mesh=%s evidence=%s"),
        *Skin->GetSkeletalMeshAsset()->GetPathName(),*Folder);
    const bool bCapture=FParse::Param(FCommandLine::Get(),TEXT("SabarmatiGaitFrames"));
    if (bCapture && !IFileManager::Get().MakeDirectory(*FPaths::Combine(Folder,TEXT("Frames")),true))
    { FPlatformMisc::RequestExitWithStatus(false,1); return; }
    State->Started=GetWorld()->GetTimeSeconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,State,Folder,SoleOffsets,HeelOffsets,ToeOffsets,bCapture,ResetStrip,Who=TWeakObjectPtr<ARiverfrontCharacter>(Walker),
         WallStart=FPlatformTime::Seconds(),bPassed=true](float) mutable
        {
            if (!Who.IsValid() || GetPawn()!=Who.Get() || !State->Lens.IsValid() || FPlatformTime::Seconds()-WallStart>180.)
            {
                UE_LOG(LogTemp,Error,TEXT("Gait review lost the actor or timed out."));
                FPlatformMisc::RequestExitWithStatus(false,1); return false;
            }
            const double Now=GetWorld()->GetTimeSeconds(),Age=Now-State->Started;
            if (State->Mode==2)
            {
                if (Age<.35 || FScreenshotRequest::IsScreenshotRequested()) return true;
                for (const TCHAR* File:{TEXT("walk-profile.png"),TEXT("run-profile.png")})
                    bPassed &= IFileManager::Get().FileSize(*FPaths::Combine(Folder,File))>0;
                UE_LOG(LogTemp,Display,TEXT("Sabarmati gait review finished: pass=%d %s\n%s"),bPassed,*Folder,*State->Summary);
                FPlatformMisc::RequestExitWithStatus(false,bPassed?0:1);
                return false;
            }
            const bool bRun=State->Mode==1;
            const auto Key=[&](FKey K,EInputEvent Action)
            {
                InputKey(FInputKeyEventArgs(GetWorld()->GetGameViewport()->Viewport,FInputDeviceId::CreateFromInternalId(0),
                    K,Action,FPlatformTime::Cycles64()));
            };
            const FVector Here=Who->GetActorLocation();
            SetControlRotation(FRotator::ZeroRotator);
            if (!State->bPressed && Age>.6)
            {
                if (bRun) Key(EKeys::LeftShift,IE_Pressed);
                Key(EKeys::W,IE_Pressed);
                State->bPressed=true;
            }
            if (State->bMetricsWritten)
            {
                const TCHAR* ModeName=bRun?TEXT("run"):TEXT("walk");
                if (bCapture && Now-State->CaptureStart<2.8)
                {
                    if (Now-State->LastCapture>=1./15.-.002 && !FScreenshotRequest::IsScreenshotRequested())
                    {
                        const FVector Eye=State->Lens->GetActorLocation();
                        FCollisionQueryParams Query(SCENE_QUERY_STAT(GaitCamera),false,Who.Get());
                        Query.AddIgnoredActor(State->Lens.Get());
                        for (const double Height:{-80.,-32.,65.})
                        {
                            if (GetWorld()->LineTraceTestByChannel(Eye,Here+FVector(0.,0.,Height),ECC_Visibility,Query))
                            {
                                UE_LOG(LogTemp,Error,TEXT("Gait profile camera obstructed near %s."),*Here.ToCompactString());
                                bPassed=false;
                            }
                        }
                        const FIntPoint Size=GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
                        for (const FName Bone:{FName(TEXT("Head")),FName(TEXT("Footl")),FName(TEXT("Footr"))})
                        {
                            FVector2D ScreenPoint;
                            if (!ProjectWorldLocationToScreen(Who->GetMesh()->GetSocketLocation(Bone),ScreenPoint)
                                || ScreenPoint.X<.23*Size.X || ScreenPoint.X>.77*Size.X
                                || ScreenPoint.Y<.04*Size.Y || ScreenPoint.Y>.95*Size.Y)
                            {
                                UE_LOG(LogTemp,Error,TEXT("Gait camera lost the framed joint %s."),*Bone.ToString());
                                bPassed=false;
                            }
                        }
                        const FString Name=FString::Printf(TEXT("%s-%03d.png"),ModeName,State->CaptureCount++);
                        State->FrameTimes+=FString::Printf(TEXT("%s,%.5f\n"),*Name,Now);
                        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,TEXT("Frames"),Name),false,false,false,FIntRect(),true);
                        State->LastCapture=Now;
                    }
                    return true;
                }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (bCapture)
                {
                    bPassed &= State->CaptureCount>=28;
                    for (int32 Index=0;Index<State->CaptureCount;++Index)
                    {
                        const FString Name=FString::Printf(TEXT("%s-%03d.png"),ModeName,Index);
                        bPassed &= IFileManager::Get().FileSize(*FPaths::Combine(Folder,TEXT("Frames"),Name))>0;
                    }
                    bPassed &= FFileHelper::SaveStringToFile(State->FrameTimes,
                        *FPaths::Combine(Folder,TEXT("Frames"),FString(ModeName)+TEXT("-times.csv")));
                    FApp::SetFixedDeltaTime(State->OriginalDelta);
                    FApp::SetUseFixedTimeStep(State->bClockWasFixed);
                    UE_LOG(LogTemp,Display,TEXT("Gait capture %s: %d rendered frames."),ModeName,State->CaptureCount);
                }
                Key(EKeys::W,IE_Released);
                if (bRun) Key(EKeys::LeftShift,IE_Released);
                ++State->Mode;
                State->bPhotographed=false;State->bPressed=false;State->bMetricsWritten=false;
                State->CaptureCount=0;State->LastCapture=-1.;State->FrameTimes.Reset();
                State->Started=Now;State->Samples.Reset();
                if (State->Mode==1 && !ResetStrip()) { FPlatformMisc::RequestExitWithStatus(false,1); return false; }
                return true;
            }
            if (Age<1.45 || (!State->Samples.IsEmpty() && Now<=State->Samples.Last().Seconds)) return true;
            FGaitFrame Frame;
            Frame.Seconds=Now;
            Frame.Centre=Here;
            Frame.Speed=Who->GetVelocity().Size2D();
            auto* Body=Who->GetMesh();
            for (int32 Side=0;Side<2;++Side)
            {
                const FName Hip=Side==0?TEXT("Hipl"):TEXT("Hipr");
                const FName Knee=Side==0?TEXT("Kneel"):TEXT("Kneer");
                const FName Foot=Side==0?TEXT("Footl"):TEXT("Footr");
                const FVector H=Body->GetSocketLocation(Hip),K=Body->GetSocketLocation(Knee);
                const FTransform F=Body->GetSocketTransform(Foot);
                Frame.Legs[Side].Flex=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
                    FVector::DotProduct((K-H).GetSafeNormal(),(F.GetLocation()-K).GetSafeNormal()),-1.,1.)));
                Frame.Legs[Side].Sole=F.TransformPosition(SoleOffsets[Side]);
                Frame.Legs[Side].Clearance=DBL_MAX;
                const FCollisionQueryParams Query(SCENE_QUERY_STAT(GaitFootGround),false,Who.Get());
                for (const FVector Probe:{Frame.Legs[Side].Sole,F.TransformPosition(HeelOffsets[Side]),
                    F.TransformPosition(ToeOffsets[Side])})
                {
                    FHitResult Ground;
                    if (!GetWorld()->LineTraceSingleByObjectType(Ground,Probe+FVector(0.,0.,50.),
                        Probe-FVector(0.,0.,120.),FCollisionObjectQueryParams(ECC_WorldStatic),Query))
                    { UE_LOG(LogTemp,Error,TEXT("Gait shoe left the pavement.")); FPlatformMisc::RequestExitWithStatus(false,1); return false; }
                    Frame.Legs[Side].Clearance=FMath::Min(Frame.Legs[Side].Clearance,Probe.Z-Ground.ImpactPoint.Z);
                }
            }
            State->Samples.Add(Frame);
            if (!State->bPhotographed && Age>=2.1 && (Frame.Legs[0].Flex>(bRun?65.:40.) || Age>6.))
            {
                const TCHAR* File=bRun?TEXT("run-profile.png"):TEXT("walk-profile.png");
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,File),false,false,false,FIntRect(),true);
                State->bPhotographed=true;
            }
            if (Age<7.1) return true;
            State->bMetricsWritten=true;
            if (bCapture)
            {
                // Synchronous PNG readback can take longer than one display frame.
                // Drive only this separate film strip at a known simulation cadence.
                State->bClockWasFixed=FApp::UseFixedTimeStep();
                State->OriginalDelta=FApp::GetFixedDeltaTime();
                FApp::SetFixedDeltaTime(1./30.);
                FApp::SetUseFixedTimeStep(true);
                State->CaptureStart=Now;
            }
            for (int32 Side=0;Side<2;++Side)
                bPassed &= MeasureLeg(State->Samples,Side,bRun,State->Summary);
            FString Raw=TEXT("t_s,actor_x,actor_y,speed_cm_s,left_knee_deg,right_knee_deg,left_sole_x,left_sole_y,left_sole_z,right_sole_x,right_sole_y,right_sole_z,left_clearance_cm,right_clearance_cm\n");
            for (const auto& Sample:State->Samples)
            {
                const auto& L=Sample.Legs[0];const auto& R=Sample.Legs[1];
                Raw+=FString::Printf(TEXT("%.4f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n"),
                    Sample.Seconds,Sample.Centre.X,Sample.Centre.Y,Sample.Speed,L.Flex,R.Flex,
                    L.Sole.X,L.Sole.Y,L.Sole.Z,R.Sole.X,R.Sole.Y,R.Sole.Z,L.Clearance,R.Clearance);
            }
            bPassed &= FFileHelper::SaveStringToFile(Raw,*FPaths::Combine(Folder,bRun?TEXT("run.csv"):TEXT("walk.csv")));
            bPassed &= FFileHelper::SaveStringToFile(State->Summary,*FPaths::Combine(Folder,TEXT("summary.csv")));
            return true;
        }));
}
#endif
