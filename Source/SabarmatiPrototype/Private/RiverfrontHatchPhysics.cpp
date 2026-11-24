#include "RiverfrontCar.h"
#include "RiverfrontHatchWheel.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/RichCurve.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundWave.h"

URiverfrontHatchWheel::URiverfrontHatchWheel()
{
    WheelRadius = 31.6f;
    WheelWidth = 19.5f;
    AxleType = EAxleType::Front;
    bAffectedBySteering = true;
    bAffectedByEngine = true;
    bAffectedByBrake = true;
    bAffectedByHandbrake = false;
    MaxSteerAngle = 33.f;
    SpringRate = 330.f;
    SpringPreload = 112.f;
    SuspensionMaxRaise = 15.f;
    SuspensionMaxDrop = 13.f;
    SuspensionDampingRatio = .63f;
    FrictionForceMultiplier = 1.04f;
    MaxBrakeTorque = 1400.f;
}

URiverfrontHatchRearWheel::URiverfrontHatchRearWheel()
{
    AxleType = EAxleType::Rear;
    bAffectedBySteering = false;
    bAffectedByEngine = false;
    bAffectedByHandbrake = true;
    MaxBrakeTorque = 1000.f;
}

void ARiverfrontCar::ConfigureHatchbackPhysics()
{
    const TCHAR* ChassisPackage = TEXT("/Game/Sabarmati/Riverfront/Vehicles/Physics/SK_HatchPhysics");
    if (!FPackageName::DoesPackageExist(ChassisPackage)) return;
    auto* Mesh = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/Vehicles/Physics/SK_HatchPhysics.SK_HatchPhysics"));
    if (!Mesh || !Mesh->GetPhysicsAsset()) return;
    const FName Names[] = {TEXT("FrontLeft"), TEXT("RearLeft"), TEXT("FrontRight"), TEXT("RearRight")};
    for (const FName Axle : Names)
        if (Mesh->GetRefSkeleton().FindBoneIndex(Axle) == INDEX_NONE) return;

    auto& Motion = *VehicleMovement;
    Motion.Mass = 1210.f;
    Motion.bEnableCenterOfMassOverride = true;
    Motion.CenterOfMassOverride = FVector(15, 0, -10);
    Motion.bReverseAsBrake = false;
    Motion.DifferentialSetup.DifferentialType = EVehicleDifferential::FrontWheelDrive;
    Motion.EngineSetup.MaxTorque = 185.f;
    Motion.EngineSetup.MaxRPM = 5800.f;
    Motion.EngineSetup.EngineBrakeEffect = .075f;
    FRichCurve& Torque = *Motion.EngineSetup.TorqueCurve.GetRichCurve();
    Torque.AddKey(0.f, .48f);
    Torque.AddKey(1200.f, .75f);
    Torque.AddKey(2850.f, 1.f);
    Torque.AddKey(4600.f, .89f);
    Torque.AddKey(5800.f, .62f);
    Motion.TransmissionSetup.bUseAutoReverse = false;
    Motion.TransmissionSetup.ForwardGearRatios = {3.76f, 2.18f, 1.48f, 1.08f, .84f, .69f};
    Motion.TransmissionSetup.ReverseGearRatios = {3.4f};
    Motion.TransmissionSetup.FinalRatio = 3.78f;
    Motion.TransmissionSetup.ChangeUpRPM = 3900.f;
    Motion.TransmissionSetup.ChangeDownRPM = 1500.f;
    Motion.TransmissionSetup.GearChangeTime = .24f;
    for (int32 Index=0; Index<WheelPivots.Num(); ++Index)
    {
        auto& Wheel = Motion.WheelSetups.AddDefaulted_GetRef();
        Wheel.WheelClass = Index%2 ? URiverfrontHatchRearWheel::StaticClass() : URiverfrontHatchWheel::StaticClass();
        Wheel.BoneName = Names[Index];
    }
    Motion.SetWheelTraceAllChannels(ECR_Ignore);
    Motion.SetWheelTraceResponseToChannel(ECC_WorldStatic, ECR_Block);
    Motion.SetWheelTraceResponseToChannel(ECC_WorldDynamic, ECR_Block);
    const FTransform Placement = GetActorTransform();
    PhysicsChassis->SetSkeletalMesh(Mesh);
    PhysicsChassis->SetCollisionProfileName(TEXT("Vehicle"));
    PhysicsChassis->SetWorldTransform(Placement);
    SetRootComponent(PhysicsChassis);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->AttachToComponent(PhysicsChassis, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    PhysicsChassis->RegisterComponent();
    PhysicsChassis->SetSimulatePhysics(true);
    Motion.SetUpdatedComponent(PhysicsChassis);
    Motion.RegisterComponent();
    bChaosDrive = Motion.HasValidPhysicsState();
    if (!bChaosDrive)
    {
        Motion.UnregisterComponent();
        PhysicsChassis->SetSimulatePhysics(false);
        Body->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        SetRootComponent(Body);
        Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        PhysicsChassis->UnregisterComponent();
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati hatchback: Chaos vehicle did not initialize."));
        return;
    }
    Motion.Activate();
    Motion.SetParked(true);
    FVector Front = Mesh->GetRefSkeleton().GetBoneAbsoluteTransform(Mesh->GetRefSkeleton().FindBoneIndex(Names[0])).GetLocation();
    FHitResult Floor;
    const FVector FrontWorld = Placement.TransformPosition(Front);
    const bool bRoad = GetWorld()->LineTraceSingleByChannel(Floor,FrontWorld+FVector(0,0,15),FrontWorld-FVector(0,0,55),
        ECC_WorldDynamic, FCollisionQueryParams(SCENE_QUERY_STAT(RiverfrontFrontWheel), false, this));
    UE_LOG(LogTemp, Display, TEXT("Sabarmati hatchback: Chaos front-wheel drive, four independent suspensions. Front axle %s, terrain probe=%d, chassis %s."),
        *Front.ToCompactString(),bRoad,*GetActorLocation().ToCompactString());
}

void ARiverfrontCar::StopHatchbackPhysics(bool bPark)
{
    if (!bChaosDrive) return;
    PhysicsChassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
    PhysicsChassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    VehicleMovement->SetThrottleInput(0.f);
    VehicleMovement->SetSteeringInput(0.f);
    VehicleMovement->SetBrakeInput(0.f);
    VehicleMovement->SetParked(bPark);
    VehicleMovement->SetTargetGear(1, true);
}

void ARiverfrontCar::UpdateHatchbackWheels()
{
    auto& Motion = *VehicleMovement;
    if (!Motion.HasValidPhysicsState()) return;
    const float Friction = FMath::Lerp(1.04f,.73f,Wetness);
    const bool bFrictionChanged = !FMath::IsNearlyEqual(Friction, AppliedTyreFriction, .006f);
    ContactCount = 0;
    TurnAngle = 0.f;
    for (int32 Index=0; Index<Motion.Wheels.Num(); ++Index)
    {
        const auto* Tyre = Motion.Wheels[Index].Get();
        USceneComponent* Wheel = WheelPivots[Index];
        USceneComponent* Pivot = Wheel->GetAttachParent();
        const FVector Rest = PhysicsChassis->GetBoneTransform(PhysicsChassis->GetBoneIndex(Motion.WheelSetups[Index].BoneName), FTransform::Identity).GetLocation();
        Pivot->SetRelativeLocation(Rest + FVector(0,0,Tyre->GetSuspensionOffset()));
        Pivot->SetRelativeRotation(FRotator(0, Tyre->GetSteerAngle(), 0));
        Wheel->SetRelativeRotation(FRotator(Tyre->GetRotationAngle(), 0, 0));
        ContactCount += Motion.GetWheelState(Index).bInContact ? 1 : 0;
        TurnAngle += (Index%2==0 ? .5f * Tyre->GetSteerAngle() : 0.f);
        if (bFrictionChanged) Motion.SetWheelFrictionMultiplier(Index, Friction);
    }
    if (bFrictionChanged) AppliedTyreFriction = Friction;
}

void ARiverfrontCar::TickHatchbackPhysics(float DeltaSeconds)
{
    auto& Motion = *VehicleMovement;
    const float PreviousSpeed = Speed;
    Speed = Motion.GetForwardSpeed();
    const int32 PreviousGear = Gear;
    Gear = Motion.GetCurrentGear();
    EngineRpm = FMath::Max(850.f, Motion.GetEngineRotationSpeed());
    const bool bReverse = Throttle < -.05f;
    const bool bOpposing = Throttle*Speed < -32.f;
    const int32 TargetGear = Motion.GetTargetGear();
    if (!bOpposing && FMath::Abs(Throttle) > .05f && (bReverse ? TargetGear>=0 : TargetGear<=0))
        Motion.SetTargetGear(bReverse ? -1 : 1, true);
    const float Limit = bReverse ? 460.f : 2660.f;
    const float Limiter = FMath::Clamp((Limit-FMath::Abs(Speed))/140.f, 0.f, 1.f);
    Motion.SetThrottleInput(bBraking || bOpposing || Fuel<=0.f ? 0.f
        : FMath::Abs(Throttle)*Limiter*FMath::Lerp(.54f,1.f,Condition));
    Motion.SetBrakeInput(bBraking || bOpposing ? 1.f : 0.f);
    Motion.SetSteeringInput(Steering);

    const double Yaw = GetActorRotation().Yaw;
    FRotator Look = Controller->GetControlRotation();
    Look.Yaw += FMath::FindDeltaAngleDegrees(double(TravelYaw), Yaw);
    Controller->SetControlRotation(Look);
    TravelYaw = Yaw;

    Fuel = FMath::Max(0.f,Fuel-FMath::Abs(Speed)*DeltaSeconds/13000000.f);
    if (Gear!=PreviousGear && ShiftSound)
        UGameplayStatics::PlaySoundAtLocation(this,ShiftSound,GetActorLocation(),.27f,1.f);
    HitSoundDelay = FMath::Max(0.f, HitSoundDelay-DeltaSeconds);
    const float SpeedLost = FMath::Abs(PreviousSpeed)-FMath::Abs(Speed);
    if (SpeedLost>155.f && !bBraking && !bOpposing && HitSoundDelay<=0.f && ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this,ImpactSound,GetActorLocation(),FMath::Clamp(SpeedLost/1700.f,.17f,.8f));
        Condition = FMath::Max(.12f,Condition-FMath::Clamp(SpeedLost/12500.f,.012f,.19f));
        HitSoundDelay = .45f;
    }
    UpdateMotorAudio();
    UpdateSignalLenses(DeltaSeconds);
}
