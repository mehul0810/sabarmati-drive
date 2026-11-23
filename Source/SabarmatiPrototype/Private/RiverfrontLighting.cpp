#include "RiverfrontWorld.h"
#include "RiverfrontCar.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"

namespace
{
struct FDistrictSky
{
    FRotator Direction;
    float KeyIntensity, Kelvin, SkyIntensity, Haze;
    FLinearColor HazeColor;
};

constexpr int32 MaxPracticalLights = 14;
const FDistrictSky Day = {FRotator(-43, 100, 0), 5.1f, 5750.f, .80f, .007f, FLinearColor(.35,.41,.49)};
const FDistrictSky Golden = {FRotator(-18, 63, 0), 4.1f, 4900.f, .82f, .010f, FLinearColor(.42,.31,.22)};
const FDistrictSky Night = {FRotator(-29, -61, 0), .23f, 8400.f, .36f, .006f, FLinearColor(.009,.015,.033)};
}

const TCHAR* ARiverfrontWorld::TimeName() const
{
    switch (TimeOfDay)
    {
    case ERiverfrontTime::Day: return TEXT("day");
    case ERiverfrontTime::Night: return TEXT("night");
    default: return TEXT("golden");
    }
}

void ARiverfrontWorld::AddPractical(const FVector& Lens, double Yaw)
{
    // A small diffuser sits on the underside of the modeled luminaire.
    PracticalFixtures.Emplace(FRotator(-65, Yaw, 0), Lens);
    if (!LensInstances)
    {
        UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/M_PracticalLens.M_PracticalLens"));
        if (!Parent || !BoxMesh)
        {
            return;
        }
        LensInstances = NewObject<UInstancedStaticMeshComponent>(this);
        AddInstanceComponent(LensInstances);
        LensInstances->SetupAttachment(GetRootComponent());
        LensInstances->SetMobility(EComponentMobility::Static);
        LensInstances->SetStaticMesh(BoxMesh);
        LampMaterial = UMaterialInstanceDynamic::Create(Parent, this);
        LensInstances->SetMaterial(0, LampMaterial);
        LensInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        LensInstances->SetCanEverAffectNavigation(false);
        LensInstances->SetCastShadow(false);
        LensInstances->RegisterComponent();
    }
    LensInstances->AddInstance(FTransform(FRotator(0, Yaw, 0), Lens, FVector(.35,.18,.026)));
}

void ARiverfrontWorld::BuildLighting()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    World->SpawnActor<ASkyAtmosphere>();
    if (auto* Actor = World->SpawnActor<ADirectionalLight>())
    {
        Sun = Cast<UDirectionalLightComponent>(Actor->GetLightComponent());
        Sun->SetMobility(EComponentMobility::Movable);
        Sun->SetUseTemperature(true);
        Sun->SetAtmosphereSunLight(true);
        Sun->SetLightSourceAngle(.65f);
        Sun->ContactShadowLength = .045f;
    }
    if (auto* Actor = World->SpawnActor<ASkyLight>())
    {
        Sky = Actor->GetLightComponent();
        Sky->SetMobility(EComponentMobility::Movable);
    }
    if (auto* Actor = World->SpawnActor<AExponentialHeightFog>(FVector(0,0,-250), FRotator::ZeroRotator))
    {
        Haze = Actor->GetComponent();
        Haze->SetFogHeightFalloff(.24f);
    }
    if (IsCityMode())
    {
        ApplyTimeOfDay(ERiverfrontTime::Day);
        return;
    }
    NightWindows = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sabarmati/Riverfront/M_OccupiedWindow.M_OccupiedWindow"));
#if !UE_BUILD_SHIPPING
    // A/B control: the previous lighting on the exact same geometry, render settings and camera route.
    if (FParse::Param(FCommandLine::Get(), TEXT("SabarmatiBenchmark"))
        && FParse::Param(FCommandLine::Get(), TEXT("SabarmatiLightingReference")))
    {
        TimeOfDay = ERiverfrontTime::Day;
        if (Sun)
        {
            Sun->SetWorldRotation(FRotator(-34,104,0));
            Sun->SetIntensity(5.05f);
            Sun->SetTemperature(5400.f);
            Sun->SetLightSourceAngle(1.15f);
        }
        if (Sky)
        {
            Sky->SetIntensity(.92f);
            Sky->SetCaptureIsDirty();
        }
        if (Haze)
        {
            Haze->SetFogDensity(.008f);
        }
        UE_LOG(LogTemp, Display, TEXT("Sabarmati lighting reference active."));
        return;
    }
#endif
    if (auto* Post = World->SpawnActor<APostProcessVolume>())
    {
        Post->bUnbound = true;
        auto& Settings = Post->Settings;
        Settings.bOverride_BloomIntensity = true;
        Settings.BloomIntensity = .18f;
        Settings.bOverride_AmbientOcclusionIntensity = true;
        Settings.AmbientOcclusionIntensity = .67f;
        Settings.bOverride_AmbientOcclusionRadius = true;
        Settings.AmbientOcclusionRadius = 62.f;
        Settings.bOverride_ScreenSpaceReflectionIntensity = true;
        Settings.ScreenSpaceReflectionIntensity = 72.f;
    }
    FString Requested;
    FParse::Value(FCommandLine::Get(), TEXT("SabarmatiTime="), Requested);
    ApplyTimeOfDay(Requested == TEXT("day") ? ERiverfrontTime::Day
        : Requested == TEXT("night") ? ERiverfrontTime::Night : ERiverfrontTime::Golden);
}

void ARiverfrontWorld::CycleTimeOfDay()
{
    if (!IsCityMode())
    {
        bClock=false;
        ApplyTimeOfDay(TimeOfDay == ERiverfrontTime::Golden ? ERiverfrontTime::Night
            : TimeOfDay == ERiverfrontTime::Night ? ERiverfrontTime::Day : ERiverfrontTime::Golden);
    }
}

void ARiverfrontWorld::ApplyTimeOfDay(ERiverfrontTime Time)
{
    TimeOfDay = Time;
    const bool AtNight = Time == ERiverfrontTime::Night;
    const FDistrictSky& Lighting = AtNight ? Night : Time == ERiverfrontTime::Day ? Day : Golden;
    if (Sun)
    {
        Sun->SetWorldRotation(Lighting.Direction);
        Sun->SetIntensity(Lighting.KeyIntensity);
        Sun->SetTemperature(Lighting.Kelvin);
    }
    if (Haze)
    {
        Haze->SetFogDensity(Lighting.Haze);
        Haze->SetFogInscatteringColor(Lighting.HazeColor);
        Haze->SetDirectionalInscatteringColor(Lighting.HazeColor * .6f);
        Haze->SetFogMaxOpacity(AtNight ? .53f : .82f);
    }
    if (Sky)
    {
        Sky->SetIntensity(Lighting.SkyIntensity);
        Sky->SetLowerHemisphereColor(AtNight ? FLinearColor(.004,.006,.010) : FLinearColor(.085,.067,.047));
        Sky->SetCaptureIsDirty();
    }
    if (LampMaterial)
    {
        LampMaterial->SetScalarParameterValue(TEXT("Glow"), AtNight ? 3.4f : 0.f);
    }
    UMaterialInterface* Glazing = AtNight && NightWindows ? NightWindows.Get()
        : SurfaceMaterials.FindRef(static_cast<int32>(ETint::Glass)).Get();
    if (Glazing)
    {
        for (const auto& Pair : Batches)
        {
            if (Pair.Key % static_cast<int32>(ETint::Count) == static_cast<int32>(ETint::Glass))
            {
                Pair.Value->SetMaterial(0, Glazing);
            }
        }
    }
    for (TActorIterator<ARiverfrontCar> It(GetWorld()); It; ++It)
    {
        It->SetNightDriving(AtNight);
    }
    DrySunIntensity=Lighting.KeyIntensity;
    DryFogDensity=Lighting.Haze;
    ApplyWeatherLight();
    SetActorTickEnabled(true);
    if (AtNight)
    {
        RefreshPracticalLights(true);
    }
    else
    {
        for (USpotLightComponent* Lamp : PracticalLights)
        {
            Lamp->SetVisibility(false);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati district lighting: %s, %d fitted lamps, %d pooled emitters."),
        TimeName(), PracticalFixtures.Num(), PracticalLights.Num());
}

void ARiverfrontWorld::RefreshPracticalLights(bool Force)
{
    const APlayerController* Player = GetWorld()->GetFirstPlayerController();
    if (!Player || !IsNight() || PracticalFixtures.IsEmpty())
    {
        return;
    }
    FVector Eye;
    FRotator Angle;
    Player->GetPlayerViewPoint(Eye, Angle);
    if (!Force && FVector::DistSquared(Eye, LastLightFocus) < FMath::Square(170.))
    {
        return;
    }
    LastLightFocus = Eye;
    TArray<TPair<double,int32>, TInlineAllocator<40>> Nearby;
    for (int32 I = 0; I < PracticalFixtures.Num(); ++I)
    {
        const double D2 = FVector::DistSquared(PracticalFixtures[I].GetLocation(), Eye);
        if (D2 < FMath::Square(7400.))
        {
            Nearby.Emplace(D2, I);
        }
    }
    Nearby.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
    const int32 Count = FMath::Min(Nearby.Num(), MaxPracticalLights);
    while (PracticalLights.Num() < Count)
    {
        USpotLightComponent* Lamp = NewObject<USpotLightComponent>(this);
        AddInstanceComponent(Lamp);
        Lamp->SetupAttachment(GetRootComponent());
        Lamp->SetMobility(EComponentMobility::Movable);
        Lamp->SetCastShadows(false);
        Lamp->SetIntensityUnits(ELightUnits::Lumens);
        Lamp->SetIntensity(360.f);
        Lamp->SetUseTemperature(true);
        Lamp->SetTemperature(3520.f);
        Lamp->SetInnerConeAngle(36.f);
        Lamp->SetOuterConeAngle(65.f);
        Lamp->SetAttenuationRadius(1350.f);
        Lamp->SetSourceRadius(8.f);
        Lamp->SetMaxDrawDistance(7400.f);
        Lamp->MaxDistanceFadeRange = 1800.f;
        Lamp->RegisterComponent();
        PracticalLights.Add(Lamp);
    }
    for (int32 I = 0; I < PracticalLights.Num(); ++I)
    {
        auto* Lamp = PracticalLights[I].Get();
        if (I < Count)
        {
            Lamp->SetWorldTransform(PracticalFixtures[Nearby[I].Value]);
        }
        Lamp->SetVisibility(I < Count);
    }
}
