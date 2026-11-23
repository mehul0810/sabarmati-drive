#include "RiverfrontPlayerController.h"
#include "RiverfrontCar.h"
#include "RiverfrontWorld.h"
#include "RiverfrontLocalProfile.h"
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
    if (!Doc->TryGetArrayField(bCitySites?TEXT("city"):TEXT("riverfront"),Rows)) return;
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
            else continue;
            if (Point.IsNearlyZero()) continue;
        }
        else if (Where->Type==EJson::Array && Where->AsArray().Num()==3)
        {
            const auto& XYZ=Where->AsArray(); Point=FVector(XYZ[0]->AsNumber(),XYZ[1]->AsNumber(),XYZ[2]->AsNumber());
        }
        else continue;
        const auto Kind=Type==TEXT("Tea")?ERiverSiteKind::Tea:Type==TEXT("Print")?ERiverSiteKind::Print:
            Type==TEXT("Service")?ERiverSiteKind::Service:ERiverSiteKind::Visit;
        Sites.Add({FName(*Id),Name,Action,Discovery,Point,Kind,float(FMath::Clamp(Reach,50.,800.))});
    }
}
int32 ARiverfrontPlayerController::NearbySite() const
{
    const APawn* Pawn=GetPawn();
    if (!Pawn) return INDEX_NONE;
    const bool Seated=Cast<ARiverfrontCar>(Pawn)!=nullptr;
    double Nearest=TNumericLimits<double>::Max();
    int32 Found=INDEX_NONE;
    for (int32 I=0;I<Sites.Num();++I)
    {
        const auto& Site=Sites[I];
        if ((Site.Kind==ERiverSiteKind::Service)!=Seated) continue;
        const double D=FVector::DistSquared(Pawn->GetActorLocation(),Site.Point);
        if (D<Nearest && D<FMath::Square(Site.Reach)) { Nearest=D; Found=I; }
    }
    return Found;
}
FString ARiverfrontPlayerController::PlacePrompt() const
{
    const int32 I=NearbySite();
    if (I==INDEX_NONE) return FString();
    return FString(Cast<ARiverfrontCar>(GetPawn())?TEXT("F   "):TEXT("E   "))+Sites[I].Action;
}
void ARiverfrontPlayerController::Notice(const FString& Text)
{
    InteractionMessage=Text;MessageUntil=GetWorld()->GetTimeSeconds()+5.;
}
bool ARiverfrontPlayerController::SaveExploration()
{
    if (!HasLocalProfile() || bUnreadableProfile) return false;
    auto* Save=Cast<URiverfrontLocalProfile>(UGameplayStatics::LoadGameFromSlot(ProfileSlot,0));
    if (!Save || Save->Schema!=URiverfrontLocalProfile::CurrentSchema || URiverfrontLocalProfile::CleanDisplayName(Save->DisplayName)!=LocalDisplayName) return false;
    for (const FName Here:Visited) Save->VisitedSites.AddUnique(Here);
    if (BestTrial>0 && (Save->BestRiverRunSeconds<=0 || BestTrial<Save->BestRiverRunSeconds)) Save->BestRiverRunSeconds=BestTrial;
    return UGameplayStatics::SaveGameToSlot(Save,ProfileSlot,0);
}
void ARiverfrontPlayerController::InteractWithPlace()
{
    if (IsShowingMenu()) return;
    const int32 I=NearbySite(); if (I==INDEX_NONE) return;
    const FRiverSite& Site=Sites[I];
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
    Notice(Saved?Site.Discovery:TEXT("This visit could not be saved to the local profile."));
}
void ARiverfrontPlayerController::CycleDestination()
{
    if (IsShowingMenu() || Sites.IsEmpty()) return;
    SelectedSite=(SelectedSite+2)%(Sites.Num()+1)-1;
    Notice(SelectedSite==INDEX_NONE?TEXT("Waypoint cleared."):Sites[SelectedSite].Name);
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
    // The west and east bank driveways connect only at these three road bridges.
    const bool OtherBank=(Here.Y< -16000. && End.Y>2000.) || (Here.Y>2000. && End.Y< -16000.);
    if (!bCitySites && OtherBank && Cast<ARiverfrontCar>(GetPawn()))
    {
        double Best=TNumericLimits<double>::Max(),Bridge=3150.;
        for (double X:{-64400.,3150.,59310.})
        {
            const double Travel=FMath::Abs(Here.X-X)+FMath::Abs(End.X-X);
            if (Travel<Best) { Best=Travel;Bridge=X; }
        }
        const double Entry=Here.Y>0?4090.:-26010.;
        return FVector(Bridge,FMath::Abs(Here.X-Bridge)>550.?Entry:(Here.Y>0?-26010.:4090.),90.);
    }
    return End;
}
FString ARiverfrontPlayerController::NavigationText() const
{
    if (!GetPawn()) return FString();
    const bool Challenge=Checkpoints.IsValidIndex(NextCheckpoint);
    if (!Challenge && !Destination()) return FString();
    const double M=FVector::Dist2D(GetPawn()->GetActorLocation(),NextWaypoint())/100.;
    const FString Name=Challenge?FString::Printf(TEXT("Gate %d/%d"),NextCheckpoint+1,Checkpoints.Num()):Destination()->Name;
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
