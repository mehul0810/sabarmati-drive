#include "RiverfrontWorld.h"
#include "RiverfrontCar.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Misc/CommandLine.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"

const TCHAR* ARiverfrontWorld::WeatherName() const
{
    return Weather==2?TEXT("Monsoon"):Weather==1?TEXT("Haze"):TEXT("Clear");
}

void ARiverfrontWorld::BuildWeather()
{
    const bool Reference=FParse::Param(FCommandLine::Get(),TEXT("SabarmatiBenchmark")) && FParse::Param(FCommandLine::Get(),TEXT("SabarmatiLightingReference"));
    if (!Reference)
    {
        if (auto* SkyMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Sabarmati/Riverfront/M_DistrictSky.M_DistrictSky")))
        {
            PhotoSky=UMaterialInstanceDynamic::Create(SkyMaterial,this);
            PhotoDome=NewObject<UStaticMeshComponent>(this);
            AddInstanceComponent(PhotoDome);
            PhotoDome->SetupAttachment(GetRootComponent());
            PhotoDome->SetMobility(EComponentMobility::Movable);
            PhotoDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PhotoDome->SetCanEverAffectNavigation(false);
            PhotoDome->SetCastShadow(false);
            PhotoDome->SetStaticMesh(SphereMesh);
            PhotoDome->SetRelativeScale3D(FVector(8000));
            PhotoDome->SetMaterial(0,PhotoSky);
            PhotoDome->RegisterComponent();
            UpdatePhotoSky();
            if (Sky) Sky->SetCaptureIsDirty();
        }
    }
    UMaterialInterface* Dry=SurfaceMaterials.FindRef(int32(ETint::Asphalt));
    if (auto* Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Sabarmati/Riverfront/M_DistrictRoad.M_DistrictRoad")))
    {
        RoadWeather=UMaterialInstanceDynamic::Create(Surface,this);
        TInlineComponentArray<UInstancedStaticMeshComponent*> Existing(this);
        for (auto* Comp:Existing)
            if (Comp->GetMaterial(0)==Dry) Comp->SetMaterial(0,RoadWeather);
        SurfaceMaterials.Add(int32(ETint::Asphalt),RoadWeather);
    }
    RainStreaks=NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(RainStreaks);
    RainStreaks->SetupAttachment(GetRootComponent());
    RainStreaks->SetMobility(EComponentMobility::Movable);
    RainStreaks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RainStreaks->SetCanEverAffectNavigation(false);
    RainStreaks->SetCastShadow(false);
    RainStreaks->SetStaticMesh(BoxMesh);
    RainStreaks->SetMaterial(0,SurfaceMaterials.FindRef(int32(ETint::ClearGlass)));
    RainStreaks->SetVisibility(false);
    RainStreaks->RegisterComponent();
    FRandomStream Drops(98512);
    TArray<FTransform> Initial;
    for (int32 I=0;I<265;++I)
    {
        RainOffsets.Emplace(Drops.FRandRange(-610,610),Drops.FRandRange(-610,610),Drops.FRandRange(0,560));
        Initial.Emplace(FRotator(0,0,-10),RainOffsets.Last(),FVector(.008,.009,.11+I%8*.009));
    }
    RainStreaks->AddInstances(Initial,false);
    const auto Audio=[this](const TCHAR* Name)
    {
        auto* A=NewObject<UAudioComponent>(this);
        AddInstanceComponent(A); A->SetupAttachment(GetRootComponent());
        A->bAutoActivate=false; A->bAllowSpatialization=true;
        A->SetSound(LoadObject<USoundWave>(nullptr,Name));
        A->RegisterComponent();
        return A;
    };
    RainAudio=Audio(TEXT("/Game/Sabarmati/Riverfront/Audio/S_Rain.S_Rain"));
    WindAudio=Audio(TEXT("/Game/Sabarmati/Riverfront/Audio/S_Wind.S_Wind"));
    if (!IsCityMode())
    {
        if (auto* River=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_River.S_River")))
        {
            for (const FVector& Shore : {FVector(-6000,1530,-65),FVector(3600,1500,-65),FVector(-5000,-23730,-65)})
                UGameplayStatics::SpawnSoundAtLocation(this,River,Shore,FRotator::ZeroRotator,.34f,1.f, FMath::Abs(Shore.X)*.0003);
        }
    }
    if (!IsCityMode())
    {
        if (auto* Voices=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_MarketVoices.S_MarketVoices")))
        {
            for (const FVector& P:{GetTeaCounter()+FVector(150,60,50),FVector(43400,-28700,145),FVector(-5350,2640,145)})
                UGameplayStatics::SpawnSoundAtLocation(this,Voices,P,FRotator::ZeroRotator,.16f,.94f+FMath::Frac(FMath::Abs(P.X)*.007f)*.12f,2.f);
        }
        if (auto* Bed=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_DistantTraffic.S_DistantTraffic")))
            for (const FVector& P:{FVector(3150,4090,95),FVector(-8120,4090,95),FVector(3000,-26010,95)})
                UGameplayStatics::SpawnSoundAtLocation(this,Bed,P,FRotator::ZeroRotator,.23f,1.f,1.3f);
    }
    SetActorTickEnabled(true);
}

void ARiverfrontWorld::CycleWeather()
{
    Weather=(Weather+1)%3;
    if (Weather==2 && RainAudio) RainAudio->FadeIn(1.5f,.46f);
    else if (RainAudio && RainAudio->IsPlaying()) RainAudio->FadeOut(1.8f,0);
    if (Weather && WindAudio && !WindAudio->IsPlaying()) WindAudio->FadeIn(1.7f,.22f);
    if (!Weather && WindAudio) WindAudio->FadeOut(2.f,0);
    SetActorTickInterval(Weather==2 || bClock || RainAmount>.01f?.045f:.21f);
    UE_LOG(LogTemp,Display,TEXT("Sabarmati weather: %s."),WeatherName());
}

void ARiverfrontWorld::StartMorningClock()
{
    ClockSeconds = 7.25f * 30.f;
    bClock = true;
    SetActorTickInterval(.045f);
    SetActorTickEnabled(true);
    UpdateWeather(0.f);
}

void ARiverfrontWorld::ToggleClock()
{
    bClock=!bClock;
    ClockSeconds=IsNight()?660.f:TimeOfDay==ERiverfrontTime::Golden?535.f:320.f;
    SetActorTickInterval(bClock || Weather==2?.045f:.21f);
    UE_LOG(LogTemp,Display,TEXT("Sabarmati continuous clock: %d."),bClock);
}

void ARiverfrontWorld::ApplyWeatherLight()
{
    UpdatePhotoSky();
    if (Sun) Sun->SetIntensity(DrySunIntensity*FMath::Lerp(Weather==1?.79f:1.f,.40f,RainAmount));
    if (Haze)
    {
        Haze->SetFogDensity(DryFogDensity+(Weather==1?.009f:0.f)+RainAmount*.018f);
        if (RainAmount>.01f) Haze->SetFogInscatteringColor(IsNight()?FLinearColor(.014,.025,.036):FLinearColor(.31,.35,.37));
    }
}

void ARiverfrontWorld::UpdateWeather(float Dt)
{
    if (!RainStreaks) return;
    const auto* Player=GetWorld()->GetFirstPlayerController();
    if (!Player || !Player->PlayerCameraManager) return;
    const FVector Eye=Player->PlayerCameraManager->GetCameraLocation();
    if (PhotoDome && FVector::DistSquared(PhotoDome->GetComponentLocation(),Eye)>FMath::Square(16000.))
        PhotoDome->SetWorldLocation(Eye);
    const float Prior=RainAmount, OldWet=SurfaceWetness;
    RainAmount=FMath::FInterpConstantTo(RainAmount,Weather==2?1.f:0.f,Dt,.30f);
    SurfaceWetness=FMath::FInterpConstantTo(SurfaceWetness,Weather==2?1.f:0.f,Dt,Weather==2?.12f:.035f);
    if (CanopyLeaves && !FMath::IsNearlyEqual(Prior,RainAmount))
        CanopyLeaves->SetScalarParameterValue(TEXT("WindCm"),2.8f+RainAmount*7.5f);
    if (RoadWeather && !FMath::IsNearlyEqual(OldWet,SurfaceWetness))
    {
        RoadWeather->SetScalarParameterValue(TEXT("Wetness"),SurfaceWetness);
        for (TActorIterator<ARiverfrontCar> It(GetWorld());It;++It) It->SetRoadWetness(SurfaceWetness);
    }
    if (bClock)
    {
        ClockSeconds=FMath::Fmod(ClockSeconds+Dt,720.f);
        const float Hour=ClockSeconds/30.f;
        const ERiverfrontTime Band=Hour<5.65f || Hour>=19.35f?ERiverfrontTime::Night:
            (Hour<7.3f || Hour>=16.6f?ERiverfrontTime::Golden:ERiverfrontTime::Day);
        if (TimeOfDay!=Band) ApplyTimeOfDay(Band);
        const float Day=FMath::Max(0.f,FMath::Sin((Hour-5.5f)*PI/14.f));
        const float Warm=FMath::Clamp(Day/.52f,0.f,1.f);
        DrySunIntensity=FMath::Lerp(.34f,5.1f,FMath::SmoothStep(.0f,.53f,Day));
        DryFogDensity=FMath::Lerp(.006f,.009f,Day);
        if (Sun)
        {
            Sun->SetWorldRotation(FRotator(Day>.01f?-4.-Day*45.:-29.,Hour*15.-105.,0));
            Sun->SetTemperature(Day>.01f?FMath::Lerp(3750.f,5750.f,Warm):8400.f);
        }
    }
    if (bClock || Weather || Prior!=RainAmount || Weather!=LastWeather)
    {
        ApplyWeatherLight();
        if (Weather!=LastWeather && Weather!=2 && RainAmount<=.01f && Haze)
            Haze->SetFogInscatteringColor(IsNight()?FLinearColor(.009,.015,.033):TimeOfDay==ERiverfrontTime::Golden?
                FLinearColor(.42,.31,.22):FLinearColor(.35,.41,.49));
        LastWeather=Weather;
    }
    if (RainAudio && RainAudio->IsPlaying()) RainAudio->SetWorldLocation(Eye+FVector(155,140,140));
    if (WindAudio && WindAudio->IsPlaying()) WindAudio->SetWorldLocation(Eye+FVector(-410,460,100));
    if (RainAmount<.008f)
    {
        if (RainStreaks->IsVisible()) { RainStreaks->SetVisibility(false); SetActorTickInterval(bClock?.045f:.21f); }
        return;
    }
    RainStreaks->SetVisibility(true);
    // Skip rain below a real roof. Keep the audible rainfall outside the room.
    FHitResult Roof;
    const bool Sheltered=GetWorld()->LineTraceSingleByObjectType(Roof,Eye+FVector(0,0,12),Eye+FVector(0,0,1250),
        FCollisionObjectQueryParams(ECC_WorldStatic));
    if (Sheltered) { RainStreaks->SetVisibility(false); if (RainAudio) RainAudio->SetVolumeMultiplier(.15f); return; }
    if (RainAudio) RainAudio->SetVolumeMultiplier(.46f*RainAmount);
    TArray<FTransform> Transforms;
    Transforms.Reserve(RainOffsets.Num());
    const double Time=GetWorld()->GetTimeSeconds();
    for (int32 I=0;I<RainOffsets.Num();++I)
    {
        const FVector& P=RainOffsets[I];
        const double Fall=FMath::Fmod(P.Z+Time*(733.+I%7*18.),560.);
        const FVector At=Eye+FVector(P.X,P.Y-Fall*.12,360.-Fall);
        Transforms.Emplace(FRotator(0,0,-9),At,FVector(.008,.009,.13+I%6*.009)*RainAmount);
    }
    RainStreaks->BatchUpdateInstancesTransforms(0,Transforms,true,true,true);
}

void ARiverfrontWorld::UpdatePhotoSky()
{
    if (!PhotoSky || !Sun) return;
    const FVector Source=-Sun->GetForwardVector();
    PhotoSky->SetScalarParameterValue(TEXT("SolarAzimuth"),FMath::Atan2(Source.Y,Source.X));
    float Day=TimeOfDay==ERiverfrontTime::Day?1.f:0.f;
    float Night=IsNight()?1.f:0.f;
    if (bClock)
    {
        const float Solar=FMath::Sin((ClockSeconds/30.f-5.5f)*PI/14.f);
        Day=FMath::SmoothStep(.1f,.64f,Solar);
        Night=1.f-FMath::SmoothStep(-.09f,.095f,Solar);
    }
    const FLinearColor Clear=FMath::Lerp(FLinearColor(1.42f,1.32f,1.21f),FLinearColor(1.63f,1.66f,1.69f),Day);
    const FLinearColor SkyLight=FMath::Lerp(Clear,FLinearColor(.040f,.052f,.090f),Night);
    PhotoSky->SetScalarParameterValue(TEXT("DayMix"),FMath::Lerp(Day,.92f,RainAmount));
    PhotoSky->SetVectorParameterValue(TEXT("SkyTint"),SkyLight*FMath::Lerp(Weather==1?.88f:1.f,.48f,RainAmount));
}
