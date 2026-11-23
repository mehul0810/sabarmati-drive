#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RiverfrontCar.generated.h"

class ARiverfrontCharacter;
class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UMaterialInterface;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontCar : public APawn
{
    GENERATED_BODY()

public:
    ARiverfrontCar();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

    void UseCrossoverBody() { bCrossover = true; }
    static FVector GetCrossoverBounds() { return FVector(229, 99, 53); }
    const TCHAR* GetVehicleLabel() const { return bCrossover ? TEXT("Crossover") : TEXT("Hatchback"); }

    bool IsNearDoor(const ARiverfrontCharacter& Character) const;
    void Enter(ARiverfrontCharacter& Character);
    float GetSpeedKph() const { return FMath::Abs(Speed) * 0.036f; }
    FString GetHint() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USpringArmComponent> Arm;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> Camera;
    UPROPERTY()
    TObjectPtr<ARiverfrontCharacter> Driver;
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

    FTransform Home;
    float Speed = 0.f;
    float Throttle = 0.f;
    float Steering = 0.f;
    float WheelAngle = 0.f;
    float TurnAngle = 0.f;
    float HintRemaining = 0.f;
    bool bBraking = false;
    bool bDetailedVisuals = false;
    bool bCrossover = false;
    FString Hint;

    bool InstallDetailedVisuals();
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
