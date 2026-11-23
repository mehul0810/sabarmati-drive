#include "RiverfrontPedestrian.h"
#include "RiverfrontResidentOrientation.h"
#include "EngineUtils.h"

#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RiverfrontWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontTraffic.h"
#include "RiverfrontCar.h"
#include "Misc/PackageName.h"

namespace
{
struct FRouteSample
{
    FVector Position;
    FVector Forward;
};

struct FPedestrianRoute
{
    FVector Start;
    FVector End;
    double Radius;
    double StartFraction;
    double Speed;
};

constexpr const TCHAR* AvatarMeshPath = TEXT("/Game/Sabarmati/Avatar/SK_Sabarmati.SK_Sabarmati");
constexpr const TCHAR* WalkPath = TEXT("/Game/Sabarmati/Avatar/A_Walk.A_Walk");
constexpr const TCHAR* WalkAltPath = TEXT("/Game/Sabarmati/Avatar/A_WalkAlt.A_WalkAlt");

// Rounded ends keep position and heading continuous, without frequent reversal animations.
FRouteSample SampleCircuit(const FVector& A, const FVector& B, double Radius, double Distance)
{
    const FVector Delta = B - A;
    const double Length = Delta.Size2D();
    const FVector Along = Delta.GetSafeNormal2D();
    const FVector Across(-Along.Y, Along.X, 0.0);
    const double ArcLength = UE_PI * Radius;

    if (Distance < Length)
    {
        return {A + Along * Distance + Across * Radius, Along};
    }
    Distance -= Length;
    if (Distance < ArcLength)
    {
        const double T = Distance / Radius;
        return {
            B + Radius * (Across * FMath::Cos(T) + Along * FMath::Sin(T)),
            Along * FMath::Cos(T) - Across * FMath::Sin(T)
        };
    }
    Distance -= ArcLength;
    if (Distance < Length)
    {
        return {B - Along * Distance - Across * Radius, -Along};
    }

    const double T = (Distance - Length) / Radius;
    return {
        A - Radius * (Across * FMath::Cos(T) + Along * FMath::Sin(T)),
        -Along * FMath::Cos(T) + Across * FMath::Sin(T)
    };
}

bool ValidateCircuit(UWorld& World, const FPedestrianRoute& Route, double& GroundZ,
    int32 RouteIndex)
{
    const double Perimeter = 2.0 * (FVector::Dist2D(Route.Start, Route.End) + UE_PI * Route.Radius);
    const int32 Samples = FMath::Max(12, FMath::CeilToInt(Perimeter / 140.0));
    const FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(RiverfrontPedestrianRoute), false);
    // Keep the clearance capsule just off the deck. It checks obstacles, not walkable floor.
    const FCollisionShape Person = FCollisionShape::MakeCapsule(33.0f, 96.0f);
    FVector PreviousCenter = FVector::ZeroVector;
    for (int32 I = 0; I <= Samples; ++I)
    {
        const FVector Sample = SampleCircuit(Route.Start, Route.End, Route.Radius,
            Perimeter * I / Samples).Position;
        FHitResult Ground;
        if (!World.LineTraceSingleByObjectType(Ground, Sample + FVector(0, 0, 20),
            Sample - FVector(0, 0, 30), StaticOnly, Query) ||
            Ground.ImpactNormal.Z < .8 || FMath::Abs(Ground.ImpactPoint.Z - Route.Start.Z) > 12.0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Sabarmati crowd: route %d missing walkable floor at %s."),
                RouteIndex, *Sample.ToCompactString());
            return false;
        }
        if (I == 0)
        {
            GroundZ = Ground.ImpactPoint.Z;
        }
        else if (FMath::Abs(Ground.ImpactPoint.Z - GroundZ) > 6.0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Sabarmati crowd: route %d changes height at %s."),
                RouteIndex, *Sample.ToCompactString());
            return false;
        }

        const FVector Center(Sample.X, Sample.Y, GroundZ + 101.0);
        FHitResult Obstacle;
        if ((I == 0 && World.OverlapAnyTestByObjectType(Center, FQuat::Identity, StaticOnly, Person, Query)) ||
            (I > 0 && World.SweepSingleByObjectType(Obstacle, PreviousCenter, Center,
                FQuat::Identity, StaticOnly, Person, Query)))
        {
            UE_LOG(LogTemp, Warning, TEXT("Sabarmati crowd: route %d lacks clearance near %s (%s)."),
                RouteIndex, *Sample.ToCompactString(), *GetNameSafe(Obstacle.GetComponent()));
            return false;
        }
        PreviousCenter = Center;
    }
    return true;
}

UMaterialInstanceDynamic* ApplyClothingColor(USkeletalMeshComponent& Mesh, const TCHAR* SlotName, FColor Color)
{
    const int32 Slot = Mesh.GetMaterialIndex(FName(SlotName));
    if (Slot == INDEX_NONE)
    {
        return nullptr;
    }
    UMaterialInstanceDynamic* Material = Mesh.CreateDynamicMaterialInstance(Slot);
    if (Material)
    {
        // This is the color parameter on the imported glTF garment material.
        Material->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(Color));
    }
    return Material;
}
} // namespace

ARiverfrontPedestrian::ARiverfrontPedestrian()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PedestrianMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetCullDistance(14000.0f);
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Mesh->bEnableUpdateRateOptimizations = true;
}

int32 ARiverfrontPedestrian::SpawnRiverfrontCrowd(UWorld& World)
{
    const bool Public=FPackageName::DoesPackageExist(TEXT("/Game/Sabarmati/Riverfront/People/SK_RiverResident"));
    USkeletalMesh* Model=LoadObject<USkeletalMesh>(nullptr,Public?TEXT("/Game/Sabarmati/Riverfront/People/SK_RiverResident.SK_RiverResident"):AvatarMeshPath);
    UAnimSequence* Walk=LoadObject<UAnimSequence>(nullptr,Public?TEXT("/Game/Sabarmati/Riverfront/People/A_Walk.A_Walk"):WalkPath);
    UAnimSequence* WalkAlt=LoadObject<UAnimSequence>(nullptr,Public?TEXT("/Game/Sabarmati/Riverfront/People/A_WalkAlt.A_WalkAlt"):WalkAltPath);
    if (!Model || !Walk || Walk->GetSkeleton()!=Model->GetSkeleton())
    {
        UE_LOG(LogTemp,Warning,TEXT("Sabarmati crowd: walk mesh or clip unavailable."));
        return 0;
    }
    if (!WalkAlt || WalkAlt->GetSkeleton()!=Model->GetSkeleton()) WalkAlt=Walk;

    const bool HasKurta=FPackageName::DoesPackageExist(TEXT("/Game/Sabarmati/Riverfront/People/Kurta/SK_RiverResidentKurta"));
    auto* Second=HasKurta?LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Sabarmati/Riverfront/People/Kurta/SK_RiverResidentKurta.SK_RiverResidentKurta")):nullptr;
    auto* SecondWalk=HasKurta?LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Sabarmati/Riverfront/People/Kurta/A_WalkAlt.A_WalkAlt")):nullptr;
    const bool SecondReady=Second && SecondWalk && SecondWalk->GetSkeleton()==Second->GetSkeleton();
    // The approach in front of spawn, both banks and independently paced bridge walkers.
    // Wide loops stay away from road edges; landing bypasses fit between curb and bollards.
    const double Bridge = ARiverfrontWorld::AtalX;
    const FPedestrianRoute Routes[] = {
        {{-6850, 3504, 2}, {-4650, 3504, 2}, 38, .71, 131},
        {{-4100, 3060, 2}, {-980, 3060, 2}, 75, .25, 123},
        {{-640, 3050, 2}, {2120, 3050, 2}, 42, .20, 143},
        {{4200, 3005, 2}, {6700, 3005, 2}, 38, .62, 117},
        {{-7130, 2360, -84}, {-6070, 2360, -84}, 63, .34, 121},
        {{-6800, -25424, 2}, {-4560, -25424, 2}, 38, .52, 119},
        {{-4110, -24980, 2}, {-130, -24980, 2}, 85, .27, 137},
        {{4280, -25040, 2}, {6800, -25040, 2}, 79, .74, 126},
        {{Bridge, 2710, 23}, {Bridge, 770, 23}, 46, .23, 109},
        {{Bridge + 184, 2880, 23}, {Bridge + 184, -1780, 23}, 69, .09, 133},
        {{Bridge - 195, -2940, 23}, {Bridge - 195, 2800, 23}, 76, .44, 121},
        {{Bridge + 160, -5900, 23}, {Bridge + 160, -12900, 23}, 77, .45, 142},
        {{Bridge - 184, -12900, 23}, {Bridge - 184, -5900, 23}, 70, .18, 128},
        {{Bridge + 177, -15400, 23}, {Bridge + 177, -24740, 23}, 75, .70, 131},
        {{Bridge - 184, -24680, 23}, {Bridge - 184, -15400, 23}, 75, .32, 115},
        {{12800, 3230, 2}, {18600, 3230, 2}, 78, .28, 124},
        {{-23800, -25130, 2}, {-16700, -25130, 2}, 80, .61, 129},
    };

    int32 Count = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Routes); ++Index)
    {
        const FPedestrianRoute& Route = Routes[Index];
        double GroundZ = 0.0;
        if (!ValidateCircuit(World, Route, GroundZ, Index))
        {
            continue;
        }

        FVector GroundedStart = Route.Start;
        GroundedStart.Z = GroundZ;
        FVector GroundedEnd = Route.End;
        GroundedEnd.Z = GroundZ;
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ARiverfrontPedestrian* Walker = World.SpawnActor<ARiverfrontPedestrian>(
            GroundedStart, FRotator::ZeroRotator, Parameters))
        {
            const bool Alternate=SecondReady && Index%3!=0;
            Walker->Configure(Alternate?Second:Model, Alternate?SecondWalk:(Index%3==1?WalkAlt:Walk), GroundedStart, GroundedEnd,
                Route.Radius, Route.StartFraction, Route.Speed, Index);
            ++Count;
        }
    }

    // Shop customers use the very same physical doorways and counter aisles as the player.
    if (TActorIterator<ARiverfrontWorld> District(&World); District)
    {
        for (const bool Tea:{true,false})
        {
            const auto& Path=District->GetShopVisit(Tea);
            if (Path.Num()<3) continue;
            if (auto* Customer=World.SpawnActor<ARiverfrontPedestrian>(Path[0],FRotator::ZeroRotator))
            {
                const bool Alternate=Tea && SecondReady;
                Customer->Configure(Alternate?Second:Model,Alternate?SecondWalk:Walk,Path[0],Path.Last(),1.,0.,Tea?102.:116.,Tea?12:7);
                Customer->Errand=Path; Customer->ErrandStep=1;
                Customer->SetActorLocation(Path[0]); Customer->RestLeft=Tea?1.8:7.2; Customer->SetMoving(false);
            }
        }
    }
    // Staggered walkers start on opposite kerbs. They enter only on the walk phase.
    for (int32 I=0;I<2;++I)
    {
        const FVector A(Bridge+(I?59.:-59.),I?4685.:3470.,2);
        const FVector B(A.X,I?3470.:4685.,2);
        if (auto* Person=World.SpawnActor<ARiverfrontPedestrian>(A,FRotator::ZeroRotator))
        {
            Person->Configure(I && SecondReady?Second:Model,I && SecondReady?SecondWalk:WalkAlt,A,B,1,0.,I?147.:132.,19+I);
            Person->bAtCrosswalk=true;
            Person->SetActorLocationAndRotation(A,FRotator(0,(B-A).Rotation().Yaw+Person->MeshYawToForward,0));
            Person->SetMoving(false);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati crowd: %d/%d animated pedestrian routes active."),
        Count, static_cast<int32>(UE_ARRAY_COUNT(Routes)));
    return Count;
}

void ARiverfrontPedestrian::Configure(USkeletalMesh* Model, UAnimSequence* Walk,
    const FVector& StartTurn, const FVector& EndTurn, double Radius, double StartFraction,
    double SpeedCmPerSecond, int32 Variant)
{
    RouteStart = StartTurn;
    RouteEnd = EndTurn;
    TurnRadius = Radius;
    RouteLength = 2.0 * (FVector::Dist2D(RouteStart, RouteEnd) + UE_PI * Radius);
    Distance = RouteLength * StartFraction;
    Speed = SpeedCmPerSecond;

    Mesh->SetSkeletalMesh(Model);
    MeshYawToForward=RiverfrontResident::YawToActorForward(*Model);
    const double HeightScale = .90 + ((Variant * 7) % 17) * .01;
    const double BuildScale = .88 + ((Variant * 13) % 23) * .01;
    SetActorScale3D(FVector(BuildScale, BuildScale, HeightScale));

    const FColor Shirts[] = {
        FColor(149, 107, 70), FColor(172, 188, 191), FColor(104, 60, 79),
        FColor(104, 126, 91), FColor(169, 120, 100), FColor(65, 92, 138),
        FColor(164, 150, 105)
    };
    const FColor Trousers[] = {
        FColor(56, 66, 76), FColor(85, 77, 69), FColor(47, 53, 59), FColor(108, 109, 97)
    };
    const FColor Shirt = Shirts[Variant % UE_ARRAY_COUNT(Shirts)];
    ApplyClothingColor(*Mesh, TEXT("top_teal_woven"), Shirt);
    ApplyClothingColor(*Mesh, TEXT("top_seams"), FColor(Shirt.R * 83 / 100, Shirt.G * 83 / 100, Shirt.B * 83 / 100));
    ApplyClothingColor(*Mesh, TEXT("bottom_denim"), Trousers[(Variant / 3) % UE_ARRAY_COUNT(Trousers)]);

    Gait=Walk;
    const FString IdlePath=FPackageName::GetLongPackagePath(Walk->GetOutermost()->GetName())+TEXT("/A_Idle.A_Idle");
    IdlePose=LoadObject<UAnimSequence>(nullptr,*IdlePath);
    if (IdlePose && IdlePose->GetSkeleton()!=Walk->GetSkeleton()) IdlePose=nullptr;
    Mesh->PlayAnimation(Walk, true);
    Mesh->SetPosition(Walk->GetPlayLength() * FMath::Frac((Variant + 1) * .618f), false);
    Mesh->SetPlayRate(FMath::Clamp(static_cast<float>(Speed / (132.0 * BuildScale)), .75f, 1.25f));
    RestAt=12.+(Variant*13)%34;
    UpdateRouteTransform();
}

void ARiverfrontPedestrian::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (RouteLength<=UE_SMALL_NUMBER) return;
    const auto* PC=GetWorld()->GetFirstPlayerController();
    const APawn* Player=PC?PC->GetPawn():nullptr;
    const bool Far=Player && FVector::DistSquared2D(Player->GetActorLocation(),GetActorLocation())>FMath::Square(12500.);
    SetActorTickInterval(Far?.22f:.035f);
    if (!Errand.IsEmpty()) {TickErrand(DeltaSeconds);return;}
    if (bAtCrosswalk)
    {
        const auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
        auto* Traffic=Mode?Mode->GetTraffic():nullptr;
        if (!bSteppingOff)
        {
            RestLeft=FMath::Max(0.,RestLeft-DeltaSeconds);
            if (!Traffic || !Traffic->WalkSignal() || RestLeft>0) return;
            if (const auto* Car=Cast<ARiverfrontCar>(Player);Car && Car->GetSpeedKph()>7 &&
                FVector::Dist2D(Car->GetActorLocation(),GetActorLocation())<1450.) return;
            bSteppingOff=true; SetMoving(true);
        }
        const FVector Target=bReturning?RouteStart:RouteEnd;
        const FVector To=Target-GetActorLocation();
        const FVector Direction=To.GetSafeNormal2D();
        const double Step=FMath::Min(To.Size2D(),Speed*DeltaSeconds);
        SetActorLocationAndRotation(GetActorLocation()+Direction*Step,FRotator(0,Direction.Rotation().Yaw+MeshYawToForward,0));
        if (To.Size2D()<4.)
        {
            bReturning=!bReturning; bSteppingOff=false; ++Crossings; RestLeft=5.; SetMoving(false);
        }
        return;
    }
    if (RestLeft>0.)
    {
        RestLeft-=DeltaSeconds;
        if (RestLeft<=0) SetMoving(true);
        return;
    }
    RestAt-=DeltaSeconds;
    if (RestAt<=0)
    {
        RestAt=31.+FMath::Fmod(Distance/17.,46.);
        RestLeft=2.5+FMath::Fmod(Distance/109.,5.);
        SetMoving(false); return;
    }
    Distance=FMath::Fmod(Distance+Speed*DeltaSeconds,RouteLength);
    UpdateRouteTransform();
}

void ARiverfrontPedestrian::SetMoving(bool Moving)
{
    UAnimSequence* Clip=Moving || !IdlePose?Gait.Get():IdlePose.Get();
    if (Clip) Mesh->PlayAnimation(Clip,true);
    Mesh->SetPlayRate(Moving?FMath::Clamp(float(Speed/132.),.76f,1.26f):(IdlePose?.87f:0.f));
}

void ARiverfrontPedestrian::UpdateRouteTransform()
{
    const FRouteSample Sample = SampleCircuit(RouteStart, RouteEnd, TurnRadius, Distance);
    const FRotator Facing(0,Sample.Forward.Rotation().Yaw+MeshYawToForward,0);
    SetActorLocationAndRotation(Sample.Position, Facing, false);
}

ARiverfrontPedestrian* ARiverfrontPedestrian::SpawnStreetWalker(UWorld& World,const FVector& A,const FVector& B,int32 Variant)
{
    const FPedestrianRoute Route{A,B,19.,FMath::Frac(.137*(Variant+1)),111.+(Variant%39)};
    double Ground=0.;
    if (!ValidateCircuit(World,Route,Ground,Variant)) return nullptr;
    const bool Kurta=Variant%3==0;
    auto* Model=LoadObject<USkeletalMesh>(nullptr,Kurta?
        TEXT("/Game/Sabarmati/Riverfront/People/Kurta/SK_RiverResidentKurta.SK_RiverResidentKurta"):
        TEXT("/Game/Sabarmati/Riverfront/People/SK_RiverResident.SK_RiverResident"));
    auto* Clip=LoadObject<UAnimSequence>(nullptr,Kurta?
        TEXT("/Game/Sabarmati/Riverfront/People/Kurta/A_WalkAlt.A_WalkAlt"):
        TEXT("/Game/Sabarmati/Riverfront/People/A_Walk.A_Walk"));
    if (!Model || !Clip || Clip->GetSkeleton()!=Model->GetSkeleton()) return nullptr;
    const FVector Start(A.X,A.Y,Ground),End(B.X,B.Y,Ground);
    auto* Walker=World.SpawnActor<ARiverfrontPedestrian>(Start,(B-A).Rotation());
    if (Walker) Walker->Configure(Model,Clip,Start,End,Route.Radius,Route.StartFraction,Route.Speed,Variant);
    return Walker;
}

void ARiverfrontPedestrian::TickErrand(float Delta)
{
    if (RestLeft>0.)
    {
        RestLeft-=Delta;
        if (RestLeft<=0.) SetMoving(true);
        return;
    }
    const FVector To=Errand[ErrandStep]-GetActorLocation();
    const FVector Facing=To.GetSafeNormal();
    const double Step=FMath::Min(To.Size(),Speed*Delta);
    SetActorLocationAndRotation(GetActorLocation()+Facing*Step,FRotator(0,Facing.Rotation().Yaw+MeshYawToForward,0));
    if (To.Size()>4.) return;
    if (ErrandStep==Errand.Num()-1 || ErrandStep==0)
    {
        if (ErrandStep) ++CounterVisits;
        ErrandDirection=-ErrandDirection;
        RestLeft=ErrandStep?6.+FMath::Fmod(Speed,4.):11.+FMath::Fmod(Speed,7.);
        SetMoving(false);
    }
    ErrandStep+=ErrandDirection;
}
