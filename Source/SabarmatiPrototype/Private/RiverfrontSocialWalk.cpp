#include "RiverfrontPedestrian.h"
#include "RiverfrontCar.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontTraffic.h"
#include "RiverfrontWorld.h"
#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
constexpr int32 PromenadeKerb = 1;
constexpr int32 ShopKerb = 2;
constexpr int32 FamilySize = 3;

// Props are authored in resident mesh-space centimetres and attached in the corresponding
// bone's reference pose. The actual walking animation then carries the prop with the bone.
void AttachCarriedPart(ARiverfrontPedestrian& Person, USkeletalMeshComponent& Mesh, FName Bone,
    const FTransform& Reference, UStaticMesh& Shape, UMaterialInterface& Surface,
    const FVector& Centre, const FRotator& Rotation, const FVector& Size)
{
    auto* Piece = NewObject<UStaticMeshComponent>(&Person);
    Person.AddInstanceComponent(Piece);
    Piece->SetupAttachment(&Mesh, Bone);
    Piece->SetRelativeTransform(FTransform(Rotation, Centre, Size / 100.).GetRelativeTransform(Reference));
    Piece->SetStaticMesh(&Shape);
    Piece->SetMaterial(0, &Surface);
    Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Piece->SetCanEverAffectNavigation(false);
    Piece->SetCullDistance(3500.f);
    Piece->RegisterComponent();
}


// Routes live on the stationary district, while the walkers move along them. The first section
// has three pavement lanes; those lanes narrow into a queue through the actual shop entrance.
USplineComponent* MakeFamilyRoute(ARiverfrontWorld& District, int32 Rank)
{
    const TArray<FVector>& Doorway = District.GetShopVisit(true);
    if (Doorway.Num() != 4) return nullptr;
    const double Lane = Rank - 1.;
    const double CrossX = ARiverfrontWorld::AtalX - 55. + Lane * 77.;
    const FVector Entrance = Doorway[0];
    const FVector IntoShop = (Doorway[1] - Entrance).GetSafeNormal2D();
    TArray<FVector> Stops = {
        {-6200. + Lane * 115., 3504. + Lane * 32., 2.},
        {CrossX, 3504. + Lane * 32., 2.},
        {CrossX, 4715. + Lane * 24., 4.},
        {CrossX, 5070. - Lane * 77., 4.},
        {Entrance.X - 285. - Rank * 65., 5070. - Lane * 77., 4.},
        Entrance - IntoShop * (Rank * 90.),
        Doorway[1] - IntoShop * (Rank * 90.),
        Doorway[2] - IntoShop * (Rank * 90.),
        Doorway.Last() + FVector(Lane * 30., -Rank * 88., 0.)
    };

    auto* Spline = NewObject<USplineComponent>(&District);
    District.AddInstanceComponent(Spline);
    Spline->SetupAttachment(District.GetRootComponent());
    Spline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Spline->SetCanEverAffectNavigation(false);
    Spline->RegisterComponent();
    Spline->SetSplinePoints(Stops, ESplineCoordinateSpace::World, false);
    for (int32 Index = 0; Index < Stops.Num(); ++Index)
        Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    Spline->UpdateSpline();

    const FCollisionObjectQueryParams Statics(ECC_WorldStatic);
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(SocialFootway), false);
    const FCollisionShape Body = FCollisionShape::MakeCapsule(25.f, 79.f);
    FVector Previous = Stops[0] + FVector(0., 0., 93.);
    const int32 Samples = FMath::CeilToInt(Spline->GetSplineLength() / 75.);
    for (int32 I = 0; I <= Samples; ++I)
    {
        const FVector Feet = Spline->GetLocationAtDistanceAlongSpline(Spline->GetSplineLength() * I / Samples,
            ESplineCoordinateSpace::World);
        const FVector Centre = Feet + FVector(0., 0., 93.);
        FHitResult Hit;
        const bool bFloor = District.GetWorld()->LineTraceSingleByObjectType(Hit, Feet + FVector(0.,0.,23.),
            Feet - FVector(0.,0.,20.), Statics, Query) && Hit.ImpactNormal.Z > .75;
        if (!bFloor || District.GetWorld()->SweepSingleByObjectType(Hit, Previous, Centre,
            FQuat::Identity, Statics, Body, Query))
        {
            UE_LOG(LogTemp, Warning, TEXT("Sabarmati family: route %d blocked near %s on %s (floor=%d)."),
                Rank, *Feet.ToCompactString(), *GetNameSafe(Hit.GetComponent()), bFloor);
            if (const auto* Instances = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
            {
                FTransform Solid;
                if (Instances->GetInstanceTransform(Hit.Item, Solid, true))
                    UE_LOG(LogTemp, Warning, TEXT("Sabarmati family: blocked instance %d at %s scaled %s."), Hit.Item,
                        *Solid.GetLocation().ToCompactString(), *Solid.GetScale3D().ToCompactString());
            }
            Spline->DestroyComponent();
            return nullptr;
        }
        Previous = Centre;
    }
    return Spline;
}
}

bool ARiverfrontPedestrian::SpawnFamilyOuting(ARiverfrontWorld& District, USkeletalMesh& FirstModel,
    UAnimSequence& FirstWalk, USkeletalMesh& SecondModel, UAnimSequence& SecondWalk)
{
    const FColor ShirtDyes[] = {FColor(143, 150, 100), FColor(139, 79, 64), FColor(151, 179, 192)};
    const FColor TrouserDyes[] = {FColor(69, 72, 65), FColor(183, 174, 148), FColor(65, 73, 85)};
    TArray<USplineComponent*> Routes;
    TArray<ARiverfrontPedestrian*> Walkers;
    for (int32 Rank = 0; Rank < FamilySize; ++Rank)
    {
        auto* Route = MakeFamilyRoute(District, Rank);
        if (!Route) break;
        Routes.Add(Route);
        const FVector Start = Route->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
        auto* Person = District.GetWorld()->SpawnActor<ARiverfrontPedestrian>(Start, FRotator::ZeroRotator);
        if (!Person) break;
        Walkers.Add(Person);
        const double Paces[] = {126., 118., 123.};
        const bool bElder = Rank == 1;
        Person->Configure(bElder ? &SecondModel : &FirstModel, bElder ? &SecondWalk : &FirstWalk,
            Start, Route->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World), 1., 0., Paces[Rank], 27 + Rank, bElder);
        Person->SetGarmentColor(TEXT("top_teal_woven"), ShirtDyes[Rank]);
        Person->SetGarmentColor(TEXT("top_seams"), ShirtDyes[Rank]);
        Person->SetGarmentColor(TEXT("bottom_denim"), TrouserDyes[Rank]);
        if (bElder) Person->SetGarmentColor(TEXT("scarf"), FColor(170, 144, 96));
        Person->SetActorLocation(Start);
        Person->FamilyRoute = Route;
        Person->Tags.Add(TEXT("RiverfrontFamily"));
        Person->SetMoving(false);
        Person->SetActorTickEnabled(Rank == 0);
        const FLinearColor SkinTones[] = {FLinearColor(.85f,.76f,.70f),
            FLinearColor(1.08f,1.00f,.94f), FLinearColor(.75f,.69f,.65f)};
        // The elder's silver threads retain the luminance variation in the original fibre map.
        const FLinearColor HairTints[] = {FLinearColor(.86f, .90f, .94f),
            FLinearColor(5.5f, 8.8f, 12.8f), FLinearColor(1.30f, 1.06f, .91f)};
        Person->DressCloseResident(SkinTones[Rank], HairTints[Rank], Rank != 2);
    }
    if (Walkers.Num() != FamilySize)
    {
        for (auto* Walker : Walkers) Walker->Destroy();
        for (auto* Route : Routes) Route->DestroyComponent();
        return false;
    }

    auto* Leader = Walkers[0];
    for (auto* Relative : Walkers) Leader->Family.Add(Relative);
    Leader->RestLeft = 1.2;
    Leader->EnablePlayerAwareness();
    UE_LOG(LogTemp, Display, TEXT("Sabarmati family: 3 relatives on cleared footways to Nadi Chai, first kerb %s."),
        *Routes[0]->GetLocationAtSplinePoint(PromenadeKerb, ESplineCoordinateSpace::World).ToCompactString());
    return true;
}

void ARiverfrontPedestrian::LookTowards(const FVector& Point, float Delta)
{
    const FVector To = Point - GetActorLocation();
    if (To.IsNearlyZero()) return;
    const double Yaw = FMath::FixedTurn(GetActorRotation().Yaw, To.Rotation().Yaw + MeshYawToForward, 155. * Delta);
    SetActorRotation(FRotator(0., Yaw, 0.));
}

void ARiverfrontPedestrian::TickFamily(float Delta)
{
    for (const auto& Member : Family)
        if (auto* Person=Member.Get()) Person->UpdateGait(Delta);
    const bool bCrossing = (FamilyFrom == PromenadeKerb && FamilyTo == ShopKerb)
        || (FamilyFrom == ShopKerb && FamilyTo == PromenadeKerb);
    if (!bFamilyDeparted)
    {
        RestLeft = FMath::Max(0., RestLeft - Delta);
        if (RestLeft > 0.)
        {
            // At the two ends of the outing, the relatives turn to one another while waiting.
            if (FamilyFrom == 0 || FamilyFrom == FamilyRoute->GetNumberOfSplinePoints() - 1)
                for (int32 I = 0; I < Family.Num(); ++I)
                    if (auto* P = Family[I].Get())
                        if (auto* Companion = Family[(I + 1) % Family.Num()].Get())
                            P->LookTowards(Companion->GetActorLocation(), Delta);
            return;
        }
        if (bCrossing)
        {
            const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
            const auto* Traffic = Mode ? Mode->GetTraffic() : nullptr;
            if (!Traffic || !Traffic->WalkSignal())
            {
                if (!bWaitedAtKerb)
                {
                    for (const auto& Member : Family)
                        if (auto* P = Member.Get()) ++P->KerbWaits;
                    bWaitedAtKerb = true;
                }
                return;
            }
            const auto* PC = GetWorld()->GetFirstPlayerController();
            const auto* Car = PC ? Cast<ARiverfrontCar>(PC->GetPawn()) : nullptr;
            if (Car && Car->GetSpeedKph() > 7.f && FVector::Dist2D(Car->GetActorLocation(), GetActorLocation()) < 1450.) return;
        }
        for (const auto& Member : Family)
            if (auto* P = Member.Get())
            {
                P->bAtCrosswalk = bCrossing;
                P->bSteppingOff = bCrossing;
                P->SetMoving(true);
            }
        bFamilyDeparted = true;
    }

    // Follow the slowest relative without locking gait phases or abruptly pulling a straggler forward.
    double SlowestFraction = 1.;
    const int32 Direction = FamilyTo > FamilyFrom ? 1 : -1;
    for (const auto& Member : Family)
        if (const auto* P = Member.Get())
        {
            const double Start = P->FamilyRoute->GetDistanceAlongSplineAtSplinePoint(FamilyFrom);
            const double End = P->FamilyRoute->GetDistanceAlongSplineAtSplinePoint(FamilyTo);
            SlowestFraction = FMath::Min(SlowestFraction, (P->FamilyPosition - Start) / (End - Start));
        }
    bool bTogether = true;
    for (const auto& Member : Family)
        if (auto* P = Member.Get())
        {
            const double Start = P->FamilyRoute->GetDistanceAlongSplineAtSplinePoint(FamilyFrom);
            const double End = P->FamilyRoute->GetDistanceAlongSplineAtSplinePoint(FamilyTo);
            const double SegmentLength = FMath::Abs(End - Start);
            const double Limit = Start + Direction * FMath::Min(SegmentLength, SegmentLength * SlowestFraction + 38.);
            P->FamilyPosition = FMath::FInterpConstantTo(P->FamilyPosition, Limit, double(Delta), P->Speed*P->GaitFraction);
            const FVector At = P->FamilyRoute->GetLocationAtDistanceAlongSpline(P->FamilyPosition, ESplineCoordinateSpace::World);
            const FVector Travel = At - P->GetActorLocation();
            if (Travel.SizeSquared2D() > .01)
                P->LookTowards(P->GetActorLocation() + Travel * 100., Delta);
            P->SetActorLocation(At);
            const bool bArrived = FMath::Abs(P->FamilyPosition - End) < .5;
            const bool bGather = FamilyTo == 0 || FamilyTo == P->FamilyRoute->GetNumberOfSplinePoints() - 1
                || (Direction > 0 && FamilyTo == PromenadeKerb) || (Direction < 0 && FamilyTo == ShopKerb);
            if (bArrived && bGather) P->SetMoving(false);
            bTogether &= bArrived;
        }
    if (!bTogether) return;

    const int32 Last = FamilyRoute->GetNumberOfSplinePoints() - 1;
    for (const auto& Member : Family)
        if (auto* P = Member.Get())
        {
            if (bCrossing) ++P->Crossings;
            P->bAtCrosswalk = P->bSteppingOff = false;
            if (FamilyTo == Last) ++P->CounterVisits;
        }
    if (bCrossing || FamilyTo == Last)
        UE_LOG(LogTemp, Display, TEXT("Sabarmati family: %s together; crossings=%d counter_visits=%d."),
            bCrossing ? TEXT("cleared kerb") : TEXT("reached tea counter"), Crossings, CounterVisits);
    FamilyFrom = FamilyTo;
    FamilyTo += FamilyFrom == Last ? -1 : FamilyFrom == 0 ? 1 : Direction;
    RestLeft = FamilyFrom == Last ? 8. : FamilyFrom == 0 ? 6.5 : 0.;
    bFamilyDeparted = false;
    bWaitedAtKerb = false;
}

// A small original unbadged camera on a neck strap. The accessory follows the public rig's chest.
void ARiverfrontPedestrian::EquipVisitorCamera()
{
    auto* Model = Mesh->GetSkeletalMeshAsset();
    const FName Chest(TEXT("Chest"));
    if (!Model || Mesh->GetBoneIndex(Chest) == INDEX_NONE) return;
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Body = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sabarmati/Riverfront/Vehicles/Fleet/MI_Fleet_rubber.MI_Fleet_rubber"));
    auto* Ring = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sabarmati/Riverfront/Vehicles/Fleet/MI_Fleet_iron.MI_Fleet_iron"));
    if (!Cube || !Cylinder || !Body || !Ring) return;
    const FTransform ChestReference(Model->GetComposedRefPoseMatrix(Chest));
    auto Part = [&](UStaticMesh* Shape, UMaterialInterface* Surface, const FVector& Centre, const FRotator& Rotation, const FVector& Size)
    {
        AttachCarriedPart(*this, *Mesh, Chest, ChestReference, *Shape, *Surface, Centre, Rotation, Size);
    };
    Part(Cube, Body, FVector(0., 17., 112.), FRotator::ZeroRotator, FVector(13., 6., 8.));
    Part(Cylinder, Ring, FVector(2.2, 21., 112.), FRotator(0.,0.,-90.), FVector(5.3, 5.3, 4.));
    Part(Cylinder, Body, FVector(2.2, 23.2, 112.), FRotator(0.,0.,-90.), FVector(4.5, 4.5, 1.8));
    for (double Side : {-1., 1.})
    {
        const FVector Shoulder(Side * 7.5, 5.5, 144.);
        const FVector Lug(Side * 6., 16., 114.);
        const FVector Strap = Shoulder - Lug;
        Part(Cube, Body, (Lug + Shoulder) * .5, FRotationMatrix::MakeFromZ(Strap).Rotator(), FVector(1.1, .38, Strap.Size()));
    }
}

// The visiting printmaker carries a cloth-bound A3 portfolio with paper edges, corner
// protectors and a flat webbing handle. All pieces are attached to the animated hand.
void ARiverfrontPedestrian::EquipPrintPortfolio()
{
    const FName Hand(TEXT("Handr"));
    auto* Model = Mesh->GetSkeletalMeshAsset();
    if (!Model || Mesh->GetBoneIndex(Hand) == INDEX_NONE) return;
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cover = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sabarmati/Riverfront/Vehicles/Fleet/MI_Fleet_teal.MI_Fleet_teal"));
    auto* Paper = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sabarmati/Riverfront/Vehicles/Fleet/MI_Fleet_cream.MI_Fleet_cream"));
    auto* Trim = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sabarmati/Riverfront/Vehicles/Fleet/MI_Fleet_rubber.MI_Fleet_rubber"));
    if (!Cube || !Cover || !Paper || !Trim) return;
    const FTransform Reference(Model->GetComposedRefPoseMatrix(Hand));
    const FVector Grip = Reference.GetTranslation();
    const double Side = Grip.X < 0. ? -1. : 1.;
    const FVector Centre = Grip + FVector(3.6 * Side, 0., -20.);
    auto Part = [&](UMaterialInterface& Material, const FVector& Offset, const FVector& Size)
    {
        AttachCarriedPart(*this, *Mesh, Hand, Reference, *Cube, Material,
            Centre + Offset, FRotator::ZeroRotator, Size);
    };
    Part(*Paper, FVector::ZeroVector, FVector(2.1, 41.2, 29.2));
    for (double Face : {-1.,1.})
        Part(*Cover, FVector(Face * 1.2,0.,0.), FVector(.32,42.6,30.2));
    Part(*Trim, FVector(0.,0.,-14.9), FVector(2.8,42.6,1.2));
    for (double End : {-1.,1.})
        Part(*Trim, FVector(0.,End*5.7,17.2), FVector(1.7,1.4,5.));
    Part(*Trim, FVector(0.,0.,19.4), FVector(1.7,12.7,1.25));
}

// The family and the two visitors get an individual haircut and complexion.
void ARiverfrontPedestrian::DressCloseResident(FLinearColor ComplexionTint, FLinearColor FibreTint, bool bSweptHair)
{
    auto* Model = Mesh->GetSkeletalMeshAsset();
    if (!Model || !Model->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"))) return;
    const int32 SkinSlot = Mesh->GetMaterialIndex(TEXT("skin"));
    if (SkinSlot != INDEX_NONE)
        if (auto* Skin = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(SkinSlot)))
            Skin->SetVectorParameterValue(TEXT("BaseColorFactor"), ComplexionTint);
    auto* Fibre = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_RiverSweptHair.M_RiverSweptHair"));
    bool bStyled = false;
    const int32 HairSlot = Mesh->GetMaterialIndex(TEXT("hair"));
    const FName HeadBone(TEXT("Head"));
    const FReferenceSkeleton& Skeleton = Model->GetRefSkeleton();
    const int32 HeadIndex = Skeleton.FindBoneIndex(HeadBone);
    if (bSweptHair && Fibre && HairSlot != INDEX_NONE && HeadIndex != INDEX_NONE)
    {
        auto* Cut = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/SM_RiverSweptHair/StaticMeshes/SM_RiverSweptHair.SM_RiverSweptHair"));
        if (Cut)
        {
            auto* Hair = NewObject<UStaticMeshComponent>(this);
            AddInstanceComponent(Hair);
            Hair->SetupAttachment(Mesh, HeadBone);
            Hair->SetStaticMesh(Cut);
            const FTransform Head = Skeleton.GetBoneAbsoluteTransform(HeadIndex);
            Hair->SetRelativeTransform(FTransform(FQuat::Identity, Head.GetLocation()).GetRelativeTransform(Head));
            if (auto* Material = Hair->CreateDynamicMaterialInstance(0, Fibre))
                Material->SetVectorParameterValue(TEXT("BaseColorFactor"), FibreTint);
            Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Hair->SetCanEverAffectNavigation(false);
            Hair->SetCullDistance(14000.f);
            Hair->RegisterComponent();
            for (int32 LOD = 0; LOD < Mesh->GetNumLODs(); ++LOD)
                Mesh->ShowMaterialSection(HairSlot, INDEX_NONE, false, LOD);
            bStyled = true;
        }
    }
    if (!bStyled && Fibre && HairSlot != INDEX_NONE)
        if (auto* Hair = Mesh->CreateDynamicMaterialInstance(HairSlot, Fibre))
            Hair->SetVectorParameterValue(TEXT("BaseColorFactor"), FibreTint);
}
