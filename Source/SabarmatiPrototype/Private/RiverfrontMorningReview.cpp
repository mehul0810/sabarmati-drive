#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontCharacter.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontLifeProfile.h"
#include "RiverfrontLocalProfile.h"
#include "RiverfrontPedestrian.h"
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
#include "Engine/SkeletalMesh.h"
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
    Wallet, ObserveStreet, Checkpoint, Quit, Continue, PickUp, Paid, ReturnToStore, Watch, PayWatch, Kurta, PayKurta, FinishedFitting
};

struct FMorningEvidence
{
    EMorningStep Step = EMorningStep::Title;
    double Since = FPlatformTime::Seconds();
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
    FVector WakeOrigin = FVector::ZeroVector;
    double WakeTravel = 0.;
    bool bSawWake = false;
    bool bWakePhoto = false;
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

    auto State = MakeShared<FMorningEvidence>();
    State->Route = FRiverfrontFirstMorningWorld::ExitRoute();
    UE_LOG(LogTemp,Display,TEXT("Sabarmati morning review started: %s, continue=%d"), *Folder, bContinue);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,State,Folder,Id,bColdRun,bContinue](float)
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
            const auto VerifyFittingView = [&](const TCHAR* Site) -> FString
            {
                if (!PlayerCameraManager || !LifeCamera.IsValid() || GetViewTarget()!=LifeCamera.Get())
                    return FString::Printf(TEXT("%s fitting camera missing"),Site);
                const FVector Body=Walker->GetActorLocation();
                const FVector ToLens=PlayerCameraManager->GetCameraLocation()-Body;
                const double Facing=FVector::DotProduct(Walker->GetActorForwardVector(), ToLens.GetSafeNormal2D());
                const auto Size=GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
                FVector2D Pixel=FVector2D::ZeroVector;
                const bool OnScreen=ProjectWorldLocationToScreen(Body,Pixel,true);
                const double FramedX=Pixel.X/FMath::Max(1,Size.X);
                const FString Values=FString::Printf(TEXT("%s_front_dot=%.3f %s_screen_x=%.3f pixel_y=%.0f screen=%dx%d"),Site,Facing,Site,FramedX,Pixel.Y,Size.X,Size.Y);
                if (!OnScreen || Facing<.55 || FramedX<=0 || FramedX>=.5 || Pixel.Y<0 || Pixel.Y>=Size.Y) return Values;
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
                    if (!PressMorningButton(Overlay,TEXT("MorningCreateProfile")) || !HasLocalProfile())
                        return Fail(TEXT("title profile creation"));
                    const auto* P = RiverfrontLife::Load(ProfileSlot);
                    if (!P || P->CashRupees != 100000 || P->BankRupees != 500000 || P->bFirstMorningSeen)
                        return Fail(TEXT("initial balances or wake state"));
                }
                else if (!HasMorningPurchase(RiverfrontLife::Load(ProfileSlot))) return Fail(TEXT("cold profile contents"));
                Next(EMorningStep::Start);
                break;
            }
            case EMorningStep::Start:
                if (!PressMorningButton(Overlay,TEXT("MorningStart"))) return Fail(TEXT("title primary action"));
                Next(bContinue ? EMorningStep::Continue : EMorningStep::Home);
                break;
            case EMorningStep::Home:
            {
                if (bWaking && WakeupSequence.IsValid() && LifeCamera.IsValid() && GetViewTarget() == LifeCamera.Get() && PlayerCameraManager)
                {
                    auto* SequencePlayer=WakeupSequence->GetSequencePlayer();
                    if (SequencePlayer && SequencePlayer->IsPlaying())
                    {
                        const FVector View=PlayerCameraManager->GetCameraLocation();
                        if (!State->bSawWake) { State->WakeOrigin=View; State->bSawWake=true; }
                        State->WakeTravel=FMath::Max(State->WakeTravel,FVector::Distance(View,State->WakeOrigin));
                        if (!State->bWakePhoto && SequencePlayer->GetCurrentTime().AsSeconds()>.9)
                        { Shot(TEXT("00-wake-up")); State->bWakePhoto=true; }
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
                if (!Walker->GetWardrobeShirtColor(State->OriginalShirt)) return Fail(TEXT("starter outfit material"));
                Shot(TEXT("01-apartment"));
                const auto Size = GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
                State->Results = FString::Printf(TEXT("viewport=%dx%d\nwake_camera_displacement_cm=%.2f\nstarting_cash=100000\nstarting_bank=500000\n"),Size.X,Size.Y,State->WakeTravel);
                WalkAgain();
                break;
            }
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
                if (!PressMorningButton(Overlay,TEXT("LifeTone.warm"))) return Fail(TEXT("wardrobe skin-tone control"));
                Next(EMorningStep::HairChoice);
                break;
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
                if (!Saved || Saved->bCroppedHair!=Cropped || !Hair || !Hair->GetStaticMesh()
                    || Hair->IsVisible()==Cropped || (!Cropped && Hair->GetAttachSocketName()!=TEXT("Head")))
                    return Fail(Cropped ? TEXT("persisted cropped choice or actual hair component visibility") : TEXT("persisted swept choice or attached visible hair mesh"));
                if (Cropped)
                {
                    if (!State->Pictures.Contains(TEXT("02a-cropped-hair.png"))) { Shot(TEXT("02a-cropped-hair"),true); return true; }
                    if (FScreenshotRequest::IsScreenshotRequested()) return true;
                    if (!PressMorningButton(Overlay,TEXT("LifeHair.swept"))) return Fail(TEXT("wardrobe swept haircut"));
                    Next(EMorningStep::Swept);
                }
                else
                {
                    State->Results+=TEXT("cropped_hair_saved=1\ncropped_hides_swept_mesh=1\nswept_hair_saved=1\nswept_mesh_visible_on_head=1\n");
                    Next(EMorningStep::Tone);
                }
                break;
            }
            case EMorningStep::Tone:
                if (Age < .6) return true;
                if (!State->Pictures.Contains(TEXT("02-wardrobe.png"))) { Shot(TEXT("02-wardrobe"),true); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                if (!PressMorningButton(Overlay,TEXT("LifeClose"))) return Fail(TEXT("wardrobe close"));
                if (const auto* P=RiverfrontLife::Load(ProfileSlot); !P || P->AppearanceTone!=TEXT("warm") || P->CashRupees!=100000)
                    return Fail(TEXT("persisted free appearance change"));
                WalkAgain();
                break;
            case EMorningStep::Store:
                if (Screen!=EScreen::Life || !PressMorningButton(Overlay,TEXT("LifeItem.shirt-indigo")))
                    return Fail(TEXT("shop shirt preview"));
                Next(EMorningStep::Preview);
                break;
            case EMorningStep::Preview:
            {
                if (const FString ViewError=VerifyFittingView(TEXT("shop")); !ViewError.IsEmpty()) return Fail(ViewError);
                const auto* P=RiverfrontLife::Load(ProfileSlot);
                if (!P || P->CashRupees!=100000 || P->EquippedOutfit==TEXT("shirt-indigo") || P->OwnedItems.Contains(TEXT("shirt-indigo"))
                    || !Walker->GetWardrobeShirtColor(State->IndigoShirt) || State->IndigoShirt.Equals(State->OriginalShirt,.01f))
                    return Fail(TEXT("free preview must change the live material and preserve the save"));
                if (!PressMorningButton(Overlay,TEXT("LifeBuyCash"))) return Fail(TEXT("native cash checkout button"));
                Next(EMorningStep::Purchase);
                break;
            }
            case EMorningStep::Purchase:
            {
                if (Age < .6) return true;
                const auto Receipt=FindMorningControl(Overlay,TEXT("LifeReceipt"));
                if (!HasMorningPurchase(RiverfrontLife::Load(ProfileSlot)) || Walker->GetWardrobeOutfit()!=TEXT("shirt-indigo")
                    || !Receipt || Receipt->GetType()!=FName(TEXT("STextBlock")) || StaticCastSharedPtr<STextBlock>(Receipt)->GetText().IsEmpty())
                    return Fail(TEXT("purchased appearance, receipt, or account balances"));
                State->Results+=TEXT("receipt=")+StaticCastSharedPtr<STextBlock>(Receipt)->GetText().ToString()+TEXT("\n");
                Shot(TEXT("04-shop-receipt"),true);
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
                if (State->Pictures.IsEmpty()) { Shot(TEXT("08-continued-at-shop")); return true; }
                if (FScreenshotRequest::IsScreenshotRequested()) return true;
                const auto Capture=FPaths::Combine(Folder,State->Pictures[0]);
                if (IFileManager::Get().FileSize(*Capture)<1000) return Age < 20. || Fail(TEXT("continue photograph"));
                if (!FFileHelper::SaveStringToFile(FString::Printf(
                    TEXT("cold_process=%d\ncontinued_at_shop=1\nrun_at=%s\ncash=%lld\nbank=%lld\nequipped=%s\nappearance=%s\n"),
                    bColdRun,*Walker->GetActorLocation().ToString(),P->CashRupees,P->BankRupees,*P->EquippedOutfit.ToString(),*P->AppearanceTone.ToString()),
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
                const auto* Saved=RiverfrontLife::Load(ProfileSlot);
                const auto* Mesh=Walker->GetMesh()->GetSkeletalMeshAsset();
                const bool PublicMesh=State->InitialMesh.StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
                const bool Switched=Mesh && Mesh->GetPathName()!=State->InitialMesh && Mesh->GetPathName()==TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Tailored/SK_FittedKurta.SK_FittedKurta");
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
