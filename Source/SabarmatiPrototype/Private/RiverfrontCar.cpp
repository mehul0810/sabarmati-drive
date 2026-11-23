#include "RiverfrontCar.h"
#include "Components/SpotLightComponent.h"
#include "EngineUtils.h"
#include "RiverfrontWorld.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontGameMode.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float TopSpeed = 1680.f; // cm/s, about 60 km/h.
constexpr float ReverseLimit = 460.f;
constexpr float DoorReach = 148.f;
constexpr float ParkThreshold = 36.f;
constexpr double WheelRadius = 34.0;
constexpr double DetailedWheelRadius = 31.6;
constexpr double ModelGroundOffset = -68.0;

UStaticMesh* LoadVehicleMesh(const TCHAR* Name)
{
    const FString Package = FString(TEXT("/Game/Sabarmati/Riverfront/Vehicles/")) + Name;
    return FPackageName::DoesPackageExist(Package)
        ? LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") + Name)) : nullptr;
}

bool FindGround(UWorld* World, const FVector& Position, const AActor* Ignored, FHitResult& Floor)
{
    if (!World)
    {
        return false;
    }
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(RiverfrontGroundTrace), false, Ignored);
    return World->LineTraceSingleByChannel(Floor, Position + FVector(0, 0, 180),
        Position - FVector(0, 0, 175), ECC_Visibility, Query) && Floor.ImpactNormal.Z > .65
        && Position.Z - Floor.ImpactPoint.Z >= 27. && Position.Z - Floor.ImpactPoint.Z <= 137.;
}
}

ARiverfrontCar::ARiverfrontCar()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("ChassisCollision"));
    SetRootComponent(Body);
    Body->InitBoxExtent(FVector(216, 95, 50));
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Body->SetGenerateOverlapEvents(false);
    Body->SetCanEverAffectNavigation(false);
    bUseControllerRotationYaw = false;

    Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
    Arm->SetupAttachment(Body);
    Arm->TargetArmLength = 625.f;
    Arm->TargetOffset = FVector(0, 0, 111);
    Arm->bUsePawnControlRotation = true;
    Arm->bEnableCameraLag = true;
    Arm->CameraLagSpeed = 9.f;
    Arm->CameraLagMaxDistance = 120.f;
    Arm->ProbeSize = 14.f;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
    Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
    Camera->SetFieldOfView(82.f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    BoxMesh = Cube.Object;
    SphereMesh = Sphere.Object;
    CylinderMesh = Cylinder.Object;
    BaseMaterial = Material.Object;

    const FColor Paint(155, 55, 37);
    const FColor BrightPaint(181, 66, 46);
    const FColor Black(34, 34, 33);
    const FColor Glass(54, 82, 86);
    const FColor Cream(247, 224, 169);
    const FColor Chrome(160, 160, 151);

    Panel(TEXT("Undertray"), BoxMesh, Body, FVector(0, 0, -23), FVector(388, 153, 19), Black);
    Panel(TEXT("Flanks"), SphereMesh, Body, FVector(0, 0, 0), FVector(438, 190, 85), Paint);
    Panel(TEXT("Sill"), BoxMesh, Body, FVector(-2, 0, -5), FVector(384, 168, 34), Paint);
    Panel(TEXT("Bonnet"), SphereMesh, Body, FVector(133, 0, 25), FVector(148, 171, 38), BrightPaint);
    Panel(TEXT("Cabin"), SphereMesh, Body, FVector(-33, 0, 48), FVector(232, 153, 77), Glass);
    Panel(TEXT("Roof"), SphereMesh, Body, FVector(-41, 0, 74), FVector(164, 132, 33), BrightPaint);
    Panel(TEXT("FrontBumper"), BoxMesh, Body, FVector(204, 0, -14), FVector(22, 160, 29), Black);
    Panel(TEXT("RearBumper"), BoxMesh, Body, FVector(-205, 0, -15), FVector(18, 157, 26), Black);
    Panel(TEXT("Grille"), BoxMesh, Body, FVector(216, 0, 1), FVector(8, 84, 18), Black);
    Panel(TEXT("FrontPlate"), BoxMesh, Body, FVector(222, 0, -13), FVector(4, 42, 12), Cream);
    Panel(TEXT("RearPlate"), BoxMesh, Body, FVector(-216, 0, 1), FVector(4, 41, 12), Cream);

    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        USpotLightComponent* Light = CreateDefaultSubobject<USpotLightComponent>(
            Side < 0 ? TEXT("LeftDippedBeam") : TEXT("RightDippedBeam"));
        Light->SetupAttachment(Body);
        Light->SetRelativeLocationAndRotation(FVector(200, Side * 65, 4), FRotator(-5.5, Side * 4., 0));
        Light->SetIntensityUnits(ELightUnits::Candelas);
        Light->SetIntensity(685.f);
        Light->SetUseTemperature(true);
        Light->SetTemperature(4650.f);
        Light->SetInnerConeAngle(10.f);
        Light->SetOuterConeAngle(25.f);
        Light->SetAttenuationRadius(4100.f);
        Light->SetSourceRadius(4.f);
        Light->SetCastShadows(false);
        Light->SetVisibility(false);
        Headlights.Add(Light);
        Panel(*FString::Printf(TEXT("Headlamp%d"), Side + 1), BoxMesh, Body,
            FVector(205, Side * 62, 15), FVector(17, 32, 12), Cream);
        Panel(*FString::Printf(TEXT("Taillamp%d"), Side + 1), BoxMesh, Body,
            FVector(-207, Side * 62, 17), FVector(14, 29, 13), FColor(204, 41, 39));
        Panel(*FString::Printf(TEXT("Mirror%d"), Side + 1), SphereMesh, Body,
            FVector(56, Side * 95, 55), FVector(26, 18, 12), Paint);
        Panel(*FString::Printf(TEXT("Pillar%d"), Side + 1), BoxMesh, Body,
            FVector(-57, Side * 73, 51), FVector(8, 6, 49), Black);

        for (int32 Axle = 0; Axle < 2; ++Axle)
        {
            const FString Tag = FString::Printf(TEXT("%d%d"), Side + 1, Axle);
            USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(FName(*(TEXT("Steer") + Tag)));
            Pivot->SetupAttachment(Body);
            Pivot->SetRelativeLocation(FVector(Axle == 0 ? 134 : -134, Side * 85, -34));
            if (Axle == 0)
            {
                SteerPivots.Add(Pivot);
            }
            USceneComponent* Spinner = CreateDefaultSubobject<USceneComponent>(FName(*(TEXT("Spin") + Tag)));
            Spinner->SetupAttachment(Pivot);
            WheelPivots.Add(Spinner);

            Panel(*(TEXT("Tyre") + Tag), CylinderMesh, Spinner, FVector::ZeroVector,
                FVector(68, 68, 25), Black, FRotator(0, 0, 90));
            Panel(*(TEXT("Rim") + Tag), CylinderMesh, Spinner, FVector(0, Side * 13.5, 0),
                FVector(44, 44, 4), Chrome, FRotator(0, 0, 90));
            for (int32 Spoke = 0; Spoke < 3; ++Spoke)
            {
                Panel(*FString::Printf(TEXT("Spoke%s%d"), *Tag, Spoke), BoxMesh, Spinner,
                    FVector(0, Side * 16.5, 0), FVector(31, 2, 3), Black, FRotator(60 * Spoke, 0, 0));
            }
        }
    }
}

void ARiverfrontCar::Panel(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent,
    const FVector& Offset, const FVector& Size, FColor Color, const FRotator& Rotation)
{
    UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
    Part->SetupAttachment(Parent);
    Part->SetStaticMesh(Mesh);
    Part->SetRelativeLocation(Offset);
    Part->SetRelativeRotation(Rotation);
    Part->SetRelativeScale3D(Size / 100.0);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Panels.Add(Part);
    PanelColors.Add(Color);
}

bool ARiverfrontCar::InstallDetailedVisuals()
{
    UStaticMesh* Chassis = LoadVehicleMesh(bCrossover
        ? TEXT("SM_SabarmatiCrossover") : TEXT("SM_SabarmatiHatch"));
    UStaticMesh* LeftWheel = LoadVehicleMesh(TEXT("SM_HatchWheelL"));
    UStaticMesh* RightWheel = LoadVehicleMesh(TEXT("SM_HatchWheelR"));
    if (!Chassis || !LeftWheel || !RightWheel || WheelPivots.Num() != 4 || SteerPivots.Num() != 2)
    {
        return false;
    }

    const auto PlaceMesh = [this](UStaticMesh* Mesh, USceneComponent* Parent,
        const FVector& Location, double Yaw)
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(Part);
        Part->SetupAttachment(Parent);
        Part->SetStaticMesh(Mesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetRelativeLocationAndRotation(Location, FRotator(0, Yaw, 0));
        Part->RegisterComponent();
        return Part;
    };

    // Interchange maps glTF +Z to Unreal +Y. Turn that into the pawn's +X forward.
    UStaticMesh* Articulated=nullptr;
    UStaticMesh* LeftDoor=nullptr;
    UStaticMesh* RightDoor=nullptr;
    if (!bCrossover && FPackageName::DoesPackageExist(TEXT("/Game/Sabarmati/Riverfront/Vehicles/Coachwork/SM_HatchCabin")))
    {
        Articulated=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Vehicles/Coachwork/SM_HatchCabin.SM_HatchCabin"));
        LeftDoor=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Vehicles/Coachwork/SM_HatchDoorL.SM_HatchDoorL"));
        RightDoor=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Vehicles/Coachwork/SM_HatchDoorR.SM_HatchDoorR"));
    }
    const bool Hinged=Articulated && LeftDoor && RightDoor;
    DetailedBody=PlaceMesh(Hinged?Articulated:Chassis,Body,FVector(0,0,ModelGroundOffset),-90.);
    if (Hinged)
    {
        for (const int32 Side : {-1,1})
        {
            auto* Hinge=NewObject<USceneComponent>(this);
            AddInstanceComponent(Hinge);
            Hinge->SetupAttachment(Body);
            Hinge->SetRelativeLocation(FVector(62,Side*79,79+ModelGroundOffset));
            Hinge->RegisterComponent(); DoorHinges.Add(Hinge);
            DoorSkins.Add(PlaceMesh(Side<0 ? LeftDoor : RightDoor,Hinge,FVector::ZeroVector,-90.));
        }
    }
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const bool bRight = SideIndex == 1;
        for (int32 Axle = 0; Axle < 2; ++Axle)
        {
            USceneComponent* Wheel = WheelPivots[SideIndex * 2 + Axle];
            USceneComponent* Pivot = Wheel->GetAttachParent();
            Pivot->SetRelativeLocation(FVector(Axle == 0 ? 122 : -138, bRight ? 77 : -77,
                DetailedWheelRadius + ModelGroundOffset));
            PlaceMesh(bRight ? RightWheel : LeftWheel, Wheel, FVector::ZeroVector, bRight ? 90.0 : -90.0);
        }
    }
    UMaterialInterface* Window = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/M_DistrictGlass.M_DistrictGlass"));
    UMaterialInterface* Finish = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/M_DistrictCarPaint.M_DistrictCarPaint"));
    TArray<UStaticMeshComponent*> Shells{DetailedBody.Get()};
    for (const auto& Door:DoorSkins) Shells.Add(Door);
    UMaterialInstanceDynamic* Paint=Finish?UMaterialInstanceDynamic::Create(Finish,this):nullptr;
    if (Paint) Paint->SetVectorParameterValue(TEXT("BodyColor"),bCrossover?FLinearColor(.14,.052,.021):FLinearColor(.12,.19,.16));
    for (auto* Shell:Shells)
    {
        const auto Slots=Shell->GetMaterialSlotNames();
        for (int32 Slot=0;Slot<Slots.Num();++Slot)
        {
            const FString Name=Slots[Slot].ToString().ToLower();
            if (Window && Name.Contains(TEXT("glass")) && !Name.Contains(TEXT("mirror"))) Shell->SetMaterial(Slot,Window);
            else if (Paint && (Name==TEXT("paint") || Name==TEXT("paint_body") || Name==TEXT("crossover_paint") || Name==TEXT("crossover_paint_body")))
                Shell->SetMaterial(Slot,Paint);
        }
    }
    for (UStaticMeshComponent* Part : Panels)
    {
        Part->SetHiddenInGame(true);
        Part->SetVisibility(false);
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati Drive: detailed RHD %s installed with four animated wheels."),
        GetVehicleLabel());
    return true;
}

void ARiverfrontCar::BeginPlay()
{
    Super::BeginPlay();
    Home = GetActorTransform();
    if (bCrossover)
    {
        // A taller body and longer bumpers share the hatch's axle positions.
        Body->SetBoxExtent(GetCrossoverBounds(), true);
        Arm->TargetOffset.Z += 14.0;
        Arm->TargetArmLength += 26.0;
    }
    bDetailedVisuals = InstallDetailedVisuals();
    TravelYaw = GetActorRotation().Yaw;
    auto Loop = [&](const TCHAR* Path, const FVector& At)
    {
        auto* Audio=NewObject<UAudioComponent>(this);
        AddInstanceComponent(Audio);
        Audio->SetupAttachment(Body);
        Audio->SetRelativeLocation(At);
        Audio->bAutoActivate=false;
        Audio->SetSound(LoadObject<USoundWave>(nullptr,Path));
        Audio->RegisterComponent();
        return Audio;
    };
    MotorLoop=Loop(TEXT("/Game/Sabarmati/Riverfront/Audio/S_Engine.S_Engine"),FVector(123,0,0));
    TyreLoop=Loop(TEXT("/Game/Sabarmati/Riverfront/Audio/S_Tyres.S_Tyres"),FVector(0,0,-36));
    ShiftSound=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_Shift.S_Shift"));
    ImpactSound=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_Contact.S_Contact"));
    if (auto* Lens = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Sabarmati/Riverfront/M_PracticalLens.M_PracticalLens")))
    {
        TurnMaterial=UMaterialInstanceDynamic::Create(Lens,this);
        ReverseMaterial=UMaterialInstanceDynamic::Create(Lens,this);
        TurnMaterial->SetVectorParameterValue(TEXT("LensTint"), FLinearColor(1,.21,.006));
        TurnMaterial->SetScalarParameterValue(TEXT("Glow"),5.f);
        ReverseMaterial->SetVectorParameterValue(TEXT("LensTint"),FLinearColor(.92,.96,1));
        ReverseMaterial->SetScalarParameterValue(TEXT("Glow"),3.f);
        const double Length=bCrossover ? 211. : 200.;
        for (int32 Side : {-1,1})
        {
            for (int32 Position=0;Position<3;++Position)
            {
                auto* Part=NewObject<UStaticMeshComponent>(this);
                AddInstanceComponent(Part);
                Part->SetupAttachment(Body);
                Part->SetStaticMesh(SphereMesh);
                Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Part->SetCastShadow(false);
                Part->SetRelativeLocation(FVector(Position==0 ? Length : -Length,Side*(Position==2?48.:67.),Position==0?2.:11.));
                Part->SetRelativeScale3D(FVector(.045,.135,.047));
                Part->SetMaterial(0,Position==2?ReverseMaterial.Get():TurnMaterial.Get());
                Part->SetVisibility(false);
                Part->RegisterComponent();
                SignalLenses.Add(Part);
            }
        }
    }
    if (TActorIterator<ARiverfrontWorld> District{GetWorld()}; District)
    {
        SetNightDriving(District->IsNight());
    }
    if (!bDetailedVisuals && BaseMaterial)
    {
        for (int32 Index = 0; Index < Panels.Num(); ++Index)
        {
            if (UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(BaseMaterial, Panels[Index].Get()))
            {
                Paint->SetVectorParameterValue(TEXT("Color"), FLinearColor(PanelColors[Index]));
                Panels[Index]->SetMaterial(0, Paint);
            }
        }
    }
}

void ARiverfrontCar::SetNightDriving(bool AtNight)
{
    if (bNightDriving != AtNight)
    {
        bNightDriving = AtNight;
        UpdateDrivingLights();
    }
}

void ARiverfrontCar::UpdateDrivingLights()
{
    const bool On = bNightDriving && IsValid(Driver);
    for (USpotLightComponent* Light : Headlights)
    {
        Light->SetVisibility(On);
    }
    if (!DetailedBody)
    {
        return;
    }
    if ((On || IsValid(Driver)) && !HeadLens)
    {
        UMaterialInterface* Lens = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/M_PracticalLens.M_PracticalLens"));
        if (!Lens)
        {
            return;
        }
        HeadLens = UMaterialInstanceDynamic::Create(Lens, this);
        TailLens = UMaterialInstanceDynamic::Create(Lens, this);
        HeadLens->SetVectorParameterValue(TEXT("LensTint"), FLinearColor(1.0,.83,.62));
        TailLens->SetVectorParameterValue(TEXT("LensTint"), FLinearColor(1.0,.012,.004));
        HeadLens->SetScalarParameterValue(TEXT("Glow"), 4.8f);
        TailLens->SetScalarParameterValue(TEXT("Glow"), 1.8f);
    }
    const TArray<FName> Slots = DetailedBody->GetMaterialSlotNames();
    for (int32 I = 0; I < Slots.Num(); ++I)
    {
        const FString Slot = Slots[I].ToString();
        if (Slot.Contains(TEXT("light_front")))
        {
            DetailedBody->SetMaterial(I, On ? HeadLens.Get() : nullptr);
        }
        else if (Slot.Contains(TEXT("lamp_red")) || Slot.Contains(TEXT("light_rear")))
        {
            DetailedBody->SetMaterial(I, (On || bBraking || Throttle*Speed < -10) ? TailLens.Get() : nullptr);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati %s dipped beams: %s."), GetVehicleLabel(), On ? TEXT("on") : TEXT("off"));
}

void ARiverfrontCar::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("Forward"), this, &ARiverfrontCar::SetThrottle);
    Input->BindAxis(TEXT("Side"), this, &ARiverfrontCar::SetSteering);
    Input->BindAxis(TEXT("LookYaw"), this, &APawn::AddControllerYawInput);
    Input->BindAxis(TEXT("LookPitch"), this, &APawn::AddControllerPitchInput);
    Input->BindAction(TEXT("BrakeOrJump"), IE_Pressed, this, &ARiverfrontCar::Brake);
    Input->BindAction(TEXT("BrakeOrJump"), IE_Released, this, &ARiverfrontCar::ReleaseBrake);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &ARiverfrontCar::Exit);
    Input->BindAction(TEXT("VehicleCamera"),IE_Pressed,this,&ARiverfrontCar::CycleCamera);
    Input->BindAction(TEXT("LeftSignal"),IE_Pressed,this,&ARiverfrontCar::SignalLeft);
    Input->BindAction(TEXT("RightSignal"),IE_Pressed,this,&ARiverfrontCar::SignalRight);
    Input->BindAction(TEXT("Hazards"),IE_Pressed,this,&ARiverfrontCar::Hazard);
    Input->BindAction(TEXT("Horn"),IE_Pressed,this,&ARiverfrontCar::SoundHorn);
    Input->BindAction(TEXT("VehicleDoor"),IE_Pressed,this,&ARiverfrontCar::CockpitDoor);
    Input->BindAction(TEXT("Reset"), IE_Pressed, this, &ARiverfrontCar::ResetCar);
    Input->BindAction(TEXT("DebugStats"), IE_Pressed, this, &ARiverfrontCar::ToggleStats);
}

bool ARiverfrontCar::IsNearDoor(const ARiverfrontCharacter& Character) const
{
    if (IsValid(Driver) || FMath::Abs(Character.GetActorLocation().Z - GetActorLocation().Z) > 130)
    {
        return false;
    }
    const FTransform CarTransform = GetActorTransform();
    for (const double Side : {-1.0, 1.0})
    {
        if (FVector::Dist2D(Character.GetActorLocation(),
            CarTransform.TransformPosition(FVector(10, Side * 101, 0))) <= DoorReach)
        {
            return true;
        }
    }
    return false;
}

void ARiverfrontCar::Enter(ARiverfrontCharacter& Character)
{
    APlayerController* Player = Cast<APlayerController>(Character.GetController());
    if (!Player || !IsNearDoor(Character))
    {
        return;
    }
    if (DoorHinges.Num()==2)
    {
        OpenSide(GetActorTransform().InverseTransformPosition(Character.GetActorLocation()).Y<0?-1:1);
        DoorHold=.72f;
    }
    Driver = &Character;
    TravelYaw=GetActorRotation().Yaw;
    HasCameraSample=false;
    if (MotorLoop && TyreLoop) { MotorLoop->FadeIn(.24f,.32f); TyreLoop->FadeIn(.18f,.02f); }
    Character.SetInCar(true);
    Player->Possess(this);
    UpdateDrivingLights();
    Player->SetControlRotation(FRotator(-13, GetActorRotation().Yaw, 0));
    const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    SetHint(Mode && Mode->IsCityMode() ? TEXT("Explore the mapped Ahmedabad streets.")
        : TEXT("Cross the bridge, then follow the far bank."));
    UE_LOG(LogTemp, Display, TEXT("Sabarmati Drive: entered parked %s."), GetVehicleLabel());
}

void ARiverfrontCar::Exit()
{
    APlayerController* Player = Cast<APlayerController>(Controller);
    if (!Player || !IsValid(Driver))
    {
        return;
    }
    if (FMath::Abs(Speed) > ParkThreshold)
    {
        SetHint(TEXT("Brake to a stop before getting out."));
        return;
    }

    UCapsuleComponent* Capsule = Driver->GetCapsuleComponent();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(
        Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(RiverfrontExit), false, Driver.Get());
    const FVector Extents = Body->GetScaledBoxExtent();
    const double LateralOffset = Extents.Y + 66.0;
    const double LengthOffset = Extents.X + 60.0;
    for (const FVector& Offset : {FVector(0, LateralOffset, 0), FVector(0, -LateralOffset, 0),
        FVector(-LengthOffset, 0, 0), FVector(LengthOffset, 0, 0)})
    {
        FVector Candidate = GetActorTransform().TransformPosition(Offset);
        FHitResult Floor;
        if (!FindGround(GetWorld(), Candidate, this, Floor))
        {
            continue;
        }
        Candidate.Z = Floor.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + 3.;
        if (GetWorld()->OverlapBlockingTestByProfile(Candidate, FQuat::Identity,
            Capsule->GetCollisionProfileName(), Shape, Query))
        {
            continue;
        }
        if (DoorHinges.Num()==2) { OpenSide(Offset.Y<0?-1:1); DoorHold=1.1f; }
        ARiverfrontCharacter* Walker = Driver.Get();
        Walker->SetActorLocationAndRotation(Candidate, FRotator(0, GetActorRotation().Yaw, 0),
            false, nullptr, ETeleportType::TeleportPhysics);
        Walker->SetInCar(false);
        Player->Possess(Walker);
        Player->SetControlRotation(FRotator(-7, GetActorRotation().Yaw, 0));
        Driver = nullptr;
        if (MotorLoop && TyreLoop) { MotorLoop->FadeOut(.28f,.0f); TyreLoop->FadeOut(.15f,0.f); }
        ViewMode=Signal=0;
        bBraking=false;
        Throttle=0;
        UpdateSignalLenses(0);
        UpdateDrivingLights();
        Speed = Throttle = Steering = 0;
        bBraking = false;
        return;
    }
    SetHint(TEXT("No clear spot to get out. Move a little and try again."));
}

void ARiverfrontCar::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    HintRemaining = FMath::Max(0.f, HintRemaining - DeltaSeconds);
    UpdateDoors(DeltaSeconds);

    if (!Controller || !IsValid(Driver))
    {
        return;
    }
    const float T=FMath::Clamp(DeltaSeconds,0.f,.10f);
    const float Mass=bCrossover?1540.f:1210.f;
    const float Grip=(bCrossover?.89f:.96f)*FMath::Lerp(1.f,.65f,Wetness);
    const float AbsSpeed=FMath::Abs(Speed);
    const float MaxVelocity=bCrossover?2420.f:2660.f;
    const bool Opposing=Throttle*Speed < -12.f;
    const float PrevSpeed=Speed;
    const int32 PreviousGear=Gear;
    HitSoundDelay=FMath::Max(0.f,HitSoundDelay-T);
    if (bBraking || Opposing)
    {
        Speed=FMath::FInterpConstantTo(Speed,0.f,T,(bBraking?800.f:885.f)*Grip);
    }
    else if (Fuel>0.f && FMath::Abs(Throttle)>.05f)
    {
        const float Force=(bCrossover?4820.f:4050.f)*FMath::Lerp(1.f,.50f,FMath::Clamp(AbsSpeed/MaxVelocity,0.f,1.f));
        const float Accel=100.f*Force/Mass*FMath::Lerp(.54f,1.f,Condition);
        Speed=FMath::FInterpConstantTo(Speed,Throttle>0?MaxVelocity:-ReverseLimit,T,Accel*FMath::Abs(Throttle));
    }
    else
    {
        const float Resistance=16.f+AbsSpeed*AbsSpeed*.000009f;
        Speed=FMath::FInterpConstantTo(Speed,0.f,T,Resistance);
    }
    const float SteerLimit=FMath::Lerp(34.f,13.f,FMath::Clamp(AbsSpeed/2100.f,0.f,1.f));
    TurnAngle=FMath::FInterpTo(TurnAngle,Steering*SteerLimit,T,7.f);
    float TurnRate=FMath::RadiansToDegrees((Speed/260.f)*FMath::Tan(FMath::DegreesToRadians(TurnAngle)));
    const float MaxRate=FMath::RadiansToDegrees(981.f*Grip/FMath::Max(110.f,AbsSpeed));
    TurnRate=FMath::Clamp(TurnRate,-MaxRate,MaxRate);
    Fuel=FMath::Max(0.f,Fuel-(FMath::Abs(Speed)*T/13000000.f));
    const float Accel=(Speed-PrevSpeed)/FMath::Max(T,.001f);
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FMath::Abs(Speed)*T/22.f));
    const double StepT=T/Steps;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(RiverfrontCarTurn),false,this);
    for (int32 S=0; S<Steps; ++S)
    {
        const FVector Here=GetActorLocation();
        const FRotator Previous=GetActorRotation();
        const double Yaw=Previous.Yaw+TurnRate*StepT;
        const double Response=(bBraking?3.:7.)*Grip;
        TravelYaw+=FMath::FindDeltaAngleDegrees(double(TravelYaw),Yaw)*(1.-FMath::Exp(-Response*StepT));
        FVector Desired=Here+FRotator(0,TravelYaw,0).Vector()*(Speed*StepT);
        double H[4];
        if (!SampleWheels(Desired,Yaw,H))
        {
            ContactCount=0;
            Speed=0;
            break;
        }
        ContactCount=4;
        const double Front=(H[0]+H[2])*.5, Rear=(H[1]+H[3])*.5;
        const double Right=(H[2]+H[3])*.5, Left=(H[0]+H[1])*.5;
        const double Pitch=FMath::RadiansToDegrees(FMath::Atan2(Front-Rear,260.));
        const double Bank=FMath::RadiansToDegrees(FMath::Atan2(Right-Left,154.));
        const double Camber=FMath::Clamp(Speed*FMath::DegreesToRadians(TurnRate)*.0024,-3.4,3.4);
        BodyRoll=FMath::FInterpTo(BodyRoll,float(Camber),StepT,5.);
        const FRotator Facing(FMath::FInterpTo(Previous.Pitch,Pitch+FMath::Clamp(Accel*.0017,-1.6,1.2),StepT,7.),
            Yaw,FMath::FInterpTo(Previous.Roll,-Bank+BodyRoll,StepT,8.));
        Desired.Z=(Front+Rear)*.5+68.;
        if (GetWorld()->OverlapBlockingTestByProfile(Desired,Facing.Quaternion(),Body->GetCollisionProfileName(),
            FCollisionShape::MakeBox(Body->GetScaledBoxExtent()),Query))
        {
            Speed=0;
            break;
        }
        FHitResult Hit;
        SetActorLocationAndRotation(Desired,Facing,true,&Hit);
        if (Hit.bBlockingHit) { Speed=0; break; }
        FRotator Look=Controller->GetControlRotation();
        Look.Yaw+=TurnRate*StepT;
        Controller->SetControlRotation(Look);
        const double Radius=bDetailedVisuals?DetailedWheelRadius:WheelRadius;
        WheelAngle=FMath::Fmod(WheelAngle-float(FMath::RadiansToDegrees(FVector::Dist2D(Here,GetActorLocation())/Radius))*FMath::Sign(Speed),360.f);
        if (bDetailedVisuals)
        {
            for (int32 I=0;I<4;++I)
            {
                auto* Pivot=WheelPivots[I]->GetAttachParent();
                FVector Local=Pivot->GetRelativeLocation();
                const double Contact=GetActorTransform().TransformPosition(FVector(Local.X,Local.Y,-68.)).Z;
                Local.Z=DetailedWheelRadius-68.+FMath::Clamp(H[I]-Contact,-9.,9.);
                Pivot->SetRelativeLocation(Local);
            }
        }
    }
    const float Ratios[]={3.76f,2.18f,1.48f,1.08f,.84f,.69f};
    const float WheelRpm=FMath::Abs(Speed)*60.f/(2.f*PI*float(DetailedWheelRadius));
    const float Coupled=WheelRpm*4.08f;
    if (Gear<6 && Coupled*Ratios[Gear-1]>3800.f) ++Gear;
    else if (Gear>1 && Coupled*Ratios[Gear-1]<1400.f) --Gear;
    EngineRpm=FMath::FInterpTo(EngineRpm,FMath::Max(850.f,Coupled*(Speed<0?3.4f:Ratios[Gear-1])),T,6.f);
    if (PreviousGear!=Gear && ShiftSound)
        UGameplayStatics::PlaySoundAtLocation(this,ShiftSound,GetActorLocation(),.27f,1.f);
    if (Speed==0 && FMath::Abs(PrevSpeed)>155 && HitSoundDelay<=0 && ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this,ImpactSound,GetActorLocation(),FMath::Clamp(FMath::Abs(PrevSpeed)/1700.f,.17f,.8f));
        Condition=FMath::Max(.12f,Condition-FMath::Clamp(FMath::Abs(PrevSpeed)/12500.f,.012f,.19f));
        HitSoundDelay=.45f;
    }
    UpdateMotorAudio();
    UpdateSignalLenses(T);
    for (USceneComponent* Steer : SteerPivots)
    {
        Steer->SetRelativeRotation(FRotator(0, TurnAngle, 0));
    }
    for (USceneComponent* Wheel : WheelPivots)
    {
        Wheel->SetRelativeRotation(FRotator(WheelAngle, 0, 0));
    }
}

void ARiverfrontCar::ResetCar()
{
    if (TeleportTo(Home.GetLocation(), Home.Rotator(), false, false))
    {
        Speed = Throttle = Steering = 0;
        bBraking=false; TurnAngle=BodyRoll=0; Gear=1; Signal=0; TravelYaw=Home.Rotator().Yaw;
        if (Controller)
        {
            Controller->SetControlRotation(FRotator(-13, Home.Rotator().Yaw, 0));
        }
        SetHint(TEXT("Back at the starting point."));
    }
}

void ARiverfrontCar::ToggleStats()
{
    if (APlayerController* Player = Cast<APlayerController>(Controller))
    {
        Player->ConsoleCommand(TEXT("stat unit"));
    }
}

FString ARiverfrontCar::GetHint() const
{
    return HintRemaining > 0.f ? Hint : FString();
}

void ARiverfrontCar::SetHint(const TCHAR* Message)
{
    Hint = Message;
    HintRemaining = 3.9f;
}

bool ARiverfrontCar::SampleWheels(const FVector& Centre,double Yaw,double (&H)[4]) const
{
    const FRotator Heading(0,Yaw,0);
    for (int32 I=0;I<4;++I)
    {
        const FVector Wheel=Centre+Heading.RotateVector(FVector(I%2==0?122.:-138.,I<2?-77.:77.,0));
        FHitResult Hit;
        if (!FindGround(GetWorld(),Wheel,this,Hit)) return false;
        H[I]=Hit.ImpactPoint.Z;
    }
    return true;
}

void ARiverfrontCar::CycleCamera()
{
    CameraFrom=LastCameraLocation;
    RotationFrom=LastCameraRotation;
    ViewBlend=HasCameraSample?0.f:1.f;
    ViewMode=(ViewMode+1)%3;
    if (Controller) Controller->SetControlRotation(FRotator(ViewMode==0?-13.:-3.,GetActorRotation().Yaw,0));
}

void ARiverfrontCar::CalcCamera(float Dt,FMinimalViewInfo& View)
{
    Super::CalcCamera(Dt,View);
    if (ViewMode!=0 && Controller)
    {
        const FRotator Look=Controller->GetControlRotation(), Facing=GetActorRotation();
        View.Location=GetActorTransform().TransformPosition(ViewMode==1?
            FVector(-33,37,bCrossover?61:53):FVector(115,0,bCrossover?37:29));
        const double Relative=FMath::FindDeltaAngleDegrees(Facing.Yaw,Look.Yaw);
        View.Rotation=FRotator(FMath::Clamp(Look.Pitch>180?Look.Pitch-360:Look.Pitch,-42.,38.)+Facing.Pitch,
            Facing.Yaw+FMath::Clamp(Relative,-105.,105.),Facing.Roll*.42);
        View.FOV=ViewMode==1?83.f:76.f;
    }
    if (ViewBlend<1.f)
    {
        ViewBlend=FMath::Min(1.f,ViewBlend+FMath::Max(0.f,Dt)/.28f);
        const float A=FMath::SmoothStep(0.f,1.f,ViewBlend);
        View.Location=FMath::Lerp(CameraFrom,View.Location,A);
        View.Rotation=FQuat::Slerp(RotationFrom.Quaternion(),View.Rotation.Quaternion(),A).Rotator();
    }
    LastCameraLocation=View.Location;
    LastCameraRotation=View.Rotation;
    HasCameraSample=true;
}

void ARiverfrontCar::UpdateSignalLenses(float Dt)
{
    BlinkTime=FMath::Fmod(BlinkTime+Dt, .82f);
    for (int32 I=0;I<SignalLenses.Num();++I)
    {
        const int32 Side=I<3?-1:1;
        const bool Active=I%3==2?Speed<-18.f:((Signal==2 || Signal==Side) && BlinkTime<.41f);
        SignalLenses[I]->SetVisibility(IsValid(Driver) && Active);
    }
    const bool Stopping=IsValid(Driver) && (bBraking || Throttle*Speed<-10.f);
    if (Stopping!=bStopLamps)
    {
        bStopLamps=Stopping;
        UpdateDrivingLights();
    }
    if (TailLens)
        TailLens->SetScalarParameterValue(TEXT("Glow"),Stopping?6.5f:(bNightDriving?1.8f:0.f));
}

void ARiverfrontCar::SoundHorn()
{
    if (auto* Wave=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_Horn.S_Horn")))
        UGameplayStatics::PlaySoundAtLocation(this,Wave,GetActorLocation(),.8f,bCrossover?.91f:1.f);
}

void ARiverfrontCar::UpdateMotorAudio()
{
    if (!MotorLoop || !TyreLoop) return;
    MotorLoop->SetPitchMultiplier(FMath::Clamp(EngineRpm/940.f,.8f,4.3f));
    MotorLoop->SetVolumeMultiplier((.23f+.19f*FMath::Abs(Throttle))*(ViewMode==1?.76f:1.f));
    TyreLoop->SetVolumeMultiplier(FMath::Clamp(FMath::Abs(Speed)/2100.f,0.f,.55f)*(.50f+Wetness*.65f));
    TyreLoop->SetPitchMultiplier(.75f+FMath::Abs(Speed)/3100.f);
}

bool ARiverfrontCar::OpenSide(int32 Side)
{
    if (DoorHinges.Num()!=2 || GetSpeedKph()>2.f) return false;
    FCollisionQueryParams Test(SCENE_QUERY_STAT(SabarmatiDoorSwing),false,this);
    if (Driver) Test.AddIgnoredActor(Driver);
    if (auto* PC=GetWorld()->GetFirstPlayerController()) Test.AddIgnoredActor(PC->GetPawn());
    const auto T=GetActorTransform();
    FHitResult Obstacle;
    const bool Blocked=GetWorld()->SweepSingleByChannel(Obstacle,T.TransformPosition(FVector(12,Side*99,13)),
        T.TransformPosition(FVector(5,Side*162,13)),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(15),Test);
    if (Blocked) { SetHint(TEXT("The door needs more space.")); return false; }
    DoorSide=Side; TargetOpen=true; DoorHold=-1.f;
    return true;
}
void ARiverfrontCar::OperateDoor(const FVector& User)
{
    if (TargetOpen) { TargetOpen=false; DoorHold=0; }
    else OpenSide(GetActorTransform().InverseTransformPosition(User).Y<0?-1:1);
}
void ARiverfrontCar::CockpitDoor() { OperateDoor(GetActorTransform().TransformPosition(FVector(0,95,10))); }
void ARiverfrontCar::UpdateDoors(float Dt)
{
    if (DoorHinges.Num()!=2) return;
    if (DoorHold>0.f) { DoorHold-=Dt; if (DoorHold<=0.f) TargetOpen=false; }
    if (GetSpeedKph()>2.f) TargetOpen=false;
    DoorAngle=FMath::FInterpConstantTo(DoorAngle,TargetOpen?61.f:0.f,Dt,112.f);
    for (int32 I=0;I<2;++I)
    {
        const int32 Side=I==0?-1:1;
        DoorHinges[I]->SetRelativeRotation(FRotator(0,Side==DoorSide?-Side*DoorAngle:0,0));
    }
}
