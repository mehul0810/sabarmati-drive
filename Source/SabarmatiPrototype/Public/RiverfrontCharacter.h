#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RiverfrontCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class UAnimSequence;
class UMaterialInterface;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ARiverfrontCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    void SetInCar(bool bInCar);

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USpringArmComponent> Arm;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> Camera;
    UPROPERTY()
    TObjectPtr<USceneComponent> PreviewRig;
    UPROPERTY()
    TObjectPtr<USceneComponent> LeftLeg;
    UPROPERTY()
    TObjectPtr<USceneComponent> RightLeg;
    UPROPERTY()
    TObjectPtr<USceneComponent> LeftArm;
    UPROPERTY()
    TObjectPtr<USceneComponent> RightArm;
    UPROPERTY()
    TObjectPtr<UStaticMesh> PreviewSphere;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY()
    TObjectPtr<UAnimSequence> ActiveSequence;
    UPROPERTY()
    TArray<TObjectPtr<UAnimSequence>> AnimationClips;
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> PreviewPieces;
    TArray<FColor> PreviewColors;
    double GaitPhase = 0.0;
    bool bUsesImportedMesh = false;

    void MakePiece(const TCHAR* Name, USceneComponent* Parent, const FVector& Offset,
        const FVector& Size, FColor Tone);
    void MoveForward(float Amount);
    void MoveSide(float Amount);
    void Sprint();
    void StopSprint();
    void EnterCar();
    void ResetWalk();
    void ToggleStats();
};
