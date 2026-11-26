#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontNavigation.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void ARiverfrontPlayerController::RunNavigationReview()
{
    const auto Fail=[](const TCHAR* Reason)
    {
        UE_LOG(LogTemp,Error,TEXT("Sabarmati navigation review failed: %s"),Reason);
        FPlatformMisc::RequestExitWithStatus(false,1);
    };
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    const auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    auto* Car=Mode?Mode->GetCar():nullptr;
    if (!Walker || !Car || Mode->IsCityMode()) { Fail(TEXT("riverfront player and car")); return; }
    const auto Select=[this](FName Id)
    {
        for (int32 I=0; I<=Sites.Num(); ++I)
        {
            CycleDestination();
            if (Destination() && Destination()->Id==Id) return true;
        }
        return false;
    };
    if (!Select(TEXT("ravivari"))) { Fail(TEXT("select market destination")); return; }
    const double Height=Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Walker->SetActorLocation(FVector(-8500.,4800.,Height),false,nullptr,ETeleportType::TeleportPhysics);
    FVector Guide=NextWaypoint();
    if (!FMath::IsNearlyEqual(Guide.X,ARiverfrontWorld::AtalX,1.) || !FMath::IsNearlyEqual(Guide.Y,3410.,1.)
        || !NavigationText().Contains(TEXT("via bridge"))) { Fail(TEXT("walking approach guidance")); return; }
    Walker->SetActorLocation(FVector(ARiverfrontWorld::AtalX,RiverfrontCrossings::CentreY,Height+23.),false,nullptr,ETeleportType::TeleportPhysics);
    Guide=NextWaypoint();
    if (!FMath::IsNearlyEqual(Guide.X,ARiverfrontWorld::AtalX,1.) || !FMath::IsNearlyEqual(Guide.Y,-25330.,1.))
    { Fail(TEXT("walking bridge exit guidance")); return; }
    if (!Select(TEXT("home-wardrobe"))) { Fail(TEXT("select home")); return; }
    Guide=NextWaypoint();
    if (!FMath::IsNearlyEqual(Guide.Y,3410.,1.)) { Fail(TEXT("home guidance across bridge")); return; }

    const FVector Door=Car->GetActorTransform().TransformPosition(FVector(12.,-174.,37.));
    Walker->SetActorLocation(Door,false,nullptr,ETeleportType::TeleportPhysics);
    if (!Car->Enter(*Walker) || GetPawn()!=Car) { Fail(TEXT("enter hatchback")); return; }
    if (!Select(TEXT("ravivari"))) { Fail(TEXT("select driving destination")); return; }
    Car->SetActorLocationAndRotation(FVector(-37000.,4090.,68.),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
    Guide=NextWaypoint();
    if (!SabarmatiDistrict::Contains(Guide,false) || Guide.X<2800. || Guide.X>3150. || !FMath::IsNearlyEqual(Guide.Y,4090.,1.))
    { Fail(TEXT("driving approach stays inside district")); return; }
    Car->SetActorLocation(FVector(Guide.X,RiverfrontCrossings::CentreY,68.+RiverfrontCrossings::DeckZ(RiverfrontCrossings::CentreY)),
        false,nullptr,ETeleportType::TeleportPhysics);
    Guide=NextWaypoint();
    if (!FMath::IsNearlyEqual(Guide.Y,-26010.,1.)) { Fail(TEXT("driving bridge exit guidance")); return; }
    UE_LOG(LogTemp,Display,TEXT("Sabarmati navigation review complete: selected destinations and checked walking, home and driving bridge guidance."));
    FPlatformMisc::RequestExitWithStatus(false,0);
}
#endif
