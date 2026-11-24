#include "RiverfrontGameMode.h"
#include "DistrictLoadProgress.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "GameFramework/PlayerStart.h"
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontHUD.h"
#include "RiverfrontPedestrian.h"
#include "RiverfrontPigeon.h"
#include "RiverfrontTraffic.h"
#include "RiverfrontPlayerController.h"
#include "RiverfrontWorld.h"

ARiverfrontGameMode::ARiverfrontGameMode()
{
    DefaultPawnClass = ARiverfrontCharacter::StaticClass();
    HUDClass = ARiverfrontHUD::StaticClass();
    PlayerControllerClass = ARiverfrontPlayerController::StaticClass();
    WalkSpawn = ARiverfrontWorld::SpawnPoint;
    SpawnYaw = ARiverfrontWorld::SpawnYaw;
}

void ARiverfrontGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    if (!PlayerAvatarClass.IsNull() && !FParse::Param(FCommandLine::Get(), TEXT("SabarmatiPublicAvatar")))
    {
        UClass* Avatar = PlayerAvatarClass.Get();
        if (!Avatar && FPackageName::DoesPackageExist(PlayerAvatarClass.ToSoftObjectPath().GetLongPackageName()))
        {
            Avatar = PlayerAvatarClass.LoadSynchronous();
        }
        if (Avatar && Avatar->IsChildOf(ARiverfrontCharacter::StaticClass()) &&
            !Avatar->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            DefaultPawnClass = Avatar;
            UE_LOG(LogTemp, Display, TEXT("Sabarmati player class: %s"), *Avatar->GetPathName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Sabarmati player class %s is unavailable or incompatible; using %s."),
                *PlayerAvatarClass.ToString(), *GetNameSafe(DefaultPawnClass.Get()));
        }
    }
    bCityRequested = UGameplayStatics::HasOption(Options, TEXT("SabarmatiCity"));
#if !UE_BUILD_SHIPPING
    bCityRequested |= !UGameplayStatics::HasOption(Options, TEXT("SabarmatiRiverfront")) &&
        FParse::Param(FCommandLine::Get(), TEXT("SabarmatiCity"));
#endif
}

void ARiverfrontGameMode::BeginPlay()
{
    FVector CarSpawn = ARiverfrontWorld::CarPosition;
    double CarYaw = 0.0;
    ARiverfrontWorld* River = GetWorld()->SpawnActor<ARiverfrontWorld>();
    if (River)
    {
        if (bCityRequested)
        {
            FVector CityWalk = WalkSpawn;
            FVector CityCar = CarSpawn;
            double CityYaw = SpawnYaw;
            bCityBuilt = River->BuildCity(CityWalk, CityCar, CityYaw);
            if (bCityBuilt)
            {
                WalkSpawn = CityWalk;
                CarSpawn = CityCar;
                SpawnYaw = CarYaw = CityYaw;
                if (RiverStart)
                {
                    const FRotator Facing(0.0, SpawnYaw, 0.0);
                    RiverStart->SetActorLocationAndRotation(WalkSpawn, Facing);
                    // Standalone startup can already have chosen a spawn before world construction.
                    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
                    {
                        Pawn->TeleportTo(WalkSpawn, Facing, false, true);
                    }
                }
            }
        }
        if (!bCityBuilt)
        {
            River->Build();
            ARiverfrontPedestrian::SpawnRiverfrontCrowd(*GetWorld());
            ARiverfrontPigeon::SpawnPromenadeFlock(*River);
        }
    }

    DistrictLoadProgress::Reached(4);
    const FRotator CarFacing(0.0, CarYaw, 0.0);
    Car = GetWorld()->SpawnActor<ARiverfrontCar>(CarSpawn, CarFacing);
    if (IsValid(Car) && IsValid(River) && FPackageName::DoesPackageExist(
        TEXT("/Game/Sabarmati/Riverfront/Vehicles/SM_SabarmatiCrossover")))
    {
        // The Atal zebra crossing sits 11.7 m behind the hatch. Keep the second car's
        // rear clear of the stripes as well as leaving room to exit the first car.
        const double RiverfrontSpacings[] = {620., 705., 765.};
        const double CitySpacings[] = {900., 1160., 1460.};
        for (const double Distance : bCityBuilt ? CitySpacings : RiverfrontSpacings)
        {
            if (ARiverfrontCar* Parked = SpawnCrossover(*River, CarSpawn - CarFacing.Vector() * Distance, CarFacing))
            {
                Crossover = Parked;
                break;
            }
        }
    }
    DistrictLoadProgress::Reached(5);
    Traffic=GetWorld()->SpawnActor<ARiverfrontTraffic>();
    if (Traffic) Traffic->Start(bCityBuilt, bCityBuilt ? WalkSpawn : FRiverfrontFirstMorningWorld::StreetPoint());
    Super::BeginPlay();
    DistrictLoadProgress::Reached(6);
}

ARiverfrontTraffic* ARiverfrontGameMode::GetTraffic() const { return Traffic.Get(); }

ARiverfrontCar* ARiverfrontGameMode::SpawnCrossover(ARiverfrontWorld& WorldActor,
    const FVector& Position, const FRotator& Facing)
{
    UWorld* World = GetWorld();
    const FVector Extents = ARiverfrontCar::GetCrossoverBounds();
    const FQuat Orientation = Facing.Quaternion();
    // Validate both axles and both sides. Decorative meshes and water aren't road surfaces.
    for (const double X : {-160.0, 160.0})
    {
        for (const double Y : {-81.0, 81.0})
        {
            const FVector Spot = Position + Orientation.RotateVector(FVector(X, Y, 0.0));
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, Spot + FVector(0, 0, 80), Spot - FVector(0, 0, 110),
                ECC_Visibility) || Hit.GetActor() != &WorldActor || FMath::Abs(Hit.ImpactPoint.Z) > 3.0)
            {
                return nullptr;
            }
        }
    }
    if (World->OverlapBlockingTestByProfile(Position, Orientation, TEXT("BlockAllDynamic"),
        FCollisionShape::MakeBox(Extents), FCollisionQueryParams::DefaultQueryParam))
    {
        return nullptr;
    }
    const FTransform Placement(Facing, Position);
    ARiverfrontCar* Result = World->SpawnActorDeferred<ARiverfrontCar>(ARiverfrontCar::StaticClass(), Placement,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (Result)
    {
        Result->UseCrossoverBody();
        UGameplayStatics::FinishSpawningActor(Result, Placement);
        UE_LOG(LogTemp, Display, TEXT("Sabarmati Drive: parked crossover at %s; road probes=4/4."),
            *Position.ToCompactString());
    }
    return Result;
}

ARiverfrontCar* ARiverfrontGameMode::FindEnterableCar(const ARiverfrontCharacter& Character) const
{
    ARiverfrontCar* Result = nullptr;
    double Nearest = TNumericLimits<double>::Max();
    for (ARiverfrontCar* Candidate : {Car.Get(), Crossover.Get()})
    {
        if (!IsValid(Candidate) || !Candidate->IsNearDoor(Character))
        {
            continue;
        }
        const double Distance = FVector::DistSquared2D(Candidate->GetActorLocation(), Character.GetActorLocation());
        if (Distance < Nearest)
        {
            Nearest = Distance;
            Result = Candidate;
        }
    }
    return Result;
}

AActor* ARiverfrontGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (!IsValid(RiverStart))
    {
        RiverStart = GetWorld()->SpawnActor<APlayerStart>(
            WalkSpawn, FRotator(0, SpawnYaw, 0));
    }
    return RiverStart ? RiverStart.Get() : Super::ChoosePlayerStart_Implementation(Player);
}
