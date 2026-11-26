#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RiverfrontCar.generated.h"

class ARiverfrontCharacter;
class ARiverfrontRoadAgent;
class UBoxComponent;
class UPoseableMeshComponent;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class USpotLightComponent;
class USkeletalMeshComponent;
class UAudioComponent;
class USoundWave;
class UMaterialInstanceDynamic;
class USpringArmComponent;
class UMaterialInterface;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontCar : public APawn
{
    GENERATED_BODY()

public:
    ARiverfrontCar();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

    void UseCrossoverBody() { bCrossover = true; }
    static FVector GetCrossoverBounds() { return FVector(229, 99, 53); }
    const TCHAR* GetVehicleLabel() const;
    void UseTrafficBody(ARiverfrontRoadAgent& Agent);
    FVector GetCollisionExtents() const;
    FVector GetTravelVelocity() const;

    virtual void NotifyHit(UPrimitiveComponent* MyComp,AActor* Other,UPrimitiveComponent* OtherComp,
        bool bSelfMoved,FVector HitLocation,FVector HitNormal,FVector NormalImpulse,const FHitResult& Hit) override;
    void SetNightDriving(bool AtNight);
    void OperateDoor(const FVector& User);
    bool IsDoorOpen() const { return DoorAngle>10.f; }
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& View) override;
    void SetRoadWetness(float Amount) { Wetness = FMath::Clamp(Amount, 0.f, 1.f); }
    int32 GetGear() const { return Speed < -15.f ? -1 : Gear; }
    int32 GetViewMode() const { return ViewMode; }
    float GetEngineRpm() const { return EngineRpm; }
    bool HasFourContacts() const { return ContactCount == 4; }
    int32 GetSignal() const { return Signal; }
    bool TravelToStreet(const FVector& Position, double Heading);
    void ServiceVehicle() { Fuel=1.f; Condition=1.f; }
    float GetFuel() const { return Fuel; }
    float GetCondition() const { return Condition; }
    bool IsOccupied() const { return Driver.Get() != nullptr; }
    bool FindDriverExit(FVector& Point, int32& Side) const;
    bool FindDoorLanding(const ARiverfrontCharacter& Walker,FVector& Point,int32& Side) const;
    bool IsNearDoor(const ARiverfrontCharacter& Character) const;
    bool Enter(ARiverfrontCharacter& Character);
    float GetSpeedKph() const { return FMath::Abs(Speed) * 0.036f; }
    FString GetHint() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UChaosWheeledVehicleMovementComponent> VehicleMovement;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> PhysicsChassis;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USpringArmComponent> Arm;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> Camera;
    UPROPERTY()
    TObjectPtr<ARiverfrontCharacter> Driver;
    UPROPERTY() TObjectPtr<ARiverfrontRoadAgent> FleetBody;
    UPROPERTY() TObjectPtr<UPoseableMeshComponent> SeatedPlayer;
    void SeatPlayer(const ARiverfrontCharacter& Walker);
    void ClearSeat();
    UPROPERTY()
    TObjectPtr<UStaticMesh> BoxMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> SphereMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Panels;
    UPROPERTY()
    TArray<TObjectPtr<USceneComponent>> SteerPivots;
    UPROPERTY()
    TArray<TObjectPtr<USceneComponent>> WheelPivots;
    TArray<FColor> PanelColors;

    UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Headlights;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> DetailedBody;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> CockpitWheel;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeadLens;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TailLens;
    bool bNightDriving = false;
    void UpdateDrivingLights();
    void FitInstruments();
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Instruments;
    void CycleCamera();
    void CockpitDoor();
    void UpdateDoors(float Delta);
    bool OpenSide(int32 Side);
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> DoorHinges;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DoorSkins;
    float DoorAngle=0.f, DoorHold=0.f;
    int32 DoorSide=1;
    bool TargetOpen=false;
    void SignalLeft() { Signal = Signal == -1 ? 0 : -1; }
    void SignalRight() { Signal = Signal == 1 ? 0 : 1; }
    void Hazard() { Signal = Signal == 2 ? 0 : 2; }
    void SoundHorn();
    void UpdateMotorAudio();
    UPROPERTY() TObjectPtr<UAudioComponent> MotorLoop;
    UPROPERTY() TObjectPtr<UAudioComponent> TyreLoop;
    UPROPERTY() TObjectPtr<USoundWave> ShiftSound;
    UPROPERTY() TObjectPtr<USoundWave> ImpactSound;
    float ClosingSpeed(const UPrimitiveComponent* Other,const FVector& Normal) const;
    FVector ChassisVelocityBeforeImpact=FVector::ZeroVector;
    bool HandleImpact(const FHitResult& Hit,float ImpactSpeed);
    bool bWrecked=false;
    FHitResult PhysicsImpact;
    float PhysicsImpactSpeed=0.f;
    float HitSoundDelay=0.f, Fuel=1.f, Condition=1.f;
    void UpdateSignalLenses(float DeltaTime);
    bool SampleWheels(const FVector& Centre, double Yaw, double (&Heights)[4], bool bRequireCityRoad = false) const;
    int32 Gear = 1, Signal = 0, ViewMode = 0, ContactCount = 0;
    float EngineRpm=850.f, Wetness=0.f, TravelYaw=0.f, BodyRoll=0.f, BlinkTime=0.f, LastSpeed=0.f;
    float ViewBlend=1.f;
    bool HasCameraSample=false, bStopLamps=false;
    FVector LastCameraLocation=FVector::ZeroVector, CameraFrom=FVector::ZeroVector;
    FRotator LastCameraRotation=FRotator::ZeroRotator, RotationFrom=FRotator::ZeroRotator;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SignalLenses;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TurnMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ReverseMaterial;
    FTransform Home;
    double GroundHeight = 68.;
    float Speed = 0.f;
    float Throttle = 0.f;
    float Steering = 0.f;
    float WheelAngle = 0.f;
    float TurnAngle = 0.f;
    float HintRemaining = 0.f;
    bool bBraking = false;
    bool bDetailedVisuals = false;
    bool bCrossover = false;
    bool bChaosDrive = false;
    float AppliedTyreFriction = 1.f;
    FString Hint;

    bool InstallDetailedVisuals();
    void ConfigureHatchbackPhysics();
    void TickHatchbackPhysics(float DeltaSeconds);
    void UpdateHatchbackWheels();
    void StopHatchbackPhysics(bool bPark);
    void Panel(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent,
        const FVector& Offset, const FVector& Size, FColor Color,
        const FRotator& Rotation = FRotator::ZeroRotator);
    void SetThrottle(float Axis) { Throttle = Axis; }
    void SetSteering(float Axis) { Steering = Axis; }
    void Brake() { bBraking = true; }
    void ReleaseBrake() { bBraking = false; }
    void Exit();
    void ResetCar();
    void ToggleStats();
    void SetHint(const TCHAR* Message);
};
