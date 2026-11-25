#include "RiverfrontWorld.h"
#include "RiverfrontCrossings.h"

#include "Components/TextRenderComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
// North is +X; west is +Y. Cross-bank offsets and deck heights preserve the
// existing Atal slice. This is a straightened reference corridor, not a survey.
// Sardar and Ellis are roughly 590 m south and 640 m north of the Atal center.
constexpr double SouthEnd = -82000.0;
constexpr double NorthEnd = 84000.0;
constexpr double SardarX = -64400.0;
constexpr double EllisX = 58600.0;
constexpr double EllisRoadOffset = 710.0;
constexpr double CoreLower = 7600.0;
constexpr double CoreUpper = 7750.0;
constexpr double RiverCenter = -10960.0;
constexpr double RoadDeckLength = 31800.0;
constexpr double SardarOpening = 970.0;
constexpr double EllisOpening = 1190.0;
constexpr double PortalHalfWidth = 162.0;
constexpr double FlowerX = 19600.0;
constexpr double MarketX = 43400.0;
constexpr double BJParkX = 73200.0;
constexpr double EventX = -28000.0;

constexpr double BankPosition(double Bank, double Distance)
{
    return Bank * Distance + (Bank < 0.0 ? 2.0 * RiverCenter : 0.0);
}
}

void ARiverfrontWorld::VisitRiverfrontMapRoads(FMapRoadVisitor Visit)
{
    const double West = BankPosition(1., 4090.);
    const double East = BankPosition(-1., 4090.);
    for (const double Bank : {West, East})
        Visit(FVector(SouthEnd, Bank, 0), FVector(NorthEnd, Bank, 0), true);
    for (const double Crossing : {SardarX, RiverfrontCrossings::CentralRoadX,
        EllisX - EllisRoadOffset, EllisX + EllisRoadOffset})
        Visit(FVector(Crossing, West, 0), FVector(Crossing, East, 0), true);
}

void ARiverfrontWorld::BuildRiverfrontCorridor()
{
    const auto Stretch = [this](ETint Tint, double Bank, double Start, double End,
        double Distance, double Width, double Top, double Depth, bool bSolid)
    {
        if (End > Start)
        {
            Box(Tint, FVector((Start + End) * .5, BankPosition(Bank, Distance), Top - Depth * .5),
                FVector(End - Start, Width, Depth), bSolid);
        }
    };

    const auto Sign = [this](double X, double Bank, double Distance, const TCHAR* Name, double Width)
    {
        const double Y = BankPosition(Bank, Distance);
        Box(ETint::Rail, FVector(X, Y, 151), FVector(Width, 12, 76));
        for (const double Side : {-1.0, 1.0})
        {
            Box(ETint::Rail, FVector(X + Side * (Width * .5 - 12.0), Y, 68), FVector(8, 8, 138));
            const double Face = Side * Bank;
            UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
            AddInstanceComponent(Text);
            Text->SetupAttachment(GetRootComponent());
            Text->SetMobility(EComponentMobility::Static);
            Text->SetText(FText::FromString(Name));
            Text->SetHorizontalAlignment(EHTA_Center);
            Text->SetVerticalAlignment(EVRTA_TextCenter);
            Text->SetWorldSize(31.f);
            Text->SetTextRenderColor(FColor(255, 246, 221));
            Text->SetRelativeLocation(FVector(X, Y + Face * 7.0, 151));
            Text->SetRelativeRotation(FRotator(0, Face * 90.0, 0));
            Text->RegisterComponent();
        }
    };

    const bool bHasLeafMaterial = IsValid(SurfaceMaterials.FindRef(static_cast<int32>(ETint::Leaf)).Get());
    // All outer-bank and park trees share one spatially culled, three-LOD canopy batch.
    MatureTree=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Foliage/Jacaranda/SM_Jacaranda.SM_Jacaranda"));
    UHierarchicalInstancedStaticMeshComponent* Grove=nullptr;
    if (MatureTree)
    {
        Grove=NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        AddInstanceComponent(Grove); Grove->SetupAttachment(GetRootComponent());
        Grove->SetMobility(EComponentMobility::Static); Grove->SetStaticMesh(MatureTree);
        Grove->SetCollisionEnabled(ECollisionEnabled::NoCollision);Grove->SetCanEverAffectNavigation(false);
        Grove->SetCullDistances(42000,66000);
        const int32 Leaf=Grove->GetMaterialIndex(TEXT("jacaranda_tree_leaves"));
        if (Leaf!=INDEX_NONE)
        {
            CanopyLeaves=UMaterialInstanceDynamic::Create(Grove->GetMaterial(Leaf),this);
            Grove->SetMaterial(Leaf,CanopyLeaves);
        }
        Grove->bAutoRebuildTreeOnInstanceChanges=false;
        Grove->RegisterComponent();
    }
    const auto Tree = [this, bHasLeafMaterial,Grove](double X, double Y, double Size, FRandomStream& Random)
    {
        const double Height = (295.0 + Random.FRandRange(0, 130)) * Size;
        const FVector Base(X, Y, 4);
        if (Grove)
        {
            const FBoxSphereBounds Bounds=MatureTree->GetBounds();
            const double Scale=Height*1.85/FMath::Max(1.,Bounds.BoxExtent.Z*2.);
            const double Z=Base.Z-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale;
            Grove->AddInstance(FTransform(FRotator(0,Random.FRandRange(-180,180),0),FVector(X,Y,Z),FVector(Scale)));
            Shape(EShape::Cylinder,ETint::Bark,Base+FVector(0,0,Height*.28),FVector(22*Size,22*Size,Height*.56),true);
            return;
        }
        Shape(EShape::Cylinder, ETint::Bark, Base + FVector(0, 0, Height * .42),
            FVector(25 * Size, 26 * Size, Height * .86), true);
        const double Turn = Random.FRandRange(-PI, PI);
        for (int32 Bough = 0; Bough < 4; ++Bough)
        {
            const double Angle = Turn + (Bough * 0.5 * PI) + Random.FRandRange(-.42, .42);
            const double Reach = Random.FRandRange(77.0, 138.0) * Size;
            const FVector Fork = Base + FVector(Reach * FMath::Cos(Angle), Reach * FMath::Sin(Angle),
                Height * Random.FRandRange(.81, 1.12));
            Beam(ETint::Bark, Base + FVector(0, 0, Height * .55), Fork, 10.0 * Size);
            for (int32 Spray = 0; Spray < 6; ++Spray)
            {
                const FVector Tip = Fork + Random.VRand() * (Random.FRandRange(35, 105) * Size);
                if (Spray < 2)
                {
                    Beam(ETint::Bark, Fork, Tip, 4.6 * Size);
                }
                if (bHasLeafMaterial)
                {
                    // Use the same scanned leaf cutout as the core instead of solid crowns.
                    const double Width = Random.FRandRange(62, 109) * Size;
                    Shape(EShape::Plane, ETint::Leaf, Tip, FVector(Width, Width * 1.44, 100), false,
                        FRotator(Random.FRandRange(-64, 64), Random.FRandRange(-180, 180),
                            Random.FRandRange(-62, 62)));
                }
            }
        }
    };

    const auto BreakForRoad = [](double X, double Extra = 0.0)
    {
        return FMath::Abs(X - SardarX) < SardarOpening + Extra ||
            FMath::Abs(X - EllisX) < EllisOpening + Extra;
    };

    // Close-to-shore ground remains continuous through the core/extension seam.
    // Segmented barriers leave space for road crossings and pairs of stairs.
    const double StairCenters[] = {SardarX - 1510, SardarX + 1510, -44200, -20100,
        18000, 41600, EllisX - 1640, EllisX + 1640, 75400};
    FRandomStream Dress(210183);
    for (const double Bank : {-1.0, 1.0})
    {
        for (const bool bNorth : {false, true})
        {
            const double Start = bNorth ? CoreUpper : SouthEnd;
            const double End = bNorth ? NorthEnd : -CoreUpper;
            const double LowerStart = bNorth ? CoreLower : SouthEnd;
            const double LowerEnd = bNorth ? NorthEnd : -CoreLower;
            Stretch(ETint::Paving, Bank, Start, End, 3100, 960, 2, 50, true);
            Stretch(ETint::Asphalt, Bank, Start, End, 4090, 1020, 0, 56, true);
            Stretch(ETint::Paving, Bank, Start, End, 5020, 840, 4, 56, true);
            Stretch(ETint::Stone, Bank, Start, End, 10060, 9240, 0, 100, true);
            Stretch(ETint::Paving, Bank, LowerStart, LowerEnd, 2350, 310, -84, 52, true);
            Stretch(ETint::Retaining, Bank, LowerStart, LowerEnd, 2200, 28, -89, 92, true);
            for (const double Height : {-49.0, 9.0})
            {
                Stretch(ETint::Rail, Bank, LowerStart, LowerEnd, 2205, 9,
                    Height + 4.5, 9, true);
            }

            // Sorted intervals ensure that each stair/crossing opens both the rail and wall.
            struct FOpening { double X; double Half; };
            TArray<FOpening> Openings{{SardarX, SardarOpening}, {EllisX, EllisOpening}};
            for (const double Stair : StairCenters)
            {
                Openings.Add({Stair, PortalHalfWidth});
            }
            Openings.Sort([](const FOpening& A, const FOpening& B) { return A.X < B.X; });
            double Previous = LowerStart;
            double WallPrevious = bNorth ? 10250.0 : LowerStart;
            const double WallEnd = bNorth ? LowerEnd : -10250.0;
            const auto BankRails = [&](double From, double To)
            {
                for (const double Height : {35.0, 105.0})
                {
                    Stretch(ETint::Rail, Bank, From, To, 2503, 12, Height + 5, 10, true);
                }
                Stretch(ETint::PaleStone, Bank, From, To, 2508, 62, 9.5, 17, false);
            };
            for (const FOpening& Gap : Openings)
            {
                if (Gap.X > LowerStart && Gap.X < LowerEnd)
                {
                    BankRails(Previous, Gap.X - Gap.Half);
                    Stretch(ETint::Retaining, Bank, WallPrevious, FMath::Min(WallEnd, Gap.X - Gap.Half),
                        2630, 280, -.5, 195, true);
                    Previous = Gap.X + Gap.Half;
                    WallPrevious = FMath::Max(WallPrevious, Previous);
                }
            }
            BankRails(Previous, LowerEnd);
            Stretch(ETint::Retaining, Bank, WallPrevious, WallEnd, 2630, 280, -.5, 195, true);

            for (double X = Start + 155; X < End; X += 440)
            {
                Box(ETint::Rail, FVector(X, BankPosition(Bank, 2205), -33), FVector(9, 9, 102));
                bool bAtStair = false;
                for (const double Stair : StairCenters)
                {
                    bAtStair |= FMath::Abs(X - Stair) <= PortalHalfWidth + 15;
                }
                if (!BreakForRoad(X, 120))
                {
                    Box(ETint::RoadPaint, FVector(X, BankPosition(Bank, 4090), .55), FVector(182, 11, 1));
                    if (!bAtStair)
                    {
                        Box(ETint::Rail, FVector(X, BankPosition(Bank, 2503), 54), FVector(9, 9, 108), true);
                    }
                }
            }
            for (double X = Start + 440; X < End - 320; X += 1960)
            {
                if (BreakForRoad(X, 220))
                {
                    continue;
                }
                const double LampY = BankPosition(Bank, 3500);
                Box(ETint::Rail, FVector(X, LampY, 228), FVector(12, 12, 452), true);
                Beam(ETint::Rail, FVector(X, LampY, 451),
                    FVector(X, LampY + Bank * 82, 482), 9);
                Box(ETint::Paint, FVector(X, LampY + Bank * 84, 481), FVector(26, 45, 7));
                AddPractical(FVector(X, LampY + Bank * 84, 476), 90. * Bank);
            }
            for (double X = Start + 800; X < End - 450; X += 2380)
            {
                if (BreakForRoad(X, 930))
                {
                    continue;
                }
                const double Y = BankPosition(Bank, 5690 + Dress.FRandRange(-25, 165));
                Tree(X, Y, Dress.FRandRange(.83, 1.2), Dress);
            }
            for (double X=Start+1410.; X<End-900.; X+=1870.)
            {
                if (!BreakForRoad(X,690.))
                    Tree(X,BankPosition(Bank,3423.),Dress.FRandRange(.78f,1.12f),Dress);
            }
            for (double X = Start + 900; X < End - 600; X += 2600)
            {
                const bool bParkOrMarket = (Bank > 0.0 && (FMath::Abs(X - FlowerX) < 4900 ||
                    FMath::Abs(X - BJParkX) < 4700 || FMath::Abs(X - EventX) < 7100)) ||
                    (Bank < 0.0 && FMath::Abs(X - MarketX) < 5100);
                if (bParkOrMarket || BreakForRoad(X, 2200))
                {
                    continue;
                }
                // Closer shop and apartment fronts leave a continuous public sidewalk and an accessible rear lane.
                const int32 Parcel=FMath::FloorToInt((X-SouthEnd)/2600.);
                if (Parcel%2==0 && FMath::Abs(X+14100.)>2000.)
                {
                    const int32 Style=(Parcel+(Bank<0?1:0))%5;
                    const double Front=BankPosition(Bank,6300.+Style*67.);
                    const double FrontWidth = 1150. + (Parcel % 4) * 90.;
                    constexpr double FrontDepth = 1070.;
                    DetailFrontage(X, Front, Bank, FrontWidth, FrontDepth, 2 + Parcel % 3,
                        Style == 0 ? ETint::Terracotta : Style == 2 ? ETint::FacadeStone : ETint::Plaster, Style);
#if !UE_BUILD_SHIPPING
                    if (Bank > 0. && bNorth && ServiceLaneCircuit.IsEmpty())
                    {
                        const double Left = X - FrontWidth * .5 - 130., Right = X + FrontWidth * .5 + 130.;
                        const double Entry = Front - 145., Rear = Front + FrontDepth + 145.;
                        ServiceLaneCircuit = {FVector(Left, Entry, 99.), FVector(Right, Entry, 99.),
                            FVector(Right, Rear, 99.), FVector(Left, Rear, 99.), FVector(Left, Entry, 99.)};
                    }
#endif
                }
                const double Width = Dress.FRandRange(1180, 1820);
                const double Depth = Dress.FRandRange(1090, 1730);
                const double Height = Dress.FRandRange(680, 2050);
                const double Y = BankPosition(Bank, Dress.FRandRange(9400, 12000));
                const ETint Wall = Dress.RandRange(0, 3) == 0 ? ETint::FacadeStone : ETint::Plaster;
                Box(Wall, FVector(X, Y, Height * .5), FVector(Width, Depth, Height), true);
                DressServiceElevations(X, Y - Bank * Depth * .5, Bank, Width, Depth, Height, Parcel % 5, false);
                Box(ETint::Retaining, FVector(X, Y, Height + 22), FVector(Width + 18, Depth + 18, 44));
                for (double Floor = 235; Floor < Height - 85; Floor += 282)
                {
                    const double FrontY = Y - Bank * (Depth * .5 + 2);
                    Box(ETint::Shadow, FVector(X, FrontY, Floor + 22), FVector(Width * .83, 7, 97));
                    Box(ETint::PaleStone, FVector(X, FrontY - Bank * 32, Floor - 36),
                        FVector(Width * .86, 74, 19));
                    for (const double Unit : {-.33, -.11, .11, .33})
                    {
                        Box(ETint::Glass, FVector(X + Unit * Width, FrontY - Bank * 5, Floor + 23),
                            FVector(Width * .166, 4, 74));
                    }
                }
            }

            // Perimeter is explicit, leaving the full two-bank road and park routes inside it.
            const double Limit = bNorth ? End : Start;
            Box(ETint::Retaining, FVector(Limit, BankPosition(Bank, 8500), 47),
                FVector(68, 12700, 122), true);
            Stretch(ETint::Retaining, Bank, Start, End, 14690, 60, 112, 126, true);
        }

        for (const double Stair : StairCenters)
        {
            for (int32 Step = 0; Step < 5; ++Step)
            {
                const double Top = -14.0 * (1 + Step);
                const double Depth = Top + 111;
                Box(ETint::Paving, FVector(Stair, BankPosition(Bank, 2606 - Step * 37), Top - Depth * .5),
                    FVector(2 * PortalHalfWidth, 38, Depth), true);
            }
            for (const double Side : {-1.0, 1.0})
            {
                const double X = Stair + Side * (PortalHalfWidth - 7.0);
                Beam(ETint::Rail, FVector(X, BankPosition(Bank, 2630), 96),
                    FVector(X, BankPosition(Bank, 2429), 24), 9);
                Box(ETint::Rail, FVector(X, BankPosition(Bank, 2625), 44), FVector(9, 9, 110));
                Box(ETint::Rail, FVector(X, BankPosition(Bank, 2430), -23), FVector(9, 9, 100));
            }
        }

        // The heritage deck itself is visual-only; the side spans are the crossings.
        Box(ETint::Rail, FVector(EllisX, BankPosition(Bank, 2650), 50),
            FVector(649, 16, 100), true);
        for (const double Side : {-1.0, 1.0})
        {
            Box(ETint::Retaining, FVector(EllisX + Side * 1107, BankPosition(Bank, 2650), 53),
                FVector(149, 90, 109), true);
        }
        Sign(-4250, Bank, 3420, TEXT("ATAL BRIDGE  /  RIVERFRONT"), 540);
        Sign(SardarX + 1450, Bank, 3505, TEXT("SARDAR BRIDGE"), 350);
        Sign(EllisX - 1550, Bank, 3505, TEXT("ELLIS BRIDGE"), 320);
    }

    // The upper road climbs above the original lower promenade on both banks.
    // Each strip has a matching solid top. Paint, parapets and the adjacent walk
    // follow the same profile as the car to avoid floating corners at the joins.
    const auto Strip = [this](ETint Tint, double X, double Y0, double Y1,
        double Width, double Thickness, double Offset, bool Solid = false)
    {
        const double Z0 = RiverfrontCrossings::DeckZ(Y0) + Offset;
        const double Z1 = RiverfrontCrossings::DeckZ(Y1) + Offset;
        const FVector Along(0, Y1 - Y0, Z1 - Z0);
        const FVector Centre(X, (Y0 + Y1) * .5, (Z0 + Z1) * .5 - Thickness * .5);
        Box(Tint, Centre, FVector(Along.Size() + .08, Width, Thickness), Solid, Along.Rotation());
    };
    const auto SpanStrip = [&](ETint Tint, double X, double Width, double Depth,
        double Offset, double SpanHalfLength, bool Solid)
    {
        double Prior = RiverCenter - SpanHalfLength;
        for (double Station : {-14760., -13480., 13480., 14760.})
        {
            const double Y = RiverCenter + Station;
            if (Y > Prior && Y < RiverCenter + SpanHalfLength)
            {
                Strip(Tint, X, Prior, Y, Width, Depth, Offset, Solid);
                Prior = Y;
            }
        }
        Strip(Tint, X, Prior, RiverCenter + SpanHalfLength, Width, Depth, Offset, Solid);
    };
    const auto RoadSpan = [&](double X, double Width, bool bWide)
    {
        SpanStrip(ETint::Retaining, X, Width + 42, 30, -3, RoadDeckLength * .5, false);
        SpanStrip(ETint::Asphalt, X, Width, 24, 0, RoadDeckLength * .5, true);
        constexpr double BarrierLength = 26870.0;
        for (const double Edge : {-1.0, 1.0})
        {
            const double GuardX = X + Edge * (Width * .5 + 20);
            SpanStrip(ETint::PaleStone, GuardX, 27, 45, 42.5, BarrierLength * .5, true);
            SpanStrip(ETint::Rail, GuardX, 12, 14, 98, BarrierLength * .5, true);
            for (double Y = -BarrierLength * .5 + 160; Y < BarrierLength * .5; Y += 250)
            {
                Box(ETint::Rail, FVector(GuardX, RiverCenter + Y, 64 + RiverfrontCrossings::DeckZ(RiverCenter + Y)), FVector(9, 9, 53));
            }
        }
        if (bWide)
        {
            for (double Y = -14600; Y <= 14600; Y += 370)
            {
                for (const double Offset : {-375.0, 0.0, 375.0})
                {
                    Strip(ETint::RoadPaint, X + Offset, RiverCenter + Y - 83, RiverCenter + Y + 83, 12, .7, .65);
                }
            }
        }
        else
        {
            for (double Y = -14600; Y <= 14600; Y += 370)
            {
                Strip(ETint::RoadPaint, X, RiverCenter + Y - 79, RiverCenter + Y + 79, 11, .7, .65);
            }
        }
        for (const double PierY : {-9900.0, -6600.0, -3300.0, 0.0, 3300.0, 6600.0, 9900.0})
        {
            Box(ETint::Retaining, FVector(X, RiverCenter + PierY, -115),
                FVector(Width * .81, 177, 486));
        }
    };
    RoadSpan(SardarX, 1520, true);
    RoadSpan(EllisX - EllisRoadOffset, 640, false);
    RoadSpan(EllisX + EllisRoadOffset, 640, false);

    for (const double Side : {-1.0, 1.0})
    {
        const double X = SardarX + Side * 846.0;
        SpanStrip(ETint::Paving, X, 140, 27, 6, RoadDeckLength * .5, true);
        SpanStrip(ETint::Rail, X + Side * 64, 11, 13, 116, 13620, true);
        for (double Y = -13000; Y < 13000; Y += 1990)
        {
            const FVector Mast(X + Side * 54, RiverCenter + Y, 479 + RiverfrontCrossings::DeckZ(RiverCenter + Y));
            Beam(ETint::Rail, Mast - FVector(0, 0, 445), Mast, 13);
            Beam(ETint::Rail, Mast, Mast - FVector(Side * 96, 0, 0), 9);
        }

        const double TrussX = EllisX + Side * 307.0;
        const double StartY = RiverCenter - 13150;
        constexpr double Step = 657.5;
        const FVector Front(TrussX, StartY, 189);
        const FVector Back(TrussX, StartY + 40 * Step, 189);
        Beam(ETint::Rail, Front, Back, 18);
        Beam(ETint::Rail, Front + FVector(0, 0, 485), Back + FVector(0, 0, 485), 18);
        for (int32 Panel = 0; Panel <= 40; ++Panel)
        {
            const FVector Bottom = Front + FVector(0, Panel * Step, 0);
            const FVector Top = Bottom + FVector(0, 0, 485);
            Beam(ETint::Rail, Bottom, Top, 11);
            Beam(ETint::Rail, Top, Top - FVector(Side * 614, 0, 0), 12);
            if (Panel < 40)
            {
                const FVector Next = Bottom + FVector(0, Step, 0);
                if (Panel % 2 == 0)
                {
                    Beam(ETint::Rail, Bottom, Next + FVector(0, 0, 485), 13);
                }
                else
                {
                    Beam(ETint::Rail, Top, Next, 13);
                }
            }
        }
    }
    SpanStrip(ETint::Retaining, EllisX, 645, 29, -1, 13595, false);
    SpanStrip(ETint::DeckWood, EllisX, 590, 7, 7, 13350, false);

    // Flower Park: distinct planting rooms, soft beds and pedestrian aisles.
    Box(ETint::Paving, FVector(FlowerX, 8520, .5), FVector(7850, 5800, 3));
    Sign(FlowerX-850., 1.0, 5475, TEXT("RIVERFRONT FLOWER PARK"), 540);
    for (const double Side:{-1.,1.})
    {
        // Stone piers, a shaded timber screen and a clear five-metre public garden entrance.
        for (int32 I=0;I<6;++I)
        {
            const double PX=FlowerX+Side*(295.+I*455.);
            Box(ETint::FacadeStone,FVector(PX,5555.,105.),FVector(52.,53.,210.),true);
            Box(ETint::Retaining,FVector(PX,5555.,216.),FVector(60.,60.,13.));
            if (I==5) continue;
            Box(ETint::Retaining,FVector(PX+Side*228.,5555.,23.),FVector(416.,32.,46.),true);
            for (int32 Slat=0;Slat<12;++Slat)
                Box(ETint::Timber,FVector(PX+Side*(41.+Slat*33.),5555.,103.),FVector(8.,17.,118.));
            Box(ETint::Timber,FVector(PX+Side*228.,5555.,151.),FVector(416.,15.,7.));
        }
    }
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        for (int32 Row = 0; Row < 3; ++Row)
        {
            const double X = FlowerX + Side * (880 + Row * 960);
            for (const double Y : {7160.0, 8770.0, 10380.0})
            {
                Shape(EShape::Cylinder, ETint::PaleStone, FVector(X, Y, 17), FVector(677, 1120, 34), true);
                Shape(EShape::Cylinder, ETint::Bark, FVector(X, Y, 36), FVector(633, 1060, 10));
                for (int32 Flower = 0; Flower < 28; ++Flower)
                {
                    const double Angle = Dress.FRandRange(-PI, PI);
                    const double Radius = FMath::Sqrt(Dress.FRand()) * .93;
                    const FVector Plant(X + 305 * Radius * FMath::Cos(Angle),
                        Y + 510 * Radius * FMath::Sin(Angle), Dress.FRandRange(47, 74));
                    Shape(EShape::Plane, ETint::Leaf, Plant, FVector(46, 66, 100), false,
                        FRotator(24, Dress.FRandRange(-180, 180), Dress.FRandRange(-25, 25)));
                    Shape(EShape::Sphere, Row % 2 == 0 ? ETint::Flower : ETint::Sail,
                        Plant + FVector(0, 0, 9), FVector(12, 13, 7));
                }
            }
        }
    }

    // Ravivari: building-free shaded rows and paved market aisles off the east road.
    const double MarketY = BankPosition(-1.0, 8375.0);
    Box(ETint::Paving, FVector(MarketX, MarketY, .5), FVector(9200, 5900, 3));
    Sign(MarketX, -1.0, 5520, TEXT("RAVIVARI  /  RIVERFRONT MARKET"), 690);
    for (int32 Row = 0; Row < 3; ++Row)
    {
        const double Y = MarketY + (Row - 1) * 1030;
        for (int32 Stall = 0; Stall < 10; ++Stall)
        {
            const double X = MarketX + (Stall - 4.5) * 710.0 + (Stall >= 5 ? 360.0 : -360.0);
            const ETint Awning = Row == 0 ? ETint::Terracotta : Row == 1 ? ETint::Ad : ETint::Sail;
            for (const double Side : {-1.0, 1.0})
            {
                Box(ETint::Rail, FVector(X + Side * 142, Y + 90, 117), FVector(8, 8, 234));
                Box(ETint::Rail, FVector(X + Side * 142, Y - 90, 107), FVector(8, 8, 214));
            }
            Box(Awning, FVector(X, Y, 222), FVector(322, 243, 6), false, FRotator(0, 0, 5));
            for (int32 Stripe = -2; Stripe <= 2; ++Stripe)
            {
                Box(ETint::PaleStone, FVector(X + Stripe * 62, Y - 118, 215),
                    FVector(29, 7, 18));
            }
            Box(ETint::DeckWood, FVector(X, Y, 42), FVector(265, 75, 80), true);
            for (int32 Crate = 0; Crate < 3; ++Crate)
            {
                Box(Crate % 2 == 0 ? ETint::WoodLight : ETint::Terracotta,
                    FVector(X - 80 + Crate * 65, Y, 94), FVector(55, 53, 27));
            }
        }
    }

    // Two open bays at the original, unbranded service stop. A driver can pull in from the bank road.
    Box(ETint::Asphalt,FVector(-14100,5360,.3),FVector(1900,1530,2),true);
    for (double X:{-14700.,-13500.})
    {
        Box(ETint::Ceramic,FVector(X,5670,12),FVector(108,250,23),true);
        Box(ETint::Terracotta,FVector(X,5670,128),FVector(62,94,212),true);
        Box(ETint::Shadow,FVector(X,5621,144),FVector(47,4,32));
        Box(ETint::Brass,FVector(X,5616,144),FVector(30,3,8));
        Beam(ETint::Rail,FVector(X+36,5650,135),FVector(X+46,5656,63),5);
        Beam(ETint::Rail,FVector(X+46,5656,63),FVector(X+34,5675,104),5);
        Box(ETint::Rail,FVector(X,5750,214),FVector(21,21,428),true);
        AddPractical(FVector(X,5500,418),-90.);
    }
    Box(ETint::Plaster,FVector(-14100,5500,442),FVector(1730,1140,37));
    Box(ETint::Terracotta,FVector(-14100,4927,438),FVector(1730,10,26));
    Sign(-14100,1.,6180,TEXT("RIVER MOTOR WORKS"),250);
    // West-bank event lawn and B.J. Park are wider open areas, not street canyons.
    const auto Green = [&](double X, double Y, double Length, double Width)
    {
        Box(ETint::Foliage, FVector(X, Y, 1), FVector(Length, Width, 3));
        Box(ETint::Paving, FVector(X, Y, 3), FVector(Length, 290, 4));
        Box(ETint::Paving, FVector(X, Y, 3), FVector(330, Width, 4));
        for (const double Side : {-1.0, 1.0})
        {
            for (double Offset = -Length * .43; Offset < Length * .45; Offset += 1150.0)
            {
                if (FMath::Abs(Offset) > 410.0)
                {
                    Tree(X + Offset, Y + Side * (Width * .31 + Dress.FRandRange(-130, 130)),
                        Dress.FRandRange(.9, 1.45), Dress);
                }
            }
        }
    };
    Green(EventX, 9120, 10940, 6000);
    Sign(EventX, 1.0, 5510, TEXT("RIVERFRONT EVENT GROUNDS"), 590);
    Green(BJParkX, 8950, 7650, 5940);
    Sign(BJParkX, 1.0, 5520, TEXT("B.J. PARK"), 295);

    if (Grove)
    {
        Grove->bAutoRebuildTreeOnInstanceChanges=true;
        Grove->BuildTreeIfOutdated(false,true);
        UE_LOG(LogTemp,Display,TEXT("Sabarmati corridor: %d spatially culled scanned trees."),Grove->GetInstanceCount());
    }
    UE_LOG(LogTemp, Display,
        TEXT("Sabarmati corridor: 1660 m on both banks; Sardar, Atal, Ellis, Flower Park, Ravivari, Event Grounds and B.J. Park."));
}
