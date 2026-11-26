#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontCharacter.h"
#include "Components/PoseableMeshComponent.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontLifeProfile.h"
#include "RiverfrontLocalProfile.h"
#include "RiverfrontPedestrian.h"
#include "RiverfrontResidentAppearance.h"
#include "RiverfrontWorld.h"
#include "RiverfrontTraffic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Containers/Ticker.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/Children.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
enum class EMorningStep : uint8
{
    Title, Start, Home, Walk, Wardrobe, HairChoice, Cropped, Swept, Tone, Store, Preview, Purchase, CloseStore,
    Wallet, ObserveStreet, Checkpoint, Quit, Continue, PickUp, Paid, ReturnToStore, Watch, PayWatch, Kurta, PayKurta, FinishedFitting, Portrait, RestoreOutfit, BodyFront, BodyQuarter, BodyBackground, WardrobeMale, WardrobeFemale, HudControls, Removed, HeldItem, PutOnItem, EquipShopWatch, PressWear, CancelWhileDressing, ChangingBase, ChangingPreview, ConfirmOwned, MirrorIdlePerf, MirrorActivePerf, WearPullover, FinishPullover, FitBodyKurta, BedsideInput, RestAgain
};

struct FMorningEvidence
{
    EMorningStep Step = EMorningStep::Title;
    double Since = FPlatformTime::Seconds();
    double StartPressedAt = 0.;
    double StartToControlMs = -1.;
    double LastFrame = 0.;
    double WalkCm = 0.;
    int32 NextLeg = 1;
    TArray<FVector> Route;
    FVector LastWalk = FVector::ZeroVector;
    FLinearColor OriginalShirt;
    FLinearColor IndigoShirt;
    FString InitialMesh;
    TArray<double> WalkFrames;
    TArray<FString> Pictures;
    TWeakObjectPtr<ACameraActor> FamilyView;
    FRotator OutfitFront = FRotator::ZeroRotator;
    bool bKurtaPreview = false;
    bool bBodyCaptured = false;
    int32 StartingViewMode = INDEX_NONE;
    FVector WakeOrigin = FVector::ZeroVector;
    double WakeTravel = 0.;
    bool bSawWake = false;
    bool bWakePhoto = false;
    bool bTestedCancel = false;
    bool bTestedPutBack=false;
    uint32 IdleMirrorUpdates = 0;
    FVector PlantedFeet[2]{};
    double FootDrift = 0.;
    int32 PlantedSamples = 0;
    FString BodyReviewSlot;
    double PhaseFrameAt=0.;
    double LastMirrorRequest=0.;
    TArray<double> MirrorFrameTimes;
    FLinearColor DenimDye;
    bool bFamilyPhoto = false;
    bool bOldHUD = true;
    bool bCrosswalkSample = false;
    int32 FamilyCameraAngle = INDEX_NONE;
    double FamilyCameraSince = 0.;
    FVector FirstCrosswalkPosition;
    int32 PrintmakerVisits = 0;
    bool bDeliveryWalk = false;
    TMap<TWeakObjectPtr<ARiverfrontPedestrian>, FVector> FirstPositions;
    double FamilyTravelCm = 0.;
    double VisitorTravelCm = 0.;
    TMap<TWeakObjectPtr<ARiverfrontPedestrian>,double> VisitorDistances;
    int32 FamilySize = 0, Visitors = 0, KerbWaits = 0, Crossed = 0, ShopVisits = 0;
    FString Results;
};

TSharedPtr<SWidget> FindMorningControl(const TSharedPtr<SWidget>& Root, const FName Name)
{
    if (!Root || !Root->GetVisibility().IsVisible()) return nullptr;
    if (Root->GetTag() == Name) return Root;
    FChildren* Children = Root->GetChildren();
    for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
    {
        auto Found = FindMorningControl(Children->GetChildAt(Index), Name);
        if (Found) return Found;
    }
    return nullptr;
}

// Exercise the live Slate focus chain and the button's ordinary keyboard binding.
bool PressMorningButton(const TSharedPtr<SWidget>& Root, FName Tag)
{
    auto Button = FindMorningControl(Root, Tag);
    if (!Button || !Button->IsEnabled() || !Button->SupportsKeyboardFocus()) return false;
    auto& Slate = FSlateApplication::Get();
    Slate.SetKeyboardFocus(Button, EFocusCause::Navigation);
    if (Slate.GetKeyboardFocusedWidget() != Button) return false;
    const FKeyEvent Enter(EKeys::Enter, FModifierKeysState(), uint32(0), false, 13, 13);
    const bool Pressed = Slate.ProcessKeyDownEvent(Enter);
    const bool Released = Slate.ProcessKeyUpEvent(Enter);
    return Pressed || Released;
}

bool MorningBalanceVisible(const TSharedPtr<SWidget>& Menu, const TCHAR* Cash, const TCHAR* Bank)
{
    const auto Matches = [&](FName Tag, const TCHAR* Amount)
    {
        const auto Text = FindMorningControl(Menu,Tag);
        return Text && Text->GetType() == FName(TEXT("STextBlock"))
            && StaticCastSharedPtr<STextBlock>(Text)->GetText().ToString() == Amount;
    };
    return Matches(TEXT("LifeCashBalance"),Cash) && Matches(TEXT("LifeBankBalance"),Bank);
}

// The traffic collision boxes cover the chassis. Visibility also needs the rendered body.
int32 ClearFamilyCamera(UWorld& World, const FVector& Centre, const TArray<FVector>& People, FVector& Eye)
{
    TArray<FBox> Cars;
    for (TActorIterator<ARiverfrontRoadAgent> It(&World); It; ++It)
        if (FVector::DistSquared2D(It->GetActorLocation(),Centre)<FMath::Square(1500.))
            Cars.Add(It->GetComponentsBoundingBox(true).ExpandBy(15.));

    const FVector Angles[] = { {90,355,155}, {-145,365,160}, {270,235,175}, {-245,240,195},
        {20,290,205}, {200,185,220}, {-190,180,220}, {-235,-265,200}, {215,-265,185} };
    const FCollisionQueryParams Trace(SCENE_QUERY_STAT(MorningFamilyVisibility),false);
    for (int32 Angle=0; Angle<UE_ARRAY_COUNT(Angles); ++Angle)
    {
        const FVector Lens=Centre+Angles[Angle];
        bool Clear=true;
        for (const FVector& Person : People)
        {
            for (const double Height : {32.,94.,155.})
            {
                const FVector Body=Person+FVector(0.,0.,Height);
                if (World.LineTraceTestByChannel(Lens,Body,ECC_Visibility,Trace) ||
                    Cars.ContainsByPredicate([&](const FBox& Box)
                    { return Box.IsInsideOrOn(Lens) || FMath::LineBoxIntersection(Box,Lens,Body,Body-Lens); }))
                { Clear=false; break; }
            }
            if (!Clear) break;
        }
        if (Clear) { Eye=Lens; return Angle; }
    }
    return INDEX_NONE;
}

// Verify the cooked mesh separately from the saved identifier. A broken asset reference
// can otherwise leave the previous mesh visible while the profile still reads correctly.
bool WearingResidentBody(ARiverfrontCharacter& Walker, ERiverfrontGender Gender, bool bKurta = false)
{
    const USkeletalMesh* Model = Walker.GetMesh()->GetSkeletalMeshAsset();
    const bool bFemale = Gender == ERiverfrontGender::Female;
    const FName Expected = bFemale
        ? FName(bKurta ? TEXT("SK_FemaleKurta") : TEXT("SK_FemaleEveryday"))
        : FName(bKurta ? TEXT("SK_FittedKurta") : TEXT("SK_RiverEveryday"));
    return Walker.GetAvatarGender() == Gender && Model && Model->GetFName() == Expected;
}

FString CheckCrowdBodies(UWorld& World, FString& Results)
{
    int32 Counts[3][2] = {};
    for (TActorIterator<ARiverfrontPedestrian> It(&World); It; ++It)
    {
        const auto* Body = It->FindComponentByClass<USkeletalMeshComponent>();
        const auto* Model = Body ? Body->GetSkeletalMeshAsset() : nullptr;
        if (!Model || !Model->GetSkeleton()) return TEXT("walker lacks an animated body");
        const bool bFemale = It->GetAvatarGender() == ERiverfrontGender::Female;
        const FName Asset = Model->GetFName();
        const bool bFemaleMesh = Asset == TEXT("SK_FemaleEveryday") || Asset == TEXT("SK_FemaleKurta");
        const bool bMaleMesh = Asset == TEXT("SK_RiverResident") || Asset == TEXT("SK_RiverResidentKurta")
            || Asset == TEXT("SK_RiverEveryday") || Asset == TEXT("SK_FittedKurta");
        if (!(bFemale ? bFemaleMesh : bMaleMesh)) return TEXT("NPC body differs from assigned gender: ") + Model->GetPathName();
        const int32 Group = It->IsFamilyMember() ? 0 : It->IsInternationalVisitor() ? 1 : 2;
        ++Counts[Group][bFemale ? 1 : 0];
    }
    const TCHAR* Groups[] = {TEXT("family"),TEXT("visitors"),TEXT("solo_residents")};
    for (int32 Group = 0; Group < UE_ARRAY_COUNT(Groups); ++Group)
    {
        if (!Counts[Group][0] || !Counts[Group][1])
            return FString::Printf(TEXT("%s lacks one body preset (male=%d female=%d)"),Groups[Group],Counts[Group][0],Counts[Group][1]);
        Results += FString::Printf(TEXT("%s_male=%d\n%s_female=%d\n"),Groups[Group],Counts[Group][0],Groups[Group],Counts[Group][1]);
    }
    return {};
}

bool HasMorningPurchase(const URiverfrontLocalProfile* Profile)
{
    return Profile && Profile->DisplayName == TEXT("Morning Playtest")
        && Profile->CashRupees == 98750 && Profile->BankRupees == 500000
        && Profile->OwnedItems.Contains(FName(TEXT("shirt-indigo")))
        && Profile->EquippedOutfit == TEXT("shirt-indigo") && Profile->AppearanceTone == TEXT("warm");
}
}

void ARiverfrontPlayerController::RunMorningReview()
{
    const FString Id = ProfileSlot.RightChop(12);
    FGuid Guid;
    if (!ProfileSlot.StartsWith(TEXT("SabarmatiQA-")) || !FGuid::ParseExact(Id, EGuidFormats::Digits, Guid))
    {
        UE_LOG(LogTemp, Error, TEXT("Morning review requires an isolated QA slot."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }

    FString RequestedId;
    const bool bColdRun = FParse::Value(FCommandLine::Get(), TEXT("SabarmatiMorningQAId="), RequestedId);
    const bool bContinue = FParse::Param(FCommandLine::Get(), TEXT("SabarmatiMorningContinue"))
        || GetWorld()->URL.HasOption(TEXT("SabarmatiMorningContinue"));
    const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("MorningReviews"), Id));
    if ((bColdRun && !Id.Equals(RequestedId, ESearchCase::IgnoreCase))
        || !IFileManager::Get().MakeDirectory(*Folder, true)
        || bContinue != HasLocalProfile())
    {
        UE_LOG(LogTemp,Error,TEXT("Morning review failed initial save or folder check: %s, continue=%d, profile=%d"),*Folder,bContinue,HasLocalProfile());
        FPlatformMisc::RequestExitWithStatus(false,1);
        return;
    }

    const bool bClosetReview=FParse::Param(FCommandLine::Get(),TEXT("SabarmatiWardrobeReview"));
    const bool bGenderReview = bClosetReview || FParse::Param(FCommandLine::Get(), TEXT("SabarmatiGenderReview"));
    const bool bBodyReview = bGenderReview || FParse::Param(FCommandLine::Get(), TEXT("SabarmatiBodyReview"));
    const ERiverfrontGender ExpectedGender = bGenderReview ? ERiverfrontGender::Female : ERiverfrontGender::Male;
    auto State = MakeShared<FMorningEvidence>();
    State->Route = FRiverfrontFirstMorningWorld::ExitRoute();
    UE_LOG(LogTemp,Display,TEXT("Sabarmati morning review started: %s, continue=%d"), *Folder, bContinue);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,State,Folder,Id,bColdRun,bContinue,bBodyReview,bGenderReview,bClosetReview,ExpectedGender](float)
        {
            auto* Walker = Cast<ARiverfrontCharacter>(GetPawn());
            const double Now = FPlatformTime::Seconds(), Age = Now - State->Since;
            const auto Next = [&](EMorningStep Step)
            {
                State->Step = Step;
                State->Since = Now;
            };
            const auto Fail = [&](const FString& Why)
            {
                const FString Error = FString::Printf(TEXT("stage=%d leg=%d reason=%s location=%s prompt=%s\n"),
                    int32(State->Step), State->NextLeg, *Why, Walker ? *Walker->GetActorLocation().ToString() : TEXT("none"), *PlacePrompt());
                UE_LOG(LogTemp, Error, TEXT("Sabarmati morning review failed: %s"), *Error);
                FFileHelper::SaveStringToFile(Error, *FPaths::Combine(Folder,TEXT("failure.txt")));
                FPlatformMisc::RequestExitWithStatus(false,1);
                return false;
            };
            const auto Shot = [&](const TCHAR* Name, bool bWithSlate = false)
            {
                const FString File = FString(Name) + TEXT(".png");
                State->Pictures.Add(File);
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,File), bWithSlate, false, false, FIntRect(), true);
            };
            if (!Walker || State->Route.Num() < 3 || !GetWorld()->GetGameViewport()) return Fail(TEXT("world, route or pawn unavailable"));
            if (Age > (State->Step == EMorningStep::ObserveStreet ? 180. : 65.)) return Fail(TEXT("timed out"));
            if (Age < 0.15 && State->Step != EMorningStep::Walk) return true;

            const auto GameKey = [&](FKey Key)
            {
                auto* View = GetWorld()->GetGameViewport()->Viewport;
                for (const auto Event : { IE_Pressed, IE_Released })
                    InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),Key,Event,FPlatformTime::Cycles64()));
            };
            const auto DressButtonLabel=[&]()
            {
                auto Label=FindMorningControl(Overlay,TEXT("LifeDressAction"));
                return Label ? StaticCastSharedPtr<STextBlock>(Label)->GetText().ToString() : FString();
            };
            // Use the same separate actions a player presses. Timers keep each button disabled during motion.
            const auto FinishWearing=[&](FName Id)
            {
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                if (!Saved || !Saved->OwnedItems.Contains(Id)) return false;
                if (Saved->EquippedOutfit==Id || Saved->EquippedAccessory==Id) return true;
                PressMorningButton(Overlay,TEXT("LifeDress"));
                return false;
            };
            const auto VerifyFittingView = [&](const TCHAR* Site) -> FString
            {
                if (!PlayerCameraManager || !LifeCamera.IsValid() || GetViewTarget()!=LifeCamera.Get())
                    return FString::Printf(TEXT("%s fitting camera missing"),Site);
                const FVector Body=Walker->GetActorLocation();
                const FVector ToLens=PlayerCameraManager->GetCameraLocation()-Body;
                const double Facing=FVector::DotProduct(Walker->GetActorForwardVector(), ToLens.GetSafeNormal2D());
                const auto Size=GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
                FVector2D Pixel=FVector2D::ZeroVector;
                const bool Mirrored=MirrorCapture.IsValid();
                const bool OnScreen=ProjectWorldLocationToScreen(Mirrored?FRiverfrontFirstMorningWorld::MirrorCentre():Body,Pixel,true);
                const double FramedX=Pixel.X/FMath::Max(1,Size.X);
                const FString Values=FString::Printf(TEXT("%s_front_dot=%.3f %s_screen_x=%.3f pixel_y=%.0f screen=%dx%d"),Site,Facing,Site,FramedX,Pixel.Y,Size.X,Size.Y);
                if (!OnScreen || (Mirrored ? Facing>-.4 : Facing<.55) || FramedX<=0 || FramedX>=.5 || Pixel.Y<0 || Pixel.Y>=Size.Y) return Values;
                State->Results+=Values+TEXT("\n");
                return FString();
            };
            const auto WalkAgain = [&]()
            {
                State->LastWalk = Walker->GetActorLocation();
                State->LastFrame = 0.;
                Next(EMorningStep::Walk);
            };

            switch (State->Step)
            {
            case EMorningStep::Title:
            {
                if (Age < .8) return true;
                if (!bContinue)
                {
                    auto NameBox = FindMorningControl(Overlay,TEXT("MorningName"));
                    if (!NameBox || NameBox->GetType() != FName(TEXT("SEditableTextBox"))) return Fail(TEXT("title name field"));
                    StaticCastSharedPtr<SEditableTextBox>(NameBox)->SetText(FText::FromString(TEXT("Morning Playtest")));
                    if (bGenderReview)
                    {
                        if (!State->Pictures.Contains(TEXT("00-profile-selection.png")))
                        {
                            if (!PressMorningButton(Overlay,TEXT("ProfileGenderFemale"))) return Fail(TEXT("choose female at profile creation"));
                            Shot(TEXT("00-profile-selection"),true);
                            return true;
                        }
                        if (FScreenshotRequest::IsScreenshotRequested()) return true;
                    }
                    if (!PressMorningButton(Overlay,TEXT("MorningCreateProfile")) || !HasLocalProfile())
                        return Fail(TEXT("title profile creation"));
                    const auto* P = RiverfrontLife::Load(ProfileSlot);
                    if (!P || P->CashRupees != 100000 || P->BankRupees != 500000 || P->bFirstMorningSeen || P->AvatarGender != ExpectedGender)
                        return Fail(TEXT("initial balances or wake state"));
                }
                else if (!HasMorningPurchase(RiverfrontLife::Load(ProfileSlot))) return Fail(TEXT("cold profile contents"));
                Next(EMorningStep::Start);
                break;
            }
            case EMorningStep::Start:
                State->StartPressedAt = FPlatformTime::Seconds();
                if (!PressMorningButton(Overlay,TEXT("MorningStart"))) return Fail(TEXT("title primary action"));
                Next(bContinue ? EMorningStep::Continue : EMorningStep::Home);
                break;
            case EMorningStep::Home:
            {
                if (State->StartToControlMs < 0. && !bWaking && !IsPaused() && !IsShowingMenu()
                    && !IsMoveInputIgnored() && !IsLookInputIgnored())
                {
                    State->StartToControlMs = (FPlatformTime::Seconds() - State->StartPressedAt) * 1000.;
                    UE_LOG(LogTemp, Display, TEXT("Sabarmati morning: Start to player control %.1f ms."),
                        State->StartToControlMs);
                }
                if (bWaking && WakeupSequence.IsValid() && LifeCamera.IsValid() && GetViewTarget() == LifeCamera.Get() && PlayerCameraManager)
                {
                    auto* SequencePlayer=WakeupSequence->GetSequencePlayer();
                    if (SequencePlayer && SequencePlayer->IsPlaying())
                    {
                        if (bClosetReview && (!Walker->IsLyingOnBed() ||
                            (Walker->GetBedTime()<.85f && FVector::Dist(Walker->GetAvatarMesh()->GetSocketLocation(TEXT("Head")),FRiverfrontFirstMorningWorld::Pillow())>5.)))
                            return Fail(TEXT("head outside pillow during wake"));
                        const FVector View=PlayerCameraManager->GetCameraLocation();
                        if (!State->bSawWake) { State->WakeOrigin=View; State->bSawWake=true; }
                        State->WakeTravel=FMath::Max(State->WakeTravel,FVector::Distance(View,State->WakeOrigin));
                        if (!State->bWakePhoto && SequencePlayer->GetCurrentTime().AsSeconds()>.64)
                        { Shot(TEXT("00-wake-up")); State->bWakePhoto=true; }
                        if (bClosetReview && Walker->GetBedTime()>2.74f && Walker->GetBedTime()<4.27f)
                        {
                            const auto* Pose=Walker->FindComponentByClass<UPoseableMeshComponent>();
                            if (!Pose) return Fail(TEXT("missing seated pose"));
                            for (int32 I=0;I<2;++I)
                            {
                                const FVector Foot=Pose->GetSocketLocation(I?TEXT("Footr"):TEXT("Footl"));
                                if (!State->PlantedSamples) State->PlantedFeet[I]=Foot;
                                State->FootDrift=FMath::Max(State->FootDrift,FVector::Dist(Foot,State->PlantedFeet[I]));
                            }
                            ++State->PlantedSamples;
                            if (Walker->GetBedTime()>3.08f && Walker->GetBedTime()<3.8f) return true;
                            const TCHAR* Phase=Walker->GetBedTime()<3.08f?TEXT("00-seated"):TEXT("00-standing-up");
                            if (!State->Pictures.Contains(FString(Phase)+TEXT(".png")) && !FScreenshotRequest::IsScreenshotRequested()) Shot(Phase);
                        }
                    }
                    return true;
                }
                if (Age < 3.8 || GetViewTarget()!=Walker) return true;
                if (!State->bSawWake || !State->bWakePhoto || State->WakeTravel<7.)
                    return Fail(FString::Printf(TEXT("Sequencer wake frame=%d photographed=%d camera displacement=%.2f cm"),State->bSawWake,State->bWakePhoto,State->WakeTravel));
                const auto* P = RiverfrontLife::Load(ProfileSlot);
                if (IsShowingMenu() || IsPaused() || !P || !P->bFirstMorningSeen
                    || FVector::Dist(Walker->GetActorLocation(), FRiverfrontFirstMorningWorld::HomeStart()) > 110.
                    || !Walker->GetCharacterMovement()->IsMovingOnGround()) return Fail(TEXT("grounded bedside wake and handoff"));
                if (bGenderReview && !WearingResidentBody(*Walker,ExpectedGender)) return Fail(TEXT("female starter body on wake-up"));
                if (!Walker->GetWardrobeShirtColor(State->OriginalShirt)) return Fail(TEXT("starter outfit material"));
                Shot(TEXT("01-apartment"));
                const auto Size = GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
                State->Results = FString::Printf(TEXT("viewport=%dx%d\nwake_camera_displacement_cm=%.2f\nstart_to_control_ms=%.1f\nstarting_cash=100000\nstarting_bank=500000\n"),
                    Size.X, Size.Y, State->WakeTravel, State->StartToControlMs);
                if (bClosetReview)
                {
                    UE_LOG(LogTemp,Display,TEXT("Sabarmati bed planted samples=%d max drift=%.2f cm"),State->PlantedSamples,State->FootDrift);
                    if (State->PlantedSamples<3 || State->FootDrift>6.) return Fail(TEXT("planted foot drift during rise"));
                }
                if (bGenderReview)
                {
                    if (const FString Error = CheckCrowdBodies(*GetWorld(),State->Results); !Error.IsEmpty()) return Fail(Error);
                    State->Results += TEXT("created_female=1\n");
                    if (!GetHUD() || !GetHUD()->bShowHUD || ControlsVisible() || !MapVisible())
                        return Fail(TEXT("initial on-foot HUD"));
                    State->StartingViewMode = GetWorld()->GetGameViewport()->ViewModeIndex;
                    Next(EMorningStep::HudControls);
                    break;
                }
                WalkAgain();
                break;
            }
            case EMorningStep::HudControls:
                if (GetWorld()->GetGameViewport()->ViewModeIndex != State->StartingViewMode)
                    return Fail(TEXT("F1 controls shortcut changed the scene renderer"));
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (!ControlsVisible() && MapVisible() && !State->Pictures.Contains(TEXT("01-controls.png")))
                {
                    GameKey(EKeys::F1);
                    GameKey(EKeys::M);
                    return true;
                }
                if (!State->Pictures.Contains(TEXT("01-controls.png")))
                {
                    if (!ControlsVisible() || MapVisible()) return Fail(TEXT("HUD help and minimap key bindings"));
                    Shot(TEXT("01-controls")); return true;
                }
                if (ControlsVisible() && !MapVisible())
                {
                    GameKey(EKeys::F1);
                    GameKey(EKeys::M);
                    return true;
                }
                if (ControlsVisible() || !MapVisible()) return Fail(TEXT("restoring HUD controls"));
                State->Results += TEXT("map_and_controls_toggled=1\n");
                WalkAgain();
                break;
            case EMorningStep::Walk:
            {
                const FVector Here = Walker->GetActorLocation();
                const FVector Goal = State->Route[State->NextLeg];
                const FVector Delta = Goal - Here;
                const double StepDistance = FVector::Dist2D(Here,State->LastWalk);
                if (StepDistance > 250.) return Fail(TEXT("unexpected displacement during walk"));
                State->WalkCm += StepDistance;
                State->LastWalk = Here;
                if (State->LastFrame > 0) State->WalkFrames.Add((Now-State->LastFrame)*1000.);
                State->LastFrame=Now;
                if (Delta.Size2D() > 24.)
                {
                    SetControlRotation(FRotator(-8.,Delta.Rotation().Yaw,0.));
                    Walker->AddMovementInput(Delta.GetSafeNormal2D(),1.f,true);
                    return true;
                }
                if (FMath::Abs(Delta.Z) > 112.) return Fail(TEXT("route height differs from the walkable surface"));
                Walker->GetCharacterMovement()->StopMovementImmediately();
                const int32 Reached=State->NextLeg++;
                State->Results += FString::Printf(TEXT("route_%d=%s\n"),Reached,*Here.ToString());
                if (!State->bDeliveryWalk && Reached == 1)
                {
                    InteractWithPlace();
                    Next(EMorningStep::Wardrobe);
                }
                else if (State->NextLeg == State->Route.Num())
                {
                    InteractWithPlace();
                    Next(State->bDeliveryWalk ? EMorningStep::Paid : EMorningStep::Store);
                }
                else
                {
                    if (!State->bDeliveryWalk && FVector::Dist2D(Goal,FRiverfrontFirstMorningWorld::StreetPoint())<35.) Shot(TEXT("03-apartment-street"));
                    WalkAgain();
                }
                break;
            }
            case EMorningStep::Wardrobe:
                if (const FString ViewError=VerifyFittingView(TEXT("wardrobe")); !ViewError.IsEmpty()) return Fail(ViewError);
                if (!IsShowingMenu() || !MorningBalanceVisible(Overlay,TEXT("₹1,00,000"),TEXT("₹5,00,000"))) return Fail(TEXT("visible initial wallet balances"));
                if (bGenderReview)
                {
                    if (!PressMorningButton(Overlay,TEXT("LifeGenderMale"))) return Fail(TEXT("wardrobe male selector"));
                    Next(EMorningStep::WardrobeMale);
                    break;
                }
                if (!PressMorningButton(Overlay,TEXT("LifeTone.warm"))) return Fail(TEXT("wardrobe skin-tone control"));
                Next(EMorningStep::HairChoice);
                break;
            case EMorningStep::WardrobeMale:
            case EMorningStep::WardrobeFemale:
            {
                const bool bMale = State->Step == EMorningStep::WardrobeMale;
                const auto Gender = bMale ? ERiverfrontGender::Male : ERiverfrontGender::Female;
                const auto* Saved = RiverfrontLife::Load(ProfileSlot);
                if (!Saved || Saved->AvatarGender != Gender || !WearingResidentBody(*Walker,Gender)
                    || Saved->CashRupees != 100000 || Saved->BankRupees != 500000
                    || Saved->EquippedOutfit != TEXT("cotton-sand") || Saved->OwnedItems.Num() != 1)
                    return Fail(TEXT("wardrobe body, wallet or ownership after avatar change"));
                if (Age < .6) return true;
                const TCHAR* Picture = bMale ? TEXT("02-gender-male") : TEXT("02-gender-female");
                if (!State->Pictures.Contains(FString(Picture) + TEXT(".png"))) { Shot(Picture,true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (bMale)
                {
                    if (bClosetReview)
                    {
                        if (!PressMorningButton(Overlay,TEXT("LifeItem.shirt-indigo"))) return Fail(TEXT("male preview"));
                        Next(EMorningStep::CancelWhileDressing); break;
                    }
                    if (!PressMorningButton(Overlay,TEXT("LifeGenderFemale"))) return Fail(TEXT("wardrobe female selector"));
                    Next(EMorningStep::WardrobeFemale);
                }
                else
                {
                    State->Results += TEXT("wardrobe_male_and_female=1\n");
                    if (bClosetReview)
                    {
                        if (!FindMorningControl(Overlay,TEXT("LifeItem.watch")) || !PressMorningButton(Overlay,TEXT("LifeItem.kurta")))
                            return Fail(TEXT("full home catalogue"));
                        Next(EMorningStep::ChangingBase); break;
                    }
                    if (!PressMorningButton(Overlay,TEXT("LifeTone.warm"))) return Fail(TEXT("wardrobe skin-tone control"));
                    Next(EMorningStep::HairChoice);
                }
                break;
            }
            case EMorningStep::CancelWhileDressing:
                if (Walker->GetWardrobeOutfit()!=TEXT("cotton-sand") || Walker->FindComponentByClass<UPoseableMeshComponent>()
                    || !PressMorningButton(Overlay,TEXT("LifeClose")) || Walker->GetWardrobeOutfit()!=TEXT("cotton-sand"))
                    return Fail(TEXT("browsing did not undress the resident"));
                InteractWithPlace();
                if (!PressMorningButton(Overlay,TEXT("LifeGenderFemale"))) return Fail(TEXT("female wardrobe selector"));
                Next(EMorningStep::WardrobeFemale); break;
            case EMorningStep::ChangingBase:
            {
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                if (Walker->GetWardrobeOutfit()!=TEXT("cotton-sand") || !Saved || Saved->OwnedItems.Contains(TEXT("kurta")) ||
                    Saved->EquippedOutfit!=TEXT("cotton-sand") || !PressMorningButton(Overlay,TEXT("LifeClose")))
                    return Fail(TEXT("unowned kurta preview"));
                if (!Walker->TeleportTo(FRiverfrontFirstMorningWorld::ShopPoint(),FRotator::ZeroRotator)) return Fail(TEXT("shop placement"));
                InteractWithPlace();
                const FName Buttons[]={TEXT("LifeItem.shirt-indigo"),TEXT("LifeBuyCash"),TEXT("LifeItem.watch"),TEXT("LifeBuyCash")};
                for (FName Button:Buttons) if (!PressMorningButton(Overlay,Button)) return Fail(TEXT("Sutra purchase"));
                if (Walker->GetWardrobeOutfit()!=TEXT("cotton-sand") || Walker->HasWristwatchVisible()) return Fail(TEXT("purchase auto-equipped an item"));
                Next(EMorningStep::EquipShopWatch); break;
            }
            case EMorningStep::EquipShopWatch:
                if (!FinishWearing(TEXT("watch"))) return true;
                if (!PressMorningButton(Overlay,TEXT("LifeClose")) ||
                    !Walker->TeleportTo(FRiverfrontFirstMorningWorld::WardrobePoint(),FRotator::ZeroRotator)) return Fail(TEXT("wardrobe placement"));
                InteractWithPlace();
                if (!MirrorCapture.IsValid() || !Walker->HasWristwatchVisible()) return Fail(TEXT("mirror or worn watch"));
                {
                    const auto* Mesh=Walker->GetMesh();
                    auto* Denim=Mesh->GetMaterial(Mesh->GetMaterialIndex(TEXT("bottom_denim")));
                    if (!Denim || !Denim->GetVectorParameterValue(TEXT("BaseColorFactor"),State->DenimDye)) return Fail(TEXT("starting denim"));
                }
                State->IdleMirrorUpdates=MirrorUpdates;
                Next(EMorningStep::ChangingPreview); break;
            case EMorningStep::ChangingPreview:
                if (Age<.7) return true;
                if (MirrorUpdates!=State->IdleMirrorUpdates || !PressMorningButton(Overlay,TEXT("LifeItem.shirt-indigo"))
                    || Walker->GetWardrobeOutfit()!=TEXT("cotton-sand")) return Fail(TEXT("selection changed mirror or garment"));
                Next(EMorningStep::PressWear); break;
            case EMorningStep::PressWear:
                if (!DressButtonLabel().StartsWith(TEXT("Take off")) || !PressMorningButton(Overlay,TEXT("LifeDress")))
                    return Fail(TEXT("missing separate removal action"));
                Next(EMorningStep::Removed); break;
            case EMorningStep::Removed:
            {
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                if (!Saved) return Fail(TEXT("missing removal save"));
                if (!State->bTestedCancel && Age>.25)
                {
                    const FKeyEvent Escape(EKeys::Escape,FModifierKeysState(),uint32(0),false,27,27);
                    if (!FSlateApplication::Get().ProcessKeyDownEvent(Escape) || IsShowingMenu()
                        || Walker->GetWardrobeOutfit()!=TEXT("cotton-sand")) return Fail(TEXT("cancel during removal"));
                    FSlateApplication::Get().ProcessKeyUpEvent(Escape);
                    State->bTestedCancel=true;
                    InteractWithPlace();
                    PressMorningButton(Overlay,TEXT("LifeItem.shirt-indigo"));
                    Next(EMorningStep::PressWear); break;
                }
                if (Saved->EquippedOutfit!=RiverfrontLife::BaseLayer) return true;
                if (!Saved->HeldWardrobeItem.IsNone() || Walker->GetWardrobeOutfit()!=RiverfrontLife::BaseLayer)
                    return Fail(TEXT("removed shirt was replaced"));
                if (!State->Pictures.Contains(TEXT("02-base-layer.png"))) { Shot(TEXT("02-base-layer"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("close while in base layer"));
                InteractWithPlace();
                if (Walker->GetWardrobeOutfit()!=RiverfrontLife::BaseLayer || !PressMorningButton(Overlay,TEXT("LifeItem.shirt-indigo")))
                    return Fail(TEXT("base layer lost across reopen"));
                Next(EMorningStep::HeldItem); break;
            }
            case EMorningStep::HeldItem:
                if (DressButtonLabel()!=TEXT("Take from wardrobe") || !PressMorningButton(Overlay,TEXT("LifeDress")))
                    return Fail(TEXT("pick up from wardrobe"));
                Next(EMorningStep::PutOnItem); break;
            case EMorningStep::PutOnItem:
            {
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                if (!Saved || Saved->HeldWardrobeItem!=TEXT("shirt-indigo") || Saved->EquippedOutfit!=RiverfrontLife::BaseLayer
                    || !Walker->HasHeldWardrobeProp(TEXT("shirt-indigo"))) return Fail(TEXT("item in hand before dressing"));
                if (!State->Pictures.Contains(TEXT("02-in-hand.png"))) { Shot(TEXT("02-in-hand"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (!State->bTestedPutBack)
                {
                    if (!PressMorningButton(Overlay,TEXT("LifePutBack")) || Walker->HasHeldWardrobeProp(TEXT("shirt-indigo")))
                        return Fail(TEXT("return held item to shelf"));
                    State->bTestedPutBack=true;
                    Next(EMorningStep::HeldItem); break;
                }
                if (!PressMorningButton(Overlay,TEXT("LifeClose")) || !Walker->HasHeldWardrobeProp(TEXT("shirt-indigo")))
                    return Fail(TEXT("held item lost on exit"));
                InteractWithPlace();
                if (!DressButtonLabel().StartsWith(TEXT("Put on")) || !PressMorningButton(Overlay,TEXT("LifeDress")))
                    return Fail(TEXT("put on held top after reopening"));
                Next(EMorningStep::ConfirmOwned); break;
            }
            case EMorningStep::ConfirmOwned:
            {
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                if (!Saved || Saved->EquippedOutfit!=TEXT("shirt-indigo")) return true;
                const auto* Body=Walker->GetMesh();
                auto* Denim=Body->GetMaterial(Body->GetMaterialIndex(TEXT("bottom_denim")));
                FLinearColor Dye;
                if (!Saved->HeldWardrobeItem.IsNone() || Walker->HasHeldWardrobeProp(TEXT("shirt-indigo")) || !Walker->HasWristwatchVisible()
                    || Saved->EquippedAccessory!=TEXT("watch") || Saved->CashRupees!=96950 || Saved->BankRupees!=500000
                    || !Denim || !Denim->GetVectorParameterValue(TEXT("BaseColorFactor"),Dye) || !Dye.Equals(State->DenimDye))
                    return Fail(TEXT("hand, denim, watch or balance after dressing"));
                if (!State->Pictures.Contains(TEXT("03-indigo-mirror.png"))) { Shot(TEXT("03-indigo-mirror"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                // Keep Slate painting just as it does during a fitting gesture, in both timing windows.
                Overlay->RegisterActiveTimer(0.f,FWidgetActiveTimerDelegate::CreateLambda([State](double,float)
                {
                    return State->Step==EMorningStep::MirrorIdlePerf || State->Step==EMorningStep::MirrorActivePerf
                        ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
                }));
                Next(EMorningStep::MirrorIdlePerf);
                break;
            }
            case EMorningStep::MirrorIdlePerf:
            case EMorningStep::MirrorActivePerf:
            {
                // Same camera, clothing, paused world and scene for both windows. Avoid profile I/O.
                const bool bActive=State->Step==EMorningStep::MirrorActivePerf;
                if (bActive && Now-State->LastMirrorRequest>=1./30.)
                {
                    RefreshWardrobeMirror();
                    State->LastMirrorRequest=Now;
                }
                if (Age>.6 && State->PhaseFrameAt>0.) State->MirrorFrameTimes.Add((Now-State->PhaseFrameAt)*1000.);
                State->PhaseFrameAt=Now;
                if (Age<3.6) return true;
                auto& Frames=State->MirrorFrameTimes;
                if (Frames.Num()<15) return Fail(TEXT("insufficient mirror timing samples"));
                Frames.Sort();
                UE_LOG(LogTemp,Display,TEXT("Sabarmati mirror fixed scene %s samples=%d median=%.2f ms p95=%.2f ms"),
                    bActive?TEXT("refresh"):TEXT("idle"),Frames.Num(),Frames[Frames.Num()/2],Frames[FMath::CeilToInt(Frames.Num()*.95)-1]);
                Frames.Reset();
                if (!bActive) { Next(EMorningStep::MirrorActivePerf); break; }
                if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("leave mirror"));
                InteractWithPlace();
                if (!PressMorningButton(Overlay,TEXT("LifeGenderMale")) || !PressMorningButton(Overlay,TEXT("LifeClose"))
                    || !WearingResidentBody(*Walker,ERiverfrontGender::Male)) return Fail(TEXT("male outfit before sleeping"));
                if (!Walker->TeleportTo(FRiverfrontFirstMorningWorld::ShopPoint(),FRotator::ZeroRotator)) return Fail(TEXT("return to Sutra"));
                InteractWithPlace();
                const FName ShopButtons[]={TEXT("LifeItem.kurta"),TEXT("LifeBuyCash"),TEXT("LifeClose")};
                for (FName Button:ShopButtons) if (!PressMorningButton(Overlay,Button)) return Fail(TEXT("buy kurta, wear indigo"));
                if (!Walker->TeleportTo(FRiverfrontFirstMorningWorld::WardrobePoint(),FRotator::ZeroRotator)) return Fail(TEXT("return to mirror"));
                InteractWithPlace();
                if (!PressMorningButton(Overlay,TEXT("LifePortrait")) || LifeCamera->GetCameraComponent()->FieldOfView>50.
                    || !PressMorningButton(Overlay,TEXT("LifePortrait")) || bMirrorPortrait)
                    return Fail(TEXT("inspect face and return to mirror"));
                if (!PressMorningButton(Overlay,TEXT("LifeItem.kurta"))) return Fail(TEXT("preview owned pullover"));
                Next(EMorningStep::WearPullover); break;
            }
            case EMorningStep::WearPullover:
                if (!DressButtonLabel().StartsWith(TEXT("Put on")))
                {
                    PressMorningButton(Overlay,TEXT("LifeDress"));
                    return true;
                }
                if (!Walker->HasHeldWardrobeProp(TEXT("kurta")) || !PressMorningButton(Overlay,TEXT("LifeDress"))) return Fail(TEXT("start pullover gesture"));
                Next(EMorningStep::FinishPullover); break;
            case EMorningStep::FinishPullover:
            {
                if (auto* Pose=Walker->FindComponentByClass<UPoseableMeshComponent>(); Pose && !State->Pictures.Contains(TEXT("04-pullover.png")))
                    if (Pose->GetSocketLocation(TEXT("Handl")).Z>Pose->GetSocketLocation(TEXT("Shoulderl")).Z &&
                        Pose->GetSocketLocation(TEXT("Handr")).Z>Pose->GetSocketLocation(TEXT("Shoulderr")).Z)
                        Shot(TEXT("04-pullover"),true);
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                if (!Saved || Saved->EquippedOutfit!=TEXT("kurta")) return true;
                if (!State->Pictures.Contains(TEXT("04-pullover.png")) || Saved->EquippedAccessory!=TEXT("watch")
                    || !Walker->HasWristwatchVisible() || !WearingResidentBody(*Walker,ERiverfrontGender::Male,true)
                    || !PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("pullover gesture, body or retained watch"));
                if (!Walker->TeleportTo(FRiverfrontFirstMorningWorld::Bedside(),FRotator::ZeroRotator)
                    || !CanRestAtBed()) return Fail(TEXT("bedside action"));
                Next(EMorningStep::BedsideInput); break;
            }
            case EMorningStep::BedsideInput:
            {
                if (!bWaking)
                {
                    if (Age>3.) return Fail(TEXT("repeat sleep input"));
                    GameKey(EKeys::E); return true;
                }
                if (!Walker->IsLyingOnBed() || FVector::Dist(Walker->GetAvatarMesh()->GetSocketLocation(TEXT("Head")),
                    FRiverfrontFirstMorningWorld::Pillow())>5.) return Fail(TEXT("repeat sleep pose"));
                if (Walker->GetBedTime()<.48f) return true;
                Shot(TEXT("08-male-sleep"));
                Next(EMorningStep::RestAgain); break;
            }
            case EMorningStep::RestAgain:
                if (bWaking || (GetViewTarget()!=Walker && Age<7.)) return true;
                if (Walker->IsLyingOnBed() || GetViewTarget()!=Walker || !Walker->GetCharacterMovement()->IsMovingOnGround())
                    return Fail(TEXT("bedside control after second wake"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati wardrobe review passed: browse and buy without equipping, remove, cancel, persist base layer, hold, put back, reopen, wear, retain denim and watch, pullover and bedside rest."));
                FPlatformMisc::RequestExitWithStatus(false,0); return false;
            case EMorningStep::HairChoice:
            {
                if (!Walker->SupportsPublicHaircut())
                {
                    const auto* Model=Walker->GetMesh()->GetSkeletalMeshAsset();
                    if (FPlatformProperties::RequiresCookedData() || !Model || !Model->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Avatar/")))
                        return Fail(TEXT("public haircut assets unavailable"));
                    State->Results+=TEXT("haircut_review=optional_private_mesh_skipped\n");
                    Next(EMorningStep::Tone);
                    break;
                }
                if (!PressMorningButton(Overlay,TEXT("LifeHair.cropped"))) return Fail(TEXT("wardrobe cropped haircut"));
                Next(EMorningStep::Cropped);
                break;
            }
            case EMorningStep::Cropped:
            case EMorningStep::Swept:
            {
                const bool Cropped=State->Step==EMorningStep::Cropped;
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                const auto* Hair=Cast<UStaticMeshComponent>(Walker->GetDefaultSubobjectByName(TEXT("SweptHair")));
                const FName ExpectedCut(Cropped ? TEXT("SM_ResidentCrop") : TEXT("SM_ResidentSweep"));
                if (!Saved || Saved->bCroppedHair!=Cropped || !Hair || !Hair->GetStaticMesh()
                    || !Hair->IsVisible() || Hair->GetAttachSocketName()!=TEXT("Head")
                    || Hair->GetStaticMesh()->GetFName()!=ExpectedCut)
                    return Fail(TEXT("persisted haircut or fitted mesh on head"));
                if (Cropped)
                {
                    if (!State->Pictures.Contains(TEXT("02a-cropped-hair.png"))) { Shot(TEXT("02a-cropped-hair"),true); return true; }
                    if (FScreenshotRequest::IsScreenshotRequested()) return true;
                    if (!PressMorningButton(Overlay,TEXT("LifeHair.swept"))) return Fail(TEXT("wardrobe swept haircut"));
                    Next(EMorningStep::Swept);
                }
                else
                {
                    State->Results+=TEXT("cropped_hair_saved=1\ncropped_mesh_visible_on_head=1\nswept_hair_saved=1\nswept_mesh_visible_on_head=1\n");
                    Next(EMorningStep::Tone);
                }
                break;
            }
            case EMorningStep::Tone:
                if (Age < .6) return true;
                if (!State->Pictures.Contains(TEXT("02-wardrobe.png"))) { Shot(TEXT("02-wardrobe"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (bBodyReview || FParse::Param(FCommandLine::Get(), TEXT("SabarmatiAvatarPortraitReview")))
                {
                    if (!PressMorningButton(Overlay,TEXT("LifePortrait")) || !LifeCamera.IsValid()
                        || LifeCamera->GetCameraComponent()->FieldOfView >= 50.f)
                        return Fail(TEXT("wardrobe face button or close camera"));
                    Next(EMorningStep::Portrait);
                    break;
                }
                if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("wardrobe close"));
                if (const auto* P=RiverfrontLife::Load(ProfileSlot); !P || P->AppearanceTone!=TEXT("warm") || P->CashRupees!=100000)
                    return Fail(TEXT("persisted free appearance change"));
                WalkAgain();
                break;
            case EMorningStep::Portrait:
                if (Age < 1.) return true;
                if (!State->Pictures.Contains(TEXT("02b-avatar-face.png")))
                {
                    const auto* Body = Walker->GetMesh();
                    const auto Size = GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
                    const FVector Face = Body->DoesSocketExist(TEXT("Head")) ? Body->GetSocketLocation(TEXT("Head")) : Walker->GetPawnViewLocation();
                    FVector2D Pixel, Top, Bottom;
                    const FVector HeadExtent(0.,0.,12.);
                    if (Size.X <= 0 || Size.Y <= 0 || !ProjectWorldLocationToScreen(Face,Pixel,true)
                        || !ProjectWorldLocationToScreen(Face+HeadExtent,Top,true)
                        || !ProjectWorldLocationToScreen(Face-HeadExtent,Bottom,true)
                        || Pixel.X < Size.X*.05 || Pixel.X > Size.X*.75 || Top.Y < Size.Y*.02 || Bottom.Y > Size.Y*.98)
                        return Fail(TEXT("face outside portrait framing"));
                    State->Results += FString::Printf(TEXT("portrait_camera=%s\nportrait_fov=%.1f\nhead_screen_xy=%.3f,%.3f\nmesh=%s\n"),
                        *LifeCamera->GetActorLocation().ToString(),LifeCamera->GetCameraComponent()->FieldOfView,
                        Pixel.X / Size.X,Pixel.Y / Size.Y,*GetPathNameSafe(Body->GetSkeletalMeshAsset()));
                    Shot(TEXT("02b-avatar-face"));
                    return true;
                }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (!PressMorningButton(Overlay,TEXT("LifePortrait")) || !LifeCamera.IsValid()
                    || LifeCamera->GetCameraComponent()->FieldOfView <= 50.f)
                    return Fail(TEXT("wardrobe full outfit button or restored camera"));
                Next(EMorningStep::RestoreOutfit);
                break;
            case EMorningStep::RestoreOutfit:
            {
                if (Age < 1.) return true;
                if (!State->Pictures.Contains(TEXT("02c-avatar-outfit.png"))) { Shot(TEXT("02c-avatar-outfit")); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (const FString Error = VerifyFittingView(TEXT("restored")); !Error.IsEmpty()) return Fail(Error);
                for (const auto& File : State->Pictures)
                {
                    if (IFileManager::Get().FileSize(*FPaths::Combine(Folder,File)) <= 0)
                        return Fail(FString::Printf(TEXT("missing screenshot %s"), *File));
                }
                if (!FFileHelper::SaveStringToFile(State->Results,*FPaths::Combine(Folder,TEXT("portrait.txt"))))
                    return Fail(TEXT("portrait camera record"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati avatar portrait review complete: %s"),*Folder);
                if (bBodyReview)
                {
                    if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("leave wardrobe for shop"));
                    WalkAgain();
                    break;
                }
                FPlatformMisc::RequestExitWithStatus(false, 0);
                return false;
            }
            case EMorningStep::BodyFront:
            case EMorningStep::BodyQuarter:
            {
                if (Age < 1.) return true;
                const bool Front = State->Step == EMorningStep::BodyFront;
                const bool Kurta = State->bKurtaPreview;
                const TCHAR* Name = Kurta ? (Front ? TEXT("11-kurta-front") : TEXT("12-kurta-quarter"))
                                          : (Front ? TEXT("09-everyday-front") : TEXT("10-everyday-quarter"));
                if (!State->Pictures.Contains(FString(Name) + TEXT(".png")))
                {
                    const auto* Model = Walker->GetMesh()->GetSkeletalMeshAsset();
                    const FName Outfit = Kurta ? FName(TEXT("kurta")) : FName(TEXT("cotton-sand"));
                    if (!Model || Walker->GetWardrobeOutfit()!=Outfit || !LifeCamera.IsValid()
                        || !WearingResidentBody(*Walker,ExpectedGender,Kurta))
                        return Fail(TEXT("visible native shop preview or body mesh"));
                    State->Results += FString::Printf(TEXT("%s=%s actor_yaw=%.1f camera=%s fov=%.1f\n"),
                        Name,*Model->GetPathName(),Walker->GetActorRotation().Yaw,
                        *LifeCamera->GetActorLocation().ToString(),LifeCamera->GetCameraComponent()->FieldOfView);
                    Shot(Name);
                    return true;
                }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (IFileManager::Get().FileSize(*FPaths::Combine(Folder,FString(Name)+TEXT(".png")))<=0)
                    return Fail(TEXT("missing shop body photograph"));
                if (Front)
                {
                    Walker->SetActorRotation(State->OutfitFront+FRotator(0,55,0));
                    Next(EMorningStep::BodyQuarter);
                    break;
                }
                Walker->SetActorRotation(State->OutfitFront);
                if (!Kurta)
                {
                    if (!PressMorningButton(Overlay,TEXT("LifeItem.kurta"))) return Fail(TEXT("preview kurta"));
                    State->bKurtaPreview = true;
                    Next(EMorningStep::FitBodyKurta);
                    break;
                }
                // A clean plate at the same camera and exposure lets QA measure the outfit silhouette.
                Walker->SetActorHiddenInGame(true);
                Next(EMorningStep::BodyBackground);
                break;
            }
            case EMorningStep::FitBodyKurta:
                if (const auto* Saved=RiverfrontLife::Load(ProfileSlot); Saved && !Saved->OwnedItems.Contains(TEXT("kurta")))
                {
                    if (!PressMorningButton(Overlay,TEXT("LifeBuyCash"))) return Fail(TEXT("buy kurta for isolated fit photographs"));
                    return true;
                }
                if (!FinishWearing(TEXT("kurta"))) return true;
                Next(EMorningStep::BodyFront); break;
            case EMorningStep::BodyBackground:
            {
                if (Age < 1.) return true;
                if (!State->Pictures.Contains(TEXT("13-shop-background.png"))) { Shot(TEXT("13-shop-background")); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                Walker->SetActorHiddenInGame(false);
                if (IFileManager::Get().FileSize(*FPaths::Combine(Folder,TEXT("13-shop-background.png")))<=0)
                    return Fail(TEXT("missing shop background photograph"));
                if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("close shop preview"));
                ProfileSlot=TEXT("SabarmatiQA-")+Id;
                RefreshLifeAppearance();
                UGameplayStatics::DeleteGameInSlot(State->BodyReviewSlot,0);
                const auto* Saved = RiverfrontLife::Load(ProfileSlot);
                if (!Saved || Saved->EquippedOutfit!=TEXT("cotton-sand") || Walker->GetWardrobeOutfit()!=Saved->EquippedOutfit
                    || Saved->OwnedItems.Contains(TEXT("kurta")) || Saved->CashRupees!=100000 || Saved->BankRupees!=500000)
                    return Fail(TEXT("shop preview did not restore original outfit and wallet"));
                if (!FFileHelper::SaveStringToFile(State->Results,*FPaths::Combine(Folder,TEXT("body.txt"))))
                    return Fail(TEXT("body review report"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati body review complete: %s"),*Folder);
                if (bGenderReview)
                {
                    State->bBodyCaptured = true;
                    InteractWithPlace();
                    Next(EMorningStep::Store);
                    break;
                }
                FPlatformMisc::RequestExitWithStatus(false,0);
                return false;
            }
            case EMorningStep::Store:
                if (bBodyReview && !State->bBodyCaptured)
                {
                    if (Screen!=EScreen::Life || !PressMorningButton(Overlay,TEXT("LifeItem.cotton-sand")))
                        return Fail(TEXT("preview everyday outfit"));
                    if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("start isolated body fitting"));
                    State->BodyReviewSlot=TEXT("SabarmatiQA-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
                    auto* Copy=RiverfrontLife::Load(ProfileSlot);
                    if (!Copy || !UGameplayStatics::SaveGameToSlot(Copy,State->BodyReviewSlot,0)) return Fail(TEXT("body fitting profile"));
                    ProfileSlot=State->BodyReviewSlot;
                    InteractWithPlace();
                    State->OutfitFront = Walker->GetActorRotation();
                    Next(EMorningStep::BodyFront);
                    break;
                }
                if (Screen!=EScreen::Life || !PressMorningButton(Overlay,TEXT("LifeItem.shirt-indigo")))
                    return Fail(TEXT("shop shirt preview"));
                Next(EMorningStep::Preview);
                break;
            case EMorningStep::Preview:
            {
                if (const FString ViewError=VerifyFittingView(TEXT("shop")); !ViewError.IsEmpty()) return Fail(ViewError);
                const auto* P=RiverfrontLife::Load(ProfileSlot);
                if (!P || P->CashRupees!=100000 || P->EquippedOutfit==TEXT("shirt-indigo") || P->OwnedItems.Contains(TEXT("shirt-indigo"))
                    || !Walker->GetWardrobeShirtColor(State->IndigoShirt) || !State->IndigoShirt.Equals(State->OriginalShirt,.01f))
                    return Fail(TEXT("browsing must preserve the live material and save"));
                if (!PressMorningButton(Overlay,TEXT("LifeBuyCash"))) return Fail(TEXT("native cash checkout button"));
                Next(EMorningStep::Purchase);
                break;
            }
            case EMorningStep::Purchase:
            {
                if (Age < .6) return true;
                if (!State->Pictures.Contains(TEXT("04-shop-receipt.png")))
                {
                    const auto Receipt=FindMorningControl(Overlay,TEXT("LifeReceipt"));
                    if (!Receipt || Receipt->GetType()!=FName(TEXT("STextBlock"))) return Fail(TEXT("missing purchase receipt"));
                    const FString Text=StaticCastSharedPtr<STextBlock>(Receipt)->GetText().ToString();
                    if (!Text.Contains(TEXT("1,250")) || Walker->GetWardrobeOutfit()!=TEXT("cotton-sand"))
                        return Fail(TEXT("checkout changed the outfit or lost the amount"));
                    State->Results+=TEXT("receipt=")+Text+TEXT("\n");
                    Shot(TEXT("04-shop-receipt"),true);
                    return true;
                }
                if (FScreenshotRequest::IsScreenshotRequested() || !FinishWearing(TEXT("shirt-indigo"))) return true;
                if (!HasMorningPurchase(RiverfrontLife::Load(ProfileSlot)) || Walker->GetWardrobeOutfit()!=TEXT("shirt-indigo"))
                    return Fail(TEXT("shop fitting or account balances"));
                Next(EMorningStep::CloseStore);
                break;
            }
            case EMorningStep::CloseStore:
                if (Age<.6 || FScreenshotRequest::IsScreenshotRequested()) return true;
                if (!PressMorningButton(Overlay,TEXT("LifeClose")) || IsShowingMenu()) return Fail(TEXT("return to dressed character"));
                Shot(TEXT("05-equipped-shirt"));
                Next(EMorningStep::Wallet);
                break;
            case EMorningStep::Wallet:
                if (Age<.75) return true;
                if (!IsShowingMenu()) { GameKey(EKeys::I); return true; }
                if (!FindMorningControl(Overlay,TEXT("LifeClose")) || !MorningBalanceVisible(Overlay,TEXT("₹98,750"),TEXT("₹5,00,000"))) return Fail(TEXT("wallet keyboard shortcut or visible balance"));
                if (!State->Pictures.Contains(TEXT("06-wallet.png"))) { Shot(TEXT("06-wallet"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("wallet close"));
                Next(EMorningStep::ObserveStreet);
                break;
            case EMorningStep::ObserveStreet:
            {
                FVector FamilyCentre=FVector::ZeroVector;
                TArray<FVector> FamilyPositions;
                int32 Family=0, Visitors=0, MovingVisitors=0, Waits=0, Crossed=0, Shops=0, Shoppers=0;
                bool InCrosswalk=false;
                for (TActorIterator<ARiverfrontPedestrian> It(GetWorld());It;++It)
                {
                    auto* Person=*It;
                    if (!Person->IsFamilyMember() && !Person->IsInternationalVisitor()) continue;
                    const FVector Position=Person->GetActorLocation();
                    const auto Key=TWeakObjectPtr<ARiverfrontPedestrian>(Person);
                    if (!State->FirstPositions.Contains(Key)) State->FirstPositions.Add(Key,Position);
                    const double Travel=FVector::Dist2D(Position,State->FirstPositions[Key]);
                    if (Person->IsFamilyMember())
                    {
                        ++Family; FamilyCentre+=Position; FamilyPositions.Add(Position); InCrosswalk|=Person->IsCrossingStreet();
                        Waits+=Person->FamilyKerbWaits();
                        Crossed+=Person->CrossingsCompleted()>0;
                        Shops+=Person->ShopVisits(); Shoppers+=Person->ShopVisits()>0;
                        State->FamilyTravelCm=FMath::Max(State->FamilyTravelCm,Travel);
                    }
                    if (Person->IsInternationalVisitor())
                    {
                        ++Visitors;
                        double& Seen = State->VisitorDistances.FindOrAdd(Key);
                        Seen = FMath::Max(Seen,Travel);
                        MovingVisitors += Seen > 150.;
                        if (Person->IsVisitingPrintmaker()) State->PrintmakerVisits=FMath::Max(State->PrintmakerVisits,Person->ShopVisits());
                    }
                }
                if (Family<2 || Visitors<2) return Fail(FString::Printf(TEXT("crowd composition: relatives=%d visitors=%d"),Family,Visitors));
                State->VisitorTravelCm=TNumericLimits<double>::Max();
                for (const auto& Entry:State->VisitorDistances) State->VisitorTravelCm=FMath::Min(State->VisitorTravelCm,Entry.Value);
                State->FamilySize=Family; State->Visitors=Visitors; State->KerbWaits=Waits; State->Crossed=Crossed; State->ShopVisits=Shops;
                FamilyCentre/=Family;
                if (!State->bFamilyPhoto && !State->FamilyView.IsValid())
                {
                    auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
                    if (!Camera) return Fail(TEXT("family observation camera"));
                    Camera->GetCameraComponent()->SetFieldOfView(64.f);
                    State->FamilyView=Camera; SetViewTarget(Camera);
                    if (auto* HUD=GetHUD()) { State->bOldHUD=HUD->bShowHUD; HUD->bShowHUD=false; }
                }
                if (State->FamilyView.IsValid())
                {
                    if (State->bFamilyPhoto && !FScreenshotRequest::IsScreenshotRequested())
                    {
                        SetViewTarget(Walker); State->FamilyView->Destroy(); State->FamilyView.Reset();
                        if (auto* HUD=GetHUD()) HUD->bShowHUD=State->bOldHUD;
                    }
                    else
                    {
                        FVector Eye=FamilyCentre+FVector(90.,355.,155.);
                        if (InCrosswalk && !State->bCrosswalkSample)
                        { State->FirstCrosswalkPosition=FamilyCentre; State->bCrosswalkSample=true; }
                        if (!State->bFamilyPhoto && InCrosswalk && Waits>0 && State->bCrosswalkSample
                            && FVector::Dist2D(FamilyCentre,State->FirstCrosswalkPosition)>250.)
                        {
                            const int32 Angle=ClearFamilyCamera(*GetWorld(),FamilyCentre,FamilyPositions,Eye);
                            if (Angle != State->FamilyCameraAngle) { State->FamilyCameraAngle=Angle; State->FamilyCameraSince=Now; }
                            if (Angle!=INDEX_NONE && Now-State->FamilyCameraSince>.14)
                            {
                                State->FamilyView->SetActorLocationAndRotation(Eye,(FamilyCentre+FVector(0.,0.,94.)-Eye).Rotation());
                                if (PlayerCameraManager) PlayerCameraManager->UpdateCamera(0.f);
                                Shot(TEXT("07-family-walk")); State->bFamilyPhoto=true;
                                State->Results+=FString::Printf(TEXT("clear_family_view=%d sight_lines=%d\n"),Angle,FamilyPositions.Num()*3);
                            }
                        }
                        State->FamilyView->SetActorLocationAndRotation(Eye,(FamilyCentre+FVector(0.,0.,94.)-Eye).Rotation());
                    }
                }
                if (Crossed<Family || Shoppers<Family || !Waits || MovingVisitors!=Visitors || !State->PrintmakerVisits || !State->bFamilyPhoto || State->FamilyView.IsValid()) return true;
                Next(EMorningStep::Checkpoint);
                break;
            }
            case EMorningStep::Checkpoint:
            {
                if (IsShowingMenu() || State->WalkFrames.Num()<12 || State->WalkCm<300.) return Fail(TEXT("insufficient traversal"));
                State->WalkFrames.Sort();
                const int32 P95=FMath::CeilToInt(State->WalkFrames.Num()*.95)-1;
                State->Results+=FString::Printf(TEXT("walk_metres=%.2f\nwalk_frames=%d\nwalk_p95_ms=%.2f\nfamily_members=%d\nfamily_kerb_waits=%d\nfamily_crossed=%d\nfamily_shop_visits=%d\nfamily_displacement_m=%.2f\nvisitors=%d\nleast_visitor_displacement_m=%.2f\nprintmaker_shop_visits=%d\n"),
                    State->WalkCm/100.,State->WalkFrames.Num(),State->WalkFrames[P95],State->FamilySize,State->KerbWaits,
                    State->Crossed,State->ShopVisits,State->FamilyTravelCm/100.,State->Visitors,State->VisitorTravelCm/100.,State->PrintmakerVisits);
                State->Results+=TEXT("cash_after_purchase=98750\nbank_after_purchase=500000\n");
                TogglePauseMenu();
                Next(EMorningStep::Quit);
                break;
            }
            case EMorningStep::Quit:
            {
                const auto* P=RiverfrontLife::Load(ProfileSlot);
                if (Screen!=EScreen::Paused || !HasMorningPurchase(P) || !P->bHasLifeCheckpoint
                    || FVector::Dist2D(P->CheckpointLocation,FRiverfrontFirstMorningWorld::ShopPoint())>150.)
                    return Fail(TEXT("shop checkpoint on pause"));
                if (bGenderReview && !State->Pictures.Contains(TEXT("07-pause.png"))) { Shot(TEXT("07-pause"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                for (const auto& Pic : State->Pictures)
                    if (IFileManager::Get().FileSize(*FPaths::Combine(Folder,Pic))<1000) return Fail(TEXT("missing capture ")+Pic);
                if (!FFileHelper::SaveStringToFile(State->Results,*FPaths::Combine(Folder,TEXT("first-morning.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
                    return Fail(TEXT("cannot write first-morning evidence"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati first morning complete: %s"),*Folder);
                if (bColdRun)
                {
                    if (!PressMorningButton(Overlay,TEXT("MorningQuit"))) return Fail(TEXT("quit game button"));
                }
                else
                {
                    bTravelling=true; SetPause(false);
                    const FString Options=TEXT("SabarmatiRiverfront?SabarmatiMorningContinue?SabarmatiQAProfile=")+Id;
                    UGameplayStatics::OpenLevel(this,FName(TEXT("/Engine/Maps/Entry")),true,Options);
                }
                return false;
            }
            case EMorningStep::Continue:
            {
                if (Age<1.35) return true;
                const auto* P=RiverfrontLife::Load(ProfileSlot);
                if (!HasMorningPurchase(P) || IsShowingMenu() || IsPaused() || !P->bFirstMorningSeen || !P->bHasLifeCheckpoint || GetViewTarget()!=Walker
                    || FVector::Dist2D(Walker->GetActorLocation(),FRiverfrontFirstMorningWorld::ShopPoint())>150.
                    || !Walker->GetCharacterMovement()->IsMovingOnGround() || Walker->GetWardrobeOutfit()!=TEXT("shirt-indigo"))
                    return Fail(TEXT("continue at shop with paid appearance and saved balances"));
                if ((bGenderReview || FParse::Param(FCommandLine::Get(),TEXT("SabarmatiLegacyAvatarReview")))
                    && (P->AvatarGender != ExpectedGender || !WearingResidentBody(*Walker,ExpectedGender)))
                    return Fail(TEXT("cold restart did not restore the saved body"));
                if (State->Pictures.IsEmpty()) { Shot(TEXT("08-continued-at-shop")); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                const auto Capture=FPaths::Combine(Folder,State->Pictures[0]);
                if (IFileManager::Get().FileSize(*Capture)<1000) return Age < 20. || Fail(TEXT("continue photograph"));
                if (!FFileHelper::SaveStringToFile(FString::Printf(
                    TEXT("cold_process=%d\ncontinued_at_shop=1\nrun_at=%s\ncash=%lld\nbank=%lld\nequipped=%s\nappearance=%s\nbody_mesh=%s\n"),
                    bColdRun,*Walker->GetActorLocation().ToString(),P->CashRupees,P->BankRupees,*P->EquippedOutfit.ToString(),*P->AppearanceTone.ToString(),*GetNameSafe(Walker->GetMesh()->GetSkeletalMeshAsset())),
                    *FPaths::Combine(Folder,TEXT("continued.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return Fail(TEXT("continue evidence"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati morning continue verified: %s"),*Folder);
                if (!FParse::Param(FCommandLine::Get(), TEXT("SabarmatiMorningDelivery")))
                {
                    TogglePauseMenu(); QuitDrive(); return false;
                }
                TActorIterator<ARiverfrontWorld> District(GetWorld());
                if (!District || !Walker->TeleportTo(District->GetPressCounter(),FRotator(0,90.,0))) return Fail(TEXT("print counter arrival"));
                const auto& PrintRoute=District->GetShopVisit(false);
                const auto& TeaRoute=District->GetShopVisit(true);
                if (PrintRoute.Num()<3 || TeaRoute.Num()<3) return Fail(TEXT("missing delivery foot route"));
                State->Route.Reset();
                for (int32 I=PrintRoute.Num()-1; I>=0; --I) State->Route.Add(PrintRoute[I]+FVector(0,0,92));
                for (const FVector& Point:TeaRoute) State->Route.Add(Point+FVector(0,0,92));
                State->NextLeg=1; State->bDeliveryWalk=true;
                SetControlRotation(FRotator(-7,90.,0));
                Next(EMorningStep::PickUp);
                break;
            }
            case EMorningStep::PickUp:
            {
                if (Age<.5) return true;
                if (!PlacePrompt().Contains(TEXT("Pick up"))) return Fail(TEXT("offered print job"));
                InteractWithPlace();
                const auto* P=RiverfrontLife::Load(ProfileSlot);
                if (!HasMorningPurchase(P) || !P->bPrintDeliveryActive || !Destination() || Destination()->Kind!=ERiverSiteKind::Tea)
                    return Fail(TEXT("accepted print order and destination"));
                Shot(TEXT("09-print-order"));
                WalkAgain();
                break;
            }
            case EMorningStep::Paid:
            {
                if (Age<.4) return true;
                const auto* Paid=RiverfrontLife::Load(ProfileSlot);
                if (!Paid || Paid->bPrintDeliveryActive || Paid->CompletedPrintDeliveries!=1 || Paid->CashRupees!=98750
                    || Paid->BankRupees!=500850 || !Paid->LastReceipt.Contains(TEXT("850")) || State->WalkCm<300.)
                    return Fail(TEXT("delivery at the tea shop"));
                InteractWithPlace();
                const auto* Again=RiverfrontLife::Load(ProfileSlot);
                if (!Again || Again->BankRupees!=500850 || Again->CompletedPrintDeliveries!=1)
                    return Fail(TEXT("duplicate delivery payout"));
                if (!FFileHelper::SaveStringToFile(FString::Printf(TEXT("pickup=Kagaz Print House\ndropoff=Nadi Chai\nwalk_metres=%.2f\ncredit=850\nbank=500850\ncash=98750\ndeliveries=1\n"),State->WalkCm/100.),
                    *FPaths::Combine(Folder,TEXT("delivery.txt")))) return Fail(TEXT("delivery record"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati delivery verified: %s"),*Folder);
                if (!FParse::Param(FCommandLine::Get(),TEXT("SabarmatiMorningFittingReview")))
                {
                    TogglePauseMenu(); QuitDrive(); return false;
                }
                if (!Walker->TeleportTo(FRiverfrontFirstMorningWorld::ShopPoint(),FRotator(0,90.,0)))
                    return Fail(TEXT("isolated fitting review transfer to shop counter"));
                UE_LOG(LogTemp,Display,TEXT("Morning fitting review: positioned player at Sutra counter after completing paid delivery."));
                Next(EMorningStep::ReturnToStore);
                break;
            }
            case EMorningStep::ReturnToStore:
                if (Age<.5) return true;
                if (!Walker->GetMesh()->GetSkeletalMeshAsset()) return Fail(TEXT("dressed character mesh"));
                State->InitialMesh=Walker->GetMesh()->GetSkeletalMeshAsset()->GetPathName();
                InteractWithPlace(); Next(EMorningStep::Watch);
                break;
            case EMorningStep::Watch:
                if (!MorningBalanceVisible(Overlay,TEXT("₹98,750"),TEXT("₹5,00,850")) || !PressMorningButton(Overlay,TEXT("LifeItem.watch")))
                    return Fail(TEXT("watch in shop after delivery"));
                Next(EMorningStep::PayWatch);
                break;
            case EMorningStep::PayWatch:
                if (!PressMorningButton(Overlay,TEXT("LifeBuyBank"))) return Fail(TEXT("pay from bank for wristwatch"));
                Next(EMorningStep::Kurta);
                break;
            case EMorningStep::Kurta:
            {
                if (!FinishWearing(TEXT("watch"))) return true;
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                const bool PublicMesh=State->InitialMesh.StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
                if (!Saved || Saved->CashRupees!=98750 || Saved->BankRupees!=499050 || Saved->EquippedAccessory!=TEXT("watch") ||
                    !Saved->OwnedItems.Contains(TEXT("watch")) || !MorningBalanceVisible(Overlay,TEXT("₹98,750"),TEXT("₹4,99,050"))
                    || Walker->GetWardrobeAccessory()!=TEXT("watch") || (FPlatformProperties::RequiresCookedData() && !PublicMesh)
                    || (PublicMesh && !Walker->HasWristwatchVisible())) return Fail(TEXT("wristwatch mesh attachment, payment or save"));
                if (!PressMorningButton(Overlay,TEXT("LifeItem.kurta"))) return Fail(TEXT("select kurta"));
                Next(EMorningStep::PayKurta);
                break;
            }
            case EMorningStep::PayKurta:
                if (!PressMorningButton(Overlay,TEXT("LifeBuyCash"))) return Fail(TEXT("pay for kurta"));
                Next(EMorningStep::FinishedFitting);
                break;
            case EMorningStep::FinishedFitting:
            {
                if (!FinishWearing(TEXT("kurta"))) return true;
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                const auto* Mesh=Walker->GetMesh()->GetSkeletalMeshAsset();
                const bool PublicMesh=State->InitialMesh.StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
                const bool Switched=Mesh && Mesh->GetPathName()!=State->InitialMesh && WearingResidentBody(*Walker,ExpectedGender,true);
                if (!Saved || Saved->CashRupees!=96550 || Saved->BankRupees!=499050 || Saved->EquippedOutfit!=TEXT("kurta")
                    || Saved->EquippedAccessory!=TEXT("watch") || !Saved->OwnedItems.Contains(TEXT("kurta"))
                    || !Saved->OwnedItems.Contains(TEXT("shirt-indigo")) || !Saved->OwnedItems.Contains(TEXT("watch"))
                    || Walker->GetWardrobeOutfit()!=TEXT("kurta") || !MorningBalanceVisible(Overlay,TEXT("₹96,550"),TEXT("₹4,99,050"))
                    || (PublicMesh && (!Switched || !Walker->HasWristwatchVisible())))
                    return Fail(TEXT("persisted kurta, wearable silhouette or watch after dressing"));
                if (!State->Pictures.Contains(TEXT("10-kurta-and-watch.png"))) { Shot(TEXT("10-kurta-and-watch"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested() || IFileManager::Get().FileSize(*FPaths::Combine(Folder,TEXT("10-kurta-and-watch.png")))<1000)
                    return true;
                const FString Evidence=FString::Printf(TEXT("repositioned_at_shop=1\nwatch_bank_debit=1800\nkurta_cash_debit=2200\ncash=96550\nbank=499050\nsaved_outfit=kurta\nsaved_accessory=watch\nwatch_attached=%d\npublic_mesh_changed=%d\noriginal_mesh=%s\nfinal_mesh=%s\n"),
                    Walker->HasWristwatchVisible(),Switched,*State->InitialMesh,*GetPathNameSafe(Mesh));
                if (!FFileHelper::SaveStringToFile(Evidence,*FPaths::Combine(Folder,TEXT("fitting.txt")))) return Fail(TEXT("fitting evidence"));
                UE_LOG(LogTemp,Display,TEXT("Sabarmati fitting purchases verified: %s"),*Folder);
                PressMorningButton(Overlay,TEXT("LifeClose")); TogglePauseMenu(); QuitDrive(); return false;
            }
            }
            return true;
        }));
}
#endif
