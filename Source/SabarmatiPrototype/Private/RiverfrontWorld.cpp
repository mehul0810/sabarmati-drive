#include "RiverfrontWorld.h"
#include "DistrictLoadProgress.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"

const FVector ARiverfrontWorld::SpawnPoint(-4580, 3180, 110);
const FVector ARiverfrontWorld::CarPosition(-4180, 3960, 68);

namespace
{
constexpr double BridgeX = 3150.0;
constexpr double BridgeSide = 540.0;
constexpr double SegmentHalfLength = 7600.0;
constexpr double StairX = -3215.0;
constexpr double StairHalfWidth = 156.0;
// SRFDCL channel size, not a survey. Keep the starting bank and spawn positions fixed.
constexpr double ChannelWidth = 26300.0;
constexpr double NearWaterEdge = 2190.0;
constexpr double ChannelCenter = NearWaterEdge - ChannelWidth * .5;
// The riverfront roads start at the 35.8 m inner kerb on both banks.
constexpr double BridgeHalfLength = 3580.0 - ChannelCenter;
constexpr double BankY(double Bank, double Distance)
{
    return Bank * Distance + (Bank < 0.0 ? 2.0 * ChannelCenter : 0.0);
}

UMaterialInterface* LoadRiverfrontMaterial(const TCHAR* Name)
{
    const FString Package = FString(TEXT("/Game/Sabarmati/Riverfront/")) + Name;
    return FPackageName::DoesPackageExist(Package)
        ? LoadObject<UMaterialInterface>(nullptr, *(Package + TEXT(".") + FPackageName::GetLongPackageAssetName(Package))) : nullptr;
}
}

ARiverfrontWorld::ARiverfrontWorld()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = .24f;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // The riverfront is stationary; Unreal requires a static parent for these mesh batches.
    Root->SetMobility(EComponentMobility::Static);
    SetRootComponent(Root);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    BoxMesh = Cube.Object;
    CylinderMesh = Cylinder.Object;
    SphereMesh = Sphere.Object;
    PlaneMesh = Plane.Object;
    BaseMaterial = Material.Object;
}

void ARiverfrontWorld::Build()
{
    if (!Batches.IsEmpty())
    {
        return;
    }

    LoadSurfaceMaterials();

    DistrictLoadProgress::Reached(1);
    BuildBanks();
    BuildBridge();
    BuildAtalBridge();
    BuildRiverfrontCorridor();
    DistrictLoadProgress::Reached(2);
    BuildFrontage();
    BuildStreetFurniture();
    BuildWestBankGarden();
    DressStreetSurface();
    DressMarketUtilities();
    DistrictLoadProgress::Reached(3);
    BuildLighting();
    BuildWeather();
}

void ARiverfrontWorld::LoadSurfaceMaterials()
{
    if (!SurfaceMaterials.IsEmpty())
    {
        return;
    }
    const TPair<ETint, const TCHAR*> Surfaces[] = {
        {ETint::Asphalt, TEXT("M_Asphalt")}, {ETint::Paving, TEXT("M_Paving")},
        {ETint::Stone, TEXT("M_Concrete")}, {ETint::Water, TEXT("M_River")},
        {ETint::Retaining, TEXT("M_Retaining")}, {ETint::Plaster, TEXT("M_FacadePlaster")},
        {ETint::FacadeStone, TEXT("M_FacadeSandstone")}, {ETint::Terracotta, TEXT("M_FacadeTerracotta")},
        {ETint::Glass, TEXT("M_DistrictWindow")}, {ETint::Rail, TEXT("M_CoatedIron")},
        {ETint::Leaf, TEXT("Foliage/M_LeafSprig")}, {ETint::RoadPaint, TEXT("M_RoadStripe")},
        {ETint::ClearGlass, TEXT("M_DistrictGlass")}, {ETint::Brass, TEXT("M_DistrictBrass")},
        {ETint::Timber, TEXT("M_DistrictTimber")}, {ETint::Canvas, TEXT("M_DistrictCanvas")},
        {ETint::Ceramic, TEXT("M_DistrictCeramic")}
    };
    for (const auto& Surface : Surfaces)
    {
        SurfaceMaterials.Add(static_cast<int32>(Surface.Key), LoadRiverfrontMaterial(Surface.Value));
    }
    int32 LoadedMaterials = 0;
    for (const auto& Entry : SurfaceMaterials)
    {
        LoadedMaterials += IsValid(Entry.Value.Get()) ? 1 : 0;
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati riverfront: %d/%d surface materials loaded."),
        LoadedMaterials, SurfaceMaterials.Num());
}

void ARiverfrontWorld::Shape(EShape Mesh, ETint Tint, const FVector& Pos, const FVector& Size,
    bool bSolid, FRotator Rot)
{
    constexpr int32 TintCount = static_cast<int32>(ETint::Count);
    static const FColor Colors[TintCount] = {
        FColor(47, 54, 60), FColor(160, 146, 124), FColor(179, 132, 89), FColor(206, 190, 162),
        FColor(130, 74, 56), FColor(88, 84, 80), FColor(39, 46, 51), FColor(63, 93, 97),
        FColor(49, 63, 63), FColor(163, 152, 138), FColor(232, 223, 191), FColor(50, 99, 96),
        FColor(95, 135, 126), FColor(78, 64, 51), FColor(89, 107, 59), FColor(48, 79, 48),
        FColor(229, 166, 89), FColor(40, 106, 99), FColor(193, 160, 115),
        FColor(223, 209, 183), FColor(187, 142, 98), FColor(154, 86, 60), FColor(88, 124, 57),
        FColor(213, 216, 208), FColor(105, 75, 52), FColor(170, 85, 115),
        FColor(119, 87, 60), FColor(91, 65, 47), FColor(210, 206, 184),
        FColor(140, 176, 182), FColor(160, 119, 62), FColor(110, 68, 40), FColor(165, 107, 78), FColor(211, 204, 177)
    };
    const int32 MaterialIndex = static_cast<int32>(Tint);
    const int32 Key = MaterialIndex + static_cast<int32>(Mesh) * TintCount + (bSolid ? TintCount * static_cast<int32>(EShape::Count) : 0);
    UInstancedStaticMeshComponent* Batch = Batches.FindRef(Key);
    if (!Batch)
    {
        UStaticMesh* StaticMesh = Mesh == EShape::Box ? BoxMesh.Get()
            : Mesh == EShape::Cylinder ? CylinderMesh.Get()
            : Mesh == EShape::Plane ? PlaneMesh.Get() : SphereMesh.Get();
        if (!StaticMesh)
        {
            return;
        }

        Batch = NewObject<UInstancedStaticMeshComponent>(this);
        AddInstanceComponent(Batch);
        // Repeat room occupancy and surface variation at the same plot on every launch.
        Batch->InstancingRandomSeed = 18613 + Key * 31;
        Batch->SetupAttachment(GetRootComponent());
        Batch->SetStaticMesh(StaticMesh);
        Batch->SetMobility(EComponentMobility::Static);
        Batch->SetCollisionProfileName(bSolid ? FName(TEXT("BlockAll")) : FName(TEXT("NoCollision")));
        Batch->SetCanEverAffectNavigation(false);
        Batch->SetGenerateOverlapEvents(false);
        if (Tint == ETint::Water || Tint == ETint::Ripple || Tint == ETint::Paint || Tint == ETint::RoadPaint)
        {
            Batch->SetCastShadow(false);
        }
        if (UMaterialInterface* Surface = SurfaceMaterials.FindRef(MaterialIndex))
        {
            Batch->SetMaterial(0, Surface);
        }
        else if (BaseMaterial)
        {
            if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Batch))
            {
                Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colors[MaterialIndex]));
                Batch->SetMaterial(0, Material);
            }
        }
        Batch->RegisterComponent();
        Batches.Add(Key, Batch);
    }

    Batch->AddInstance(FTransform(Rot, Pos, Size / 100.0));
}

void ARiverfrontWorld::Box(ETint Tint, const FVector& Pos, const FVector& Size, bool bSolid, FRotator Rot)
{
    Shape(EShape::Box, Tint, Pos, Size, bSolid, Rot);
}

void ARiverfrontWorld::Beam(ETint Tint, const FVector& From, const FVector& To, double Width)
{
    const FVector Direction = To - From;
    Box(Tint, (From + To) * 0.5, FVector(Direction.Size(), Width, Width), false, Direction.Rotation());
}

void ARiverfrontWorld::BuildBanks()
{
    // River runs along X; park, lanes and the crossing share a zero-grade driving surface.
    Box(ETint::Water, FVector(0, ChannelCenter, -145), FVector(180000, ChannelWidth, 8));
    const bool bHasRiver = IsValid(SurfaceMaterials.FindRef(static_cast<int32>(ETint::Water)).Get());
    if (!bHasRiver)
    {
        FRandomStream WaterRandom(5191);
        for (int32 Index = 0; Index < 110; ++Index)
        {
            Box(Index % 4 == 0 ? ETint::Ripple : ETint::Water,
                FVector(WaterRandom.FRandRange(-9900, 9900), ChannelCenter + WaterRandom.FRandRange(-ChannelWidth * .49, ChannelWidth * .49), -140),
                FVector(WaterRandom.FRandRange(65, 660), WaterRandom.FRandRange(3, 9), 1), false,
                FRotator(0, WaterRandom.FRandRange(-7, 7), 0));
        }
    }

    for (const double Bank : {-1.0, 1.0})
    {
        // A low, walkable promenade wraps below the road-level esplanade.
        // Split solid retaining wall and the upper rail at the stairs.
        const double GapLeft = StairX - StairHalfWidth;
        const double GapRight = StairX + StairHalfWidth;
        const auto Span = [this, Bank](ETint Tint, double FromX, double ToX,
            double DistanceFromRiver, double Z, double Width, double Height, bool bSolid = false)
        {
            Box(Tint, FVector((FromX + ToX) * .5, BankY(Bank, DistanceFromRiver), Z),
                FVector(ToX - FromX, Width, Height), bSolid);
        };
        Span(ETint::Retaining, -10250, GapLeft, 2630, -98, 280, 195, true);
        Span(ETint::Retaining, GapRight, 10250, 2630, -98, 280, 195, true);
        Span(ETint::Retaining, -SegmentHalfLength, SegmentHalfLength, 2200, -135, 28, 92, true);
        Span(ETint::Paving, -SegmentHalfLength, SegmentHalfLength, 2350, -110, 310, 52, true);
        for (const double Z : {-49.0, 9.0})
        {
            Span(ETint::Rail, -SegmentHalfLength, SegmentHalfLength, 2205, Z, 9, 9, true);
        }
        for (double X = -7500; X <= 7550; X += 210)
        {
            Box(ETint::Rail, FVector(X, BankY(Bank, 2205), -33), FVector(9, 9, 102));
        }
        for (const double X : {BridgeX - BridgeSide - 26, BridgeX + BridgeSide + 26})
        {
            Box(ETint::Rail, FVector(X, BankY(Bank, 2345), -29), FVector(12, 285, 110), true);
        }
        for (int32 Step = 0; Step < 5; ++Step)
        {
            const double Surface = -14.0 - Step * 14.0;
            const double Height = Surface + 111.0;
            const double Y = 2606.0 - Step * 37.0;
            Box(ETint::Paving, FVector(StairX, BankY(Bank, Y), Surface - Height * .5),
                FVector(StairHalfWidth * 2.0, 38.0, Height), true);
        }
        for (const double X : {GapLeft + 7.0, GapRight - 7.0})
        {
            Beam(ETint::Rail, FVector(X, BankY(Bank, 2630), 96), FVector(X, BankY(Bank, 2429), 24), 8);
            Box(ETint::Rail, FVector(X, BankY(Bank, 2625), 44), FVector(9, 9, 110));
            Box(ETint::Rail, FVector(X, BankY(Bank, 2430), -23), FVector(9, 9, 100));
        }
        Box(ETint::Paving, FVector(0, BankY(Bank, 3100), -23), FVector(15500, 960, 50), true);
        Box(ETint::Asphalt, FVector(0, BankY(Bank, 4090), -28), FVector(15500, 1020, 56), true);
        Box(ETint::Paving, FVector(0, BankY(Bank, 5020), -24), FVector(15500, 840, 56), true);
        Box(ETint::Stone, FVector(0, BankY(Bank, 6540), -50), FVector(15500, 2240, 100), true);
        Box(ETint::Stone, FVector(0, BankY(Bank, 7640), 72), FVector(15640, 105, 144), true);

        // Coping, lower wall joints and edge drainage break up large planar surfaces.
        Span(ETint::PaleStone, -SegmentHalfLength, GapLeft, 2508, 1, 62, 17);
        Span(ETint::PaleStone, GapRight, SegmentHalfLength, 2508, 1, 62, 17);
        Box(ETint::Shadow, FVector(0, BankY(Bank, 3565), 2.3), FVector(15400, 9, 1));
        for (const double Z : {-124.0, -65.0})
        {
            Span(ETint::Shadow, -10250, GapLeft, 2488, Z, 3, 2);
            Span(ETint::Shadow, GapRight, 10250, 2488, Z, 3, 2);
        }
        for (double X = -7840; X <= 7840; X += 130)
        {
            if (FMath::Abs(X - BridgeX) > BridgeSide + 96)
            {
                Box(ETint::PaleStone, FVector(X, BankY(Bank, 3560), 3), FVector(128, 25, 10));
            }
            const double Offset = FMath::Fmod((X + 7840.0), 260.0) < 10.0 ? -28.0 : 24.0;
            if (X + Offset < GapLeft || X + Offset > GapRight)
            {
                Box(ETint::Shadow, FVector(X + Offset, BankY(Bank, 2488), -93), FVector(2, 4, 55));
            }
        }

        for (double X = -7500; X < 7600; X += 250)
        {
            if (FMath::Abs(X - BridgeX) < BridgeSide + 155)
            {
                continue;
            }
            for (const double Edge : {3593.0, 4587.0})
            {
                Box(ETint::Retaining, FVector(X, BankY(Bank, Edge), .1), FVector(248, 24, .5));
            }
        }
        // Worn thermoplastic at the west landing. The asphalt remains at driving grade.
        for (const double CrossingX : {AtalX, BridgeX - 915})
        {
            for (double Y = 3665; Y <= 4545; Y += 82)
            {
                Box(ETint::RoadPaint, FVector(CrossingX, BankY(Bank, Y), .55), FVector(285, 40, .8));
            }
        }
        for (double X = -6800; X <= 7100; X += 1197)
        {
            if (FMath::Abs(X - BridgeX) > BridgeSide + 105)
            {
                // Storm inlets are flush with the carriageway at the foot of the kerb.
                Box(ETint::Shadow, FVector(X, BankY(Bank, 3623), .25), FVector(58, 30, .48));
                for (int32 Slat = -3; Slat <= 3; ++Slat)
                {
                    Box(ETint::Rail, FVector(X + 8 * Slat, BankY(Bank, 3623), .52), FVector(3, 26, .78));
                }
            }
        }
        for (double X = -7350; X <= 7350; X += 275)
        {
            if (FMath::Abs(X - BridgeX) > 590 && FMath::Abs(X - AtalX) > 215
                && FMath::Abs(X - (BridgeX - 915)) > 215)
            {
                Box(ETint::RoadPaint, FVector(X, BankY(Bank, 4090), .48), FVector(130, 11, .7));
            }
            if (FMath::Abs(X - BridgeX) < BridgeSide + 125 || FMath::Abs(X - StairX) < StairHalfWidth ||
                FMath::Abs(X - AtalX) < AtalEndHalfWidth + 15)
            {
                continue;
            }
            Box(ETint::Rail, FVector(X, BankY(Bank, 2503), 54), FVector(10, 10, 108), true);
        }

        const double LeftEdge = BridgeX - BridgeSide;
        const double RightEdge = BridgeX + BridgeSide;
        for (const double Z : {35.0, 105.0})
        {
            Span(ETint::Rail, -SegmentHalfLength, AtalX - AtalEndHalfWidth, 2503, Z, 12, 10, true);
            Span(ETint::Rail, AtalX + AtalEndHalfWidth, GapLeft, 2503, Z, 12, 10, true);
            Span(ETint::Rail, GapRight, LeftEdge, 2503, Z, 12, 10, true);
            Span(ETint::Rail, RightEdge, SegmentHalfLength, 2503, Z, 12, 10, true);
        }


    }
}

void ARiverfrontWorld::BuildBridge()
{
    // Generic road crossing with solid bearings and regular piers. Not the Atal footbridge.
    const double DeckLength = 2.0 * BridgeHalfLength;
    Box(ETint::Retaining, FVector(BridgeX, ChannelCenter, -89), FVector(1060, DeckLength, 120));
    Box(ETint::Asphalt, FVector(BridgeX, ChannelCenter, -20), FVector(720, DeckLength, 40), true);
    for (const double Side : {-1.0, 1.0})
    {
        const double OuterX = BridgeX + Side * 520;
        const double KerbX = BridgeX + Side * 369;
        Box(ETint::Paving, FVector(BridgeX + Side * 435, ChannelCenter, -18),
            FVector(158, DeckLength, 50), true);
        Box(ETint::Retaining, FVector(KerbX, ChannelCenter, 4), FVector(18, DeckLength, 12));
        Box(ETint::Rail, FVector(OuterX, ChannelCenter, 105), FVector(11, DeckLength - 340, 11), true);
        Box(ETint::Retaining, FVector(OuterX, ChannelCenter, 21), FVector(26, DeckLength - 340, 48), true);
        const double RailEnd = BridgeHalfLength - 170.0;
        for (double Y = -RailEnd; Y <= RailEnd; Y += 162.0)
        {
            Box(ETint::Rail, FVector(OuterX, ChannelCenter + Y, 74), FVector(9, 9, 58), true);
        }
        for (double Y = -BridgeHalfLength + 780; Y < BridgeHalfLength - 780; Y += 3250.0)
        {
            const FVector Mast(OuterX, ChannelCenter + Y, 620);
            Beam(ETint::Rail, FVector(OuterX, Mast.Y, 38), Mast, 11);
            Beam(ETint::Rail, Mast, Mast + FVector(-Side * 131, 0, 23), 8);
            Box(ETint::Paint, Mast + FVector(-Side * 132, 0, 24), FVector(38, 21, 8));
        }
    }

    for (double Y = -10000.0; Y <= 10000.0; Y += 3300.0)
    {
        const double WorldY = ChannelCenter + Y;
        Box(ETint::Retaining, FVector(BridgeX, WorldY, -261), FVector(580, 193, 262));
        Box(ETint::Retaining, FVector(BridgeX, WorldY, -163), FVector(942, 224, 56));
        Box(ETint::Shadow, FVector(BridgeX, WorldY + 119, -149), FVector(570, 12, 10));
        Box(ETint::Shadow, FVector(BridgeX, WorldY, .32), FVector(722, 4, .45));
    }
    for (double Y = -BridgeHalfLength + 260; Y <= BridgeHalfLength - 260; Y += 272.0)
    {
        Box(ETint::RoadPaint, FVector(BridgeX, ChannelCenter + Y, 0.68), FVector(10, 139, 1.1));
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati riverfront: %0.1f m channel; %0.1f m road crossing."),
        ChannelWidth / 100.0, DeckLength / 100.0);
}

void ARiverfrontWorld::BuildFrontage()
{
    FRandomStream City(15111);
    for (const double Bank : {-1.0, 1.0})
    {
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const double X = -7110 + Index * 940 + City.FRandRange(-35, 35);
            const double Width = City.FRandRange(670, 830);
            const double Depth = City.FRandRange(640, 1020);
            const double FrontY = BankY(Bank, 5640 + (Index % 4) * 62 + City.FRandRange(0, 36));
            const int32 Style = (Index + (Bank < 0 ? 3 : 0)) % 5;
            const int32 Floors = City.RandRange(2, 4) + (Style == 3 ? 1 : 0);
            const ETint Wall = Style == 0 ? ETint::Terracotta
                : Style == 2 ? ETint::FacadeStone : ETint::Plaster;
            if (Bank < 0 || Index < 2 || Index > 4)
            {
                DetailFrontage(X, FrontY, Bank, Width, Depth, Floors, Wall, Style);
            }
        }

        for (int32 Index = 0; Index < 12; ++Index)
        {
            const double X = -8250 + Index * 1500 + City.FRandRange(-330, 330);
            const double Y = BankY(Bank, City.FRandRange(7850, 10300));
            const double Height = City.FRandRange(615, 2030);
            const double Width = City.FRandRange(470, 1080);
            const double Depth = City.FRandRange(440, 820);
            Box(Index % 3 == 0 ? ETint::FacadeStone : ETint::Plaster,
                FVector(X, Y, Height * .47), FVector(Width, Depth, Height));
            Box(ETint::Retaining, FVector(X, Y, Height * .97 + 16), FVector(Width + 7, Depth + 7, 32));
            Box(ETint::FacadeStone, FVector(X + Width * .2, Y + Bank * Depth * .15, Height * .97 + 61),
                FVector(Width * .35, Depth * .34, 123));
            const double Face = Y - Bank * Depth * .5;
            for (double Z = 235.; Z < Height * .94; Z += 293.)
            {
                const int32 Bays = Index % 3 + 3;
                for (int32 I = 0; I < Bays; ++I)
                {
                    const double BayWidth = Width * .85 / Bays;
                    const double WindowX = X + (I - (Bays - 1) * .5) * BayWidth;
                    Box(ETint::Glass, FVector(WindowX, Face - Bank * 4., Z), FVector(BayWidth * .69, 8., 131.));
                    Box(ETint::FacadeStone, FVector(WindowX, Face - Bank * 16., Z + 71.), FVector(BayWidth * .80, 33., 10.));
                    Box(ETint::Rail, FVector(WindowX, Face - Bank * 10., Z), FVector(4., 7., 131.));
                }
                if (Index % 3 == 0)
                {
                    Box(ETint::Retaining, FVector(X, Face - Bank * 35., Z - 65.), FVector(Width * .63, 77., 11.));
                    Box(ETint::Rail, FVector(X, Face - Bank * 72., Z - 2.), FVector(Width * .63, 5., 5.));
                    for (int32 Post = -2; Post <= 2; ++Post)
                        Box(ETint::Rail, FVector(X + Width * .147 * Post, Face - Bank * 72., Z - 32.), FVector(4., 4., 60.));
                }
            }
        }
    }
    // A connected forecourt, both side lanes, rear lane and public arcade opposite the start.
    BuildArcadeBlock(FVector(-4290, 6425, 0), 0, 2570, 1960, 1250);

}

void ARiverfrontWorld::DetailFrontage(double X, double FrontY, double Bank, double Width, double Depth,
    int32 Floors, ETint Wall, int32 Style)
{
    // Generic private facades inspired by Ahmedabad streets, not surveyed buildings or interiors.
    constexpr double FloorHeight = 302;
    const double Height = Floors * FloorHeight + 48;
    const auto Facing = [this, X, FrontY, Bank](ETint Tint, double Across, double Out, double Z,
        double W, double D, double H)
    {
        Box(Tint, FVector(X + Across, FrontY - Bank * Out, Z), FVector(W, D, H));
    };

    Box(Wall, FVector(X, FrontY + Bank * Depth * .5, Height * .5), FVector(Width, Depth, Height), true);
    Box(ETint::Retaining, FVector(X, FrontY + Bank * Depth * .5, Height + 1),
        FVector(Width + 19, Depth + 12, 20));
    Facing(ETint::Plaster, 0, 1, Height + 54, Width + 4, 20, 90);
    for (const double Side : {-1.0, 1.0})
    {
        Box(ETint::Plaster, FVector(X + Side * (Width * .5 - 8), FrontY + Bank * Depth * .5, Height + 54),
            FVector(16, Depth, 90));
    }
    DressFacade(X, FrontY, Bank, Width, Height, Style);
    const double RoofSide = Style % 2 == 0 ? 1.0 : -1.0;
    Box(Wall, FVector(X + RoofSide * Width * .23, FrontY + Bank * Depth * .65, Height + 83),
        FVector(Width * .27, Depth * .32, 165));
    Shape(EShape::Cylinder, ETint::Roof,
        FVector(X - RoofSide * Width * .24, FrontY + Bank * Depth * .49, Height + 83),
        FVector(111, 111, 149));

    const int32 Bays = Style == 3 ? 3 : 4;
    const double BayWidth = .86 * Width / Bays;
    Facing(ETint::Retaining, 0, 5, 28, Width, 15, 56);
    if (Style % 2 == 0)
    {
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const double Across = (Bay - (Bays - 1) * .5) * BayWidth;
            const bool Shutter = (Bay + Style) % 3 != 0;
            Facing(ETint::Shadow, Across, 2, 132, BayWidth * .89, 7, 211);
            Facing(Shutter ? ETint::Rail : ETint::Glass, Across, 8, 130, BayWidth * .80, 8, 197);
            if (Shutter)
            {
                for (int32 Slat = 1; Slat < 12; ++Slat)
                    Facing(ETint::Shadow, Across, 13, 35 + 16 * Slat, BayWidth * .78, 3, 1.3);
            }
            else
            {
                Facing(ETint::Rail, Across, 15, 130, 4, 8, 197);
            }
        }
        Facing(ETint::FacadeStone, 0, 49, 276, Width + 5, 107, 20);
    }
    else
    {
        Facing(ETint::Shadow, -Width * .18, 3, 132, 104, 9, 227);
        Facing(ETint::Rail, -Width * .18, 10, 123, 85, 8, 205);
        Facing(ETint::Glass, Width * .22, 5, 152, Width * .30, 12, 118);
        Facing(ETint::Retaining, -Width * .18, 47, 267, 150, 95, 18);
        Facing(ETint::Retaining, -Width * .18, 37, 11, 130, 74, 22);
    }

    for (int32 Floor = 1; Floor < Floors; ++Floor)
    {
        const double Z = FloorHeight * Floor;
        Facing(ETint::FacadeStone, 0, 7, Z, Width + 7, 20, 8);
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const double Across = (Bay - (Bays - 1) * .5) * BayWidth;
            const bool Balcony = (Style == 1 && (Bay == 1 || Bay == 2)) ||
                (Style == 2 && (Bay + Floor) % 2 == 0) || (Style == 3 && Bay != 1);
            const double WindowWidth = BayWidth * (Balcony ? .59 : .56);
            Facing(ETint::Shadow, Across, 2, Z + 153, WindowWidth + 12, 7, 153);
            Facing(ETint::Glass, Across, 8, Z + 153, WindowWidth, 7, 144);
            Facing(ETint::Rail, Across, 13, Z + 153, 3.5, 7, 144);
            Facing(ETint::Retaining, Across, 30, Z + 239, WindowWidth + 25, 60, 11);
            Facing(ETint::FacadeStone, Across, 15, Z + 77, WindowWidth + 16, 32, 10);
            if (Balcony)
            {
                const double Platform = WindowWidth + 40;
                Facing(ETint::Retaining, Across, 56, Z + 13, Platform, 115, 24);
                Facing(ETint::Rail, Across, 112, Z + 113, Platform, 6, 6);
                Facing(ETint::Rail, Across, 112, Z + 34, Platform, 5, 5);
                for (int32 Bar = -4; Bar <= 4; ++Bar)
                {
                    Facing(ETint::Rail, Across + Platform * Bar / 9, 112, Z + 73, 3, 4, 77);
                }
            }
            else if ((Floor + Bay + Style) % 6 == 0)
            {
                Facing(ETint::Plaster, Across + WindowWidth * .55, 17, Z + 49, 59, 34, 41);
                Facing(ETint::Shadow, Across + WindowWidth * .55, 35, Z + 49, 33, 3, 25);
            }
        }
    }
}

UStaticMesh* ARiverfrontWorld::StreetTreeMesh()
{
    static const FString TreePath(TEXT("/Game/Sabarmati/Riverfront/Foliage/SM_RiverfrontBroadleaf"));
    return FPackageName::DoesPackageExist(TreePath)
        ? LoadObject<UStaticMesh>(nullptr, *(TreePath + TEXT(".SM_RiverfrontBroadleaf"))) : nullptr;
}

int32 ARiverfrontWorld::PlaceStreetTree(UStaticMesh& Mesh, const FVector& Base, double Height,
    FRandomStream& AppearanceRandom)
{
    const bool Mature=FMath::Fmod(FMath::Abs(Base.X*.31+Base.Y*.17),10.)<6.5;
    if (Mature && !MatureTree)
        MatureTree=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Foliage/Jacaranda/SM_Jacaranda.SM_Jacaranda"));
    UStaticMesh* Use=Mature && MatureTree?MatureTree.Get():&Mesh;
    UStaticMeshComponent* Tree = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Tree);
    Tree->SetupAttachment(GetRootComponent());
    Tree->SetMobility(EComponentMobility::Static);
    Tree->SetStaticMesh(Use);
    if (Use==MatureTree && MatureTree)
    {
        const int32 Leaf=Tree->GetMaterialIndex(TEXT("jacaranda_tree_leaves"));
        if (Leaf!=INDEX_NONE)
        {
            if (!CanopyLeaves) CanopyLeaves=UMaterialInstanceDynamic::Create(Tree->GetMaterial(Leaf),this);
            if (CanopyLeaves) Tree->SetMaterial(Leaf,CanopyLeaves);
        }
    }
    Tree->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tree->SetGenerateOverlapEvents(false);
    Tree->SetCanEverAffectNavigation(false);
    const FBoxSphereBounds Bounds = Use->GetBounds();
    const double Scale = Height*(Use==MatureTree?1.55:1.) / FMath::Max(1.0, 2.0 * Bounds.BoxExtent.Z);
    const double Ground = Base.Z - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
    const FRotator Turn(0.0, AppearanceRandom.FRandRange(-180.f, 180.f), 0.0);
    Tree->SetRelativeTransform(FTransform(Turn, FVector(Base.X, Base.Y, Ground), FVector(Scale)));
    Tree->RegisterComponent();
    if (Use==MatureTree && MatureTree) return 0;
    if (IsValid(SurfaceMaterials.FindRef(static_cast<int32>(ETint::Leaf)).Get()))
    {
        // Masked scans in a bounded volume fill out branch sprays without billboard-facing motion.
        const FTransform TreeTransform(Turn, FVector(Base.X, Base.Y, Ground), FVector(Scale));
        for (int32 Card = 0; Card < 64; ++Card)
        {
            const FVector CrownCenter(16, 68, 312);
            const FVector Scatter = AppearanceRandom.VRand();
            const double Reach = AppearanceRandom.FRandRange(.45, 1.0);
            const FVector Offset = CrownCenter + Scatter * FVector(143, 159, 115) * Reach;
            const double Width = AppearanceRandom.FRandRange(51., 91.) * Scale;
            const double Length = Width * AppearanceRandom.FRandRange(1.24, 1.64);
            const FRotator Angle(AppearanceRandom.FRandRange(-69, 69),
                AppearanceRandom.FRandRange(-180, 180), AppearanceRandom.FRandRange(-65, 65));
            Shape(EShape::Plane, ETint::Leaf, TreeTransform.TransformPosition(Offset),
                FVector(Width, Length, 100), false, Angle);
        }
        return 64;
    }
    return 0;
}

void ARiverfrontWorld::BuildStreetFurniture()
{
    FRandomStream TreeRandom(107);
    FRandomStream AppearanceRandom(9214);
    UStaticMesh* DetailedTree = StreetTreeMesh();
    int32 DetailedTreeCount = 0;
    int32 LeafCardCount = 0;
    const auto ResidentBatch=[this](const TCHAR* Path)
    {
        auto* Model=LoadObject<UStaticMesh>(nullptr,Path);
        if (!Model) return static_cast<UInstancedStaticMeshComponent*>(nullptr);
        auto* Seats=NewObject<UInstancedStaticMeshComponent>(this);
        AddInstanceComponent(Seats); Seats->SetupAttachment(GetRootComponent());
        Seats->SetMobility(EComponentMobility::Static); Seats->SetStaticMesh(Model);
        Seats->SetCollisionEnabled(ECollisionEnabled::NoCollision); Seats->SetCanEverAffectNavigation(false);
        Seats->SetCullDistances(7600,14000);Seats->RegisterComponent();return Seats;
    };
    auto* Seated=ResidentBatch(TEXT("/Game/Sabarmati/Riverfront/People/SM_SeatedResident.SM_SeatedResident"));
    auto* Phone=ResidentBatch(TEXT("/Game/Sabarmati/Riverfront/People/SM_PhoneResident.SM_PhoneResident"));
    for (const double Bank : {-1.0, 1.0})
    {
        for (double X = -6900; X <= 6900; X += 850)
        {
            if (FMath::Abs(X - BridgeX) < 740)
            {
                continue;
            }
            const double TreeY = BankY(Bank, 3420);
            const double Height = TreeRandom.FRandRange(320, 465);
            // Flush tree pit with a stone lip rather than a floating concrete pedestal.
            Box(ETint::Bark, FVector(X, TreeY, 3.2), FVector(143, 154, 2.4));
            for (const double Side : {-1., 1.})
            {
                Box(ETint::Retaining, FVector(X + Side * 75, TreeY, 5), FVector(11, 168, 6));
                Box(ETint::Retaining, FVector(X, TreeY + Side * 78, 5), FVector(140, 11, 6));
            }
            // Keep only one street row per bank and let the renderer cull occluded meshes.
            if (DetailedTree)
            {
                LeafCardCount += PlaceStreetTree(*DetailedTree, FVector(X, TreeY, 4.4), Height * 1.32, AppearanceRandom);
                ++DetailedTreeCount;
            }
            else
            {
                Shape(EShape::Cylinder, ETint::Bark, FVector(X, TreeY, Height * .42),
                    FVector(35, 35, Height * .80), true);
                for (int32 Branch = 0; Branch < 10; ++Branch)
                {
                    const FVector Crown(X + TreeRandom.FRandRange(-115, 115),
                        TreeY + TreeRandom.FRandRange(-100, 100), Height + TreeRandom.FRandRange(-55, 90));
                    Shape(EShape::Sphere, Branch % 3 == 0 ? ETint::Foliage : ETint::FoliageDark,
                        Crown, FVector(TreeRandom.FRandRange(125, 235), TreeRandom.FRandRange(125, 225),
                            TreeRandom.FRandRange(105, 165)));
                    if (Branch % 2 == 0)
                    {
                        Beam(ETint::Bark, FVector(X, TreeY, Height * .58), Crown, 10);
                    }
                }
            }
            const double LampY = BankY(Bank, 4690);
            Box(ETint::Rail, FVector(X + 325, LampY, 196), FVector(12, 12, 385), true);
            Beam(ETint::Rail, FVector(X + 325, LampY, 384),
                FVector(X + 325, LampY - Bank * 75, 429), 10);
            Box(ETint::Paint, FVector(X + 325, LampY - Bank * 83, 429), FVector(36, 55, 15));
            AddPractical(FVector(X + 325, LampY - Bank * 83, 420), -90. * Bank);

            if (FMath::Fmod(FMath::Abs(X + 6900.0), 1700.0) < 1.0)
            {
                const double BenchY = BankY(Bank, 2815);
                const double BenchX = X + 245;
                for (int32 Slat = -2; Slat <= 2; ++Slat)
                {
                    Box(ETint::Bark, FVector(BenchX, BenchY + 9 * Slat, 46), FVector(177, 8.2, 5));
                }
                for (int32 Slat = 0; Slat < 3; ++Slat)
                {
                    Box(ETint::Bark, FVector(BenchX, BenchY - Bank * (20 + Slat * 2), 57 + Slat * 10.5),
                        FVector(177, 4.2, 9.3));
                }
                for (const double Offset : {-68.0, 68.0})
                {
                    Box(ETint::Rail, FVector(BenchX + Offset, BenchY, 23), FVector(5, 38, 43));
                    Beam(ETint::Rail, FVector(BenchX + Offset, BenchY - Bank * 17, 41),
                        FVector(BenchX + Offset, BenchY - Bank * 25, 82), 5);
                }
                const int32 BenchIndex=FMath::RoundToInt((X+6900.)/1700.)+(Bank>0?1:0);
                if (Seated && BenchIndex%3==1)
                {
                    const FRotator Facing(0,Bank>0?0.:180.,0);
                    Seated->AddInstance(FTransform(Facing,FVector(BenchX-28.,BenchY,-9.),FVector(.96)));
                    if (Phone && BenchIndex%2==1)
                    {
                        const FVector Standing(BenchX+104.,BenchY+Bank*51.,2.);
                        const FRotator Looking(0,Bank>0?137.:-32.,0);
                        Phone->AddInstance(FTransform(Looking,Standing,FVector(1.02)));
                    }
                }
                // Player contact matches the slat seat, not the floor void under each leg.
                Box(ETint::Bark, FVector(BenchX, BenchY, 43), FVector(175, 42, .4), true);
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("Sabarmati promenade: %d seated residents, %d phone users."),Seated?Seated->GetInstanceCount():0,Phone?Phone->GetInstanceCount():0);
    UE_LOG(LogTemp, Display, TEXT("Sabarmati riverfront: %d detailed trees; %d masked foliage cards."),
        DetailedTreeCount, LeafCardCount);
}
