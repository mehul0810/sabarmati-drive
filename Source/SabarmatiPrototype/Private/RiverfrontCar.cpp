#include "RiverfrontCar.h"
#include "Components/SpotLightComponent.h"
#include "EngineUtils.h"
#include "RiverfrontWorld.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
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
        && FMath::Abs(Floor.ImpactPoint.Z) < 15.0;
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
    DetailedBody = PlaceMesh(Chassis, Body, FVector(0, 0, ModelGroundOffset), -90.0);
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
    if (On && !HeadLens)
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
            DetailedBody->SetMaterial(I, On ? TailLens.Get() : nullptr);
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
    Driver = &Character;
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
        ARiverfrontCharacter* Walker = Driver.Get();
        Walker->SetActorLocationAndRotation(Candidate, FRotator(0, GetActorRotation().Yaw, 0),
            false, nullptr, ETeleportType::TeleportPhysics);
        Walker->SetInCar(false);
        Player->Possess(Walker);
        Player->SetControlRotation(FRotator(-7, GetActorRotation().Yaw, 0));
        Driver = nullptr;
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

    if (!Controller || !IsValid(Driver))
    {
        return;
    }
    const float Time = FMath::Min(DeltaSeconds, .05f);
    if (bBraking)
    {
        Speed = FMath::FInterpConstantTo(Speed, 0.f, Time, 2460.f);
    }
    else if (!FMath::IsNearlyZero(Throttle))
    {
        const bool bChangingDirection = Speed * Throttle < 0;
        const float Target = bChangingDirection ? 0 : Throttle > 0 ? TopSpeed : -ReverseLimit;
        Speed = FMath::FInterpConstantTo(Speed, Target, Time, bChangingDirection ? 1500.f : 520.f);
    }
    else
    {
        Speed = FMath::FInterpConstantTo(Speed, 0.f, Time, 230.f);
    }
    TurnAngle = FMath::FInterpTo(TurnAngle, Steering * 27.f, Time, 9.f);

    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FMath::Abs(Speed) * Time / 24.f));
    const double StepTime = Time / Steps;
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        const FRotator Previous = GetActorRotation();
        FRotator Facing = Previous;
        const double Turn = FMath::Clamp(Speed / 420.0, -1.0, 1.0) * (TurnAngle / 27.0) * 65.0 * StepTime;
        Facing.Yaw += Turn;

        const FVector Here = GetActorLocation();
        const FVector Desired = Here + Facing.Vector() * (Speed * StepTime);
        FHitResult Floor;
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(RiverfrontCarTurn), false, this);
        FVector Adjusted = Desired;
        if (!FindGround(GetWorld(), Desired, this, Floor))
        {
            Speed = 0.f;
            break;
        }
        Adjusted.Z = FMath::Max(68.0, Floor.ImpactPoint.Z + 63.0);
        if (GetWorld()->OverlapBlockingTestByProfile(Adjusted, Facing.Quaternion(), Body->GetCollisionProfileName(),
            FCollisionShape::MakeBox(Body->GetScaledBoxExtent()), Query))
        {
            Speed = 0.f;
            break;
        }
        FHitResult Hit;
        SetActorLocation(Adjusted, true, &Hit);
        if (Hit.bBlockingHit)
        {
            Speed = 0.f;
            break;
        }

        SetActorRotation(Facing);
        FRotator Look = Controller->GetControlRotation();
        Look.Yaw += Turn;
        Controller->SetControlRotation(Look);
        WheelAngle = FMath::Fmod(WheelAngle - FMath::RadiansToDegrees(
            static_cast<float>(FVector::Dist2D(Here, GetActorLocation()) / (bDetailedVisuals ? DetailedWheelRadius : WheelRadius))) * FMath::Sign(Speed), 360.f);
    }

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
        bBraking = false;
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
