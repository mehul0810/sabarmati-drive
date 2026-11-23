#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverfrontWorld.generated.h"

class ARiverfrontPlayerController;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class USpotLightComponent;
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
    const TCHAR* TimeName() const;
    bool IsNight() const { return TimeOfDay == ERiverfrontTime::Night; }
    bool BuildCity(FVector& OutWalkStart, FVector& OutCarStart, double& OutYaw);
    static bool HasCityData();
    bool IsCityMode() const { return CityState.IsValid(); }
    virtual void Tick(float DeltaSeconds) override;
#if !UE_BUILD_SHIPPING
    void ReviewArcadeWalk(ARiverfrontPlayerController& Driver);
#endif

    static const FVector SpawnPoint;
    static constexpr double SpawnYaw = -162.0;
    static const FVector CarPosition;
    static constexpr double AtalX = -5350.0;
    static constexpr double AtalEndHalfWidth = 500.0;

private:
    enum class EShape : uint8 { Box, Cylinder, Sphere, Plane, Count };
    enum class ETint : uint8
    {
        Asphalt, Stone, Sandstone, PaleStone, Clay, Roof, Shadow, Glass,
        Rail, Paving, Paint, Water, Ripple, Bark, Foliage, FoliageDark,
        Sail, Ad, Retaining, Plaster, FacadeStone, Terracotta, Leaf, BridgeSteel, DeckWood, Flower, WoodLight, WoodDark, RoadPaint, Count
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
#if !UE_BUILD_SHIPPING
    TArray<FVector> ArcadeReviewRoute;
#endif
    void LoadSurfaceMaterials();
    void UpdateCityTiles(const FVector& Focus);
    void BuildBanks();
    void BuildBridge();
    void BuildAtalBridge();
    void BuildRiverfrontCorridor();
    void BuildFrontage();
    void DetailFrontage(double X, double FrontY, double Bank, double Width, double Depth,
        int32 Floors, ETint Wall, int32 Style);
    void BuildArcadeBlock(const FVector& Plot, double Yaw, double PlotWidth, double PlotDepth, double Height);
    static UStaticMesh* StreetTreeMesh();
    int32 PlaceStreetTree(UStaticMesh& Mesh, const FVector& Base, double Height, FRandomStream& AppearanceRandom);
    void BuildCityForecourt(const FVector& Plot, double Yaw, double Width, double Depth);
    void BuildStreetFurniture();
    void BuildLighting();
    void ApplyTimeOfDay(ERiverfrontTime Time);
    void AddPractical(const FVector& Lens, double Yaw);
    void RefreshPracticalLights(bool Force);
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
