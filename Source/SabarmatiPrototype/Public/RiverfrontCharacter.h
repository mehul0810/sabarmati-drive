#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RiverfrontResidentAppearance.h"
#include "RiverfrontCharacter.generated.h"

class UAnimMontage;
class UCurveVector;

USTRUCT()
struct FRiverfrontTraversalSection
{
    GENERATED_BODY()

    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    float From = 0.f;
    float To = 0.f;
    UPROPERTY(Transient) TObjectPtr<UCurveVector> Offset;
};

class UCameraComponent;
class USpringArmComponent;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class UAnimSequence;
class UBlendSpace1D;
class UMaterialInterface;
class URiverfrontAvatarMotion;
class URiverfrontLocalProfile;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ARiverfrontCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    FString GetWaterHint() const;
    void BeginSeatExit(const FVector& Seat,const FVector& Landing,const FVector& Out);
    void SetInCar(bool bInCar);
    void SetFittingPose(bool bFitting);
    bool LieOnBed(const FVector& Pillow);
    void LeaveBed();
    float GetBedTime() const { return BedTime; }
    bool SupportsClothingChange() const;
    void PoseChangingSleeves(float Lift,float PulloverWeight,float SleevePass=0.f);
    void EndChangingPose();
    bool IsLyingOnBed() const { return bLyingOnBed; }
    bool SupportsPublicHaircut() const;
    void SetCroppedHair(bool bCropped);
    void ApplyProfileAppearance(const URiverfrontLocalProfile& Profile);
    UFUNCTION(BlueprintPure, Category="Avatar")
    ERiverfrontGender GetAvatarGender() const { return AvatarGender; }
    void ApplyWardrobeLook(FName Outfit, FName Accessory, FName AppearanceTone = NAME_None);
    FName GetWardrobeOutfit() const { return WardrobeOutfit; }
    FName GetWardrobeAccessory() const { return WardrobeAccessory; }
    bool GetWardrobeShirtColor(FLinearColor& Out) const;
    bool HasWristwatchVisible() const;
    void ShowHeldWardrobeItem(FName Id);
    void PoseHeldWardrobeItem();
    bool HasHeldWardrobeProp(FName Id) const;
    USkeletalMeshComponent* GetAvatarMesh() const;
    float GetTraversalHandContact(bool bRight, FVector& OutPalm, FVector& OutElbow) const;

protected:
    // The subclass supplies its own skeletal components, materials and animation Blueprint.
    UPROPERTY(EditDefaultsOnly, Category="Avatar")
    bool bUseBlueprintAvatar = false;

    UPROPERTY(EditDefaultsOnly, Category="Avatar", meta=(EditCondition="bUseBlueprintAvatar"))
    bool bSupportsBlueprintHaircuts = false;

    // Tag the assembled body RiverfrontAvatarBody when it lives in a child actor.
    // An empty asset lets the Blueprint supply its own animation graph.
    UPROPERTY(EditDefaultsOnly, Category="Avatar", meta=(EditCondition="bUseBlueprintAvatar"))
    TObjectPtr<URiverfrontAvatarMotion> AuthoredAvatarMotion;

    // Delivered on spawn, profile restoration and temporary wardrobe previews.
    UFUNCTION(BlueprintImplementableEvent, Category="Avatar", meta=(DisplayName="Apply Avatar Appearance"))
    void ReceiveAvatarAppearance(FName Outfit, FName Accessory, FName Tone, bool bCropped);

    UFUNCTION(BlueprintImplementableEvent, Category="Avatar", meta=(DisplayName="Set Avatar Fitting Pose"))
    void ReceiveAvatarFittingPose(bool bFitting);

    // Child actor assemblies can mirror their parent's vehicle visibility here.
    UFUNCTION(BlueprintImplementableEvent, Category="Avatar", meta=(DisplayName="Set Avatar In Car"))
    void ReceiveAvatarInCar(bool bInCar);

    // Return true only if the traversal system actually started a move.
    UFUNCTION(BlueprintImplementableEvent, Category="Avatar", meta=(DisplayName="Try Avatar Traversal"))
    bool TryAvatarTraversal();

private:
    void UpdateSwimPose(float DeltaSeconds,bool InWater);
    FTransform DryMeshTransform=FTransform::Identity;
    float SwimPitch=0.f;
    bool bSwimPose=false;
    float ExitElapsed=-1.f;
    FVector ExitStart=FVector::ZeroVector, ExitLanding=FVector::ZeroVector, ExitDirection=FVector::ZeroVector;
    float Breath=16.f;
    bool bDrowned=false;
    bool bDiving=false;
    bool bBlueprintAvatarActive = false;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> AuthoredMotionBody;
    void BindAuthoredAvatarMotion(USkeletalMeshComponent* Body);
    void JumpOrTraverse();
    bool TryStreetTraversal();
    void StartTraversalSection();
    void UpdateTraversal();
    void FinishTraversal(bool bInterrupted);
    uint16 TraversalSource = 0;
    int32 TraversalSection = 0;
    float TraversalDuration = 0.f;
    UPROPERTY(Transient) TArray<FRiverfrontTraversalSection> TraversalSections;
    TWeakObjectPtr<UAnimMontage> TraversalMontage;
    bool bMantling = false;
    bool bOrientBeforeTraversal = true;
    FVector TraversalEdge = FVector::ZeroVector;
    FVector TraversalFacing = FVector::ForwardVector;
    UPROPERTY() TObjectPtr<UAnimSequence> FenceVault;
    UPROPERTY() TObjectPtr<UAnimSequence> WallMantle;

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
    TObjectPtr<UBlendSpace1D> LocomotionBlend;
    UPROPERTY()
    TArray<TObjectPtr<UAnimSequence>> AirClips;
    bool bWasAirborne = false;
    bool bPlayedFall = false;
    void StartResidentGait();
    void UpdateAirAnimation();
    UPROPERTY()
    TArray<TObjectPtr<UAnimSequence>> AnimationClips;
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> PreviewPieces;
    TArray<FColor> PreviewColors;
    UPROPERTY() TObjectPtr<class USoundWave> StoneStep;
    UPROPERTY() TObjectPtr<class USoundWave> GritStep;
    float StepTravel=0;
    int32 LastContactHalf = INDEX_NONE;
    int32 StepIndex=0;
    double GaitPhase = 0.0;
    bool bUsesImportedMesh = false;
    UPROPERTY(Transient) TObjectPtr<class UPoseableMeshComponent> ChangingMesh;
    bool bLyingOnBed=false;
    FTransform BeforeBed;
    FVector BedHead, HeadInMesh;
    TArray<int32,TInlineAllocator<4>> BedShoeLODs;
    float BedTime=0.f;
    void UpdateBedPose();
    bool PreparePosedMesh();
    bool bInFittingPose = false;
    bool bOrientBeforeFitting = true;
    UPROPERTY() TObjectPtr<USkeletalMesh> DefaultAvatarMesh;
    ERiverfrontGender AvatarGender = ERiverfrontGender::Male;
    UPROPERTY() TObjectPtr<UMaterialInterface> CottonMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> TailoredCloth;
    UPROPERTY() TObjectPtr<UMaterialInterface> EyeMaterial;
    UPROPERTY() TObjectPtr<UStaticMesh> SweptHaircut;
    UPROPERTY() TObjectPtr<UStaticMesh> CroppedHaircut;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Hair;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Wristwatch;
    FName WardrobeOutfit = TEXT("cotton-sand");
    FName WardrobeAccessory;
    FName WardrobeTone;
    UPROPERTY(Transient) TObjectPtr<class UPrimitiveComponent> HeldProp;
    FName HeldPropItem;
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
