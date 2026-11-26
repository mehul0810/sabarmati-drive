#include "RiverfrontPlayerController.h"
#include "SabarmatiDistrict.h"
#include "RiverfrontCar.h"
#include "RiverfrontPedestrian.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontLifeMenu.h"
#include "RiverfrontLifeProfile.h"
#include "RiverfrontWorld.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

namespace
{
const FName Riverfront(TEXT("Riverfront"));
const FName Ahmedabad(TEXT("Ahmedabad"));
}

bool ARiverfrontPlayerController::HasLifeCheckpoint() const
{
    const auto* Profile = RiverfrontLife::Load(ProfileSlot);
    return Profile && Profile->bHasLifeCheckpoint;
}

void ARiverfrontPlayerController::BeginLifeSession()
{
    const bool bReturnHome = GetWorld()->URL.HasOption(TEXT("SabarmatiReturnHome"));
#if !UE_BUILD_SHIPPING
    // Historical benchmark routes start at Atal; the morning review uses the normal home.
    if (!bReturnHome && ProfileSlot.StartsWith(TEXT("SabarmatiQA-")) &&
        !FParse::Param(FCommandLine::Get(), TEXT("SabarmatiMorningReview"))) return;
#endif
    auto* Walker = Cast<ARiverfrontCharacter>(GetPawn());
    const auto* Profile = RiverfrontLife::Load(ProfileSlot);
    if (!Walker || !Profile) return;
    const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    const bool bCity = Mode && Mode->IsCityMode();
    const FName District = bCity ? Ahmedabad : Riverfront;
    if (!bCity && Profile->bHasLifeCheckpoint && Profile->CheckpointDistrict == Ahmedabad &&
        !GetWorld()->URL.HasOption(TEXT("SabarmatiRiverfront")) &&
        !(Mode && Mode->CityFailed()) && ARiverfrontWorld::HasCityData())
    {
        bTravelling = true;
        UGameplayStatics::OpenLevel(this, TEXT("/Engine/Maps/Entry"), true, TEXT("SabarmatiCity?SabarmatiRestore"));
        return;
    }

    bLifeActive = true;
    for (TActorIterator<ARiverfrontPedestrian> Person(GetWorld()); Person; ++Person)
        if (Person->IsFamilyMember() || Person->IsInternationalVisitor()) Neighbours.Add(*Person);
    bHasExperiencedWake = Profile->bFirstMorningSeen;
    bParcelInHand = Profile->bPrintDeliveryActive;
    bParcelDelivered = Profile->CompletedPrintDeliveries > 0;
    Walker->ApplyProfileAppearance(*Profile);
    const bool bReturning = !bReturnHome && Profile->bHasLifeCheckpoint && Profile->CheckpointDistrict == District;
    bool bPlaced = false;
    if (bReturning)
    {
        TActorIterator<ARiverfrontWorld> World(GetWorld());
        if (SabarmatiDistrict::Contains(Profile->CheckpointLocation,bCity) &&
            (!bCity || (World && World->PrepareCityTravel(Profile->CheckpointLocation))))
            bPlaced = Walker->TeleportTo(Profile->CheckpointLocation, FRotator(0, Profile->CheckpointYaw, 0));
    }
    if (!bPlaced && !bCity)
        bPlaced = Walker->TeleportTo(FRiverfrontFirstMorningWorld::HomeStart(), FRotator(0, FRiverfrontFirstMorningWorld::HomeYaw(), 0));
    if (bReturnHome && (!bPlaced || bCity))
    {
        bLifeActive = false;
        Notice(TEXT("Could not return to Flat 2B."));
        return;
    }
    Walker->GetCharacterMovement()->StopMovementImmediately();
    SetControlRotation(FRotator(-9, Walker->GetActorRotation().Yaw, 0));
    if (bParcelInHand) PointToChai();
    if (bReturnHome)
        Notice(TEXT("Flat 2B  ·  Kesar Apartments"));
    if (!bCity && !Profile->bFirstMorningSeen && bPlaced)
        PlayWakeup();
    else if (bReturnHome)
        CommitLifeCheckpoint(Walker->GetActorLocation(), Walker->GetActorRotation().Yaw);
    else
        SaveLifeCheckpoint();
    GetWorldTimerManager().SetTimer(LifeSaveTimer, this, &ARiverfrontPlayerController::SaveLifeCheckpoint, 15.f, true);
    UE_LOG(LogTemp, Display, TEXT("Sabarmati morning: %s in %s at %s"), bReturnHome ? TEXT("Return Home") : bReturning ? TEXT("Continue") : TEXT("Start"),
        *District.ToString(), *Walker->GetActorLocation().ToCompactString());
}

void ARiverfrontPlayerController::SaveLifeCheckpoint()
{
    if (!bLifeActive || bTravelling || bWaking || !GetPawn() || Screen == EScreen::Title) return;
    FVector Location = GetPawn()->GetActorLocation();
    float Yaw = GetPawn()->GetActorRotation().Yaw;
    if (const auto* Car = Cast<ARiverfrontCar>(GetPawn()))
    {
        int32 Side = 0;
        if (!Car->FindDriverExit(Location, Side)) return;
    }
    else if (const auto* Walker = Cast<ARiverfrontCharacter>(GetPawn()))
    {
        if (!Walker->GetCharacterMovement()->IsMovingOnGround()) return;
        if (MirrorCapture.IsValid()) Location=BeforeMirrorLocation;
        if (LifeCamera.IsValid() && Screen == EScreen::Life) Yaw = BeforeFittingRotation.Yaw;
    }
    else return;
    CommitLifeCheckpoint(Location, Yaw);
}

void ARiverfrontPlayerController::CommitLifeCheckpoint(const FVector& Location, float Yaw)
{
    if (bHaveLastCheckpoint && Location.Equals(LastLifeLocation, 5.f) &&
        FMath::Abs(FMath::FindDeltaAngleDegrees(LastLifeYaw, Yaw)) < 4.f) return;
    const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    if (RiverfrontLife::SaveCheckpoint(ProfileSlot, Mode && Mode->IsCityMode() ? Ahmedabad : Riverfront, Location, Yaw, bHasExperiencedWake))
    {
        LastLifeLocation = Location;
        LastLifeYaw = Yaw;
        bHaveLastCheckpoint = true;
    }
    else Notice(TEXT("Could not save this location. Check the available space on your Mac."));
}

void ARiverfrontPlayerController::RefreshLifeAppearance()
{
    if (auto* Walker = Cast<ARiverfrontCharacter>(GetPawn()))
        if (const auto* Profile = RiverfrontLife::Load(ProfileSlot))
        {
            Walker->ApplyProfileAppearance(*Profile);
            Walker->PoseHeldWardrobeItem();
            RefreshWardrobeMirror();
        }
}

void ARiverfrontPlayerController::PrepareFittingCamera()
{
    auto* Walker = Cast<ARiverfrontCharacter>(GetPawn());
    if (!Walker) return;
    const FVector Centre = Walker->GetActorLocation() + FVector(0,0,5);
    FVector Best = Walker->GetActorForwardVector();
    double Room = 0.;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FittingRoom), false, Walker);
    for (double Angle : {20., -20., 55., -55., 90., -90., 150., -150., 180.})
    {
        const FVector Ray = Walker->GetActorForwardVector().RotateAngleAxis(Angle, FVector::UpVector);
        FHitResult Hit;
        const double Length = GetWorld()->SweepSingleByChannel(Hit, Centre, Centre + Ray*290., FQuat::Identity,
            ECC_Camera, FCollisionShape::MakeSphere(12), Query) ? FMath::Max(0., Hit.Distance-10.) : 290.;
        if (Length > Room) { Best=Ray; Room=Length; }
        if (Room >= 289.) break;
    }
    if (Room < 80.) return;
    const FVector Eye = Centre + Best*Room + FVector(0,0,15);
    const FVector Right = FVector::CrossProduct(FVector::UpVector, -Best);
    const FRotator Aim = (Centre + Right*(Room*.20) - Eye).Rotation();
    if (auto* View=GetWorld()->SpawnActor<ACameraActor>(Eye,Aim))
    {
        BeforeFittingRotation=Walker->GetActorRotation();
        Walker->SetFittingPose(true);
        Walker->SetActorRotation(FRotator(0,Best.Rotation().Yaw,0));
        View->GetCameraComponent()->SetFieldOfView(70);
        LifeCamera=View;
        FullOutfitView = View->GetActorTransform();
        SetViewTarget(View);
        if (PlayerCameraManager)
        {
            PlayerCameraManager->SetGameCameraCutThisFrame();
            PlayerCameraManager->UpdateCamera(0.f);
        }
    }
}

bool ARiverfrontPlayerController::SetFittingPortraitView(bool bPortrait)
{
    ACameraActor* View = LifeCamera.Get();
    auto* Walker = Cast<ARiverfrontCharacter>(GetPawn());
    if (!View || !Walker) return false;

    if (bPortrait)
    {
        const USkeletalMeshComponent* Body = Walker->GetAvatarMesh();
        const FVector Face = Body && Body->DoesSocketExist(TEXT("Head"))
            ? Body->GetSocketLocation(TEXT("Head")) + FVector(0,0,2)
            : Walker->GetPawnViewLocation();
        const FVector Forward = Walker->GetActorForwardVector();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, -Forward);
        const FVector Desired = Face + Forward * 80. + FVector(0,0,1.5);
        FHitResult Hit;
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(WardrobePortrait), false, Walker);
        const bool bObstructed = GetWorld()->SweepSingleByChannel(Hit, Face, Desired, FQuat::Identity,
            ECC_Camera, FCollisionShape::MakeSphere(6), Query);
        if (bObstructed && Hit.Distance < 40.) return false;
        const FVector Eye = bObstructed ? Hit.Location : Desired;
        const double Distance = FVector::Distance(Face, Eye);
        // Leave the right-hand side clear for the wardrobe controls.
        View->SetActorLocationAndRotation(Eye, (Face + Right * (Distance * .105) - Eye).Rotation());
        View->GetCameraComponent()->SetFieldOfView(46.f);
    }
    else
    {
        View->SetActorTransform(FullOutfitView);
        View->GetCameraComponent()->SetFieldOfView(MirrorCapture.IsValid()?62.f:70.f);
    }
    bMirrorPortrait=bPortrait;
    if (!bPortrait) RefreshWardrobeMirror();
    if (PlayerCameraManager)
    {
        PlayerCameraManager->SetGameCameraCutThisFrame();
        PlayerCameraManager->UpdateCamera(0.f);
    }
    return true;
}

void ARiverfrontPlayerController::OpenWallet()
{
    OpenLifeMenu(ERiverfrontLifeMenu::Wallet);
}

void ARiverfrontPlayerController::OpenLifeMenu(ERiverfrontLifeMenu Page)
{
    if (Screen != EScreen::Playing || !HasLocalProfile() || !GetWorld()->GetGameViewport()) return;
    if (bWaking) FinishWakeup();
    SaveLifeCheckpoint();
    if (Page == ERiverfrontLifeMenu::Shop || (Page == ERiverfrontLifeMenu::Wardrobe && !OpenWardrobeMirror())) PrepareFittingCamera();
    if (LifeCamera.IsValid())
        if (auto* Walker=Cast<ARiverfrontCharacter>(GetPawn())) Walker->PoseHeldWardrobeItem();
    Screen = EScreen::Life;
    SetPause(true);
    FlushPressedKeys();
    bShowMouseCursor = true;
    TSharedRef<SWidget> Menu = MakeRiverfrontLifeMenu(*GetWorld(), Page, ProfileSlot,
        FSimpleDelegate::CreateUObject(this, &ARiverfrontPlayerController::RefreshLifeAppearance),
        FSimpleDelegate::CreateUObject(this, &ARiverfrontPlayerController::CloseLifeMenu));
    Overlay = Menu;
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu, 20);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Menu);
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Input);
    FSlateApplication::Get().SetAllUserFocus(Menu, EFocusCause::SetDirectly);
}

void ARiverfrontPlayerController::CloseLifeMenu()
{
    if (Screen != EScreen::Life) return;
    RefreshLifeAppearance();
    if (LifeCamera.IsValid())
    {
        if (auto* Walker = Cast<ARiverfrontCharacter>(GetPawn()))
        {
            if (MirrorCapture.IsValid()) Walker->SetActorLocation(BeforeMirrorLocation);
            Walker->SetActorRotation(BeforeFittingRotation);
            Walker->SetFittingPose(false);
        }
        SetViewTarget(GetPawn());
        MirrorCapture.Reset();
        bMirrorPortrait=false;
        LifeCamera->Destroy();
        LifeCamera.Reset();
    }
    ResumeDrive();
}

ARiverfrontPedestrian* ARiverfrontPlayerController::NearbyCompanion() const
{
    if (!GetPawn() || !GetPawn()->IsA<ARiverfrontCharacter>()) return nullptr;
    double RangeSquared = FMath::Square(170.);
    ARiverfrontPedestrian* Nearest = nullptr;
    for (const auto& Candidate : Neighbours)
    {
        if (!Candidate.IsValid()) continue;
        const double Distance = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
        if (Distance < RangeSquared)
        {
            RangeSquared = Distance;
            Nearest = Candidate.Get();
        }
    }
    return Nearest;
}

void ARiverfrontPlayerController::GreetCompanion(ARiverfrontPedestrian& Person)
{
    if (Person.ActorHasTag(TEXT("VisitingPrintmaker")))
        Notice(TEXT("Amina · I’m here from Nairobi for a printmaking exchange. Have you seen the block prints at Sutra?"));
    else if (Person.IsInternationalVisitor())
        Notice(TEXT("Inês · I’m visiting from Lisbon. I’ve been waiting for this light on Atal Bridge."));
    else
        Notice(TEXT("We meet for chai on this bank. Come across with us when the signal changes."));
}

void ARiverfrontPlayerController::PointToChai()
{
    SelectedSite = Sites.IndexOfByPredicate([](const FRiverSite& Site) { return Site.Kind == ERiverSiteKind::Tea; });
}

bool ARiverfrontPlayerController::UpdatePrintDelivery(ERiverSiteKind Kind)
{
    if (!bLifeActive || bCitySites || bParcelDelivered) return false;
    if (Kind==ERiverSiteKind::Print && !bParcelInHand)
    {
        const FText Error=RiverfrontLife::StartPrintDelivery(ProfileSlot);
        if (!Error.IsEmpty()) { Notice(Error.ToString()); return true; }
        bParcelInHand=true;
        PointToChai();
        Notice(TEXT("Kagaz Print House • Take the parcel to Nadi Chai. Fee ₹850."));
        return true;
    }
    if (Kind==ERiverSiteKind::Tea && bParcelInHand)
    {
        const FText Error=RiverfrontLife::CompletePrintDelivery(ProfileSlot);
        if (!Error.IsEmpty()) { Notice(Error.ToString()); return true; }
        bParcelInHand=false;
        bParcelDelivered=true;
        Notice(TEXT("Print order delivered. ₹850 paid into your bank account."));
        return true;
    }
    return false;
}
