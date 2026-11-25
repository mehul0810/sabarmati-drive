#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverfrontWorld.generated.h"

class ARiverfrontPlayerController;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class USpotLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
enum class ERiverfrontTime : uint8 { Day, Golden, Night };
class UStaticMesh;
class UMaterialInterface;
class UInstancedStaticMeshComponent;
struct FAhmedabadCityState;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontWorld : public AActor
{
    GENERATED_BODY()

public:
    ARiverfrontWorld();
    void Build();
    void CycleTimeOfDay();
    void CycleWeather();
    void ToggleClock();
    void StartMorningClock();
    float GetWetness() const { return SurfaceWetness; }
    const TCHAR* WeatherName() const;
    const TCHAR* TimeName() const;
    bool IsNight() const { return TimeOfDay == ERiverfrontTime::Night; }
    bool BuildCity(FVector& OutWalkStart, FVector& OutCarStart, double& OutYaw);
    static bool HasCityData();
    const TArray<FVector>& GetShopVisit(bool Tea) const { return Tea?TeaVisit:PrintVisit; }
    const FVector& GetTeaCounter() const { return TeaCounter; }
    const FVector& GetPressCounter() const { return PressCounter; }
    bool PrepareCityTravel(const FVector& Focus);
    using FMapRoadVisitor = TFunctionRef<void(const FVector&, const FVector&, bool)>;
    void VisitMapRoads(const FVector& Focus, double Radius, FMapRoadVisitor Visit) const;
    bool FindCityFootway(const FVector& Origin, double Yaw, FVector& Surface) const;
    bool HasStreetDistricts() const { return bStreetDistricts; }
    bool IsCityMode() const { return CityState.IsValid(); }
    virtual void Tick(float DeltaSeconds) override;
#if !UE_BUILD_SHIPPING
    int32 CityBuildingsNear(const FVector& At, double Radius) const;
    const TArray<FVector>& GetShopPath() const { return ShopPath; }
    const TArray<FVector>& GetServiceLaneCircuit() const { return ServiceLaneCircuit; }
    void ReviewArcadeWalk(ARiverfrontPlayerController& Driver);
#endif

    static const FVector SpawnPoint;
    static constexpr double SpawnYaw = -162.0;
    static const FVector CarPosition;
    static constexpr double AtalX = -5350.0;
    static constexpr double AtalEndHalfWidth = 500.0;

private:
    friend struct FRiverfrontFirstMorningWorld;
    enum class EShape : uint8 { Box, Cylinder, Sphere, Plane, Count };
    enum class ETint : uint8
    {
        Asphalt, Stone, Sandstone, PaleStone, Clay, Roof, Shadow, Glass,
        Rail, Paving, Paint, Water, Ripple, Bark, Foliage, FoliageDark,
        Sail, Ad, Retaining, Plaster, FacadeStone, Terracotta, Leaf, BridgeSteel, DeckWood, Flower, WoodLight, WoodDark, RoadPaint, ClearGlass, Brass, Timber, Canvas, Ceramic, Earth, Count
    };

    UPROPERTY()
    TObjectPtr<UStaticMesh> BoxMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> SphereMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> PlaneMesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY()
    TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> Batches;
    UPROPERTY()
    TMap<int32, TObjectPtr<UMaterialInterface>> SurfaceMaterials;

    void Shape(EShape Mesh, ETint Tint, const FVector& Pos, const FVector& Size,
        bool bSolid = false, FRotator Rot = FRotator::ZeroRotator);
    void Box(ETint Tint, const FVector& Pos, const FVector& Size,
        bool bSolid = false, FRotator Rot = FRotator::ZeroRotator);
    void Beam(ETint Tint, const FVector& From, const FVector& To, double Width);
    TSharedPtr<FAhmedabadCityState> CityState;
    bool bStreetDistricts=false;
#if !UE_BUILD_SHIPPING
    TArray<FVector> ArcadeReviewRoute;
    TArray<FVector> ShopPath;
    TArray<FVector> ServiceLaneCircuit;
#endif
    void LoadSurfaceMaterials();
    void UpdateCityTiles(const FVector& Focus, bool Immediate=false);
    void BuildBanks();
    void BuildDistrictBoundary(bool City);
    void BuildBridge();
    void BuildAtalBridge();
    void BuildRiverfrontCorridor();
    static void VisitRiverfrontMapRoads(FMapRoadVisitor Visit);
    void BuildFrontage();
    void DressStreetSurface();
    void DressMarketUtilities();
    void DressServiceElevations(double X, double Front, double Bank, double Width, double Depth,
        double Height, int32 Style, bool bStreet);
    void DressFacade(double X, double Front, double Bank, double Width, double Height, int32 Style);
    void DetailFrontage(double X, double FrontY, double Bank, double Width, double Depth,
        int32 Floors, ETint Wall, int32 Style);
    void BuildArcadeBlock(const FVector& Plot, double Yaw, double PlotWidth, double PlotDepth, double Height);
    void FitShop(const FVector& Opening, const FRotator& Facing, double Width, double Depth, double Ceiling, bool IsTea);
    void BuildWestBankGarden();
    TArray<FVector> TeaVisit, PrintVisit;
    FVector PressCounter=FVector::ZeroVector;
    FVector TeaCounter = FVector::ZeroVector;
    static UStaticMesh* StreetTreeMesh();
    int32 PlaceStreetTree(UStaticMesh& Mesh, const FVector& Base, double Height, FRandomStream& AppearanceRandom);
    void BuildCityForecourt(const FVector& Plot, double Yaw, double Width, double Depth);
    void BuildStreetFurniture();
    void BuildLighting();
    void ApplyTimeOfDay(ERiverfrontTime Time);
    void AddPractical(const FVector& Lens, double Yaw);
    void RefreshPracticalLights(bool Force);
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> PhotoDome;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PhotoSky;
    void UpdatePhotoSky();
    void BuildWeather();
    void UpdateWeather(float Delta);
    void ApplyWeatherLight();
    int32 Weather=0, LastWeather=0;
    bool bClock=false;
    float ClockSeconds=0, SurfaceWetness=0, RainAmount=0;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RainStreaks;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RoadWeather;
    UPROPERTY() TObjectPtr<UStaticMesh> MatureTree;
    UPROPERTY() TObjectPtr<UMaterialInterface> StreetGround;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CanopyLeaves;
    UPROPERTY() TObjectPtr<UAudioComponent> RainAudio;
    UPROPERTY() TObjectPtr<UAudioComponent> WindAudio;
    TArray<FVector> RainOffsets;
    float DrySunIntensity=0, DryFogDensity=0;
    ERiverfrontTime TimeOfDay = ERiverfrontTime::Golden;
    FVector LastLightFocus = FVector::ZeroVector;
    TArray<FTransform> PracticalFixtures;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
    UPROPERTY() TObjectPtr<USkyLightComponent> Sky;
    UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Haze;
    UPROPERTY() TObjectPtr<UMaterialInterface> NightWindows;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> LensInstances;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LampMaterial;
    UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> PracticalLights;
};
