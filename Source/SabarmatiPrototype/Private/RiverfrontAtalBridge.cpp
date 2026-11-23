#include "RiverfrontWorld.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
// SRFDCL and Gujarat Tourism references: 300 m pedestrian bridge, 10-14 m wide.
// The surrounding prototype is not surveyed. This is original game geometry.
constexpr double MidY = -10960.0;
// The reference bridge is nominally 300 m. The two promenade abutments in this
// straightened district are 285 m apart. Every visible structural member ends there.
constexpr double HalfLength = 14250.0;
constexpr int32 BayCount = 44;
constexpr double DeckZ = 23.0;

struct FCanopySection
{
    TArray<FVector> Positions;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;

    void AddFace(const FVector& A, const FVector& B, const FVector& C)
    {
        const FVector Normal = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
        const int32 Start = Positions.Num();
        Positions.Append({A, B, C});
        Normals.Append({Normal, Normal, Normal});
        UVs.Append({FVector2D(0, 0), FVector2D(1, 0), FVector2D(.5, 1)});
        Indices.Append({Start, Start + 1, Start + 2});
    }

    void AddSail(const FVector& A, const FVector& B, const FVector& C)
    {
        // Split the membrane at a slightly raised center for a taut fabric silhouette.
        const FVector Peak = (A + B + C) / 3.0 + FVector(0, 0, 20);
        for (const bool bFront : {true, false})
        {
            AddFace(A, bFront ? B : Peak, bFront ? Peak : B);
            AddFace(B, bFront ? C : Peak, bFront ? Peak : C);
            AddFace(C, bFront ? A : Peak, bFront ? Peak : A);
        }
    }
};
}

void ARiverfrontWorld::BuildAtalBridge()
{
    Box(ETint::Retaining, FVector(AtalX, MidY, -47), FVector(1010, 2 * HalfLength, 72));
    Box(ETint::Paving, FVector(AtalX, MidY, DeckZ - 12), FVector(1000, 2 * HalfLength, 24), true);
    for (const double SupportY : {MidY - 5000, MidY + 5000})
    {
        Shape(EShape::Cylinder, ETint::Retaining, FVector(AtalX, SupportY, -128), FVector(500, 280, 200));
        Box(ETint::Retaining, FVector(AtalX, SupportY, -75), FVector(1020, 220, 55));
    }

    UProceduralMeshComponent* Fabric = NewObject<UProceduralMeshComponent>(this);
    AddInstanceComponent(Fabric);
    Fabric->SetupAttachment(GetRootComponent());
    Fabric->SetMobility(EComponentMobility::Static);
    Fabric->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Fabric->SetCanEverAffectNavigation(false);
    Fabric->RegisterComponent();
    TArray<FCanopySection, TInlineAllocator<4>> Sections;
    Sections.SetNum(4);
    const FColor FabricPalette[] = {FColor(215, 79, 46), FColor(236, 164, 52),
        FColor(227, 194, 79), FColor(81, 126, 151)};
    const auto Tube = [this](const FVector& From, const FVector& To, double Diameter)
    {
        const FVector Delta = To - From;
        Shape(EShape::Cylinder, ETint::BridgeSteel, (From + To) * .5,
            FVector(Diameter, Diameter, Delta.Size()), false, FRotationMatrix::MakeFromZ(Delta).Rotator());
    };
    const auto HalfWidthAt = [](double T)
    {
        return AtalEndHalfWidth + 200.0 * FMath::Square(FMath::Sin(PI * T));
    };
    const auto Frame = [HalfWidthAt](int32 Index, double Side, bool bCrown)
    {
        const double T = static_cast<double>(Index) / BayCount;
        const double Y = MidY + HalfLength * (2.0 * T - 1.0);
        const double Width = HalfWidthAt(T);
        const double Waves = FMath::Square(FMath::Sin(3.0 * PI * T));
        return FVector(AtalX + Side * (bCrown ? 115.0 : Width), Y,
            bCrown ? 505.0 + 210.0 * Waves : DeckZ + 55.0);
    };

    for (int32 Bay = 0; Bay < BayCount; ++Bay)
    {
        const double Side = Bay % 2 == 0 ? -1.0 : 1.0;
        const FVector Crest = Frame(Bay, Side, true);
        const FVector NextCrest = Frame(Bay + 1, -Side, true);
        const double Y = .5 * (Crest.Y + NextCrest.Y);
        Tube(Crest, NextCrest, 23.0);
        if (Bay % 3 == 1)
        {
            const FVector Housing = (Crest + NextCrest) * .5 - FVector(0,0,15);
            Box(ETint::Rail, Housing, FVector(41,25,9));
            AddPractical(Housing - FVector(0,0,6), Side * 20.);
        }
        for (const double RailSide : {-1.0, 1.0})
        {
            const FVector Outer = Frame(Bay, RailSide, false);
            const FVector NextOuter = Frame(Bay + 1, RailSide, false);
            Tube(Outer, NextOuter, 28.0);
            Tube(Bay % 2 == 0 ? Outer : NextOuter, Bay % 2 == 0 ? NextCrest : Crest, 21.0);
            Tube(RailSide == Side ? Crest : NextCrest, (Outer + NextOuter) * .5, 16.0);

            if (FMath::Abs(Y - MidY) < HalfLength)
            {
                // The widening deck shoulders lie beside the continuous central collision slab.
                const double Width = HalfWidthAt((Y - MidY + HalfLength) / (2.0 * HalfLength));
                const double Shoulder = Width - AtalEndHalfWidth;
                if (Shoulder > 1.0)
                {
                    Box(ETint::Paving, FVector(AtalX + RailSide * (AtalEndHalfWidth + Shoulder * .5 - .5), Y, DeckZ - 12),
                        FVector(Shoulder + 1.0, NextOuter.Y - Outer.Y + 1, 24), true);
                }
                const auto GuardAt = [RailSide](const FVector& Edge, double Height)
                {
                    return FVector(Edge.X - RailSide * 42.0, Edge.Y, DeckZ + Height);
                };
                for (const double Z : {37.0, 102.0})
                {
                    const FVector From = GuardAt(Outer, Z);
                    const FVector To = GuardAt(NextOuter, Z);
                    Box(ETint::Rail, .5 * (From + To), FVector((To - From).Size() + 1, 11, 16),
                        true, (To - From).Rotation());
                }
                Box(ETint::Rail, GuardAt((Outer + NextOuter) * .5, 58), FVector(7, 7, 116));
            }
        }

        if ((Bay + 1) % 5 != 0)
        {
            const FVector Edge = Frame(Bay, Side, false);
            const FVector NextEdge = Frame(Bay + 1, Side, false);
            const FVector Corner = FMath::Lerp((Edge + NextEdge) * .5, (Crest + NextCrest) * .5, .41);
            const FVector Inset = Corner + FVector(-Side * 10.0, 0, 50.0);
            Sections[(Bay / 5 + Bay % 3) % Sections.Num()].AddSail(
                FMath::Lerp(Crest, Inset, .06), FMath::Lerp(NextCrest, Inset, .06), Inset);
        }

        if (Bay >= 2 && Bay <= BayCount - 3)
        {
            // Model separate boards with staggered end joints so the timber reads from eye level.
            constexpr ETint BoardColors[] = {ETint::DeckWood, ETint::WoodLight, ETint::WoodDark};
            for (int32 Board = 0; Board < 13; ++Board)
            {
                const double BoardPitch = 2.0 * HalfLength / (BayCount * 13.0);
                const double BoardY = Y + (Board - 6) * BoardPitch;
                const double SeamX = AtalX + ((Board + Bay) % 3 - 1) * 93;
                const double LeftLength = SeamX - (AtalX - 301);
                const double RightLength = AtalX + 301 - SeamX;
                Box(BoardColors[(Board * 13 + Bay * 7) % 3],
                    FVector(SeamX - LeftLength * .5, BoardY, DeckZ + .5), FVector(LeftLength - 1, BoardPitch - 1.2, 1));
                Box(BoardColors[(Board * 7 + Bay * 13 + 1) % 3],
                    FVector(SeamX + RightLength * .5, BoardY, DeckZ + .5), FVector(RightLength - 1, BoardPitch - 1.2, 1));
            }
            if (Bay % 7 == 4)
            {
                const double BenchX = AtalX + Side * 361;
                Shape(EShape::Cylinder, ETint::Stone, FVector(BenchX, Y, DeckZ + 23), FVector(116, 116, 46), true);
                Shape(EShape::Cylinder, ETint::DeckWood, FVector(BenchX, Y, DeckZ + 47), FVector(133, 133, 7));
                Shape(EShape::Cylinder, ETint::Stone, FVector(BenchX, Y, DeckZ + 68), FVector(68, 68, 39));
                Shape(EShape::Sphere, ETint::FoliageDark, FVector(BenchX, Y, DeckZ + 99), FVector(66, 62, 41));
                Shape(EShape::Sphere, ETint::Foliage, FVector(BenchX - Side * 9, Y - 11, DeckZ + 116), FVector(46, 48, 35));
            }
        }
    }

    for (int32 Section = 0; Section < Sections.Num(); ++Section)
    {
        const FCanopySection& Mesh = Sections[Section];
        Fabric->CreateMeshSection_LinearColor(Section, Mesh.Positions, Mesh.Indices, Mesh.Normals,
            Mesh.UVs, {}, {}, false);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Fabric);
            Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(FabricPalette[Section]));
            Fabric->SetMaterial(Section, Material);
        }
    }
    for (const double Side : {-1.0, 1.0})
    {
        for (const double End : {-1.0, 1.0})
        {
            const double LandingY = MidY + End * HalfLength;
            // Two shallow risers meet each landing; pedestrian bollards keep cars on the road.
            Box(ETint::Paving, FVector(AtalX, LandingY + End * 52, 3.5), FVector(1000, 110, 17), true);
            Box(ETint::Paving, FVector(AtalX + Side * 394, LandingY + End * 128, 2.5), FVector(87, 156, 4));
            for (int32 Post = 0; Post < 4; ++Post)
            {
                const double X = AtalX + Side * (64.0 + Post * 126.0);
                Shape(EShape::Cylinder, ETint::Rail, FVector(X, LandingY + End * 122, 46), FVector(15, 15, 90), true);
                Shape(EShape::Cylinder, ETint::BridgeSteel, FVector(X, LandingY + End * 122, 72), FVector(16, 16, 6));
            }
        }
    }

    // Low, oval planters frame the west landing without blocking the main path.
    FRandomStream Planting(4879);
    for (const double Side : {-1.0, 1.0})
    {
        const double X = AtalX + Side * 715;
        const double Y = 2730.0;
        Shape(EShape::Cylinder, ETint::Stone, FVector(X, Y, 22), FVector(330, 240, 44), true);
        Shape(EShape::Cylinder, ETint::Bark, FVector(X, Y, 43), FVector(304, 214, 8));
        for (int32 Plant = 0; Plant < 81; ++Plant)
        {
            const double Radius = FMath::Sqrt(Planting.GetFraction()) * .88;
            const double Angle = Planting.FRandRange(0, 2 * PI);
            const FVector Root(X + FMath::Cos(Angle) * Radius * 146, Y + FMath::Sin(Angle) * Radius * 101,
                53 + Planting.FRandRange(2, 23));
            Shape(EShape::Plane, ETint::Leaf, Root, FVector(41, 54, 100), false,
                FRotator(Planting.FRandRange(-35, 35), Planting.FRandRange(-180, 180), Planting.FRandRange(-32, 32)));
            if (Plant % 3 == 0)
            {
                Shape(EShape::Sphere, Plant % 2 == 0 ? ETint::Sail : ETint::Flower,
                    Root + FVector(0, 0, 8), FVector(4, 4.5, 2.4));
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati: Atal pedestrian bridge built, %d canopy bays."), BayCount);
}
