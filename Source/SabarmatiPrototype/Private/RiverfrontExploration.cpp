#include "RiverfrontPlayerController.h"
#include "SabarmatiDistrict.h"
#include "RiverfrontCar.h"
#include "RiverfrontLifeMenu.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontPedestrian.h"
#include "RiverfrontTraffic.h"
#include "RiverfrontCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RiverfrontWorld.h"
#include "RiverfrontLifeProfile.h"
#include "RiverfrontNavigation.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWave.h"

void ARiverfrontPlayerController::LoadSites()
{
    TActorIterator<ARiverfrontWorld> District(GetWorld());
    if (!District) return;
    bCitySites=District->IsCityMode();
    FString Contents;
    TSharedPtr<FJsonObject> Doc;
    if (!FFileHelper::LoadFileToString(Contents,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("CityData/ExplorationSites.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents),Doc) || !Doc.IsValid()) return;
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    const TCHAR* DistrictKey=District->IsGrandCity() ? TEXT("grand-city") : bCitySites ? TEXT("city") : TEXT("riverfront");
    if (!Doc->TryGetArrayField(DistrictKey,Rows)) return;
    Sites.Reset();
    for (const auto& Item:*Rows)
    {
        const auto& O=Item->AsObject();
        if (!O.IsValid()) continue;
        FString Id,Name,Action,Discovery,Type;
        double Reach=0;
        if (!O->TryGetStringField(TEXT("id"),Id) || !O->TryGetStringField(TEXT("name"),Name) ||
            !O->TryGetStringField(TEXT("action"),Action) || !O->TryGetStringField(TEXT("note"),Discovery) ||
            !O->TryGetStringField(TEXT("kind"),Type) || !O->TryGetNumberField(TEXT("reach_cm"),Reach)) continue;
        const auto Where=O->TryGetField(TEXT("location"));
        if (!Where) continue;
        FVector Point;
        if (Where->Type==EJson::String)
        {
            const FString Anchor=Where->AsString();
            if (Anchor==TEXT("tea")) Point=District->GetTeaCounter();
            else if (Anchor==TEXT("press")) Point=District->GetPressCounter();
            else if (Anchor==TEXT("wardrobe")) Point=FRiverfrontFirstMorningWorld::WardrobePoint();
            else if (Anchor==TEXT("fitting-room")) Point=FRiverfrontFirstMorningWorld::FittingRoomPoint();
            else if (Anchor==TEXT("cloth")) Point=FRiverfrontFirstMorningWorld::ShopPoint();
            else continue;
            if (Point.IsNearlyZero()) continue;
        }
        else if (Where->Type==EJson::Array && Where->AsArray().Num()==3)
        {
            const auto& XYZ=Where->AsArray(); Point=FVector(XYZ[0]->AsNumber(),XYZ[1]->AsNumber(),XYZ[2]->AsNumber());
        }
        else continue;
        if (!SabarmatiDistrict::Contains(Point,bCitySites)) continue;
        const auto Kind=Type==TEXT("Tea")?ERiverSiteKind::Tea:Type==TEXT("Print")?ERiverSiteKind::Print:
            Type==TEXT("Service")?ERiverSiteKind::Service:Type==TEXT("Wardrobe")?ERiverSiteKind::Wardrobe:
            Type==TEXT("FittingRoom")?ERiverSiteKind::FittingRoom:
            Type==TEXT("ClothesShop")?ERiverSiteKind::ClothesShop:ERiverSiteKind::Visit;
        double Heading=0.;
        TOptional<double> Travel;
        if (bCitySites && O->TryGetNumberField(TEXT("street_heading"),Heading) && FMath::IsFinite(Heading))
        {
            if (!District->HasStreetDistricts()) continue;
            Travel=Heading;
        }
        Sites.Add({FName(*Id),Name,Action,Discovery,Point,Kind,float(FMath::Clamp(Reach,50.,1100.)),Travel});
    }
}
bool ARiverfrontPlayerController::CanRestAtBed() const
{
    return bLifeActive && !bWaking && !bCitySites && Cast<ARiverfrontCharacter>(GetPawn())
        && FVector::DistSquared(GetPawn()->GetActorLocation(),FRiverfrontFirstMorningWorld::Bedside())<FMath::Square(105.);
}

ARiverfrontPlayerController::FInteractionTarget ARiverfrontPlayerController::FindInteraction() const
{
    const APawn* Pawn=GetPawn();
    if (!Pawn || IsShowingMenu() || bWaking) return {};
    const auto* Walker=Cast<ARiverfrontCharacter>(Pawn);
    const bool bDriving=Pawn->IsA<ARiverfrontCar>();
    if (!bDriving && (!Walker || !Walker->CanUseWorldInteraction())) return {};

    const FVector Origin=Pawn->GetActorLocation();
    const FVector Eye=Origin+FVector(0,0,55.);
    const FVector Facing=FRotator(0,GetControlRotation().Yaw,0).Vector();
    const bool bRecent=ShownInteractionPawn.Get()==Pawn && GFrameCounter-ShownInteractionFrame<=2
        && FVector::DistSquared(Origin,ShownInteractionLocation)<FMath::Square(60.);
    FInteractionTarget Best;
    double BestScore=-1.;
    const auto Consider=[&](FInteractionTarget Candidate,double Reach,bool bPlace)
    {
        const FVector Delta=Candidate.Anchor-Origin;
        const double Range=Delta.Size2D();
        // At the marker itself there is no useful direction to face.
        const bool bAtPlace=bPlace && Range<=38.;
        const double Alignment=FVector::DotProduct(Facing,Delta.GetSafeNormal2D());
        if (!bAtPlace && !bDriving && Alignment<.29) return;
        const double Score=(bAtPlace || bDriving ? .87 : Alignment)-.42*FMath::Min(Range/Reach,1.)
            +(bRecent && ShownInteraction.Matches(Candidate) ? .08 : 0.);
        if (Score<=BestScore) return;
        FCollisionQueryParams Sight(SCENE_QUERY_STAT(RiverfrontInteractionSight),false,Pawn);
        if (const auto* Actor=Candidate.Actor.Get()) Sight.AddIgnoredActor(Actor);
        if (GetWorld()->LineTraceTestByChannel(Eye,Candidate.Anchor,ECC_Visibility,Sight)) return;
        BestScore=Score;
        Best=MoveTemp(Candidate);
    };
    if (CanRestAtBed()) Consider({EInteraction::Bed,nullptr,INDEX_NONE,FRiverfrontFirstMorningWorld::Bedside()+FVector(0,0,36)},105.,true);
    for (int32 Index=0;Index<Sites.Num();++Index)
    {
        const auto& Site=Sites[Index];
        if ((Site.Kind==ERiverSiteKind::Service)!=bDriving || FVector::DistSquared(Origin,Site.Point)>FMath::Square(Site.Reach)) continue;
        Consider({EInteraction::Place,nullptr,Index,Site.Point+FVector(0,0,42)},Site.Reach,true);
    }
    if (Walker)
    {
        for (const auto& Neighbour : Neighbours)
            if (auto* Person=Neighbour.Get(); Person && !Person->IsHidden()
                && FVector::DistSquared(Origin,Person->GetActorLocation()+FVector(0,0,90))<FMath::Square(170.))
                Consider({EInteraction::Pedestrian,Person,INDEX_NONE,Person->GetActorLocation()+FVector(0,0,136)},170.,false);
        if (const auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>())
        {
            Mode->ForEachEnterableCar(*Walker,[&](ARiverfrontCar& Car)
            {
                Consider({EInteraction::ParkedCar,&Car,INDEX_NONE,Car.DoorInteractionPoint(Origin)+FVector(0,0,44)},148.,false);
            });
            if (const auto* Traffic=Mode->GetTraffic())
                Traffic->ForEachEnterableVehicle(*Walker,[&](ARiverfrontRoadAgent& Agent)
                {
                    Consider({EInteraction::Traffic,&Agent,INDEX_NONE,Agent.AccessPoint(Origin)+FVector(0,0,46)},152.,false);
                });
        }
    }
    return Best;
}

ARiverfrontPlayerController::FInteractionTarget ARiverfrontPlayerController::ConfirmInteraction() const
{
    const FInteractionTarget Current=FindInteraction();
    // A change between drawing a prompt and pressing the key must not trigger a different action.
    if (const APawn* Pawn=GetPawn(); Pawn && ShownInteractionPawn.Get()==Pawn && GFrameCounter-ShownInteractionFrame<=2
        && FVector::DistSquared(Pawn->GetActorLocation(),ShownInteractionLocation)<FMath::Square(60.)
        && !ShownInteraction.Matches(Current)) return {};
    return Current;
}

FRiverfrontInteractionPrompt ARiverfrontPlayerController::DescribeInteraction(const FInteractionTarget& Target) const
{
    FRiverfrontInteractionPrompt Prompt;
    if (Target.Kind==EInteraction::None) return Prompt;
    Prompt.Binding=TEXT("Interact");
    Prompt.Anchor=Target.Anchor;
    switch (Target.Kind)
    {
    case EInteraction::Bed:
        Prompt.Action=TEXT("Sleep until morning");
        Prompt.Subject=TEXT("Flat 2B");
        // Place the marker above the middle of the quilt.
        Prompt.Anchor=FRiverfrontFirstMorningWorld::Pillow()+FVector(-38,-58,16);
        break;
    case EInteraction::Place:
        if (const auto* Site=Sites.IsValidIndex(Target.Site) ? &Sites[Target.Site] : nullptr)
        {
            Prompt.Subject=Site->Name;
            Prompt.Action=Site->Action;
            if (Site->Kind==ERiverSiteKind::Wardrobe) Prompt.Anchor=FRiverfrontFirstMorningWorld::MirrorCentre();
            if (Site->Kind==ERiverSiteKind::FittingRoom) Prompt.Anchor=FRiverfrontFirstMorningWorld::ShopMirrorCentre();
            if (!bCitySites && bParcelInHand && Site->Kind==ERiverSiteKind::Tea) Prompt.Action=TEXT("Deliver the print order");
            if (!bCitySites && bLifeActive && !bParcelInHand && !bParcelDelivered && Site->Kind==ERiverSiteKind::Print)
                Prompt.Action=TEXT("Pick up the print order");
            if (const auto* Car=Cast<ARiverfrontCar>(GetPawn()))
            {
                Prompt.Binding=TEXT("UsePlace");
                Prompt.Anchor.Reset();
                if (Car->GetSpeedKph()>1.f)
                {
                    Prompt.Action=TEXT("Park to refuel and repair");
                    Prompt.Binding=NAME_None;
                }
            }
        }
        break;
    case EInteraction::Pedestrian:
        Prompt.Action=TEXT("Say hello");
        if (const auto* Person=Cast<ARiverfrontPedestrian>(Target.Actor.Get()))
        {
            if (Person->IsVisitingPrintmaker()) Prompt.Subject=TEXT("Amina");
            else if (Person->IsInternationalVisitor()) Prompt.Subject=TEXT("Inês");
        }
        break;
    case EInteraction::ParkedCar:
        if (const auto* Car=Cast<ARiverfrontCar>(Target.Actor.Get()))
            Prompt.Action=FString::Printf(TEXT("Enter %s"),Car->GetVehicleLabel());
        break;
    case EInteraction::Traffic:
        if (const auto* Agent=Cast<ARiverfrontRoadAgent>(Target.Actor.Get()))
            Prompt.Action=FString::Printf(TEXT("Drive %s"),Agent->GetVehicleLabel());
        break;
    default: break;
    }
    return Prompt;
}

FRiverfrontInteractionPrompt ARiverfrontPlayerController::UpdateInteractionPrompt()
{
    ShownInteraction=FindInteraction();
    ShownInteractionPawn=GetPawn();
    if (const APawn* Pawn=GetPawn()) ShownInteractionLocation=Pawn->GetActorLocation();
    ShownInteractionFrame=GFrameCounter;
    auto Prompt=DescribeInteraction(ShownInteraction);
    if (Prompt.Action.IsEmpty())
        if (const auto* Car=Cast<ARiverfrontCar>(GetPawn()); Car && Car->IsStoppedForExit() && Car->IsOccupied())
            Prompt={TEXT("Exit vehicle"),Car->GetVehicleLabel(),TEXT("Interact"),{}};
    return Prompt;
}

FString ARiverfrontPlayerController::PlacePrompt() const
{
    return DescribeInteraction(FindInteraction()).Action;
}

void ARiverfrontPlayerController::Interact()
{
    UseInteraction(ConfirmInteraction(),false);
}

void ARiverfrontPlayerController::OpenFocusedVehicleDoor()
{
    const FInteractionTarget Target=ConfirmInteraction();
    if (auto* Car=Cast<ARiverfrontCar>(Target.Actor.Get())) Car->OperateDoor(GetPawn()->GetActorLocation());
}

void ARiverfrontPlayerController::Notice(const FString& Text)
{
    InteractionMessage=Text;MessageUntil=GetWorld()->GetTimeSeconds()+5.;
}
bool ARiverfrontPlayerController::SaveExploration()
{
    if (!HasLocalProfile() || bUnreadableProfile) return false;
    return RiverfrontLife::MergeExploration(ProfileSlot, LocalDisplayName, Visited, BestTrial);
}
void ARiverfrontPlayerController::InteractWithPlace()
{
    UseInteraction(ConfirmInteraction(),true);
}

void ARiverfrontPlayerController::UseInteraction(const FInteractionTarget& Target,bool bPlaceOnly)
{
    if (Target.Kind==EInteraction::ParkedCar || Target.Kind==EInteraction::Traffic)
    {
        auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
        if (bPlaceOnly || !Walker) return;
        if (auto* Car=Cast<ARiverfrontCar>(Target.Actor.Get())) Car->Enter(*Walker);
        else if (auto* Agent=Cast<ARiverfrontRoadAgent>(Target.Actor.Get()))
            if (const auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>(); Mode && Mode->GetTraffic())
                Mode->GetTraffic()->EnterVehicle(*Walker,*Agent);
        return;
    }
    if (Target.Kind==EInteraction::Bed) { PlayWakeup(); return; }
    if (Target.Kind==EInteraction::Pedestrian)
    {
        if (auto* Person=Cast<ARiverfrontPedestrian>(Target.Actor.Get())) GreetCompanion(*Person);
        return;
    }
    if (Target.Kind!=EInteraction::Place || !Sites.IsValidIndex(Target.Site)) return;
    const FRiverSite& Site=Sites[Target.Site];
    if (Site.Kind==ERiverSiteKind::Wardrobe) { OpenLifeMenu(ERiverfrontLifeMenu::Wardrobe); return; }
    if (Site.Kind==ERiverSiteKind::FittingRoom) { OpenLifeMenu(ERiverfrontLifeMenu::FittingRoom); return; }
    if (Site.Kind==ERiverSiteKind::ClothesShop) { OpenLifeMenu(ERiverfrontLifeMenu::Shop); return; }
    auto* Car=Cast<ARiverfrontCar>(GetPawn());
    if (Site.Kind==ERiverSiteKind::Service)
    {
        if (!Car || Car->GetSpeedKph()>1.) { Notice(TEXT("Park beside the pumps to refuel and repair.")); return; }
        Car->ServiceVehicle();
    }
    if (Site.Kind==ERiverSiteKind::Tea || Site.Kind==ERiverSiteKind::Print)
    {
        if (auto* Bell=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_TeaBell.S_TeaBell")))
            UGameplayStatics::PlaySoundAtLocation(this,Bell,Site.Point,.48,Site.Kind==ERiverSiteKind::Tea?.92:1.19);
    }
    const bool New=!Visited.Contains(Site.Id);
    if (New) Visited.Add(Site.Id);
    const bool Saved=!New || SaveExploration();
    if (!Saved) Visited.Remove(Site.Id);
    if (Saved && UpdatePrintDelivery(Site.Kind)) return;
    Notice(Saved?Site.Discovery:TEXT("This visit could not be saved to the local profile."));
}
void ARiverfrontPlayerController::CycleDestination()
{
    if (IsShowingMenu() || Sites.IsEmpty()) return;
    SelectedSite=(SelectedSite+2)%(Sites.Num()+1)-1;
    Notice(SelectedSite==INDEX_NONE?FString(TEXT("Waypoint cleared.")):Sites[SelectedSite].Name+
        (Sites[SelectedSite].StreetHeading.IsSet()?TEXT("    V  travel"):TEXT("")));
}
const FRiverSite* ARiverfrontPlayerController::Destination() const
{
    return Sites.IsValidIndex(SelectedSite)?&Sites[SelectedSite]:nullptr;
}
FVector ARiverfrontPlayerController::NextWaypoint() const
{
    if (Checkpoints.IsValidIndex(NextCheckpoint)) return Checkpoints[NextCheckpoint];
    const auto* Site=Destination(); if (!Site || !GetPawn()) return FVector::ZeroVector;
    const FVector Here=GetPawn()->GetActorLocation(), End=Site->Point;
    if (bCitySites) return End;
    const auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    if (Walker && Walker->GetCharacterMovement()->IsSwimming()) return End;
    const bool Driving=Cast<ARiverfrontCar>(GetPawn())!=nullptr;
    const double Height=Walker?Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():68.;
    return RiverfrontNavigation::Waypoint(Here,End,Driving,Height);
}
FString ARiverfrontPlayerController::NavigationText() const
{
    if (!GetPawn()) return FString();
    const bool Challenge=Checkpoints.IsValidIndex(NextCheckpoint);
    if (!Challenge && !Destination()) return FString();
    const double M=FVector::Dist2D(GetPawn()->GetActorLocation(),NextWaypoint())/100.;
    FString Name=Challenge?FString::Printf(TEXT("Gate %d/%d"),NextCheckpoint+1,Checkpoints.Num()):Destination()->Name;
    if (!Challenge && !NextWaypoint().Equals(Destination()->Point,1.)) Name+=TEXT(" · via bridge");
    return FString::Printf(TEXT("%s   %.0f m"),*Name,M);
}
FString ARiverfrontPlayerController::ActivityText() const
{
    if (GetWorld()->GetTimeSeconds()<MessageUntil) return InteractionMessage;
    if (NextCheckpoint!=INDEX_NONE) return FString::Printf(TEXT("River run   %.1f s   /   G to end"),GetWorld()->GetTimeSeconds()-TrialStart);
    return FString();
}
void ARiverfrontPlayerController::ToggleChallenge()
{
    if (IsShowingMenu()) return;
    if (NextCheckpoint!=INDEX_NONE) { NextCheckpoint=INDEX_NONE; Notice(TEXT("River run ended.")); return; }
    if (bCitySites || !Cast<ARiverfrontCar>(GetPawn())) { Notice(TEXT("Enter a vehicle on the riverfront to start the river run.")); return; }
    Checkpoints={FVector(-3600,3970,70),FVector(3100,3980,70),FVector(8700,3980,70),FVector(19400,4000,70)};
    if (FVector::Dist2D(GetPawn()->GetActorLocation(),Checkpoints[0])>2000.)
    { Notice(TEXT("The river run starts at the parked cars by Atal Bridge.")); return; }
    NextCheckpoint=0; TrialStart=GetWorld()->GetTimeSeconds(); Notice(TEXT("River run started. Follow the four gates and stop at the last."));
}
void ARiverfrontPlayerController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if (IsShowingMenu() || !Checkpoints.IsValidIndex(NextCheckpoint)) return;
    const auto* Car=Cast<ARiverfrontCar>(GetPawn());
    if (!Car) { NextCheckpoint=INDEX_NONE; Notice(TEXT("River run ended when you left the car.")); return; }
    if (FVector::Dist2D(Car->GetActorLocation(),Checkpoints[NextCheckpoint])>510.) return;
    const bool Finish=NextCheckpoint+1==Checkpoints.Num();
    if (Finish && Car->GetSpeedKph()>2.) { Notice(TEXT("Stop at the last gate to finish.")); return; }
    if (++NextCheckpoint<Checkpoints.Num()) return;
    NextCheckpoint=INDEX_NONE;
    const float Time=GetWorld()->GetTimeSeconds()-TrialStart;
    const float Prior=BestTrial;
    if (BestTrial<=0 || Time<BestTrial) BestTrial=Time;
    if (!SaveExploration()) { BestTrial=Prior; Notice(TEXT("Run finished. The local record could not be saved.")); return; }
    Notice(FString::Printf(TEXT("River run finished in %.1f s. Personal best %.1f s."),Time,BestTrial));
}

#if !UE_BUILD_SHIPPING
bool ARiverfrontPlayerController::VerifyReviewProfile(float& RecordedTime) const
{
    if (!ProfileSlot.StartsWith(TEXT("SabarmatiQA-"))) return false;
    const auto* Saved=Cast<URiverfrontLocalProfile>(UGameplayStatics::LoadGameFromSlot(ProfileSlot,0));
    if (!Saved || !Saved->VisitedSites.Contains(TEXT("tea")) || !Saved->VisitedSites.Contains(TEXT("service"))) return false;
    RecordedTime=Saved->BestRiverRunSeconds;
    return RecordedTime>0.f && FMath::IsNearlyEqual(RecordedTime,BestTrial);
}
#endif

void ARiverfrontPlayerController::TravelToDestination()
{
    const auto* Stop=Destination();
    if (IsShowingMenu() || !Stop || !Stop->StreetHeading.IsSet() || !GetPawn()) return;
    auto* Car=Cast<ARiverfrontCar>(GetPawn());
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    if (Car && Car->GetSpeedKph()>1.f) { Notice(TEXT("Stop before travelling to a city street.")); return; }
    TActorIterator<ARiverfrontWorld> World(GetWorld());
    const FVector Before=GetPawn()->GetActorLocation();
#if !UE_BUILD_SHIPPING
    const double BeginTravel=FPlatformTime::Seconds();
#endif
    if (!World || !World->PrepareCityTravel(Stop->Point)) return;
    const double Yaw=Stop->StreetHeading.GetValue();
    const FRotator Heading(0,Yaw,0);
    bool Moved=false;
    if (Car) Moved=Car->TravelToStreet(Stop->Point,Yaw);
    else if (Walker)
    {
        FVector Surface;
        if (World->FindCityFootway(Stop->Point,Yaw,Surface))
        {
            const FVector Feet=Surface+FVector(0,0,Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.);
            Moved=Walker->TeleportTo(Feet,Heading);
            if (Moved) Walker->GetCharacterMovement()->StopMovementImmediately();
        }
    }
    if (!Moved) { World->PrepareCityTravel(Before); Notice(TEXT("The arrival point is occupied.")); return; }
    SetControlRotation(FRotator(-6,Yaw,0));
    Notice(Stop->Name);
#if !UE_BUILD_SHIPPING
    LastStreetTravelMs=(FPlatformTime::Seconds()-BeginTravel)*1000.;
#endif
}
