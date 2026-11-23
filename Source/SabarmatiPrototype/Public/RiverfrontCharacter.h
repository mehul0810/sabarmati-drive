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
    void SetFittingPose(bool bFitting);
    bool SupportsPublicHaircut() const;
    void SetCroppedHair(bool bCropped);
    void ApplyWardrobeLook(FName Outfit, FName Accessory, FName AppearanceTone = NAME_None);
    FName GetWardrobeOutfit() const { return WardrobeOutfit; }
    FName GetWardrobeAccessory() const { return WardrobeAccessory; }
    bool GetWardrobeShirtColor(FLinearColor& Out) const;
    bool HasWristwatchVisible() const;

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
    UPROPERTY() TObjectPtr<class USoundWave> StoneStep;
    UPROPERTY() TObjectPtr<class USoundWave> GritStep;
    float StepTravel=0;
    int32 StepIndex=0;
    double GaitPhase = 0.0;
    bool bUsesImportedMesh = false;
    bool bInFittingPose = false;
    bool bOrientBeforeFitting = true;
    UPROPERTY() TObjectPtr<USkeletalMesh> DefaultAvatarMesh;
    UPROPERTY() TObjectPtr<USkeletalMesh> KurtaMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> CottonMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> FaceMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> TailoredCloth;
    UPROPERTY() TObjectPtr<UMaterialInterface> EyeMaterial;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Hair;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Wristwatch;
    FName WardrobeOutfit = TEXT("cotton-sand");
    FName WardrobeAccessory;
    FName WardrobeTone;
    void UseWardrobeMesh(USkeletalMesh* Model, bool bKurta);
    void UpdateWatchAttachment();
    bool bCroppedHair = false;
    void UpdateHair();

    void MakePiece(const TCHAR* Name, USceneComponent* Parent, const FVector& Offset,
        const FVector& Size, FColor Tone);
    void MoveForward(float Amount);
    void MoveSide(float Amount);
    void Sprint();
    void StopSprint();
    void EnterCar();
    void OpenCarDoor();
    void ResetWalk();
    void ToggleStats();
};
