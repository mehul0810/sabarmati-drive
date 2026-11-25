#include "RiverfrontWorld.h"
#include "SabarmatiDistrict.h"
#include "DistrictLoadProgress.h"
#include "AhmedabadFacadeDetail.h"
#include "RiverfrontPedestrian.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"

namespace
{
constexpr double TileSize = 60000.0;
constexpr int32 StreamRadius = 2;
constexpr int32 MaxTileCount = 2300;
constexpr int32 MaxStripCount = 400000;
constexpr int32 MaxTileItems = 26000;
enum EBatch : int32 { Dry, Road, Water, Markings, BridgeDetail, WallA, WallB, WallC, Roof, FacadeGlass, FacadeTrim, Sidewalk, Kerb, Iron, StreetTree, Shade, Tank, TreePit, TrunkGuard, LampPole, LampHead, BatchCount };
const uint8 Signature[] = {'S', 'A', 'B', 'O', 'S', 'M', '2', '\0'};

struct FCityRoad
{
    int32 Ax = 0, Ay = 0, Bx = 0, By = 0;
    uint16 Width = 0;
    uint8 Bridge = 0;
    uint8 Major = 0;
};

struct FCityGround
{
    int32 X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;
    uint8 Wet = 0;
};

struct FCityBuilding
{
    int32 X = 0, Y = 0;
    uint16 Width = 0, Depth = 0, Height = 0;
    int16 Rotation = 0;
    uint8 Palette = 0;
};

struct FCityTile
{
    TArray<FCityRoad> Roads;
    TArray<FCityGround> Ground;
    TArray<FCityBuilding> Buildings;
    bool HasStreetPlots = false;
};

FString DataPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("CityData"), TEXT("AhmedabadRoads.bin"));
}

FIntPoint TileAt(const FVector& Location)
{
    return FIntPoint(FMath::FloorToInt(Location.X / TileSize), FMath::FloorToInt(Location.Y / TileSize));
}

void AppendBox(TArray<FTransform>& Batch, const FVector& Center, const FVector& Size,
    const FRotator& Rotation = FRotator::ZeroRotator)
{
    Batch.Emplace(Rotation, Center, Size / 100.0);
}

void AddFootpaths(TArray<FTransform> (&Batch)[BatchCount], const FCityRoad& RoadStrip,
    const TArray<FCityRoad>& Network, UStaticMesh* Canopy)
{
    const FVector A(RoadStrip.Ax, RoadStrip.Ay, 0), B(RoadStrip.Bx, RoadStrip.By, 0);
    const double Length = FVector::Distance(A, B);
    if (Length < 350)
    {
        return;
    }
    const FVector Along = (B - A) / Length;
    const FVector Across(-Along.Y, Along.X, 0);
    const FRotator Facing = Along.Rotation();
    TArray<const FCityRoad*, TInlineAllocator<32>> Neighbours;
    constexpr double SearchPad = 1600;
    for (const FCityRoad& Other : Network)
    {
        if (FMath::Max(Other.Ax, Other.Bx) < FMath::Min(A.X,B.X) - SearchPad
            || FMath::Min(Other.Ax, Other.Bx) > FMath::Max(A.X,B.X) + SearchPad
            || FMath::Max(Other.Ay, Other.By) < FMath::Min(A.Y,B.Y) - SearchPad
            || FMath::Min(Other.Ay, Other.By) > FMath::Max(A.Y,B.Y) + SearchPad)
        {
            continue;
        }
        const FVector C(Other.Ax, Other.Ay, 0), D(Other.Bx, Other.By, 0);
        // A collinear split at a tile edge is still the same stretch of road.
        const bool bSharedEnd = A == C || A == D || B == C || B == D;
        if (bSharedEnd && FMath::Abs(Along.Dot((D - C).GetSafeNormal())) > .99)
        {
            continue;
        }
        Neighbours.Add(&Other);
    }
    const int32 Count = FMath::CeilToInt(Length / 360.);
    const double Panel = Length / Count;
    for (const double Sign : {-1., 1.})
    {
        const FVector Out = Across * Sign;
        for (int32 I = 0; I < Count; ++I)
        {
            const FVector Edge = A + Along * ((I + .5) * Panel) + Out * (.5 * RoadStrip.Width);
            // Narrow the verge between divided carriageways. Omit it at actual junctions.
            for (const double Width : {Canopy?228.:156., 156., 26.})
            {
                const FVector Middle = Edge + Out * (Width * .5);
                bool bClear = true;
                for (const FCityRoad* Other : Neighbours)
                {
                    FVector OnPath, OnRoad;
                    FMath::SegmentDistToSegmentSafe(Middle - Along * (Panel * .5), Middle + Along * (Panel * .5),
                        FVector(Other->Ax, Other->Ay, 0), FVector(Other->Bx, Other->By, 0), OnPath, OnRoad);
                    if (FVector::DistSquared(OnPath, OnRoad) < FMath::Square((Width + Other->Width) * .5 + 14.))
                    {
                        bClear = false;
                        break;
                    }
                }
                if (!bClear)
                {
                    continue;
                }
                if (Width > 26.)
                {
                    AppendBox(Batch[Sidewalk], Middle + Out * 11 + FVector(0,0,1),
                        FVector(Panel, Width - 22, 18), Facing);
                }
                AppendBox(Batch[Kerb], Edge + Out * 11 + FVector(0,0,1.5),
                    FVector(Panel - 1, 22, 19), Facing);
                if (Canopy && Width>26.)
                {
                    // The street survey supplies the free verge, junctions exclude both lamps and pits.
                    const int32 Seed=HashCombine(GetTypeHash(RoadStrip.Ax/23),GetTypeHash(RoadStrip.Ay/37))&0x3fffffff;
                    const int32 Bay=I+(Seed%41)+(Sign>0?3:0);
                    const FVector Verge=Edge+Out*39.;
                    if (Bay%7==0)
                    {
                        const double H=570.+(Bay*29)%155;
                        const FBoxSphereBounds Bounds=Canopy->GetBounds();
                        const double Scale=H/FMath::Max(1.,Bounds.BoxExtent.Z*2.);
                        const FVector Base=Verge+FVector(0,0,10.-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale);
                        Batch[StreetTree].Emplace(FRotator(0.,(Bay*57)%360,0.),Base,FVector(Scale));
                        AppendBox(Batch[TreePit],Verge+FVector(0,0,10.1),FVector(117.,76.,3.),Facing);
                        // Physical trunk/boom collision ends at the first fork; the leaf cards stay nonblocking.
                        AppendBox(Batch[TrunkGuard],Verge+FVector(0,0,H*.24),FVector(62.,62.,H*.48),Facing);
                    }
                    else if (Bay%12==2)
                    {
                        AppendBox(Batch[LampPole],Verge+FVector(0,0,274.),FVector(15.,15.,530.),Facing);
                        const FVector Top=Verge-Out*76.+FVector(0,0,551.);
                        AppendBox(Batch[LampHead],Top+Out*37.,FVector(9.,89.,11.),Facing);
                        AppendBox(Batch[LampHead],Top-Out*13.,FVector(30.,46.,10.),Facing);
                        AppendBox(Batch[LampHead],Top-Out*14.-FVector(0,0,5.),FVector(24.,36.,3.),Facing);
                    }
                }
                if (Width > 26. && (I + Count) % 8 == 0)
                {
                    const FVector Drain = Edge - Out * 16;
                    AppendBox(Batch[Iron], Drain + FVector(0,0,.4), FVector(64,26,.6), Facing);
                    for (int32 Slat = -3; Slat <= 3; ++Slat)
                    {
                        AppendBox(Batch[Kerb], Drain + Along * (Slat * 8.) + FVector(0,0,.82),
                            FVector(3,22,.6), Facing);
                    }
                }
                break;
            }
        }
    }
}

}

struct FAhmedabadCityState
{
    TMap<FIntPoint, FCityTile> Tiles;
    TMap<FIntPoint, TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>>> Loaded;
    TMap<FIntPoint,TArray<TWeakObjectPtr<ARiverfrontPedestrian>>> Residents;
    FIntPoint FocusTile = FIntPoint(MAX_int32, MAX_int32);
    bool LoadedNeighbourhood = false;
    FVector Car = FVector::ZeroVector;
    FVector Walk = FVector::ZeroVector;
    double Yaw = 0.0;
    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    uint32 Ways = 0;
    uint32 Strips = 0;

    uint32 BuildingCount = 0;
    TOptional<FCityBuilding> ArcadePlot;
    TArray<FVector> BridgeEnds;
    double BridgeHeight(const FVector& Point) const
    {
        double Distance=6000.;
        for (const FVector& End:BridgeEnds) Distance=FMath::Min(Distance,FVector::Dist2D(Point,End));
        return FMath::Min(460.,Distance*.08);
    }
    TArray<FCityRoad> ApproachRoads;
    bool ReadBuildings(const TCHAR* Filename)
    {
        const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("CityData"),
            Filename);
        const int64 Size = IFileManager::Get().FileSize(*Path);
        TArray<uint8> Bytes;
        if (Size <= 12 || Size > 250000 || !FFileHelper::LoadFileToArray(Bytes, *Path))
        {
            return false;
        }
        constexpr uint8 BuildingSignature[] = {'S', 'A', 'B', 'B', 'L', 'D', '1', '\0'};
        FMemoryReader Reader(Bytes, true);
        uint8 Header[sizeof(BuildingSignature)]{};
        Reader.Serialize(Header, sizeof(Header));
        uint32 Count = 0;
        Reader << Count;
        if (FMemory::Memcmp(Header, BuildingSignature, sizeof(Header)) != 0 || Count > 5000
            || Size != 12 + 17 * static_cast<int64>(Count))
        {
            return false;
        }
        TArray<FCityBuilding> Validated;
        Validated.Reserve(Count);
        for (uint32 N = 0; N < Count; ++N)
        {
            FCityBuilding Building;
            Reader << Building.X << Building.Y << Building.Width << Building.Depth << Building.Height
                << Building.Rotation << Building.Palette;
            if (Building.Width < 125 || Building.Depth < 125 || Building.Height < 150 || Building.Height > 3200
                || Building.Palette > 2 || FMath::Abs(Building.Rotation) > 18000 || !Tiles.Contains(
                    TileAt(FVector(Building.X, Building.Y, 0))))
            {
                return false;
            }
            Validated.Add(Building);
        }
        if (Reader.IsError() || !Reader.AtEnd())
        {
            return false;
        }
        // Dress one large mapped plot within walking distance of the city start.
        int32 ArcadeIndex = INDEX_NONE;
        double BestDistance = FMath::Square(12500.);
        for (int32 I = 0; I < Validated.Num(); ++I)
        {
            const FCityBuilding& B = Validated[I];
            const double Distance = FVector::DistSquared2D(Walk, FVector(B.X, B.Y, 0));
            if (FMath::Max(B.Width, B.Depth) >= 2200 && FMath::Min(B.Width, B.Depth) >= 1500
                && B.Height >= 900 && Distance < BestDistance)
            {
                ArcadeIndex = I;
                BestDistance = Distance;
            }
        }
        for (int32 I = 0; I < Validated.Num(); ++I)
        {
            const FCityBuilding& Building = Validated[I];
            if (I == ArcadeIndex)
            {
                ArcadePlot = Building;
            }
            else
            {
                FCityTile& Tile=Tiles.FindChecked(TileAt(FVector(Building.X, Building.Y, 0)));
                Tile.Buildings.Add(Building);
            }
        }
        BuildingCount += Count;
        return true;
    }

    bool Read(const FString& Path)
    {
        const int64 Size = IFileManager::Get().FileSize(*Path);
        if (Size <= 60 || Size > 28 * 1024 * 1024)
        {
            return false;
        }
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Path))
        {
            return false;
        }
        FMemoryReader Reader(Bytes, true);
        uint8 Header[sizeof(Signature)]{};
        Reader.Serialize(Header, sizeof(Header));
        if (FMemory::Memcmp(Header, Signature, sizeof(Header)) != 0)
        {
            return false;
        }
        uint32 TileCm = 0, TileCount = 0;
        int32 CarX = 0, CarY = 0, WalkX = 0, WalkY = 0;
        float Angle = 0.f;
        Reader << TileCm << MinX << MinY << MaxX << MaxY << CarX << CarY << WalkX << WalkY
            << Angle << Ways << Strips << TileCount;
        if (TileCm != static_cast<uint32>(TileSize) || TileCount < 4 || TileCount > MaxTileCount
            || Ways < 1000 || Strips < Ways || Strips > MaxStripCount
            || MinX >= MaxX || MinY >= MaxY || !FMath::IsFinite(Angle) || MaxX - MinX > 8000000
            || MaxY - MinY > 8000000 || CarX <= MinX || CarX >= MaxX || CarY <= MinY || CarY >= MaxY)
        {
            return false;
        }
        Car = FVector(CarX, CarY, 68);
        Walk = FVector(WalkX, WalkY, 110);
        Yaw = Angle;
        Tiles.Reserve(TileCount);
        uint32 SeenRoads = 0;
        for (uint32 I = 0; I < TileCount; ++I)
        {
            FIntPoint Key;
            uint32 Roads = 0, Ground = 0;
            Reader << Key.X << Key.Y << Roads << Ground;
            if (Reader.IsError() || Roads > MaxTileItems || Ground > MaxTileItems || Ground == 0
                || Tiles.Contains(Key) || FMath::Abs(Key.X) > 100 || FMath::Abs(Key.Y) > 100
                || Reader.Tell() + static_cast<int64>(Roads) * 20 + static_cast<int64>(Ground) * 17 > Reader.TotalSize())
            {
                return false;
            }
            FCityTile& Tile = Tiles.Add(Key);
            Tile.Roads.Reserve(Roads);
            Tile.Ground.Reserve(Ground);
            for (uint32 N = 0; N < Roads; ++N)
            {
                FCityRoad Item;
                Reader << Item.Ax << Item.Ay << Item.Bx << Item.By << Item.Width << Item.Bridge << Item.Major;
                if (Item.Width < 210 || Item.Width > 2000 || Item.Bridge > 1 || Item.Major > 1
                    || Item.Ax < MinX || Item.Ax > MaxX || Item.Bx < MinX || Item.Bx > MaxX
                    || Item.Ay < MinY || Item.Ay > MaxY || Item.By < MinY || Item.By > MaxY
                    || FVector2D(Item.Bx - Item.Ax, Item.By - Item.Ay).SizeSquared() > FMath::Square(85000.0))
                {
                    return false;
                }
                Tile.Roads.Add(Item);
            }
            SeenRoads += Roads;
            for (uint32 N = 0; N < Ground; ++N)
            {
                FCityGround Item;
                Reader << Item.X0 << Item.Y0 << Item.X1 << Item.Y1 << Item.Wet;
                if (Item.Wet > 1 || Item.X1 <= Item.X0 || Item.Y1 <= Item.Y0
                    || Item.X0 < MinX || Item.X1 > MaxX || Item.Y0 < MinY || Item.Y1 > MaxY)
                {
                    return false;
                }
                Tile.Ground.Add(Item);
            }
        }
        return !Reader.IsError() && Reader.AtEnd() && SeenRoads == Strips
            && Tiles.Contains(TileAt(Car)) && Tiles.Contains(TileAt(Walk));
    }
};

bool ARiverfrontWorld::HasCityData()
{
    return IFileManager::Get().FileExists(*DataPath());
}

bool ARiverfrontWorld::BuildCity(FVector& OutWalkStart, FVector& OutCarStart, double& OutYaw)
{
    if (CityState.IsValid() || !Batches.IsEmpty())
    {
        return false;
    }
    const TSharedRef<FAhmedabadCityState> Parsed = MakeShared<FAhmedabadCityState>();
    if (!Parsed->Read(DataPath()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Ahmedabad City: offline OpenStreetMap data missing or invalid."));
        return false;
    }

    // Restrict extra kerbs and footpaths to the built-up approach. Keep junctions
    // open using the same neighbouring centreline data even across tile boundaries.
    DistrictLoadProgress::Reached(1);
    const FIntPoint StartTile = TileAt(Parsed->Walk);
    for (const auto& Pair : Parsed->Tiles)
    {
        if (FMath::Abs(Pair.Key.X - StartTile.X) > 2 || FMath::Abs(Pair.Key.Y - StartTile.Y) > 2)
        {
            continue;
        }
        for (const FCityRoad& R : Pair.Value.Roads)
        {
            if (FMath::PointDistToSegment(FVector(Parsed->Walk.X, Parsed->Walk.Y, 0),
                FVector(R.Ax,R.Ay,0), FVector(R.Bx,R.By,0)) < 65000)
            {
                Parsed->ApproachRoads.Add(R);
            }
        }
    }
    bStreetDistricts=Parsed->ReadBuildings(TEXT("AhmedabadBuildings.bin"));
    MatureTree=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Foliage/Jacaranda/SM_Jacaranda.SM_Jacaranda"));
    LoadSurfaceMaterials();
    // Keep the offline survey intact and stream just the current playable district.
    const FVector Centre=SabarmatiDistrict::Center(true);
    const double Half=SabarmatiDistrict::HalfWidth;
    Parsed->MinX=Centre.X-Half; Parsed->MaxX=Centre.X+Half;
    Parsed->MinY=Centre.Y-Half; Parsed->MaxY=Centre.Y+Half;
    Parsed->BuildingCount=Parsed->ArcadePlot.IsSet()?1:0;
    Parsed->Strips=0;
    for (auto It=Parsed->Tiles.CreateIterator(); It; ++It)
    {
        auto& Tile=It.Value();
        Tile.Ground.RemoveAll([&](FCityGround& G)
        {
            G.X0=FMath::Max(G.X0,Parsed->MinX); G.X1=FMath::Min(G.X1,Parsed->MaxX);
            G.Y0=FMath::Max(G.Y0,Parsed->MinY); G.Y1=FMath::Min(G.Y1,Parsed->MaxY);
            return G.X1<=G.X0 || G.Y1<=G.Y0;
        });
        if (Tile.Ground.IsEmpty()) { It.RemoveCurrent(); continue; }
        Tile.Roads.RemoveAll([](FCityRoad& R)
        {
            FVector A(R.Ax,R.Ay,0), B(R.Bx,R.By,0);
            if (!SabarmatiDistrict::Clip(A,B,true)) return true;
            R.Ax=FMath::RoundToInt(A.X); R.Ay=FMath::RoundToInt(A.Y);
            R.Bx=FMath::RoundToInt(B.X); R.By=FMath::RoundToInt(B.Y);
            return false;
        });
        Tile.Buildings.RemoveAll([](const FCityBuilding& B){return !SabarmatiDistrict::Contains(FVector(B.X,B.Y,0),true);});
        Tile.HasStreetPlots=true;
        Parsed->BuildingCount+=Tile.Buildings.Num();
        Parsed->Strips+=Tile.Roads.Num();
    }
    TSet<FIntPoint> StreetNodes;
    for (const auto& Pair:Parsed->Tiles)
        for (const FCityRoad& R:Pair.Value.Roads)
            if (!R.Bridge) {StreetNodes.Add({R.Ax,R.Ay}); StreetNodes.Add({R.Bx,R.By});}
    for (const auto& Pair:Parsed->Tiles)
        for (const FCityRoad& R:Pair.Value.Roads)
            if (R.Bridge)
                for (const FIntPoint P:{FIntPoint(R.Ax,R.Ay),FIntPoint(R.Bx,R.By)})
                    if (StreetNodes.Contains(P)) Parsed->BridgeEnds.AddUnique(FVector(P.X,P.Y,0));
    CityState = Parsed;
    UpdateCityTiles(Parsed->Car, true);
    const auto RoadTrace = [this](const FVector& Point)
    {
        FHitResult Hit;
        return GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0, 0, 150),
            Point - FVector(0, 0, 140), ECC_Visibility)
            && Hit.GetActor() == this && FMath::Abs(Hit.ImpactPoint.Z) < 1.9;
    };
    const FVector Heading = FRotator(0, Parsed->Yaw, 0).Vector();
    if (!RoadTrace(Parsed->Car) || !RoadTrace(Parsed->Walk)
        || !RoadTrace(Parsed->Car + Heading * 180) || !RoadTrace(Parsed->Car - Heading * 180))
    {
        UE_LOG(LogTemp, Warning, TEXT("Ahmedabad City: route spawn floor validation failed; keeping riverfront."));
        for (const auto& Tile : Parsed->Loaded)
        {
            for (const auto& Batch : Tile.Value)
            {
                if (Batch.IsValid())
                {
                    Batch->DestroyComponent();
                }
            }
        }
        CityState.Reset();
        return false;
    }
    TArray<FCityGround> Land;
    TArray<FCityGround> WaterCells;
    for (const auto& Pair:Parsed->Tiles)
        for (const FCityGround& G:Pair.Value.Ground)
            (G.Wet?WaterCells:Land).Add(G);
    double Best=DBL_MAX;
    FVector Access, Inland;
    for (const FCityGround& Wet:WaterCells)
    {
        AddRiverVolume(FVector((Wet.X0+Wet.X1)*.5,(Wet.Y0+Wet.Y1)*.5,0),Wet.X1-Wet.X0,Wet.Y1-Wet.Y0);
        for (const FCityGround& DryCell:Land)
        {
            const bool West=DryCell.X1==Wet.X0, East=DryCell.X0==Wet.X1;
            const bool South=DryCell.Y1==Wet.Y0, North=DryCell.Y0==Wet.Y1;
            FVector Point, Normal;
            if ((West || East) && FMath::Min(Wet.Y1,DryCell.Y1)-FMath::Max(Wet.Y0,DryCell.Y0)>300)
            {
                Point=FVector(West?Wet.X0:Wet.X1,(FMath::Min(Wet.Y1,DryCell.Y1)+FMath::Max(Wet.Y0,DryCell.Y0))*.5,0);
                Normal=FVector(West?-1:1,0,0);
            }
            else if ((South || North) && FMath::Min(Wet.X1,DryCell.X1)-FMath::Max(Wet.X0,DryCell.X0)>300)
            {
                Point=FVector((FMath::Min(Wet.X1,DryCell.X1)+FMath::Max(Wet.X0,DryCell.X0))*.5,South?Wet.Y0:Wet.Y1,0);
                Normal=FVector(0,South?-1:1,0);
            }
            else continue;
            const double D=FVector::DistSquared2D(Point,Parsed->Walk);
            if (D<Best) { Best=D; Access=Point; Inland=Normal; }
        }
    }
    if (Best<DBL_MAX) AddWaterAccess(Access,Inland,-6.);
    for (const FVector Street:{FVector(10237,25487,1.8),FVector(20998,25504,-4.38),FVector(28570,65572,140.47)})
    {
        const FRotator R(0,Street.Z,0);
        const FVector Origin(Street.X,Street.Y,0.);
        for (double Side:{-1.,1.})
        {
            const FVector P=Origin+R.RotateVector(FVector(0,Side*720.,0));
            FHitResult Floor;
            if (GetWorld()->LineTraceSingleByChannel(Floor,P+FVector(0,0,220),P-FVector(0,0,90),ECC_Visibility)
                && Floor.ImpactPoint.Z<15. && !GetWorld()->OverlapBlockingTestByChannel(Floor.ImpactPoint+FVector(0,0,114),
                    R.Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(103.,65.,101.))))
            { AddTeaCart(Floor.ImpactPoint,R.Yaw+(Side>0?180.:0.)); break; }
        }
    }
    BuildDistrictBoundary(true);
    DistrictLoadProgress::Reached(2);
    if (Parsed->ArcadePlot.IsSet())
    {
        const FCityBuilding& B = Parsed->ArcadePlot.GetValue();
        const FVector Plot(B.X, B.Y, -6);
        double Yaw = B.Rotation * .01 + (B.Depth > B.Width ? 90. : 0.);
        if (FRotator(0, Yaw, 0).UnrotateVector(Parsed->Walk - Plot).Y > 0)
        {
            Yaw += 180.;
        }
        BuildCityForecourt(Plot, Yaw, FMath::Max(B.Width, B.Depth), FMath::Min(B.Width, B.Depth));
        BuildArcadeBlock(Plot, Yaw, FMath::Max(B.Width, B.Depth), FMath::Min(B.Width, B.Depth), B.Height);
    }
    DistrictLoadProgress::Reached(3);
    BuildLighting();
    BuildWeather();
    SetActorTickEnabled(true);
    OutWalkStart = Parsed->Walk;
    OutCarStart = Parsed->Car;
    OutYaw = Parsed->Yaw;
    UE_LOG(LogTemp, Display,
        TEXT("Ahmedabad City: survey contains %u OSM roads; district has %u strips, %u exterior shells, %.2f x %.2f km, %d total tiles, %d active; spawn probes=4/4."),
        Parsed->Ways, Parsed->Strips, Parsed->BuildingCount, (Parsed->MaxX - Parsed->MinX) / 100000.0,
        (Parsed->MaxY - Parsed->MinY) / 100000.0, Parsed->Tiles.Num(), Parsed->Loaded.Num());
    return true;
}

void ARiverfrontWorld::BuildCityForecourt(const FVector& Plot, double Yaw, double Width, double Depth)
{
    const FRotator Facing(0, Yaw, 0);
    const FVector Along = Facing.Vector(), Out = Facing.RotateVector(FVector(0,-1,0));
    UStaticMesh* Tree = StreetTreeMesh();
    FRandomStream Planting(1905);
    const int32 Count = FMath::Max(1, FMath::RoundToInt(Width / 400.));
    const double Panel = Width / Count;
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector Start = Plot + Along * ((I + .5) * Panel - Width * .5) + Out * (Depth * .5);
        double ToKerb = 2300.;
        for (const FCityRoad& R : CityState->ApproachRoads)
        {
            const FVector A(R.Ax,R.Ay,Plot.Z), B(R.Bx,R.By,Plot.Z);
            const FVector Delta = FMath::ClosestPointOnSegment(Start,A,B) - Start;
            if (Delta.Dot(Out) <= 0 || FMath::Abs(Delta.Dot(Along)) > Panel * .55)
            {
                continue;
            }
            ToKerb = FMath::Min(ToKerb, Delta.Dot(Out) - R.Width * .5);
        }
        const double DepthToPave = ToKerb - 160.;
        if (ToKerb >= 2300. || DepthToPave < 120.)
        {
            continue;
        }
        const FVector End = Start + Out * DepthToPave;
        bool bClear = true;
        for (const FCityRoad& R : CityState->ApproachRoads)
        {
            FVector NearPlot, NearRoad;
            FMath::SegmentDistToSegmentSafe(Start, End, FVector(R.Ax,R.Ay,Plot.Z),
                FVector(R.Bx,R.By,Plot.Z), NearPlot, NearRoad);
            if (FVector::DistSquared(NearPlot, NearRoad) < FMath::Square(R.Width * .5 + Panel * .5 * FMath::Abs(Out.Dot(
                FVector(R.Bx - R.Ax, R.By - R.Ay, 0).GetSafeNormal())) + 4.))
            {
                bClear = false;
                break;
            }
        }
        if (!bClear)
        {
            continue;
        }
        const FVector Center = (Start + End) * .5;
        Box(ETint::Paving, Center + FVector(0,0,3), FVector(Panel, DepthToPave, 10), true, Facing);
        // Keep the sightline and walk from the kerb through the central gallery open.
        if (DepthToPave < 440. || FMath::Abs((I + .5) * Panel - Width * .5) < 390.)
        {
            continue;
        }
        const FVector Verge = End - Out * 128 + FVector(0,0,8);
        if (I % 4 == 1 && Tree)
        {
            Box(ETint::Bark, Verge, FVector(158,145,4), false, Facing);
            PlaceStreetTree(*Tree, Verge + FVector(0,0,2), Planting.FRandRange(460,630), Planting);
        }
        if (I % 5 == 0)
        {
            Shape(EShape::Cylinder, ETint::Rail, Verge + FVector(0,0,220), FVector(13,13,440), true);
            const FVector Top = Verge + Out * 77 + FVector(0,0,480);
            Beam(ETint::Rail, Verge + FVector(0,0,435), Top, 9);
            Box(ETint::Rail, Top, FVector(26,57,8), false, Facing);
        }
    }
}

void ARiverfrontWorld::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateWeather(DeltaSeconds);
    if (IsNight())
    {
        RefreshPracticalLights(false);
    }
    if (CityState.IsValid())
    {
        const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
        if (Controller && Controller->GetPawn())
        {
            UpdateCityTiles(Controller->GetPawn()->GetActorLocation());
        }
    }
}

void ARiverfrontWorld::UpdateCityTiles(const FVector& Focus, bool Immediate)
{
    const TSharedPtr<FAhmedabadCityState> State = CityState;
    if (!State)
    {
        return;
    }
    const FIntPoint Center = TileAt(Focus);
    if (State->FocusTile == Center && State->LoadedNeighbourhood)
    {
        return;
    }
    State->FocusTile = Center;

    for (auto It = State->Loaded.CreateIterator(); It; ++It)
    {
        const FIntPoint Key = It.Key();
        if (FMath::Abs(Key.X - Center.X) <= StreamRadius && FMath::Abs(Key.Y - Center.Y) <= StreamRadius)
        {
            continue;
        }
        TArray<TWeakObjectPtr<ARiverfrontPedestrian>> Leaving;
        if (State->Residents.RemoveAndCopyValue(Key,Leaving))
            for (const auto& Walker:Leaving) if (Walker.IsValid()) Walker->Destroy();
        for (const auto& Batch : It.Value())
        {
            if (Batch.IsValid())
            {
                Batch->DestroyComponent();
            }
        }
        It.RemoveCurrent();
    }
    int32 Created=0;
    bool Pending=false;
    // Always install the support beneath the pawn first. Build the surrounding rings across ticks.
    for (int32 Ring=0; Ring<=StreamRadius; ++Ring)
    for (int32 DX = -StreamRadius; DX <= StreamRadius; ++DX)
    {
        for (int32 DY = -StreamRadius; DY <= StreamRadius; ++DY)
        {
            if (FMath::Max(FMath::Abs(DX),FMath::Abs(DY))!=Ring) continue;
            const FIntPoint Key = Center + FIntPoint(DX, DY);
            if (State->Loaded.Contains(Key))
            {
                continue;
            }
            const FCityTile* Tile = State->Tiles.Find(Key);
            if (!Tile)
            {
                continue;
            }
            if (!Immediate && Created>=2) { Pending=true; continue; }
            ++Created;
            TArray<FCityRoad> StreetRoads;
            if (Tile->HasStreetPlots)
            {
                for (int32 NX=-1; NX<=1; ++NX)
                    for (int32 NY=-1; NY<=1; ++NY)
                        if (const FCityTile* Adjacent=State->Tiles.Find(Key+FIntPoint(NX,NY)))
                            StreetRoads.Append(Adjacent->Roads);
            }
            TArray<FTransform> Batch[BatchCount];
            Batch[Dry].Reserve(Tile->Ground.Num());
            Batch[Road].Reserve(Tile->Roads.Num());
            for (const FCityGround& Item : Tile->Ground)
            {
                // The mapped shoreline also defines the swimming volume and the bed below it.
                const FVector Pos((Item.X0 + Item.X1) * .5, (Item.Y0 + Item.Y1) * .5, Item.Wet ? SabarmatiDistrict::SurfaceZ-4. : (SabarmatiDistrict::BedZ-6.)*.5);
                AppendBox(Batch[Item.Wet ? Water : Dry], Pos,
                    FVector(Item.X1 - Item.X0, Item.Y1 - Item.Y0, Item.Wet ? 8.0 : -6.-SabarmatiDistrict::BedZ));
            }
            for (const FCityRoad& Item : Tile->Roads)
            {
                const FVector A(Item.Ax, Item.Ay, 0), B(Item.Bx, Item.By, 0);
                const FVector Vector = B - A;
                const double Length = Vector.Size();
                const double Width = Item.Width;
                if (Length < 1)
                {
                    continue;
                }
                const auto Rotation = Vector.Rotation();
                const double Join = FMath::Min(Width * .48, 570.0);
                const FVector Middle = (A + B) * .5;
                if (!Item.Bridge) AppendBox(Batch[Road], Middle + FVector(0, 0, -12), FVector(Length + Join, Width, 24), Rotation);
                const FVector Forward = Vector / Length;
                if (Item.Bridge)
                {
                    const int32 Count=FMath::Max(1,FMath::CeilToInt(Length/300.));
                    const FVector Side(-Forward.Y,Forward.X,0);
                    for (int32 I=0;I<Count;++I)
                    {
                        FVector Start=A+Vector*(double(I)/Count), End=A+Vector*(double(I+1)/Count);
                        Start.Z=State->BridgeHeight(Start); End.Z=State->BridgeHeight(End);
                        const FVector Centre=(Start+End)*.5;
                        const FRotator Slope=(End-Start).Rotation();
                        const double Span=(End-Start).Size()+1.;
                        AppendBox(Batch[Road],Centre+FVector(0,0,-12),FVector(Span,Width,24),Slope);
                        AppendBox(Batch[BridgeDetail],Centre+FVector(0,0,-47),FVector(Span,Width+32,46),Slope);
                        for (const double Edge:{-1.,1.})
                            AppendBox(Batch[BridgeDetail],Centre+Side*(Edge*(Width*.5+6.))+FVector(0,0,38),
                                FVector(Span,15.,76.),Slope);
                    }
                }
                if (Item.Major && !Item.Bridge && (Tile->HasStreetPlots || FVector::DistSquared2D(Middle, State->Walk) < FMath::Square(53000.)))
                {
                    AddFootpaths(Batch,Item,Tile->HasStreetPlots?StreetRoads:State->ApproachRoads,
                        MatureTree.Get());
                }
                if (Item.Major && Length > 1200)
                {
                    for (double Along = 490; Along < Length - 400; Along += 620)
                    {
                        AppendBox(Batch[Markings], A + Forward * Along + FVector(0, 0, .6 + (Item.Bridge?State->BridgeHeight(A+Forward*Along):0.)),
                            FVector(173, 9, .6), Rotation);
                    }
                }
            }
            for (const FCityBuilding& Building : Tile->Buildings)
            {
                const double Height = Building.Height;
                const FRotator Rotation(0, Building.Rotation * .01, 0);
                AppendBox(Batch[WallA + Building.Palette],
                    FVector(Building.X, Building.Y, Height * .5 - 6),
                    FVector(Building.Width, Building.Depth, Height), Rotation);
                AppendBox(Batch[Roof], FVector(Building.X, Building.Y, Height + 4),
                    FVector(Building.Width + 8.0, Building.Depth + 8.0, 19.0), Rotation);
                const FVector Plot(Building.X,Building.Y,0.);
                if (Tile->HasStreetPlots)
                {
                    const int32 Seed=GetTypeHash(Building.X/71+Building.Y/131)&0xff;
                    const auto Front=[&](int32 Layer,double X,double Y,double Z,double W,double D,double H)
                    { AppendBox(Batch[Layer],Plot+Rotation.RotateVector(FVector(X,Y,Z)),FVector(W,D,H),Rotation); };
                    if (Building.Width>710 && Building.Depth>570)
                    {
                        const double Side=Seed%2?1.:-1.;
                        const double Y=Side*(Building.Depth*.5+27.);
                        const int32 Bays=FMath::Clamp(int32(Building.Width/320),2,6);
                        const double Bay=Building.Width/double(Bays);
                        for (int32 J=0;J<Bays;++J)
                        {
                            const double X=(J+.5)*Bay-Building.Width*.5;
                            Front(Shade,X,Y,245.,Bay-17.,58.,12.);
                            Front(Shade,X,Y+Side*24.,235.,Bay-19.,7.,20.);
                            Front(Iron,X+Bay*.4,Y-Side*16.,238.,4.,46.,5.);
                            if ((J+Seed)%3==1 && Height>600.)
                            {
                                for (int32 L=0;L<6;++L)
                                    Front(Shade,X-Bay*.21+L*13.,Y,510.,5.,23.,138.);
                            }
                        }
                        if (Seed%3)
                        {
                            const double Diameter=FMath::Min(136.,Building.Depth*.18);
                            Front(Tank,Building.Width*.21,-Building.Depth*.15,Height+70.,Diameter,Diameter,123.);
                            Front(Tank,Building.Width*.21,-Building.Depth*.15,Height+134.,Diameter*.92,Diameter*.92,7.);
                        }
                    }
                }
                AhmedabadFacadeDetail::Add(FVector(Building.X, Building.Y, -6), Rotation,
                    Building.Width, Building.Depth, Height, Building.Palette,
                    [&Batch](const FVector& At, const FVector& Size, const FRotator& Facing)
                    {
                        AppendBox(Batch[FacadeGlass], At, Size, Facing);
                    },
                    [&Batch](const FVector& At, const FVector& Size, const FRotator& Facing)
                    {
                        AppendBox(Batch[FacadeTrim], At, Size, Facing);
                    });
            }
            const ETint Tints[BatchCount] = {ETint::Earth, ETint::Asphalt, ETint::Water, ETint::RoadPaint,
                ETint::Retaining, ETint::Plaster, ETint::FacadeStone, ETint::Terracotta, ETint::Roof,
                ETint::Glass, ETint::Plaster, ETint::Paving, ETint::Retaining, ETint::Rail, ETint::Leaf, ETint::Canvas, ETint::Rail, ETint::Timber, ETint::Rail, ETint::Rail, ETint::Rail};
            auto& Components = State->Loaded.Add(Key);
            for (int32 Layer = 0; Layer < BatchCount; ++Layer)
            {
                if (Batch[Layer].IsEmpty())
                {
                    continue;
                }
                UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(this);
                AddInstanceComponent(Mesh);
                Mesh->SetupAttachment(GetRootComponent());
                Mesh->SetStaticMesh(Layer==StreetTree?MatureTree.Get():(Layer==Tank || Layer==TrunkGuard)?CylinderMesh.Get():BoxMesh.Get());
                Mesh->SetMobility(EComponentMobility::Static);
                Mesh->SetCanEverAffectNavigation(false);
                Mesh->SetGenerateOverlapEvents(false);
                const bool bBuildingWall = Layer >= WallA && Layer <= WallC;
                Mesh->SetCollisionProfileName(Layer == Dry || Layer == BridgeDetail || Layer == Road || Layer == Sidewalk || Layer == Kerb || Layer == TrunkGuard || Layer == LampPole || bBuildingWall
                    ? TEXT("BlockAll") : TEXT("NoCollision"));
                Mesh->SetCastShadow(Layer==BridgeDetail || bBuildingWall || Layer==Roof || Layer==FacadeTrim || Layer==StreetTree
                    || Layer==Shade || Layer==Tank || Layer==Iron);
                if (Layer==LampPole) Mesh->ComponentTags.Add(TEXT("LampPole"));
                if (Layer==LampHead) Mesh->ComponentTags.Add(TEXT("LampHead"));
                if (Layer==StreetTree) Mesh->ComponentTags.Add(TEXT("TreeCanopy"));
                if (Layer==TrunkGuard) Mesh->ComponentTags.Add(TEXT("StreetTrunk"));
                if (bBuildingWall) Mesh->ComponentTags.Add(TEXT("BuildingShell"));
                if (Layer==Roof || Layer==Shade || Layer==Tank || Layer==FacadeTrim || Layer==FacadeGlass || Layer==Iron)
                    Mesh->ComponentTags.Add(TEXT("FacadePart"));
                if (Layer==StreetTree) Mesh->SetCullDistances(17000,33000);
                if (Layer==TrunkGuard) Mesh->SetHiddenInGame(true);
                if (Layer==Sidewalk) Mesh->ComponentTags.Add(TEXT("CityFootpath"));
                if (Layer==Road) Mesh->ComponentTags.Add(TEXT("CityRoad"));
                if (UMaterialInterface* Material = SurfaceMaterials.FindRef(static_cast<int32>(Tints[Layer])))
                {
                    if (Layer!=StreetTree) Mesh->SetMaterial(0,Material);
                }
                else if (BaseMaterial && Layer!=StreetTree)
                {
                    if (auto* Default = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
                    {
                        const FColor Colors[BatchCount] = {FColor(155, 145, 127), FColor(63, 62, 62),
                            FColor(48, 93, 102), FColor(229, 222, 194), FColor(133, 126, 106),
                            FColor(202, 188, 169), FColor(166, 148, 124), FColor(174, 124, 105), FColor(93, 88, 81),
                            FColor(49, 66, 70), FColor(160, 144, 126),
                            FColor(160,145,121),FColor(147,142,130),FColor(57,60,56),FColor(52,73,47),FColor(119,105,75),FColor(64,67,63),FColor(107,83,63),FColor(71,62,52),FColor(68,65,61),FColor(68,65,61)};
                        Default->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colors[Layer]));
                        Mesh->SetMaterial(0, Default);
                    }
                }
                if (Layer==FacadeGlass)
                {
                    Mesh->ComponentTags.Add(TEXT("AhmedabadGlazing"));
                    Mesh->InstancingRandomSeed=1+int32(GetTypeHash(Key)&0x3fffffff);
                    if (IsNight() && NightWindows) Mesh->SetMaterial(0,NightWindows);
                }
                Mesh->AddInstances(Batch[Layer], false, false, false);
                Mesh->RegisterComponent();
                Components.Add(Mesh);
            }
            if (Tile->HasStreetPlots)
            {
                auto& Walkers=State->Residents.Add(Key);
                int32 Tried=0;
                for (const FCityRoad& R:Tile->Roads)
                {
                    const FVector A(R.Ax,R.Ay,10.), B(R.Bx,R.By,10.);
                    const double Length=FVector::Dist2D(A,B);
                    if (!R.Major || R.Bridge || Length<2300.) continue;
                    const FVector Along=(B-A)/Length, Across(-Along.Y,Along.X,0.);
                    const int32 Variant=int32(GetTypeHash(Key)&0xff)+Tried;
                    const FVector Middle=(A+B)*.5+Across*((R.Width*.5+82.)*(Tried%2?-1.:1.));
                    const double Span=FMath::Min(790.,Length*.24);
                    if (auto* Walker=ARiverfrontPedestrian::SpawnStreetWalker(*GetWorld(),Middle-Along*Span,Middle+Along*Span,Variant))
                        Walkers.Add(Walker);
                    if (++Tried>=10 || Walkers.Num()>=3) break;
                }
            }
        }
    }
    State->LoadedNeighbourhood=!Pending;
    UE_CLOG(!Pending && State->Loaded.Num() > 0, LogTemp, Display,
        TEXT("Ahmedabad City: streamed %d/%d tiles at (%d,%d)."),
        State->Loaded.Num(), State->Tiles.Num(), Center.X, Center.Y);
}

void ARiverfrontWorld::VisitMapRoads(const FVector& Focus, double Radius, FMapRoadVisitor Visit) const
{
    if (!CityState)
    {
        VisitRiverfrontMapRoads([&](FVector A,FVector B,bool Major)
        {
            if (SabarmatiDistrict::Clip(A,B,false)) Visit(A,B,Major);
        });
        return;
    }
    const FVector Range(Radius, Radius, 0);
    const FVector Low = Focus - Range;
    const FVector High = Focus + Range;
    const FIntPoint First = TileAt(Low);
    const FIntPoint Last = TileAt(High);
    for (int32 Y = First.Y; Y <= Last.Y; ++Y)
    {
        for (int32 X = First.X; X <= Last.X; ++X)
        {
            const FCityTile* Tile = CityState->Tiles.Find(FIntPoint(X, Y));
            if (!Tile) continue;
            for (const FCityRoad& Road : Tile->Roads)
            {
                if (FMath::Max(Road.Ax, Road.Bx) < Low.X || FMath::Min(Road.Ax, Road.Bx) > High.X
                    || FMath::Max(Road.Ay, Road.By) < Low.Y || FMath::Min(Road.Ay, Road.By) > High.Y)
                    continue;
                Visit(FVector(Road.Ax, Road.Ay, 0), FVector(Road.Bx, Road.By, 0), Road.Major || Road.Bridge);
            }
        }
    }
}

bool ARiverfrontWorld::FindCityFootway(const FVector& Origin,double Yaw,FVector& Surface) const
{
    if (!CityState) return false;
    const FVector Along=FRotator(0,Yaw,0).Vector();
    const FVector Across(-Along.Y,Along.X,0.);
    // Project onto a real, tagged sidewalk. Check the walking direction so arrivals have room to move.
    auto Footpath=[&](const FVector& At,FVector& HitPoint)
    {
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByObjectType(Hit,At+FVector(0,0,200),At-FVector(0,0,190),FCollisionObjectQueryParams(ECC_WorldStatic))
            || !Hit.GetComponent() || !Hit.GetComponent()->ComponentHasTag(TEXT("CityFootpath"))) return false;
        HitPoint=Hit.ImpactPoint;return true;
    };
    const FCityTile* Tile=CityState->Tiles.Find(TileAt(Origin));
    if (!Tile) return false;
    double Best=900., HalfWidth=0.;
    for (const auto& RoadStrip:Tile->Roads)
    {
        const FVector A(RoadStrip.Ax,RoadStrip.Ay,0.), B(RoadStrip.Bx,RoadStrip.By,0.);
        if (!RoadStrip.Major || RoadStrip.Bridge || FMath::Abs((B-A).GetSafeNormal().Dot(Along))<.94) continue;
        const double D=FVector::Dist2D(Origin,FMath::ClosestPointOnSegment(FVector(Origin.X,Origin.Y,0),A,B));
        if (D<Best) { Best=D; HalfWidth=RoadStrip.Width*.5; }
    }
    if (HalfWidth<=0.) return false;
    const FCollisionShape Body=FCollisionShape::MakeCapsule(35.f,86.f);
    for (const double Shift:{0.,-180.,180.,-420.,420.,-700.,700.})
        for (const double Sign:{-1.,1.})
            for (const double Inset:{160.,125.,93.})
            {
                const FVector Target=Origin+Along*Shift+Across*(Sign*(HalfWidth+Inset));
                FVector First,Next;
                if (Footpath(Target,First) && Footpath(Target+Along*240.,Next) && Footpath(Target+Along*535.,Next)
                    && Footpath(Target+Along*850.,Next))
                {
                    FHitResult Block;
                    const FVector Height(0,0,94.);
                    if (!GetWorld()->SweepSingleByObjectType(Block,First+Height,Next+Height,FQuat::Identity,
                        FCollisionObjectQueryParams(ECC_WorldStatic),Body))
                    {Surface=First;return true;}
                }
            }
    return false;
}

bool ARiverfrontWorld::PrepareCityTravel(const FVector& Focus)
{
    if (!CityState || !SabarmatiDistrict::Contains(Focus,true) || !CityState->Tiles.Contains(TileAt(Focus))) return false;
    UpdateCityTiles(Focus);
    return true;
}

#if !UE_BUILD_SHIPPING
int32 ARiverfrontWorld::CityBuildingsNear(const FVector& At, double Radius) const
{
    int32 Nearby=0;
    if (!CityState) return Nearby;
    for (const auto& Pair:CityState->Loaded)
    {
        const FCityTile* Tile=CityState->Tiles.Find(Pair.Key);
        if (!Tile) continue;
        for (const auto& B:Tile->Buildings)
            if (FVector::DistSquared2D(At,FVector(B.X,B.Y,0))<=FMath::Square(Radius)) ++Nearby;
    }
    return Nearby;
}
#endif
