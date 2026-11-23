#include "RiverfrontWorld.h"
#include "AhmedabadFacadeDetail.h"

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
enum EBatch : int32 { Dry, Road, Water, Markings, BridgeDetail, WallA, WallB, WallC, Roof, FacadeGlass, FacadeTrim, BatchCount };
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
}

struct FAhmedabadCityState
{
    TMap<FIntPoint, FCityTile> Tiles;
    TMap<FIntPoint, TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>>> Loaded;
    FIntPoint FocusTile = FIntPoint(MAX_int32, MAX_int32);
    FVector Car = FVector::ZeroVector;
    FVector Walk = FVector::ZeroVector;
    double Yaw = 0.0;
    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    uint32 Ways = 0;
    uint32 Strips = 0;

    uint32 BuildingCount = 0;

    bool ReadBuildings()
    {
        const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("CityData"),
            TEXT("AhmedabadBuildings.bin"));
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
        for (const FCityBuilding& Building : Validated)
        {
            Tiles.FindChecked(TileAt(FVector(Building.X, Building.Y, 0))).Buildings.Add(Building);
        }
        BuildingCount = Count;
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

    Parsed->ReadBuildings(); // Optional and all-or-nothing; road mode does not require it.
    LoadSurfaceMaterials();
    CityState = Parsed;
    UpdateCityTiles(Parsed->Car);
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
    BuildLighting();
    SetActorTickEnabled(true);
    OutWalkStart = Parsed->Walk;
    OutCarStart = Parsed->Car;
    OutYaw = Parsed->Yaw;
    UE_LOG(LogTemp, Display,
        TEXT("Ahmedabad City: %u OSM roads, %u strips, %u exterior shells, %.2f x %.2f km, %d total tiles, %d active; spawn probes=4/4."),
        Parsed->Ways, Parsed->Strips, Parsed->BuildingCount, (Parsed->MaxX - Parsed->MinX) / 100000.0,
        (Parsed->MaxY - Parsed->MinY) / 100000.0, Parsed->Tiles.Num(), Parsed->Loaded.Num());
    return true;
}

void ARiverfrontWorld::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (CityState.IsValid())
    {
        const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
        if (Controller && Controller->GetPawn())
        {
            UpdateCityTiles(Controller->GetPawn()->GetActorLocation());
        }
    }
}

void ARiverfrontWorld::UpdateCityTiles(const FVector& Focus)
{
    const TSharedPtr<FAhmedabadCityState> State = CityState;
    if (!State)
    {
        return;
    }
    const FIntPoint Center = TileAt(Focus);
    if (State->FocusTile == Center)
    {
        return;
    }
    State->FocusTile = Center;

    for (int32 DX = -StreamRadius; DX <= StreamRadius; ++DX)
    {
        for (int32 DY = -StreamRadius; DY <= StreamRadius; ++DY)
        {
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
            TArray<FTransform> Batch[BatchCount];
            Batch[Dry].Reserve(Tile->Ground.Num());
            Batch[Road].Reserve(Tile->Roads.Num());
            for (const FCityGround& Item : Tile->Ground)
            {
                // The actual shoreline clips the traversable land. Water is visual-only.
                const FVector Pos((Item.X0 + Item.X1) * .5, (Item.Y0 + Item.Y1) * .5, Item.Wet ? -35.0 : -54.0);
                AppendBox(Batch[Item.Wet ? Water : Dry], Pos,
                    FVector(Item.X1 - Item.X0, Item.Y1 - Item.Y0, Item.Wet ? 8.0 : 96.0));
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
                AppendBox(Batch[Road], Middle + FVector(0, 0, -12), FVector(Length + Join, Width, 24), Rotation);
                const FVector Forward = Vector / Length;
                if (Item.Bridge)
                {
                    AppendBox(Batch[BridgeDetail], Middle + FVector(0, 0, -58),
                        FVector(Length + Join, Width + 37, 85), Rotation);
                    const FVector Side = FVector(-Forward.Y, Forward.X, 0) * (Width * .5 + 5.0);
                    for (const double Edge : {-1.0, 1.0})
                    {
                        AppendBox(Batch[BridgeDetail], Middle + Edge * Side + FVector(0, 0, 21),
                            FVector(Length + Join, 23, 43), Rotation);
                    }
                }
                if (Item.Major && Length > 1200)
                {
                    for (double Along = 490; Along < Length - 400; Along += 620)
                    {
                        AppendBox(Batch[Markings], A + Forward * Along + FVector(0, 0, .6),
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
            const ETint Tints[BatchCount] = {ETint::Stone, ETint::Asphalt, ETint::Water, ETint::Paint,
                ETint::Retaining, ETint::Plaster, ETint::FacadeStone, ETint::Terracotta, ETint::Roof,
                ETint::Glass, ETint::PaleStone};
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
                Mesh->SetStaticMesh(BoxMesh);
                Mesh->SetMobility(EComponentMobility::Static);
                Mesh->SetCanEverAffectNavigation(false);
                Mesh->SetGenerateOverlapEvents(false);
                const bool bBuildingWall = Layer >= WallA && Layer <= WallC;
                Mesh->SetCollisionProfileName(Layer == Dry || Layer == Road || bBuildingWall
                    ? TEXT("BlockAll") : TEXT("NoCollision"));
                Mesh->SetCastShadow(Layer == BridgeDetail || bBuildingWall || Layer == Roof);
                if (UMaterialInterface* Material = SurfaceMaterials.FindRef(static_cast<int32>(Tints[Layer])))
                {
                    Mesh->SetMaterial(0, Material);
                }
                else if (BaseMaterial)
                {
                    if (auto* Default = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
                    {
                        const FColor Colors[BatchCount] = {FColor(155, 145, 127), FColor(63, 62, 62),
                            FColor(48, 93, 102), FColor(229, 222, 194), FColor(133, 126, 106),
                            FColor(202, 188, 169), FColor(166, 148, 124), FColor(174, 124, 105), FColor(93, 88, 81),
                            FColor(49, 66, 70), FColor(160, 144, 126)};
                        Default->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colors[Layer]));
                        Mesh->SetMaterial(0, Default);
                    }
                }
                Mesh->AddInstances(Batch[Layer], false, false, false);
                Mesh->RegisterComponent();
                Components.Add(Mesh);
            }
        }
    }
    for (auto It = State->Loaded.CreateIterator(); It; ++It)
    {
        const FIntPoint Key = It.Key();
        if (FMath::Abs(Key.X - Center.X) <= StreamRadius && FMath::Abs(Key.Y - Center.Y) <= StreamRadius)
        {
            continue;
        }
        for (const auto& Batch : It.Value())
        {
            if (Batch.IsValid())
            {
                Batch->DestroyComponent();
            }
        }
        It.RemoveCurrent();
    }
    UE_CLOG(State->Loaded.Num() > 0, LogTemp, Display,
        TEXT("Ahmedabad City: streamed %d/%d tiles at (%d,%d)."),
        State->Loaded.Num(), State->Tiles.Num(), Center.X, Center.Y);
}
