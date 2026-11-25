#include "RiverfrontPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "SabarmatiDistrict.h"
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontLifeProfile.h"
#include "RiverfrontLocalProfile.h"
#include "RiverfrontTraffic.h"
#include "RiverfrontWorld.h"
#include "Components/PoseableMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"

void ARiverfrontPlayerController::RunRiverReview()
{
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    const auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    if (!Walker || !Mode || !Mode->GetTraffic())
    { FPlatformMisc::RequestExitWithStatus(false,1); return; }
    if (GetWorld()->URL.HasOption(TEXT("SabarmatiReturnHome")))
    {
        const auto* Profile=RiverfrontLife::Load(ProfileSlot);
        const bool Home=FVector::Dist(Walker->GetActorLocation(),FRiverfrontFirstMorningWorld::HomeStart())<60.
            && Profile && Profile->CashRupees==100000 && Profile->BankRupees==500000;
        UE_LOG(LogTemp,Display,TEXT("Sabarmati river review: home and balances restored=%d"),Home);
        FPlatformMisc::RequestExitWithStatus(false,Home?0:1); return;
    }
    const bool City=Mode->IsCityMode();
    const FVector Centre=SabarmatiDistrict::Center(City);
    for (int32 Axis=0;Axis<2;++Axis) for (double Side:{-1.,1.})
    {
        FVector Edge=Centre+FVector(0,0,400), Normal=FVector::ZeroVector;
        Normal[Axis]=Side; Edge+=Normal*SabarmatiDistrict::HalfWidth;
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit,Edge-Normal*500.,Edge+Normal*500.,ECC_Pawn)
            || FVector::Dist2D(Hit.ImpactPoint,Edge)>3.)
        { UE_LOG(LogTemp,Error,TEXT("River review: missing district edge")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    }
    FVector Water(0,-9000,SabarmatiDistrict::SurfaceZ);
    if (City)
    {
        double Best=DBL_MAX;
        for (TActorIterator<APhysicsVolume> It(GetWorld());It;++It)
        {
            const auto Bounds=It->GetRootComponent()->Bounds;
            if (!It->bWaterVolume || Bounds.BoxExtent.X<440 || Bounds.BoxExtent.Y<440) continue;
            const double D=FVector::Dist2D(Bounds.Origin,Walker->GetActorLocation());
            if (D<Best) {Water.X=Bounds.Origin.X; Water.Y=Bounds.Origin.Y; Best=D;}
        }
        if (Best==DBL_MAX) { FPlatformMisc::RequestExitWithStatus(false,1);return; }
    }
    FHitResult Bed;
    if (!GetWorld()->LineTraceSingleByObjectType(Bed,Water,Water-FVector(0,0,350),FCollisionObjectQueryParams(ECC_WorldStatic))
        || !FMath::IsNearlyEqual(Bed.ImpactPoint.Z,SabarmatiDistrict::BedZ,1.))
    { UE_LOG(LogTemp,Error,TEXT("River review: missing bed")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    UE_LOG(LogTemp,Display,TEXT("Sabarmati river review: four boundaries, surface %.0f cm, bed %.0f cm."),Water.Z,Bed.ImpactPoint.Z);
    Walker->TeleportTo(Water+FVector(0,0,105),FRotator::ZeroRotator);
    SetControlRotation(FRotator::ZeroRotator);
    const double Start=GetWorld()->GetTimeSeconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,Walker,Water,Start,Stage=0,Since=Start,From=Water](float) mutable
        {
            const double Now=GetWorld()->GetTimeSeconds();
            const auto Key=[&](FKey K,EInputEvent Event){InputKey(FInputKeyEventArgs(GetWorld()->GetGameViewport()->Viewport,
                FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64()));};
            if (Now-Start>34.)
            {
                UE_LOG(LogTemp,Error,TEXT("River review timeout stage=%d at %s mode=%d %s"),Stage,*Walker->GetActorLocation().ToString(),
                    int32(Walker->GetCharacterMovement()->MovementMode),*Walker->GetWaterHint());
                FPlatformMisc::RequestExitWithStatus(false,1);return false;
            }
            if (Stage==0 && Walker->GetCharacterMovement()->IsSwimming())
            {
                From=Walker->GetActorLocation();
                Key(EKeys::W,IE_Pressed); Stage=1;
            }
            else if (Stage==1 && FVector::Dist2D(From,Walker->GetActorLocation())>=220.)
            {
                if (!Walker->GetCharacterMovement()->IsSwimming()) {FPlatformMisc::RequestExitWithStatus(false,1);return false;}
                UE_LOG(LogTemp,Display,TEXT("Sabarmati river review: swam %.2f m"),FVector::Dist2D(From,Walker->GetActorLocation())*.01);
                Key(EKeys::W,IE_Released); Key(EKeys::LeftShift,IE_Pressed); Stage=2;
            }
            else if (Stage==2 && Walker->GetActorLocation().Z<Water.Z-105.)
            {
                Key(EKeys::LeftShift,IE_Released); Stage=3;
            }
            else if (Stage==3 && Walker->GetActorLocation().Z>Water.Z-60.)
            {
                UE_LOG(LogTemp,Display,TEXT("Sabarmati river review: resurfaced; starting breath test."));
                Key(EKeys::LeftShift,IE_Pressed); Stage=4; Since=Now;
            }
            else if (Stage==4 && Walker->GetWaterHint().StartsWith(TEXT("Drowned")))
            {
                UE_LOG(LogTemp,Display,TEXT("Sabarmati river review: drowning after %.2f s."),Now-Since);
                return false;
            }
            return true;
        }));
}
#endif
