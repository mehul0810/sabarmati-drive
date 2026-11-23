#include "RiverfrontTraffic.h"
#include "Algo/Reverse.h"
#include "RiverfrontCar.h"
#include "RiverfrontPedestrian.h"
#include "RiverfrontCrossings.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
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
struct FVehicleShape { const TCHAR* Name; float Length, Width, Wheel, Axle, Track; int32 WheelCount; };
const FVehicleShape Bodies[]={
    {TEXT("Rickshaw"),257,141,22,77,53,3},{TEXT("Scooter"),188,52,22,63,0,2},
    {TEXT("Sedan"),467,183,32,143,78,4},{TEXT("Bus"),860,240,43,273,97,4},
    {TEXT("PremiumHatch"),406,175,31,122,73,4},{TEXT("Motorcycle"),195,60,28,65,0,2},
    {TEXT("Van"),490,194,35,149,80,4},{TEXT("LuxurySUV"),508,198,37,157,84,4},
    {TEXT("Truck"),683,228,44,215,92,4},{TEXT("Sport"),436,194,32,132,83,4},
    {TEXT("Ambulance"),513,201,35,156,84,4},{TEXT("Lorry"),810,239,48,258,100,4}};
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
    const auto& S=Bodies[Style%UE_ARRAY_COUNT(Bodies)];
    HalfLength=S.Length*.5; HalfWidth=S.Width*.5; TyreRadius=S.Wheel; IsEmergency=Style%UE_ARRAY_COUNT(Bodies)==10;
    Bounds->SetBoxExtent(FVector(HalfLength,HalfWidth,S.Wheel*.85));
    const FString Base=TEXT("/Game/Sabarmati/Riverfront/Vehicles/");
    auto* Model=MeshAt(Base+TEXT("Fleet/SM_Traffic")+S.Name);
    auto* Left=MeshAt(Base+TEXT("SM_HatchWheelL"));
    auto* Right=MeshAt(Base+TEXT("SM_HatchWheelR"));
    if (!Model || !Left || !Right) return false;
    auto Visual=[&](UStaticMesh* M,USceneComponent* Parent,const FVector& At,const FRotator& Facing,const FVector& Scale)
    {
        auto* C=NewObject<UStaticMeshComponent>(this); AddInstanceComponent(C); C->SetupAttachment(Parent);
        C->SetStaticMesh(M); C->SetRelativeLocationAndRotation(At,Facing); C->SetRelativeScale3D(Scale);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCanEverAffectNavigation(false); C->RegisterComponent(); return C;
    };
    auto* Shell=Visual(Model,Bounds,FVector(0,0,-S.Wheel-6),FRotator(0,-90,0),FVector::OneVector);
    if (const int32 Paint=Shell->GetMaterialIndex(TEXT("body")); Paint!=INDEX_NONE)
    {
        const FLinearColor Colors[]={FLinearColor(.08,.15,.14),FLinearColor(.18,.27,.30),FLinearColor(.35,.16,.082),FLinearColor(.37,.36,.29)};
        auto* Instance=Shell->CreateDynamicMaterialInstance(Paint);
        if (Instance) Instance->SetVectorParameterValue(TEXT("Tint"),Colors[Variant%4]);
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
        const double Scale=S.Wheel/31.6;
        Visual(Side?Right:Left,Pivot,FVector::ZeroVector,FRotator(0,Side?90:-90,0),FVector(Scale*(Bike?.52:1.),Scale,Scale));
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
bool ARiverfrontRoadAgent::MoveOnRoad(const FVector& Position,const FRotator& Heading,double Travel,bool Near)
{
    FHitResult Hit;
    SetActorLocationAndRotation(Position,Heading,Near,&Hit,ETeleportType::None);
    if (Hit.bBlockingHit) return false;
    if (Near)
    {
        Spin=FMath::Fmod(Spin-float(FMath::RadiansToDegrees(Travel/TyreRadius)),360.f);
        for (const auto& P:Wheels) P->SetRelativeRotation(FRotator(Spin,0,0));
        for (int32 I=0;I<Beacons.Num();++I) Beacons[I]->SetVisibility((int32(GetWorld()->GetTimeSeconds()*5.)+I)%2==0);
    }
    return true;
}
ARiverfrontTraffic::ARiverfrontTraffic()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("RoadNetwork"));
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.05f;
}
void ARiverfrontTraffic::Lane(const TArray<FVector>& Nodes,bool Reverse)
{
    if (Nodes.Num()<3) return;
    TArray<FVector> Ordered=Nodes;
    if (Reverse) Algo::Reverse(Ordered);
    const int32 N=Ordered.Num();
    TArray<FVector> P;
    for (int32 I=0;I<N;++I)
    {
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
    Spline->SetClosedLoop(true,true); Routes.Add(Spline);
}
void ARiverfrontTraffic::Start(bool Mapped)
{
    bCity=Mapped;
    if (!bCity)
    {
        const double W=4090., E=-26010.;
        for (const FVector2D Pair:{FVector2D(-64400,3150),FVector2D(3150,59310),FVector2D(-64400,59310)})
        {
            const double L=Pair.X,R=Pair.Y;
            const TArray<FVector> Nodes={{L,W,1020},{R,W,R==3150?720.:640.},{R,E,1020},{L,E,L==3150?720.:1520.}};
            Lane(Nodes,false); Lane(Nodes,true);
        }
        Junctions={PedestrianCrossing,FVector(3150,W,0),FVector(3150,E,0),FVector(-64400,W,0),FVector(59310,E,0)};
    }
    else
    {
        FString Json; TSharedPtr<FJsonObject> Doc;
        if (!FFileHelper::LoadFileToString(Json,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("CityData/AhmedabadRoutes.json"))) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Doc) || !Doc.IsValid()) return;
        const TArray<TSharedPtr<FJsonValue>>* Data=nullptr;
        if (!Doc->TryGetArrayField(TEXT("routes"),Data) || Data->Num()>32) return;
        for (const auto& V:*Data)
        {
            TArray<FVector> Nodes;
            const auto O=V->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
            if (!O || !O->TryGetArrayField(TEXT("points"),Points) || Points->Num()<3 || Points->Num()>1500) continue;
            for (const auto& Pt:*Points)
            {
                const auto& N=Pt->AsArray(); if (N.Num()!=3) continue;
                Nodes.Emplace(N[0]->AsNumber(),N[1]->AsNumber(),N[2]->AsNumber());
            }
            Lane(Nodes,false); Lane(Nodes,true);
        }
    }
    if (!Routes.Num()) return;
    TSet<TPair<FIntPoint,FIntPoint>> Seen;
    for (int32 I=0;I<Routes.Num();I+=2)
    {
        auto* Road=Routes[I].Get(); const int32 N=Road->GetNumberOfSplinePoints();
        for (int32 J=0;J<N;++J)
        {
            const FVector A=Road->GetLocationAtSplinePoint(J,ESplineCoordinateSpace::World);
            const FVector B=Road->GetLocationAtSplinePoint((J+1)%N,ESplineCoordinateSpace::World);
            if (FVector::DistSquared2D(A,B)<100.) continue;
            const TPair<FIntPoint,FIntPoint> K{FIntPoint(FMath::RoundToInt(A.X/75),FMath::RoundToInt(A.Y/75)),
                FIntPoint(FMath::RoundToInt(B.X/75),FMath::RoundToInt(B.Y/75))};
            if (!Seen.Contains(K)) { Seen.Add(K); MapLines.Emplace(A,B); }
        }
    }
    SetUpSignals();
    for (TActorIterator<ARiverfrontCar> It(GetWorld());It;++It) Parked.Add(*It);
    for (int32 I=0;I<(bCity?16:23);++I)
    {
        const int32 R=I%Routes.Num(); auto* S=Routes[R].Get();
        const double F=FMath::Frac(.19+I*.38196601125);
        const double Along=F*S->GetSplineLength();
        const FVector P=S->GetLocationAtDistanceAlongSpline(Along,ESplineCoordinateSpace::World);
        bool Clear=true;
        for (const auto& C:Parked) if (C.IsValid() && FVector::Dist2D(P,C->GetActorLocation())<1600) Clear=false;
        if (!Clear) continue;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* A=GetWorld()->SpawnActor<ARiverfrontRoadAgent>(P+FVector(0,0,90),FRotator::ZeroRotator,Params);
        if (!A || !A->Assemble(I,I*7)) { if (A) A->Destroy(); continue; }
        FSabarmatiLane L; L.Route=R; L.At=Along; L.Cruise=I%12==3?642.:760.+(I*67)%270;
        Drivers.Add(L); Agents.Add(A);
        const FVector Fwd=S->GetDirectionAtDistanceAlongSpline(Along,ESplineCoordinateSpace::World);
        double Z=(!bCity && FMath::Abs(Fwd.Y)>.70)?RiverfrontCrossings::DeckZ(P.Y):0.;
        if (!bCity && FMath::Abs(P.X-3150)<700 && FMath::Abs(Fwd.Y)>.7) Z=0.;
        A->SetActorLocationAndRotation(P+FVector(0,0,Z+A->TyreRadius+6),Fwd.Rotation());
    }
    UE_LOG(LogTemp,Display,TEXT("Sabarmati road traffic: %d agents on %d connected directed circuits."),Agents.Num(),Routes.Num());
}
int32 ARiverfrontTraffic::Phase() const
{
    const double P=FMath::Fmod(Elapsed,43.f);
    return P<16?0:P<19?1:P<20?2:P<28?3:P<30?4:5;
}
bool ARiverfrontTraffic::WalkSignal() const { return Phase()==5 && FMath::Fmod(Elapsed,43.f)<32.f; }
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
float ARiverfrontTraffic::StopDistance(const FVector& At,const FVector& D,double Length,bool& Obstacle,int32 Index)
{
    double Limit=1.e7; Obstacle=false;
    const int32 H=Phase(); const bool Along=FMath::Abs(D.X)>FMath::Abs(D.Y);
    for (int32 J=0;J<Junctions.Num();++J)
    {
        const bool Red=Along?H!=0:H!=3;
        const FVector V=Junctions[J]-At;
        if (Red && FMath::Abs(V.X*D.Y-V.Y*D.X)<295 && V.Dot(D)>0 && V.Dot(D)<2350)
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
            Ahead(C->GetActorLocation(),210,91);
        }
    if (const auto* PC=GetWorld()->GetFirstPlayerController())
        if (const APawn* Pawn=PC->GetPawn();Pawn && !Cast<ARiverfrontCar>(Pawn)) Ahead(Pawn->GetActorLocation(),36,37);
    for (TActorIterator<ARiverfrontPedestrian> It(GetWorld());It;++It)
        if (It->IsCrossingStreet()) Ahead(It->GetActorLocation(),33,38);
    return Limit;
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
    for (int32 I=0;I<Agents.Num();++I)
    {
        auto* A=Agents[I].Get(); auto& L=Drivers[I]; auto* R=Routes[L.Route].Get();
        const bool Near=FVector::DistSquared2D(A->GetActorLocation(),Eye)<FMath::Square(23000.);
        const double Length=R->GetSplineLength();
        const FVector D=R->GetDirectionAtDistanceAlongSpline(L.At,ESplineCoordinateSpace::World).GetSafeNormal2D();
        bool ParkedAhead=false;
        const float Gap=StopDistance(A->GetActorLocation(),D,A->HalfLength,ParkedAhead,I);
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
        const double Want=FMath::Min(L.Cruise*(Yielding?.45:(A->IsEmergency?1.18:1.)),FMath::Sqrt(2.*440.*FMath::Max(0.f,Gap)));
        L.Speed=FMath::FInterpConstantTo(L.Speed,Want,Dt,Want>L.Speed?170.:580.);
        if (Want<15 && !L.WasStopped) { ++Stops; L.WasStopped=true; }
        if (Want>110) L.WasStopped=false;
        const double Advance=L.Speed*Dt;
        const double Next=FMath::Fmod(L.At+Advance,Length);
        FVector P=R->GetLocationAtDistanceAlongSpline(Next,ESplineCoordinateSpace::World);
        const FVector F=R->GetDirectionAtDistanceAlongSpline(Next,ESplineCoordinateSpace::World).GetSafeNormal2D();
        P+=FVector(-F.Y,F.X,0)*L.Offset;
        double Z=0;
        if (!bCity && FMath::Abs(F.Y)>.65 && FMath::Abs(P.X-3150)>850) Z=RiverfrontCrossings::DeckZ(P.Y);
        P.Z=Z+A->TyreRadius+6;
        if (Near)
        {
            FHitResult Ground;
            if (GetWorld()->LineTraceSingleByObjectType(Ground,P+FVector(0,0,220),P-FVector(0,0,220),FCollisionObjectQueryParams(ECC_WorldStatic))
                && Ground.ImpactNormal.Z>.78 && FMath::Abs(Ground.ImpactPoint.Z-Z)<28) P.Z=Ground.ImpactPoint.Z+A->TyreRadius+6;
        }
        if (A->MoveOnRoad(P,F.Rotation(),Advance,Near))
        {
            if (Advance>0 && D.Dot(F)<.9998) ++Turns;
            TravelCm+=Advance; L.At=Next;
        }
        else L.Speed=0;
        A->SetActorHiddenInGame(!Near && FVector::DistSquared2D(P,Eye)>FMath::Square(51000.));
    }
}
