#include "RiverfrontTraffic.h"
#include "RiverfrontLocalProfile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SabarmatiDistrict.h"
#include "Algo/Reverse.h"
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontPedestrian.h"
#include "RiverfrontCrossings.h"
#include "RiverfrontWorld.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
// The pedestrian crossing covers both lanes of the 10.2 m riverfront road.
constexpr double CrossingHalfRoadWidth = 510.;
constexpr double CrossingHalfStripeWidth = 142.5;
constexpr double CrossingClearance = CrossingHalfStripeWidth + 16.;
constexpr double BoardingSpeed = 125.; // Walking up to traffic that has come to a stop.

struct FVehicleShape
{
    const TCHAR* Name;
    const TCHAR* Label;
    float Length, Width, Wheel, Axle, Track, TyreWidth;
    int32 WheelCount;
};
const FVehicleShape Bodies[]={
    {TEXT("Rickshaw"),TEXT("Auto-rickshaw"),268,136,22.5,100,56,11,3},
    {TEXT("Scooter"),TEXT("Scooter"),188,52,22,63,0,11,2},
    {TEXT("Sedan"),TEXT("Sedan"),467,183,32,143,78,21,4},
    {TEXT("Bus"),TEXT("Bus"),860,240,43,273,97,29,4},
    {TEXT("PremiumHatch"),TEXT("City hatchback"),406,175,31,122,73,18,4},
    {TEXT("Motorcycle"),TEXT("Motorcycle"),195,60,28,65,0,14,2},
    {TEXT("Van"),TEXT("Van"),490,194,35,149,80,22,4},
    {TEXT("LuxurySUV"),TEXT("SUV"),508,198,37,157,84,25,4},
    {TEXT("Truck"),TEXT("Truck"),683,228,44,215,92,29,4},
    {TEXT("Sport"),TEXT("Sport coupe"),436,194,32,132,83,24,4},
    {TEXT("Ambulance"),TEXT("Ambulance"),513,201,35,156,84,22,4},
    {TEXT("Lorry"),TEXT("Lorry"),810,239,48,258,100,31,4}};
UStaticMesh* MeshAt(const FString& Path) { return LoadObject<UStaticMesh>(nullptr,*(Path+TEXT(".")+FPaths::GetCleanFilename(Path))); }
}
ARiverfrontRoadAgent::ARiverfrontRoadAgent()
{
    Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
    SetRootComponent(Bounds); Bounds->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Bounds->SetGenerateOverlapEvents(false);
}
bool ARiverfrontRoadAgent::Assemble(int32 Style,int32 Variant)
{
    BodyStyle = Style;
    const auto& S=Bodies[BodyStyle];
    HalfLength=S.Length*.5; HalfWidth=S.Width*.5; TyreRadius=S.Wheel; IsEmergency=BodyStyle==10;
    Wheelbase=2.*S.Axle; Track=S.Track; WheelCount=S.WheelCount;
    FrontWheels=S.WheelCount==4?2:1;
    Bounds->SetBoxExtent(FVector(HalfLength,HalfWidth,S.Wheel*.85));
    const FString Base=TEXT("/Game/Sabarmati/Riverfront/Vehicles/");
    auto* Model=MeshAt(Base+TEXT("Fleet/SM_Traffic")+S.Name);
    UStaticMesh* UtilityWheel = S.WheelCount == 3 ? MeshAt(Base+TEXT("Fleet/SM_TrafficRickshawWheel")) : nullptr;
    auto* Left=UtilityWheel ? UtilityWheel : MeshAt(Base+TEXT("SM_HatchWheelL"));
    auto* Right=UtilityWheel ? UtilityWheel : MeshAt(Base+TEXT("SM_HatchWheelR"));
    if (!Model || !Left || !Right)
    {
        UE_LOG(LogTemp,Warning,TEXT("Sabarmati road traffic: missing body or wheels for %s."),S.Name);
        return false;
    }
    RoofHeight = Model->GetBounds().Origin.Z + Model->GetBounds().BoxExtent.Z;
    auto Visual=[&](UStaticMesh* M,USceneComponent* Parent,const FVector& At,const FRotator& Facing,const FVector& Scale)
    {
        auto* C=NewObject<UStaticMeshComponent>(this); AddInstanceComponent(C); C->SetupAttachment(Parent);
        C->SetStaticMesh(M); C->SetRelativeLocationAndRotation(At,Facing); C->SetRelativeScale3D(Scale);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCanEverAffectNavigation(false); C->RegisterComponent(); return C;
    };
    auto* Shell=Visual(Model,Bounds,FVector(0,0,-S.Wheel-6),FRotator(0,-90,0),FVector::OneVector);
    if (const int32 Paint=Shell->GetMaterialIndex(TEXT("body")); Paint!=INDEX_NONE)
    {
        const FLinearColor Colors[]={
            FLinearColor(.07,.13,.115), FLinearColor(.52,.50,.45), FLinearColor(.22,.033,.028),
            FLinearColor(.065,.09,.12), FLinearColor(.34,.255,.15), FLinearColor(.65,.67,.64),
            FLinearColor(.075,.16,.22), FLinearColor(.029,.031,.03)};
        auto* Instance=Shell->CreateDynamicMaterialInstance(Paint);
        if (Instance) Instance->SetVectorParameterValue(TEXT("Tint"),Colors[Variant%UE_ARRAY_COUNT(Colors)]);
    }
    if (auto* Lens=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Sabarmati/Riverfront/M_PracticalLens.M_PracticalLens")))
    {
        const auto Fit=[&](const TCHAR* Slot,const FLinearColor& Tint)
        {
            auto* Material=UMaterialInstanceDynamic::Create(Lens,this);
            Material->SetVectorParameterValue(TEXT("LensTint"),Tint);
            const int32 Index=Shell->GetMaterialIndex(Slot);
            if (Index!=INDEX_NONE) Shell->SetMaterial(Index,Material);
            return Material;
        };
        HeadLens=Fit(TEXT("lamp"),FLinearColor(.79,.75,.61));
        TailLens=Fit(TEXT("red"),FLinearColor(.69,.027,.012));
        UpdateLights(false,false,false);
    }
    if (auto* Seated = MeshAt(TEXT("/Game/Sabarmati/Riverfront/People/SM_SeatedResident")))
    {
        auto* FemaleSeated = MeshAt(TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Female/Poses/SM_FemaleSeatedResident"));
        const auto Occupant = [Seated, FemaleSeated](int32 Seat)
        {
            return (Seat % 2 == 0 && FemaleSeated) ? FemaleSeated : Seated;
        };
        const bool Open=S.WheelCount<4;
        const double DriverX=Open?(S.WheelCount==3?41.:2.):(S.Length>600?S.Length*.35:S.Length*.035);
        const double SeatZ=Open?(S.WheelCount==3?-2.:6.):(S.Length>600?44.:0.);
        const double Side=Open?0.:S.Width*.21;
        bFemaleOccupant=Variant%2==0 && FemaleSeated;
        DriverFigure=Visual(Occupant(Variant),Bounds,FVector(DriverX,Side,SeatZ-S.Wheel-6),FRotator(0,-90,0),FVector(.94));
        if (BodyStyle==3)
            for (int32 Row=0;Row<3;++Row)
                Visual(Occupant(Variant+Row+1),Bounds,FVector(75-Row*104.,-56.,38.-S.Wheel),FRotator(0,-90,0),FVector(.9));
    }
    const int32 Count=S.WheelCount;
    for (int32 I=0; I<Count;++I)
    {
        const bool Bike=Count==2 || (Count==3 && I==0);
        const bool Front=Count==2?I==0:Count==3?I==0:I<2;
        const bool Side=Count==3?I==2:I%2==1;
        const FVector Mount(Front?S.Axle:-S.Axle,Bike?0:(Side?S.Track:-S.Track),-6);
        auto* Pivot=NewObject<USceneComponent>(this); AddInstanceComponent(Pivot); Pivot->SetupAttachment(Bounds);
        Pivot->SetRelativeLocation(Mount); Pivot->RegisterComponent();
        auto* Tyre = Side ? Right : Left;
        const FVector Extents = Tyre->GetBounds().BoxExtent;
        const double RadiusScale = S.Wheel / FMath::Max(Extents.Z, 1.);
        const double WidthScale = S.TyreWidth / (2. * FMath::Max(Extents.X, 1.));
        Visual(Tyre,Pivot,FVector::ZeroVector,FRotator(0,Side?90:-90,0),FVector(WidthScale,RadiusScale,RadiusScale));
        Wheels.Add(Pivot);
    }
    if (IsEmergency)
    {
        auto* Cube=MeshAt(TEXT("/Engine/BasicShapes/Cube"));
        auto* Lens=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Sabarmati/Riverfront/M_PracticalLens.M_PracticalLens"));
        for (int32 I=0;I<2;++I)
        {
            auto* Beacon=Visual(Cube,Bounds,FVector(79,I?27:-27,201),FRotator::ZeroRotator,FVector(.24,.25,.102));
            if (Lens)
            {
                auto* MID=UMaterialInstanceDynamic::Create(Lens,this); MID->SetScalarParameterValue(TEXT("Glow"),6.f);
                MID->SetVectorParameterValue(TEXT("LensTint"),FLinearColor(1.,.32,.012)); Beacon->SetMaterial(0,MID);
            }
            Beacons.Add(Beacon);
        }
    }
    return true;
}

const TCHAR* ARiverfrontRoadAgent::GetVehicleLabel() const
{
    return Bodies[BodyStyle].Label;
}

float ARiverfrontRoadAgent::GetTopSpeed() const
{
    // Per-body speed governors in cm/s, in the same order as the imported fleet.
    static constexpr float Limits[]={1520,2220,3890,1945,3335,4720,2780,3890,2220,5555,3335,2085};
    static_assert(UE_ARRAY_COUNT(Limits)==UE_ARRAY_COUNT(Bodies));
    return Limits[BodyStyle];
}

FVector ARiverfrontRoadAgent::SeatPoint() const
{
    // Hip pivot of the seated occupant, relative to the axle origin.
    const double Side=WheelCount<4?0.:HalfWidth*.42;
    const double Height=WheelCount==2?77.:WheelCount==3?50.:HalfLength>300.?99.:52.;
    return FVector(GetAccessX(),Side,Height-TyreRadius-6.);
}

double ARiverfrontRoadAgent::GetAccessX() const
{
    if (HalfLength > 300.) return HalfLength * .70;
    return WheelCount == 3 ? 41. : 10.;
}

bool ARiverfrontRoadAgent::IsNearAccess(const ARiverfrontCharacter& Character) const
{
    const FVector Local = GetActorTransform().InverseTransformPosition(Character.GetActorLocation());
    const double Height = Local.Z + TyreRadius + 6.;
    if (IsHidden() || Height < 20. || Height > 190.
        || FMath::Abs(Local.X - GetAccessX()) > (HalfLength > 300. ? 120. : 103.)
        || FMath::Abs(Local.Y) > HalfWidth + 125.)
    {
        return false;
    }
    const FVector Anchor = GetActorTransform().TransformPosition(
        FVector(GetAccessX(), FMath::Sign(Local.Y) * (HalfWidth + 9.), Local.Z));
    FHitResult Hit;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(RiverfrontBoarding), false, &Character);
    return !GetWorld()->LineTraceSingleByChannel(Hit, Character.GetActorLocation(), Anchor, ECC_Visibility, Query)
        || Hit.GetActor() == this || Hit.GetActor() == GetAttachParentActor();
}

void ARiverfrontRoadAgent::SetManualDriver(const ARiverfrontCharacter* Driver)
{
    if (!DriverFigure) return;
    if (Driver)
    {
        auto* Mesh = Driver->GetAvatarGender() == ERiverfrontGender::Female
            ? MeshAt(TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Female/Poses/SM_FemaleSeatedResident"))
            : MeshAt(TEXT("/Game/Sabarmati/Riverfront/People/SM_SeatedResident"));
        if (Mesh) DriverFigure->SetStaticMesh(Mesh);
    }
    SetDriverVisible(Driver != nullptr);
}

void ARiverfrontRoadAgent::SetDriverVisible(bool bVisible)
{
    if (DriverFigure) DriverFigure->SetVisibility(bVisible);
}

void ARiverfrontRoadAgent::AnimateWheels(double Travel, double Steering)
{
    Spin = FMath::Fmod(Spin - float(FMath::RadiansToDegrees(Travel / TyreRadius)), 360.f);
    for (int32 I = 0; I < Wheels.Num(); ++I)
        Wheels[I]->SetRelativeRotation(FRotator(0,I<FrontWheels?Steering:0.,0).Quaternion()
            * FRotator(Spin,0,0).Quaternion());
}

bool ARiverfrontRoadAgent::MoveOnRoad(const FVector& Position,const FRotator& Heading,double Travel,bool Near)
{
    const double Turn=FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,Heading.Yaw);
    FHitResult Hit;
    SetActorLocationAndRotation(Position,Heading,Near,&Hit,ETeleportType::None);
    if (Hit.bBlockingHit) return false;
    if (Near)
    {
        const double Steering=Travel>1.?FMath::Clamp(FMath::RadiansToDegrees(
            FMath::Atan(FMath::DegreesToRadians(Turn)*Wheelbase/Travel)),-32.,32.):0.;
        AnimateWheels(Travel, Steering);
        for (int32 I=0;I<Beacons.Num();++I) Beacons[I]->SetVisibility((int32(GetWorld()->GetTimeSeconds()*5.)+I)%2==0);
    }
    return true;
}
ARiverfrontTraffic::ARiverfrontTraffic()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("RoadNetwork"));
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.05f;
}

ARiverfrontCar* ARiverfrontTraffic::FindParkedCar(const ARiverfrontCharacter& Character) const
{
    ARiverfrontCar* Closest = nullptr;
    double Best = TNumericLimits<double>::Max();
    for (const auto& WeakCar : Parked)
    {
        auto* Car = WeakCar.Get();
        if (!Car || !Car->IsNearDoor(Character)) continue;
        const double Distance = FVector::DistSquared2D(Car->GetActorLocation(), Character.GetActorLocation());
        if (Distance < Best)
        {
            Best = Distance;
            Closest = Car;
        }
    }
    return Closest;
}

const ARiverfrontRoadAgent* ARiverfrontTraffic::FindEnterableVehicle(const ARiverfrontCharacter& Character) const
{
    const ARiverfrontRoadAgent* Closest = nullptr;
    double Best = TNumericLimits<double>::Max();
    for (int32 I = 0; I < Agents.Num(); ++I)
    {
        const auto* Agent = Agents[I].Get();
        if (!IsValid(Agent) || Drivers[I].Speed > BoardingSpeed) continue;
        const double Distance = FVector::DistSquared2D(Agent->GetActorLocation(), Character.GetActorLocation());
        if (Distance < Best && Agent->IsNearAccess(Character))
        {
            Best = Distance;
            Closest = Agent;
        }
    }
    return Closest;
}

FVector ARiverfrontTraffic::RoadVelocity(const ARiverfrontRoadAgent* Agent) const
{
    const int32 Index=Agents.IndexOfByKey(Agent);
    if (Index==INDEX_NONE || Drivers[Index].bParked) return FVector::ZeroVector;
    return Agent->GetActorForwardVector()*Drivers[Index].Speed;
}

bool ARiverfrontTraffic::RemoveCrashedAgent(ARiverfrontRoadAgent* Agent)
{
    const int32 Index=Agents.IndexOfByKey(Agent);
    if (Index==INDEX_NONE) return false;
    Agents.RemoveAtSwap(Index,EAllowShrinking::No);
    Drivers.RemoveAtSwap(Index,EAllowShrinking::No);
    Agent->Destroy();
    return true;
}

bool ARiverfrontTraffic::EnterVehicle(ARiverfrontCharacter& Character)
{
    const auto* Target = FindEnterableVehicle(Character);
    const int32 Index = Target ? Agents.IndexOfByPredicate([Target](const auto& Agent)
    {
        return Agent.Get() == Target;
    }) : INDEX_NONE;
    if (Index == INDEX_NONE) return false;

    ARiverfrontRoadAgent* Agent = Agents[Index];
    FTransform Placement = Agent->GetActorTransform();
    Placement.AddToTranslation(Placement.TransformVectorNoScale(FVector(0, 0,
        Agent->GetRideHeight() - Agent->TyreRadius - 6.)));
    auto* Car = GetWorld()->SpawnActorDeferred<ARiverfrontCar>(ARiverfrontCar::StaticClass(), Placement,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Car) return false;
    Car->UseTrafficBody(*Agent);
    Car->SetRoadWetness(District.IsValid() ? District->GetWetness() : 0.f);
    Car->FinishSpawning(Placement);
    FVector Landing;
    int32 Side=1;
    ARiverfrontCharacter* Passenger=nullptr;
    if (!Drivers[Index].bParked)
    {
        if (!Car->FindDoorLanding(Character,Landing,Side)) { Car->Destroy(); return false; }
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Passenger=GetWorld()->SpawnActor<ARiverfrontCharacter>(Landing,Agent->GetActorRotation(),Params);
        if (!Passenger) { Car->Destroy();return false; }
        Passenger->SetActorEnableCollision(false);
    }
    if (!Car->Enter(Character))
    {
        if (Passenger) Passenger->Destroy();
        Car->Destroy();
        return false;
    }
    if (Passenger)
    {
        auto* Look=NewObject<URiverfrontLocalProfile>(Passenger);
        Look->AvatarGender=Agent->bFemaleOccupant?ERiverfrontGender::Female:ERiverfrontGender::Male;
        Passenger->ApplyProfileAppearance(*Look);
        Passenger->BeginSeatExit(Agent->GetActorTransform().TransformPosition(Agent->SeatPoint()),Landing,
            Car->GetActorRightVector()*Side);
        UE_LOG(LogTemp,Display,TEXT("Sabarmati traffic: displaced driver exited %s."),Agent->GetVehicleLabel());
    }
    Agents.RemoveAtSwap(Index, EAllowShrinking::No);
    Drivers.RemoveAtSwap(Index, EAllowShrinking::No);
    Parked.Add(Car);
    return true;
}
void ARiverfrontTraffic::AddDistrictRoute(const TArray<FVector>& Nodes, bool Closed)
{
    if (Nodes.Num()<2) return;
    TArray<FVector> Run;
    const auto Flush=[&](bool Loop)
    {
        if (Run.Num()>=2) { Lane(Run,false,Loop); Lane(Run,true,Loop); }
        Run.Reset();
    };
    bool Cut=false;
    for (int32 I=0; I<(Closed?Nodes.Num():Nodes.Num()-1); ++I)
    {
        FVector A=Nodes[I], B=Nodes[(I+1)%Nodes.Num()];
        const FVector OriginalA=A, OriginalB=B;
        if (!SabarmatiDistrict::Clip(A,B,bCity,900.)) { Cut=true; Flush(false); continue; }
        Cut |= !A.Equals(OriginalA,1.) || !B.Equals(OriginalB,1.);
        if (!Run.IsEmpty() && FVector::DistSquared2D(Run.Last(),A)>4.) Flush(false);
        if (Run.IsEmpty()) Run.Add(A);
        Run.Add(B);
    }
    if (Closed && !Cut && Run.Num()>2) Run.Pop();
    Flush(Closed && !Cut);
}

void ARiverfrontTraffic::Lane(const TArray<FVector>& Nodes,bool Reverse,bool bClosed)
{
    if (Nodes.Num()<2) return;
    TArray<FVector> Ordered=Nodes;
    if (Reverse) Algo::Reverse(Ordered);
    const int32 N=Ordered.Num();
    TArray<FVector> P;
    for (int32 I=0;I<N;++I)
    {
        if (!bClosed && (I==0 || I==N-1))
        {
            const int32 Link=FMath::Min(I,N-2);
            const FVector Direction=(Ordered[Link+1]-Ordered[Link]).GetSafeNormal2D();
            const double Width=Ordered[Link+(Reverse?1:0)].Z;
            P.Add(FVector(Ordered[I].X,Ordered[I].Y,0.)+
                FVector(Direction.Y,-Direction.X,0.)*FMath::Clamp(Width*.23,96.,222.));
            continue;
        }
        FVector A=Ordered[(I+N-1)%N], B=Ordered[I], C=Ordered[(I+1)%N];
        const double IncomingWidth=Reverse?B.Z:A.Z, OutWidth=Reverse?C.Z:B.Z;
        A.Z=B.Z=C.Z=0;
        const FVector D=(B-A).GetSafeNormal2D(), E=(C-B).GetSafeNormal2D();
        const double Trim=FMath::Min(475.,.24*FMath::Min(FVector::Dist2D(A,B),FVector::Dist2D(B,C)));
        const FVector LeftIn(D.Y,-D.X,0), LeftOut(E.Y,-E.X,0);
        const FVector Start=B-D*Trim+LeftIn*FMath::Clamp(IncomingWidth*.23,96.,222.);
        const FVector End=B+E*Trim+LeftOut*FMath::Clamp(OutWidth*.23,96.,222.);
        const FVector Control=B+(LeftIn*FMath::Clamp(IncomingWidth*.23,96.,222.)+LeftOut*FMath::Clamp(OutWidth*.23,96.,222.))*.5;
        for (int32 J=0;J<=5;++J)
        {
            const double T=J/5.;
            P.Add(FMath::Lerp(FMath::Lerp(Start,Control,T),FMath::Lerp(Control,End,T),T));
        }
    }
    auto* Spline=NewObject<USplineComponent>(this);
    AddInstanceComponent(Spline); Spline->SetupAttachment(RootComponent); Spline->RegisterComponent();
    Spline->SetSplinePoints(P,ESplineCoordinateSpace::World,false);
    for (int32 I=0;I<P.Num();++I) Spline->SetSplinePointType(I,ESplinePointType::Linear,false);
    Spline->SetClosedLoop(bClosed,true); Routes.Add(Spline);
}
FTransform ARiverfrontTraffic::RoadPose(const USplineComponent& Route,double Distance,double Offset,
    double Radius,double Wheelbase,bool bSampleRoad) const
{
    constexpr auto WorldSpace=ESplineCoordinateSpace::World;
    const double Length=Route.GetSplineLength();
    const auto At=[&](double Delta)
    {
        const double Along=Route.IsClosedLoop()?FMath::Fmod(Distance+Delta+Length,Length):FMath::Clamp(Distance+Delta,0.,Length);
        return Route.GetLocationAtDistanceAlongSpline(Along,WorldSpace);
    };
    const FVector Forward=(At(70.)-At(-70.)).GetSafeNormal2D();
    FVector Centre=At(0.)+FVector(-Forward.Y,Forward.X,0.)*Offset;
    const bool bBridge=!bCity && FMath::Abs(Forward.Y)>.65;
    const auto ExpectedZ=[&](const FVector& P){return bBridge?RiverfrontCrossings::DeckZ(P.Y):0.;};
    const auto Surface=[&](const FVector& P)
    {
        const double Z=ExpectedZ(P);
        FHitResult Hit;
        if (bSampleRoad && GetWorld()->LineTraceSingleByObjectType(Hit,FVector(P.X,P.Y,Z+31.),FVector(P.X,P.Y,Z-32.),
            FCollisionObjectQueryParams(ECC_WorldStatic)) && Hit.ImpactNormal.Z>.78)
            return Hit.ImpactPoint.Z;
        return Z;
    };
    const double Front=Surface(Centre+Forward*(Wheelbase*.5));
    const double Rear=Surface(Centre-Forward*(Wheelbase*.5));
    Centre.Z=(Front+Rear)*.5+Radius+6.;
    FRotator Facing=Forward.Rotation();
    Facing.Pitch=FMath::RadiansToDegrees(FMath::Atan2(Front-Rear,Wheelbase));
    return FTransform(Facing,Centre);
}

bool ARiverfrontTraffic::SpawnVehicle(int32 Route,double Distance,int32 Style,int32 Variant,bool ParkAtKerb)
{
    const auto& Shape=Bodies[Style];
    const FTransform Pose=RoadPose(*Routes[Route],Distance,ParkAtKerb?-193.:0.,Shape.Wheel,2.*Shape.Axle,true);
    if (ParkAtKerb)
    {
        for (const FVector& Junction:Junctions)
            if (FVector::DistSquared2D(Junction,Pose.GetLocation())<FMath::Square(1650.)) return false;
        FHitResult Floor;
        const FVector P=Pose.GetLocation();
        if (!GetWorld()->LineTraceSingleByObjectType(Floor,P+FVector(0,0,20),P-FVector(0,0,100),
            FCollisionObjectQueryParams(ECC_WorldStatic)) || Floor.ImpactNormal.Z<.95 || FMath::Abs(Floor.ImpactPoint.Z)>4.) return false;
    }
    // Leave room for moving traffic to brake, even on circuits that share a lane.
    const FCollisionShape Clearance=FCollisionShape::MakeBox(FVector(Shape.Length*.5+245.,Shape.Width*.5+12.,Shape.Wheel*.8));
    if (GetWorld()->OverlapBlockingTestByProfile(Pose.GetLocation(),Pose.GetRotation(),TEXT("BlockAllDynamic"),Clearance))
        return false;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Agent=GetWorld()->SpawnActor<ARiverfrontRoadAgent>(Pose.GetLocation(),Pose.Rotator(),Params);
    if (!Agent) return false;
    if (!Agent->Assemble(Style,Variant))
    {
        Agent->Destroy();
        return false;
    }
    FSabarmatiLane LaneState;
    LaneState.Route=Route;
    LaneState.At=Distance;
    const double BaseSpeed=Shape.WheelCount<4?650.:Shape.Length>600.?620.:790.;
    LaneState.Cruise=BaseSpeed+(Variant*67)%207;
    LaneState.bParked=ParkAtKerb;
    if (ParkAtKerb) { Agent->Tags.Add(TEXT("Kerbside")); Agent->SetDriverVisible(false); }
    Drivers.Add(LaneState);
    Agents.Add(Agent);
    return true;
}

void ARiverfrontTraffic::Start(bool Mapped,const FVector& StreetStart)
{
    bCity=Mapped;
    if (TActorIterator<ARiverfrontWorld> It(GetWorld()); It) District=*It;
    if (!bCity)
    {
        const double W=4090., E=-26010.;
        // Ellis and the central vehicle bridge form a complete circuit inside the square.
        AddDistrictRoute({{3150,W,1020},{59310,W,640},{59310,E,1020},{3150,E,720}});
        for (double Bank:{W,E})
            AddDistrictRoute({{-38000,Bank,1020},{-16000,Bank,1020},{3150,Bank,1020}},false);
        Junctions={PedestrianCrossing,FVector(3150,W,0),FVector(3150,E,0),FVector(-64400,W,0),FVector(59310,E,0)};
    }
    else
    {
        FString Json; TSharedPtr<FJsonObject> Doc;
        if (!FFileHelper::LoadFileToString(Json,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("CityData/AhmedabadRoutes.json"))) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Doc) || !Doc.IsValid()) return;
        const TArray<TSharedPtr<FJsonValue>>* Data=nullptr;
        if (!Doc->TryGetArrayField(TEXT("routes"),Data) || Data->Num()>32) return;
        TActorIterator<ARiverfrontWorld> City(GetWorld());
        for (const auto& V:*Data)
        {
            TArray<FVector> Nodes;
            const auto O=V->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
            FString Area;
            if (O && O->TryGetStringField(TEXT("area"),Area) && Area!=TEXT("riverfront") && (!City || !City->HasStreetDistricts())) continue;
            if (!O || !O->TryGetArrayField(TEXT("points"),Points) || Points->Num()<3 || Points->Num()>1500) continue;
            for (const auto& Pt:*Points)
            {
                const auto& N=Pt->AsArray(); if (N.Num()!=3) continue;
                Nodes.Emplace(N[0]->AsNumber(),N[1]->AsNumber(),N[2]->AsNumber());
            }
            bool bClosed=true;
            O->TryGetBoolField(TEXT("closed"),bClosed);
            AddDistrictRoute(Nodes,bClosed);
        }
    }
    if (!Routes.Num()) return;
    SetUpSignals();
    for (TActorIterator<ARiverfrontCar> It(GetWorld());It;++It) Parked.Add(*It);
    // First populate the two directions on the street nearest the player's start.
    // Each later fill uses a different phase so the distant loops stay populated.
    int32 Nearest=0;
    double StreetDistance=DBL_MAX;
    for (int32 R=0;R<Routes.Num();R+=2)
    {
        const double Distance=FVector::DistSquared2D(StreetStart,
            Routes[R]->FindLocationClosestToWorldLocation(StreetStart,ESplineCoordinateSpace::World));
        if (Distance<StreetDistance)
        {
            StreetDistance=Distance;
            Nearest=R;
        }
    }
    constexpr int32 StreetFleet[]={0,2,1,4,6,5,3,7};
    if (StreetDistance<FMath::Square(2200.))
    {
        for (const double Offset:{-1040.,1370.,-2400.,2960.,-3800.,4380.})
        {
            for (int32 R=Nearest;R<Nearest+2 && Agents.Num()<UE_ARRAY_COUNT(StreetFleet);++R)
            {
                const auto* Spline=Routes[R].Get();
                const double Length=Spline->GetSplineLength();
                const double Along=Spline->GetDistanceAlongSplineAtLocation(StreetStart,ESplineCoordinateSpace::World);
                SpawnVehicle(R,FMath::Fmod(Along+Offset+Length,Length),StreetFleet[Agents.Num()],Agents.Num());
            }
        }
    }
    const int32 StreetCount=Agents.Num();
    const int32 KerbFleet[]={9,5,2,4,0,7};
    int32 ParkedCount=0;
    const auto* NearLane=Routes[Nearest].Get();
    const double Origin=NearLane->GetDistanceAlongSplineAtLocation(StreetStart,ESplineCoordinateSpace::World);
    for (int32 I=0;I<36 && ParkedCount<UE_ARRAY_COUNT(KerbFleet);++I)
    {
        const double Distance=Origin+(I%2?1.:-1.)*(2400.+(I/2)*1020.);
        if (Distance<700. || Distance>NearLane->GetSplineLength()-700.) continue;
        if (SpawnVehicle(Nearest,Distance,KerbFleet[ParkedCount],I+21,true)) ++ParkedCount;
    }
    if (bCity)
    {
        const FVector LocalStops[]={{10237,25487,0},{20998,25504,0},{28570,65572,0}};
        for (int32 I=0;I<UE_ARRAY_COUNT(LocalStops);++I)
        {
            int32 BestRoute=INDEX_NONE;
            double Best=FMath::Square(2200.);
            for (int32 R=0;R<Routes.Num();++R)
            {
                const FVector P=Routes[R]->FindLocationClosestToWorldLocation(LocalStops[I],ESplineCoordinateSpace::World);
                const double D=FVector::DistSquared2D(P,LocalStops[I]);
                if (D<Best) { Best=D; BestRoute=R; }
            }
            if (BestRoute==INDEX_NONE) continue;
            auto* Route=Routes[BestRoute].Get();
            const double At=Route->GetDistanceAlongSplineAtLocation(LocalStops[I],ESplineCoordinateSpace::World);
            for (double Offset:{450.,-490.,1200.,-1150.})
                if (At+Offset>650. && At+Offset<Route->GetSplineLength()-650.
                    && SpawnVehicle(BestRoute,At+Offset,KerbFleet[I],42+I,true))
                { ++ParkedCount;break; }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("Sabarmati parking: %d curbside vehicles"),ParkedCount);
    const int32 Population=bCity?30:36;
    for (int32 I=0;I<Population*2 && Agents.Num()<Population;++I)
    {
        const int32 R=I%Routes.Num();
        const double F=FMath::Frac(.19+I*.38196601125);
        const int32 Style=(Agents.Num()-StreetCount-ParkedCount)%UE_ARRAY_COUNT(Bodies);
        SpawnVehicle(R,F*Routes[R]->GetSplineLength(),Style,I+StreetCount);
    }
    UE_LOG(LogTemp,Display,TEXT("Sabarmati road traffic: %d agents on %d directed lanes, %d seeded at the starting street."),
        Agents.Num(),Routes.Num(),StreetCount);
}
int32 ARiverfrontTraffic::Phase() const
{
    const double P=FMath::Fmod(Elapsed,43.f);
    return P<16?0:P<19?1:P<20?2:P<28?3:P<30?4:5;
}
bool ARiverfrontTraffic::WalkSignal() const
{
    if (bCity || Phase() != 5 || FMath::Fmod(Elapsed,43.f) >= 32.f) return false;

    // Let the vehicle that was already in the crossing clear it before anyone steps off.
    FCollisionObjectQueryParams VehicleObjects(ECC_WorldDynamic);
    VehicleObjects.AddObjectTypesToQuery(ECC_Vehicle);
    TArray<FOverlapResult> Occupants;
    GetWorld()->OverlapMultiByObjectType(Occupants, PedestrianCrossing + FVector(0.,0.,55.),
        FQuat::Identity, VehicleObjects, FCollisionShape::MakeBox(FVector(
            CrossingClearance, CrossingHalfRoadWidth, 65.)),
        FCollisionQueryParams(SCENE_QUERY_STAT(RiverfrontCrosswalkClearance), false));
    for (const FOverlapResult& Occupant : Occupants)
    {
        const AActor* Actor = Occupant.GetActor();
        if (Actor && (Actor->IsA<ARiverfrontRoadAgent>() || Actor->IsA<ARiverfrontCar>()))
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("Atal crossing occupied by %s at %s."), *Actor->GetName(),
                *Actor->GetActorLocation().ToCompactString());
            return false;
        }
    }
    return true;
}
void ARiverfrontTraffic::SetUpSignals()
{
    if (!Junctions.Num()) return;
    auto* Lens=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Sabarmati/Riverfront/M_PracticalLens.M_PracticalLens"));
    auto* Iron=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Sabarmati/Riverfront/M_CoatedIron.M_CoatedIron"));
    UStaticMesh* Cube=MeshAt(TEXT("/Engine/BasicShapes/Cube")), *Ball=MeshAt(TEXT("/Engine/BasicShapes/Sphere"));
    if (!Lens || !Cube || !Ball) return;
    for (int32 I=0;I<6;++I)
    {
        const FLinearColor C[]={FLinearColor(1,.018,.004),FLinearColor(1,.43,.012),FLinearColor(.03,.85,.26)};
        auto* Mat=UMaterialInstanceDynamic::Create(Lens,this);
        Mat->SetVectorParameterValue(TEXT("LensTint"),C[I%3]); Signals.Add(Mat);
    }
    const auto Part=[&](UStaticMesh* M,UMaterialInterface* Mat,FVector P,FVector Size,FRotator Rot)
    {
        auto* C=NewObject<UStaticMeshComponent>(this); AddInstanceComponent(C); C->SetupAttachment(RootComponent);
        C->SetStaticMesh(M); C->SetMaterial(0,Mat); C->SetRelativeLocationAndRotation(P,Rot);
        C->SetRelativeScale3D(Size/100.); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent();
    };
    for (int32 J=0;J<Junctions.Num();++J)
    for (int32 Axis=0;Axis<(J==0?2:4);++Axis)
    {
        const double Yaw=Axis<2?Axis*180.:90.+(Axis-2)*180.;
        const FRotator R(0,Yaw,0);
        const FVector D=R.Vector(), L(D.Y,-D.X,0);
        const FVector Pole=Junctions[J]-D*571+L*535;
        Part(Cube,Iron,Pole+FVector(0,0,165),FVector(12,12,330),R);
        Part(Cube,Iron,Pole+FVector(0,0,294),FVector(25,34,90),R);
        for (int32 C=0;C<3;++C) Part(Ball,Signals[(Axis>=2?3:0)+C],Pole-D*15+FVector(0,0,320-C*25),FVector(9,20,20),R);
    }
}
float ARiverfrontTraffic::StopDistance(const FVector& At,const FVector& D,double Length,bool& Obstacle,
    int32 Index,TConstArrayView<FVector> CrossingWalkers)
{
    double Limit=1.e7; Obstacle=false;
    const int32 H=Phase(); const bool Along=FMath::Abs(D.X)>FMath::Abs(D.Y);
    for (int32 J=0;J<Junctions.Num();++J)
    {
        if (!bCity && J == 0) continue;
        const bool Red=Along?H!=0:H!=3;
        const FVector V=Junctions[J]-At;
        if (Red && Drivers[Index].ClearingJunction!=J && FMath::Abs(V.X*D.Y-V.Y*D.X)<295. && V.Dot(D)>0 && V.Dot(D)<2350)
            Limit=FMath::Min(Limit,V.Dot(D)-Length-337.);
    }
    auto Ahead=[&](FVector P,double OtherLength,double OtherWidth)
    {
        const FVector V=P-At; const double Distance=V.Dot(D);
        if (Distance>0 && Distance<2350 && FMath::Abs(V.X*D.Y-V.Y*D.X)<Agents[Index]->HalfWidth+OtherWidth+22.)
            Limit=FMath::Min(Limit,Distance-Length-OtherLength-84.);
    };
    for (int32 I=0;I<Agents.Num();++I)
        if (I!=Index) Ahead(Agents[I]->GetActorLocation(),Agents[I]->HalfLength,Agents[I]->HalfWidth);
    for (const auto& C:Parked)
        if (C.IsValid())
        {
            const FVector V=C->GetActorLocation()-At;
            if (!bCity && Along && !C->IsOccupied() && V.Dot(D)>-360 && V.Dot(D)<1600 &&
                FMath::Abs(V.X*D.Y-V.Y*D.X)<340) Obstacle=true;
            const FVector Hull = C->GetCollisionExtents();
            Ahead(C->GetActorLocation(),Hull.X,Hull.Y);
        }
    if (const auto* PC=GetWorld()->GetFirstPlayerController())
        if (const APawn* Pawn=PC->GetPawn();Pawn && !Cast<ARiverfrontCar>(Pawn)) Ahead(Pawn->GetActorLocation(),36,37);
    for (const FVector& Walker : CrossingWalkers) Ahead(Walker,33,38);
    if (!bCity && Along)
    {
        const FVector Crossing = PedestrianCrossing - At;
        const double Forward = Crossing.Dot(D);
        if (FMath::Abs(Crossing.X * D.Y - Crossing.Y * D.X) < CrossingHalfRoadWidth
            && Forward > Length + CrossingClearance && Forward < 2350.)
        {
            // Leave space for the whole vehicle after the stripes before committing
            // to the crossing. A vehicle already across the line keeps clearing it.
            const double DistanceToClear = Forward + Length + CrossingClearance;
            if (H != 0 || Limit < DistanceToClear)
                Limit = FMath::Min(Limit, Forward - Length - 337.);
        }
    }
    return Limit;
}
void ARiverfrontTraffic::UpdateJunctionClearance(FSabarmatiLane& Driver,const ARiverfrontRoadAgent& Agent,
    const FVector& Heading,int32 Signal)
{
    const FVector Here=Agent.GetActorLocation();
    if (Driver.ClearingJunction!=INDEX_NONE)
    {
        if (FVector::DistSquared2D(Here,Junctions[Driver.ClearingJunction])<FMath::Square(880.)) return;
        Driver.ClearingJunction=INDEX_NONE;
    }
    // A vehicle that has passed the stop line on green owns its turn until it
    // leaves the junction. Its heading changes as it crosses the other axis.
    const bool bAlong=FMath::Abs(Heading.X)>FMath::Abs(Heading.Y);
    if (Signal!=(bAlong?0:3)) return;
    for (int32 J=0;J<Junctions.Num();++J)
    {
        if (!bCity && J == 0) continue;
        const FVector To=Junctions[J]-Here;
        const double Ahead=To.Dot(Heading);
        if (Ahead>0. && Ahead<Agent.HalfLength+310. &&
            FMath::Abs(To.X*Heading.Y-To.Y*Heading.X)<295.)
        {
            Driver.ClearingJunction=J;
            return;
        }
    }
}
void ARiverfrontTraffic::Tick(float Dt)
{
    Super::Tick(Dt); if (!Agents.Num()) return;
    Elapsed+=Dt;
    const int32 H=Phase();
    if (H!=LastPhase && Signals.Num()==6)
    {
        for (int32 Side=0; Side<2; ++Side)
        {
            int32 Lit=Side==0?(H==0?2:H==1?1:0):(H==3?2:H==4?1:0);
            for (int32 I=0;I<3;++I) Signals[3*Side+I]->SetScalarParameterValue(TEXT("Glow"),I==Lit?5.5f:0.f);
        }
        LastPhase=H;
    }
    auto* PC=GetWorld()->GetFirstPlayerController(); if (!PC || !PC->GetPawn()) return;
    const FVector Eye=PC->GetPawn()->GetActorLocation();
    const bool Night=District.IsValid() && District->IsNight();
    TArray<TPair<double,int32>,TInlineAllocator<16>> NearLamps;
    if (Night)
    {
        for (int32 I=0; I<Agents.Num(); ++I)
        {
            const double D=FVector::DistSquared2D(Agents[I]->GetActorLocation(),Eye);
            if (D<FMath::Square(8300.)) NearLamps.Emplace(D,I);
        }
        NearLamps.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});
        if (NearLamps.Num()>4) NearLamps.SetNum(4,EAllowShrinking::No);
    }
    TArray<FVector,TInlineAllocator<4>> CrossingWalkers;
    if (!bCity)
        for (TActorIterator<ARiverfrontPedestrian> It(GetWorld()); It; ++It)
            if (It->IsCrossingStreet()) CrossingWalkers.Add(It->GetActorLocation());
    for (int32 I=0;I<Agents.Num();++I)
    {
        auto* A=Agents[I].Get(); auto& L=Drivers[I]; auto* R=Routes[L.Route].Get();
        if (L.bParked)
        {
            A->SetActorHiddenInGame(FVector::DistSquared2D(A->GetActorLocation(),Eye)>FMath::Square(51000.));
            continue;
        }
        const bool Near=FVector::DistSquared2D(A->GetActorLocation(),Eye)<FMath::Square(23000.);
        const double Length=R->GetSplineLength();
        const bool bClosed=R->IsClosedLoop();
        const FVector D=R->GetDirectionAtDistanceAlongSpline(L.At,ESplineCoordinateSpace::World).GetSafeNormal2D();
        UpdateJunctionClearance(L,*A,D,H);
        bool ParkedAhead=false;
        const float Gap=StopDistance(A->GetActorLocation(),D,A->HalfLength,ParkedAhead,I,CrossingWalkers);
        // Pull toward the road centre only on a wide bank lane with enough oncoming clearance.
        bool Passing=ParkedAhead;
        if (Passing)
            for (int32 K=0;K<Agents.Num();++K)
                if (K!=I && Agents[K]->GetActorForwardVector().Dot(D)<-.65 &&
                    FVector::DistSquared2D(A->GetActorLocation(),Agents[K]->GetActorLocation())<FMath::Square(1500.)) Passing=false;
        bool Yielding=false;
        if (!A->IsEmergency && !bCity && FMath::Abs(D.X)>.9)
        {
            for (const auto& Other:Agents)
            {
                if (!Other->IsEmergency || Other->GetActorForwardVector().Dot(D)<.8) continue;
                const FVector Approach=A->GetActorLocation()-Other->GetActorLocation();
                if (Approach.Dot(D)>0 && Approach.Dot(D)<2700 && FMath::Abs(Approach.X*D.Y-Approach.Y*D.X)<250) Yielding=true;
            }
        }
        L.Offset=FMath::FInterpConstantTo(L.Offset,Yielding?-115.:(Passing?225.:0.),Dt,105.);
        const double Available=bClosed?Gap:FMath::Min(double(Gap),Length-L.At);
        const double Want=FMath::Min(L.Cruise*(Yielding?.45:(A->IsEmergency?1.18:1.)),FMath::Sqrt(2.*440.*FMath::Max(0.,Available)));
        const bool Brake=Want<L.Speed-27. || Want<25.;
        L.Speed=FMath::FInterpConstantTo(L.Speed,Want,Dt,Want>L.Speed?170.:580.);
        A->UpdateLights(Night,Brake,NearLamps.ContainsByPredicate([I](const auto& Lamp){return Lamp.Value==I;}));
        if (Want<15 && !L.WasStopped) { ++Stops; L.WasStopped=true; }
        if (Want>110) L.WasStopped=false;
        const double Advance=bClosed?L.Speed*Dt:FMath::Min(L.Speed*Dt,Length-L.At);
        const double Next=bClosed?FMath::Fmod(L.At+Advance,Length):L.At+Advance;
        const FTransform Pose=RoadPose(*R,Next,L.Offset,A->TyreRadius,A->Wheelbase,Near);
        if (A->MoveOnRoad(Pose.GetLocation(),Pose.Rotator(),Advance,Near))
        {
            if (Advance>0 && D.Dot(Pose.GetUnitAxis(EAxis::X).GetSafeNormal2D())<.9998) ++Turns;
            TravelCm+=Advance; L.At=Next;
        }
        else L.Speed=0;
        const bool bFar=FVector::DistSquared2D(A->GetActorLocation(),Eye)>FMath::Square(51000.);
        A->SetActorHiddenInGame(bFar);
        // The mapped starting road has open ends. Re-enter only offscreen and
        // with physical space in the lane.
        if (!bClosed && L.At>=Length-1.)
        {
            L.Speed=0.;
            if (!bFar) continue;
            const FTransform Entry=RoadPose(*R,0.,0.,A->TyreRadius,A->Wheelbase,true);
            const FCollisionShape Room=FCollisionShape::MakeBox(FVector(A->HalfLength+245.,A->HalfWidth+12.,A->TyreRadius*.8));
            if (FVector::DistSquared2D(Entry.GetLocation(),Eye)>FMath::Square(51000.) &&
                !GetWorld()->OverlapBlockingTestByProfile(Entry.GetLocation(),Entry.GetRotation(),TEXT("BlockAllDynamic"),Room))
            {
                A->MoveOnRoad(Entry.GetLocation(),Entry.Rotator(),0.,false);
                L.At=L.Offset=0.;
            }
        }
    }
}

void ARiverfrontRoadAgent::UpdateLights(bool Night,bool Stopping,bool Project)
{
    const int32 State=int32(Night)+2*int32(Stopping)+4*int32(Project);
    if (State==LightState) return;
    LightState=State;
    if (HeadLens) HeadLens->SetScalarParameterValue(TEXT("Glow"),Night?4.f:.06f);
    if (TailLens) TailLens->SetScalarParameterValue(TEXT("Glow"),Stopping?4.7f:(Night?1.55f:.12f));
    if (Project && !DippedBeam)
    {
        DippedBeam=NewObject<USpotLightComponent>(this);
        AddInstanceComponent(DippedBeam); DippedBeam->SetupAttachment(Bounds);
        DippedBeam->SetMobility(EComponentMobility::Movable);
        DippedBeam->SetRelativeLocationAndRotation(FVector(HalfLength-4.,0,33.),FRotator(-8,0,0));
        DippedBeam->SetIntensityUnits(ELightUnits::Lumens);
        DippedBeam->SetIntensity(800.);DippedBeam->SetTemperature(4350.);DippedBeam->SetUseTemperature(true);
        DippedBeam->SetAttenuationRadius(2650.);DippedBeam->SetInnerConeAngle(13.);DippedBeam->SetOuterConeAngle(31.);
        DippedBeam->SetCastShadows(false);DippedBeam->RegisterComponent();
    }
    if (DippedBeam) DippedBeam->SetVisibility(Project);
}
