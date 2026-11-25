#include "RiverfrontCharacter.h"
#include "Components/PoseableMeshComponent.h"
#include "SabarmatiDistrict.h"
#include "RiverfrontAvatarMotion.h"
#include "RiverfrontLocalProfile.h"
#include "RiverfrontLocomotionInstance.h"
#include "RiverfrontResidentOrientation.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"

#include "Algo/AllOf.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace1D.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RiverfrontCar.h"
#include "RiverfrontTraffic.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontPlayerController.h"
#include "RiverfrontWorld.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float WalkSpeed = 145.f;
constexpr float SprintSpeed = 335.f;
const TCHAR* AvatarRoot = TEXT("/Game/Sabarmati/Avatar/");

USkeletalMesh* LoadPlayerMesh()
{
    const bool bPrivate = FParse::Param(FCommandLine::Get(), TEXT("SabarmatiPrivateAvatar"));
    const bool bForcePublic = FParse::Param(FCommandLine::Get(), TEXT("SabarmatiPublicAvatar"));
    if (bPrivate && !bForcePublic && FPackageName::DoesPackageExist(FString(AvatarRoot) + TEXT("SK_Sabarmati")))
    {
        if (auto* Local = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Sabarmati/Avatar/SK_Sabarmati.SK_Sabarmati")))
            return Local;
    }

    if (auto* Everyday = RiverfrontResident::LoadOutfitMesh(ERiverfrontGender::Male, false))
        return Everyday;
    return LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Sabarmati/Riverfront/People/SK_RiverResident.SK_RiverResident"));
}
}

ARiverfrontCharacter::ARiverfrontCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(33.f, 92.f);
    bUseControllerRotationYaw = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0, 585, 0);
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->MaxAcceleration = 850.f;
    Movement->BrakingDecelerationWalking = 900.f;
    Movement->JumpZVelocity = 440.f;
    Movement->GetNavAgentPropertiesRef().bCanSwim=true;
    Movement->MaxSwimSpeed=190.f;
    Movement->Buoyancy=1.f;
    Movement->AirControl = .36f;

    Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("OrbitArm"));
    Arm->SetupAttachment(GetRootComponent());
    Arm->TargetArmLength = 440.f;
    Arm->TargetOffset = FVector(0, 0, 54);
    Arm->SocketOffset = FVector(0, 38, 9);
    Arm->bUsePawnControlRotation = true;
    Arm->bEnableCameraLag = true;
    Arm->CameraLagSpeed = 13.f;
    Arm->CameraLagMaxDistance = 70.f;
    Arm->ProbeSize = 11.f;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("OrbitCamera"));
    Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
    Camera->SetFieldOfView(77.f);

    GetMesh()->SetRelativeLocation(FVector(0, 0, -92));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hair = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SweptHair"));
    Hair->SetupAttachment(GetMesh());
    Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hair->SetGenerateOverlapEvents(false);
    Hair->SetCanEverAffectNavigation(false);
    Hair->bUseAttachParentBound = true;
    Hair->SetVisibility(false);
    Wristwatch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wristwatch"));
    Wristwatch->SetupAttachment(GetMesh());
    Wristwatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Wristwatch->SetGenerateOverlapEvents(false);
    Wristwatch->SetCanEverAffectNavigation(false);
    Wristwatch->SetVisibility(false);
    PreviewRig = CreateDefaultSubobject<USceneComponent>(TEXT("FallbackRig"));
    PreviewRig->SetupAttachment(GetRootComponent());
    PreviewRig->SetRelativeLocation(FVector(0, 0, -92));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    PreviewSphere = Sphere.Object;
    BaseMaterial = Material.Object;

    const FColor Shirt(46, 85, 91);
    const FColor Trousers(43, 48, 57);
    const FColor Skin(163, 112, 84);
    const FColor Shoe(202, 190, 164);
    MakePiece(TEXT("Torso"), PreviewRig, FVector(0, 0, 124), FVector(31, 47, 61), Shirt);
    MakePiece(TEXT("Hips"), PreviewRig, FVector(1, 0, 97), FVector(28, 38, 30), Trousers);
    MakePiece(TEXT("Neck"), PreviewRig, FVector(1, 0, 151), FVector(13, 13, 20), Skin);
    MakePiece(TEXT("Head"), PreviewRig, FVector(2, 0, 171), FVector(24, 24, 29), Skin);
    MakePiece(TEXT("Hair"), PreviewRig, FVector(-1, 0, 181), FVector(25, 24, 17), FColor(41, 32, 28));

    LeftLeg = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHip"));
    RightLeg = CreateDefaultSubobject<USceneComponent>(TEXT("RightHip"));
    LeftArm = CreateDefaultSubobject<USceneComponent>(TEXT("LeftShoulder"));
    RightArm = CreateDefaultSubobject<USceneComponent>(TEXT("RightShoulder"));
    const auto SetJoint = [this](USceneComponent* Joint, const FVector& Position)
    {
        Joint->SetupAttachment(PreviewRig);
        Joint->SetRelativeLocation(Position);
    };
    SetJoint(LeftLeg, FVector(0, -10, 95));
    SetJoint(RightLeg, FVector(0, 10, 95));
    SetJoint(LeftArm, FVector(0, -24, 141));
    SetJoint(RightArm, FVector(0, 24, 141));

    MakePiece(TEXT("LeftLegBody"), LeftLeg, FVector(0, 0, -37), FVector(18, 17, 77), Trousers);
    MakePiece(TEXT("RightLegBody"), RightLeg, FVector(0, 0, -37), FVector(18, 17, 77), Trousers);
    MakePiece(TEXT("LeftShoe"), LeftLeg, FVector(7, 0, -85), FVector(33, 15, 14), Shoe);
    MakePiece(TEXT("RightShoe"), RightLeg, FVector(7, 0, -85), FVector(33, 15, 14), Shoe);
    MakePiece(TEXT("LeftSleeve"), LeftArm, FVector(0, -4, -15), FVector(18, 18, 35), Shirt);
    MakePiece(TEXT("RightSleeve"), RightArm, FVector(0, 4, -15), FVector(18, 18, 35), Shirt);
    MakePiece(TEXT("LeftForearm"), LeftArm, FVector(0, -5, -43), FVector(12, 12, 30), Skin);
    MakePiece(TEXT("RightForearm"), RightArm, FVector(0, 5, -43), FVector(12, 12, 30), Skin);
}

void ARiverfrontCharacter::MakePiece(const TCHAR* Name, USceneComponent* Parent,
    const FVector& Offset, const FVector& Size, FColor Tone)
{
    UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
    Piece->SetupAttachment(Parent);
    Piece->SetStaticMesh(PreviewSphere.Get());
    Piece->SetRelativeLocation(Offset);
    Piece->SetRelativeScale3D(Size / 100.0);
    Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewPieces.Add(Piece);
    PreviewColors.Add(Tone);
}

void ARiverfrontCharacter::BeginPlay()
{
    Super::BeginPlay();
    StoneStep=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_FootStone.S_FootStone"));
    GritStep=LoadObject<USoundWave>(nullptr,TEXT("/Game/Sabarmati/Riverfront/Audio/S_FootGrit.S_FootGrit"));

    if (bUseBlueprintAvatar)
    {
        TInlineComponentArray<USkeletalMeshComponent*> Bodies;
        GetComponents(Bodies, true);
        bBlueprintAvatarActive = Bodies.ContainsByPredicate([](const USkeletalMeshComponent* Body)
        {
            return Body && Body->GetSkeletalMeshAsset();
        });
        if (bBlueprintAvatarActive)
        {
            PreviewRig->SetVisibility(false, true);
            PreviewRig->SetHiddenInGame(true, true);
            if (AuthoredAvatarMotion)
            {
                auto* TaggedBody = Bodies.FindByPredicate([](const USkeletalMeshComponent* Body)
                {
                    return Body && Body->ComponentHasTag(TEXT("RiverfrontAvatarBody"));
                });
                BindAuthoredAvatarMotion(TaggedBody ? *TaggedBody : GetMesh());
            }
            ApplyWardrobeLook(WardrobeOutfit, WardrobeAccessory, WardrobeTone);
            return;
        }
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati avatar %s has no assembled skeletal mesh; loading the riverfront character."),
            *GetClass()->GetPathName());
    }

    // A cooked Unreal Skeletal Mesh, not a runtime read of the raw GLB.
    if (USkeletalMesh* ImportedMesh = LoadPlayerMesh())
    {
        DefaultAvatarMesh = ImportedMesh;
        CottonMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_ResidentCotton.M_ResidentCotton"));
        const bool bPublic = ImportedMesh->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
        if (bPublic)
        {
            SweptHaircut = LoadObject<UStaticMesh>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Hair/SM_ResidentSweep.SM_ResidentSweep"));
            CroppedHaircut = LoadObject<UStaticMesh>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Hair/SM_ResidentCrop.SM_ResidentCrop"));
            EyeMaterial = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_RiverEyes.M_RiverEyes"));
            TailoredCloth = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_RiverTailoredCloth.M_RiverTailoredCloth"));
        }
        Wristwatch->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/SM_RiverWristwatch/StaticMeshes/SM_RiverWristwatch.SM_RiverWristwatch")));
        PreviewRig->SetVisibility(false, true);
        PreviewRig->SetHiddenInGame(true, true);
        bUsesImportedMesh = true;
        ApplyWardrobeLook(WardrobeOutfit, WardrobeAccessory, WardrobeTone);
        return;
    }

    if (BaseMaterial)
    {
        for (int32 Index = 0; Index < PreviewPieces.Num(); ++Index)
        {
            if (UMaterialInstanceDynamic* Material =
                UMaterialInstanceDynamic::Create(BaseMaterial, PreviewPieces[Index].Get()))
            {
                Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(PreviewColors[Index]));
                PreviewPieces[Index]->SetMaterial(0, Material);
            }
        }
    }
}

USkeletalMeshComponent* ARiverfrontCharacter::GetAvatarMesh() const
{
    return AuthoredMotionBody ? AuthoredMotionBody.Get() : GetMesh();
}

void ARiverfrontCharacter::BindAuthoredAvatarMotion(USkeletalMeshComponent* Body)
{
    const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
    const auto* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
    const auto* Motion = AuthoredAvatarMotion.Get();
    const UAnimSequence* Clips[] = {Motion->FittingIdle, Motion->Takeoff, Motion->Fall,
        Motion->Landing, Motion->FenceVault, Motion->WallMantle};
    if (!Skeleton || !Motion->Gait || Motion->Gait->GetSkeleton() != Skeleton ||
        !Algo::AllOf(Clips, [Skeleton](const UAnimSequence* Clip)
        {
            return Clip && Clip->GetSkeleton() == Skeleton && Clip->GetPlayLength() > 0.f;
        }))
    {
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati avatar: %s needs a compatible body and complete retargeted motion set."),
            *GetClass()->GetPathName());
        return;
    }

    AuthoredMotionBody = Body;
    LocomotionBlend = Motion->Gait;
    AirClips = {Motion->Takeoff, Motion->Fall, Motion->Landing};
    FenceVault = Motion->FenceVault;
    WallMantle = Motion->WallMantle;
    Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (Body->GetOwner() != this) Body->GetOwner()->SetActorEnableCollision(false);
    Wristwatch->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/SM_RiverWristwatch/StaticMeshes/SM_RiverWristwatch.SM_RiverWristwatch")));
    StartResidentGait();
    UE_LOG(LogTemp, Display, TEXT("Sabarmati avatar: native movement bound to %s with %s."),
        *Mesh->GetPathName(), *Motion->GetPathName());
}

void ARiverfrontCharacter::UseWardrobeMesh(USkeletalMesh* Model, bool bKurta)
{
    if (!Model || GetMesh()->GetSkeletalMeshAsset() == Model) return;

    FinishTraversal(true);
    GetMesh()->EmptyOverrideMaterials();
    GetMesh()->SetSkeletalMesh(Model);
    GetMesh()->SetRelativeRotation(FRotator(0, RiverfrontResident::YawToActorForward(*Model), 0));
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    ActiveSequence = nullptr;
    AnimationClips.Reset(3);
    const bool bPublic = Model->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
    const FString ClipFolder = bPublic ? (bKurta ? TEXT("/Game/Sabarmati/Riverfront/People/Kurta/")
        : TEXT("/Game/Sabarmati/Riverfront/People/")) : AvatarRoot;
    for (const TCHAR* Clip : {TEXT("A_Idle"), TEXT("A_Walk"), TEXT("A_Jog")})
    {
        const FString Package = ClipFolder + Clip;
        auto* Sequence = FPackageName::DoesPackageExist(Package)
            ? LoadObject<UAnimSequence>(nullptr, *(Package + TEXT(".") + Clip)) : nullptr;
        AnimationClips.Add(Sequence && Sequence->GetSkeleton() == Model->GetSkeleton() ? Sequence : nullptr);
    }
    const FString BlendPath = ClipFolder + TEXT("BS_ResidentGait");
    LocomotionBlend = bPublic && FPackageName::DoesPackageExist(BlendPath)
        ? LoadObject<UBlendSpace1D>(nullptr, *(BlendPath + TEXT(".BS_ResidentGait"))) : nullptr;
    if (LocomotionBlend && LocomotionBlend->GetSkeleton() != Model->GetSkeleton()) LocomotionBlend = nullptr;
    AirClips.Reset(3);
    FenceVault = WallMantle = nullptr;
    if (LocomotionBlend)
    {
        const FString AirFolder = TEXT("/Game/Sabarmati/Riverfront/People/Motion/") + FString(bKurta ? TEXT("Kurta/") : TEXT("Everyday/"));
        const auto LoadClip = [&](const TCHAR* Clip) -> UAnimSequence*
        {
            const FString Package = AirFolder + Clip;
            auto* Animation = FPackageName::DoesPackageExist(Package)
                ? LoadObject<UAnimSequence>(nullptr, *(Package + TEXT(".") + Clip)) : nullptr;
            return Animation && Animation->GetSkeleton() == Model->GetSkeleton() ? Animation : nullptr;
        };
        for (const TCHAR* Clip : {TEXT("MM_Jump"), TEXT("MM_Fall_Loop"), TEXT("MM_Land")})
        {
            if (auto* Animation = LoadClip(Clip))
                AirClips.Add(Animation);
            else
            {
                AirClips.Reset();
                break;
            }
        }
        FenceVault = LoadClip(TEXT("A_FenceVault"));
        WallMantle = LoadClip(TEXT("A_WallMantle"));
        StartResidentGait();
    }
    else if (AnimationClips[0])
    {
        ActiveSequence = AnimationClips[0];
        GetMesh()->PlayAnimation(ActiveSequence, true);
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati avatar: skeletal mesh loaded, %s; clips idle=%d walk=%d jog=%d."),
        *Model->GetName(), IsValid(AnimationClips[0].Get()), IsValid(AnimationClips[1].Get()), IsValid(AnimationClips[2].Get()));
}

void ARiverfrontCharacter::StartResidentGait()
{
    bWasAirborne = bPlayedFall = false;
    auto* Body = GetAvatarMesh();
    if (AirClips.Num() == 3 || FenceVault || WallMantle)
    {
        Body->SetAnimInstanceClass(URiverfrontLocomotionInstance::StaticClass());
        if (auto* Instance = Cast<URiverfrontLocomotionInstance>(Body->GetAnimInstance()))
        {
            Instance->SetGait(LocomotionBlend, this);
            return;
        }
    }
    Body->PlayAnimation(LocomotionBlend, true);
}

void ARiverfrontCharacter::UpdateAirAnimation()
{
    auto* Instance = Cast<URiverfrontLocomotionInstance>(GetAvatarMesh()->GetAnimInstance());
    if (!Instance || IsHidden() || TraversalSource || AirClips.Num() != 3) return;

    const bool bAirborne = GetCharacterMovement()->IsFalling();
    const double VerticalSpeed = GetVelocity().Z;
    if (bAirborne && !bWasAirborne && VerticalSpeed > 100.)
    {
        bPlayedFall = false;
        Instance->PlaySlotAnimationAsDynamicMontage(AirClips[0], TEXT("DefaultSlot"), .09f, .12f);
    }
    else if (bAirborne && VerticalSpeed < -105. && !bPlayedFall)
    {
        bPlayedFall = true;
        Instance->PlaySlotAnimationAsDynamicMontage(AirClips[1], TEXT("DefaultSlot"), .14f, .13f, 1.f, 64);
    }
    else if (!bAirborne && bWasAirborne && bPlayedFall)
    {
        Instance->PlaySlotAnimationAsDynamicMontage(AirClips[2], TEXT("DefaultSlot"), .075f, .20f);
        bPlayedFall = false;
    }
    bWasAirborne = bAirborne;
}

bool ARiverfrontCharacter::SupportsPublicHaircut() const
{
    if (bBlueprintAvatarActive) return bSupportsBlueprintHaircuts;
    return DefaultAvatarMesh && SweptHaircut && CroppedHaircut &&
        DefaultAvatarMesh->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
}

void ARiverfrontCharacter::SetCroppedHair(bool bCropped)
{
    const bool bChanged = bCroppedHair != bCropped;
    bCroppedHair = bCropped;
    if (bBlueprintAvatarActive)
    {
        if (bChanged) ReceiveAvatarAppearance(WardrobeOutfit, WardrobeAccessory, WardrobeTone, bCroppedHair);
        return;
    }
    UpdateHair();
}

void ARiverfrontCharacter::UpdateHair()
{
    // The original scalp is part of the skeletal material sections. Replace it
    // only when the complete new haircut can follow this public head bone.
    Hair->SetVisibility(false);
    const USkeletalMesh* Model = GetMesh()->GetSkeletalMeshAsset();
    if (!Model || !SupportsPublicHaircut()) return;
    const FReferenceSkeleton& Skeleton = Model->GetRefSkeleton();
    const int32 HeadIndex = Skeleton.FindBoneIndex(TEXT("Head"));
    if (HeadIndex != INDEX_NONE)
    {
        const FTransform Head = Skeleton.GetBoneAbsoluteTransform(HeadIndex);
        const FTransform MeshAxesAtHead(FQuat::Identity, Head.GetLocation());
        Hair->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Head"));
        Hair->SetRelativeTransform(MeshAxesAtHead.GetRelativeTransform(Head));
        Hair->SetStaticMesh(bCroppedHair ? CroppedHaircut.Get() : SweptHaircut.Get());
        const int32 SkinSlot = GetMesh()->GetMaterialIndex(TEXT("skin"));
        if (SkinSlot != INDEX_NONE)
            Hair->SetMaterialByName(TEXT("scalp"), GetMesh()->GetMaterial(SkinSlot));
        Hair->SetVisibility(true);
        const int32 HairSlot = GetMesh()->GetMaterialIndex(TEXT("hair"));
        if (HairSlot != INDEX_NONE)
            for (int32 LOD = 0; LOD < GetMesh()->GetNumLODs(); ++LOD)
                GetMesh()->ShowMaterialSection(HairSlot, INDEX_NONE, false, LOD);
    }
}

void ARiverfrontCharacter::ApplyProfileAppearance(const URiverfrontLocalProfile& Profile)
{
    AvatarGender = Profile.AvatarGender;
    bCroppedHair = Profile.bCroppedHair;
    ApplyWardrobeLook(Profile.EquippedOutfit, Profile.EquippedAccessory, Profile.AppearanceTone);
}

void ARiverfrontCharacter::ApplyWardrobeLook(FName Outfit, FName Accessory, FName Tone)
{
    EndChangingPose();
    WardrobeOutfit = Outfit.IsNone() ? FName(TEXT("cotton-sand")) : Outfit;
    WardrobeAccessory = Accessory;
    WardrobeTone = Tone;
    if (bBlueprintAvatarActive)
    {
        ReceiveAvatarAppearance(WardrobeOutfit, WardrobeAccessory, WardrobeTone, bCroppedHair);
        if (AuthoredMotionBody) UpdateWatchAttachment();
        return;
    }
    if (!DefaultAvatarMesh) return;

    const bool bBaseLayer = WardrobeOutfit == TEXT("fitting-base");
    const bool bIndigo = WardrobeOutfit == TEXT("shirt-indigo");
    const bool bKurta = WardrobeOutfit == TEXT("kurta");
    const bool bPublic = DefaultAvatarMesh->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
    USkeletalMesh* Model = DefaultAvatarMesh;
    bool bUsingKurtaRig = false;
    if (bPublic && (bKurta || AvatarGender == ERiverfrontGender::Female))
    {
        if (USkeletalMesh* Fitted = RiverfrontResident::LoadOutfitMesh(AvatarGender, bKurta))
        {
            Model = Fitted;
            bUsingKurtaRig = bKurta;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No fitted wardrobe mesh for avatar %d, outfit %s."),
                static_cast<int32>(AvatarGender), *WardrobeOutfit.ToString());
        }
    }
    UseWardrobeMesh(Model, bUsingKurtaRig);

    // The fabric is UV-mapped on the garment and moves with the animated body.
    const FColor Shirt = bBaseLayer ? FColor(214,214,202) : bIndigo ? FColor(60, 85, 132) : bKurta ? FColor(135, 63, 48) : FColor(202, 183, 150);
    const FColor Trim = bBaseLayer ? FColor(186,184,172) : bIndigo ? FColor(39, 54, 79) : bKurta ? FColor(98, 44, 42) : FColor(144, 125, 99);
    const FColor Trousers = bBaseLayer ? FColor(108,109,103) : bIndigo ? FColor(169, 153, 121) : bKurta ? FColor(209, 201, 178) : FColor(53, 62, 73);
    const FColor Skin = Tone == TEXT("umber") ? FColor(141, 91, 66) : Tone == TEXT("warm") ?
        FColor(205, 150, 107) : FColor(180, 133, 105);

    auto SetSurface = [this, Shirt, Trousers](FName SlotName, UMaterialInterface* Surface, const FLinearColor& Colour)
    {
        const int32 Index = GetMesh()->GetMaterialIndex(SlotName);
        if (Index == INDEX_NONE) return;
        UMaterialInterface* Parent = Surface ? Surface : GetMesh()->GetSkeletalMeshAsset()->GetMaterials()[Index].MaterialInterface.Get();
        if (!Parent) return;
        auto* MID = Cast<UMaterialInstanceDynamic>(GetMesh()->GetMaterial(Index));
        if (!MID || MID->Parent != Parent) MID = GetMesh()->CreateDynamicMaterialInstance(Index, Parent);
        if (!MID) return;
        if (TailoredCloth && Surface == TailoredCloth.Get())
        {
            MID->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(Shirt));
            MID->SetVectorParameterValue(TEXT("TrouserDye"), FLinearColor(Trousers));
        }
        else
        {
            MID->SetVectorParameterValue(TEXT("BaseColorFactor"), Colour);
        }
    };
    // The everyday shirt and jeans have their own complete clothing UV atlas and normal maps.
    // Passing a null override keeps the material assigned to each skeletal mesh section.
    UMaterialInterface* Cloth = nullptr;
    if (bBaseLayer || !bPublic || bUsingKurtaRig)
        Cloth = CottonMaterial.Get();
    else if (Model->GetFName() == TEXT("SK_RiverResident"))
        Cloth = TailoredCloth ? TailoredCloth.Get() : CottonMaterial.Get();
    SetSurface(TEXT("top_teal_woven"), Cloth, Shirt);
    SetSurface(TEXT("top_seams"), Cloth, Trim);
    SetSurface(TEXT("bottom_denim"), Cloth, Trousers);
    SetSurface(TEXT("scarf"), CottonMaterial, FColor(195, 149, 91));
    if (bPublic)
    {
        // The albedo already contains the resident's complexion and lip colour.
        const FLinearColor Complexion = Tone == TEXT("umber") ? FLinearColor(.72f, .65f, .62f)
            : Tone == TEXT("warm") ? FLinearColor(1.15f, 1.06f, .98f) : FLinearColor::White;
        UMaterialInterface* FaceMaterial = RiverfrontResident::LoadSkinMaterial(AvatarGender);
        SetSurface(TEXT("skin"), FaceMaterial, Complexion);
        SetSurface(TEXT("lips"), FaceMaterial, Complexion);
        SetSurface(TEXT("hair"), nullptr, FColor(34,25,21));
        const int32 HairSlot = GetMesh()->GetMaterialIndex(TEXT("hair"));
        if (HairSlot != INDEX_NONE)
            if (auto* Material = Cast<UMaterialInstanceDynamic>(GetMesh()->GetMaterial(HairSlot)))
                Material->SetScalarParameterValue(TEXT("RoughnessFactor"), .68f);
        if (EyeMaterial)
            for (FName Name : {FName(TEXT("iris")), FName(TEXT("sclera"))})
            {
                const int32 Index = GetMesh()->GetMaterialIndex(Name);
                if (Index != INDEX_NONE) GetMesh()->SetMaterial(Index, EyeMaterial);
            }
    }
    else
    {
        // Keep the optional local mesh, its original face material and its rig.
        const FName SkinSlot(TEXT("skin_golden_brown"));
        const int32 Index = GetMesh()->GetMaterialIndex(SkinSlot);
        if (Index != INDEX_NONE && Tone.IsNone())
            GetMesh()->SetMaterial(Index, GetMesh()->GetSkeletalMeshAsset()->GetMaterials()[Index].MaterialInterface);
        else
            SetSurface(SkinSlot, nullptr, Skin);
    }
    UpdateHair();
    UpdateWatchAttachment();
    if (bInFittingPose) SetFittingPose(true);
}

void ARiverfrontCharacter::EndChangingPose()
{
    if (!ChangingMesh) return;
    for (auto* Part : { Hair.Get(), Wristwatch.Get() })
        if (Part && Part->GetAttachParent()==ChangingMesh)
            Part->AttachToComponent(GetMesh(),FAttachmentTransformRules::KeepRelativeTransform,Part->GetAttachSocketName());
    GetMesh()->SetVisibility(true);
    ChangingMesh->DestroyComponent();
    ChangingMesh=nullptr;
}

bool ARiverfrontCharacter::SupportsClothingChange() const
{
    return !bBlueprintAvatarActive && DefaultAvatarMesh
        && DefaultAvatarMesh->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
}

void ARiverfrontCharacter::PoseChangingSleeves(float Lift)
{
    auto* Source=GetMesh();
    if (!bInFittingPose || bBlueprintAvatarActive || !Source->GetSkeletalMeshAsset()) return;
    if (!ChangingMesh)
    {
        if (!Source->IsVisible() || Source->GetBoneIndex(TEXT("Shoulderl"))==INDEX_NONE) return;
        ChangingMesh=NewObject<UPoseableMeshComponent>(this);
        AddInstanceComponent(ChangingMesh);
        ChangingMesh->SetupAttachment(Source);
        ChangingMesh->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
        ChangingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        for (int32 I=0;I<Source->GetNumMaterials();++I) ChangingMesh->SetMaterial(I,Source->GetMaterial(I));
        ChangingMesh->RegisterComponent();
        for (auto* Part : { Hair.Get(), Wristwatch.Get() })
            if (Part && Part->GetAttachParent()==Source)
                Part->AttachToComponent(ChangingMesh,FAttachmentTransformRules::KeepRelativeTransform,Part->GetAttachSocketName());
        Source->SetVisibility(false);
    }
    ChangingMesh->CopyPoseFromSkeletalComponent(Source);
    for (int32 Index=0;Index<2;++Index)
    {
        const FString Side=Index?TEXT("r"):TEXT("l");
        const double Sign=Index?1.:-1.;
        const auto Aim=[&](const TCHAR* Joint,const TCHAR* Next,const FVector& Direction)
        {
            const FName Bone(*(FString(Joint)+Side)), Child(*(FString(Next)+Side));
            auto Transform=ChangingMesh->GetBoneTransformByName(Bone,EBoneSpaces::ComponentSpace);
            const FVector From=(ChangingMesh->GetBoneLocationByName(Child,EBoneSpaces::ComponentSpace)-Transform.GetLocation()).GetSafeNormal();
            const FVector To=Source->GetRelativeRotation().UnrotateVector(Direction.GetSafeNormal());
            const float Reach=Index ? FMath::SmoothStep(.12f,1.f,Lift) : FMath::SmoothStep(0.f,.84f,Lift);
            const FQuat Delta=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(From,To),Reach);
            Transform.SetRotation(Delta*Transform.GetRotation());
            ChangingMesh->SetBoneTransformByName(Bone,Transform,EBoneSpaces::ComponentSpace);
        };
        Aim(TEXT("Shoulder"),TEXT("Elbow"),FVector(.38,.30*Sign,-.89));
        Aim(TEXT("Elbow"),TEXT("Hand"),FVector(.24,-.83*Sign,Index?.45:.72));
    }
    // The fitting room pauses the world; publish the pose on Slate's active timer.
    ChangingMesh->RefreshBoneTransforms();
    ChangingMesh->MarkRenderDynamicDataDirty();
}

bool ARiverfrontCharacter::LieOnBed(const FVector& Pillow)
{
    auto* Body=GetAvatarMesh();
    // An assembled Blueprint may have additional components outside the body hierarchy.
    if (bLyingOnBed || bBlueprintAvatarActive || !Body || !Body->IsVisible()
        || !Body->DoesSocketExist(TEXT("Head"))) return false;
    SetFittingPose(true);
    BeforeBed=Body->GetRelativeTransform();
    HeadInMesh=Body->GetSocketTransform(TEXT("Head"),RTS_Component).GetLocation();
    BedHead=Pillow;
    const int32 Shoes=Body->GetMaterialIndex(TEXT("shoe"));
    if (Shoes!=INDEX_NONE)
        for (int32 LOD=0;LOD<Body->GetSkinnedAsset()->GetLODNum();++LOD)
            if (Body->IsMaterialSectionShown(Shoes,LOD))
            {
                Body->ShowMaterialSection(Shoes,INDEX_NONE,false,LOD);
                BedShoeLODs.Add(LOD);
            }
    BedTime=0.f;
    bLyingOnBed=true;
    UpdateBedPose();
    return true;
}

void ARiverfrontCharacter::UpdateBedPose()
{
    auto* Body=GetAvatarMesh();
    // Feet point towards the footboard (+body Z runs towards the pillow).
    const float BreathingOffset=.38f*FMath::Sin(BedTime*1.6f);
    const FQuat BedAxes=FRotationMatrix::MakeFromXZ(FVector(-.88,0,.45),FVector::RightVector).ToQuat();
    const FQuat Rotation=BedAxes*BeforeBed.GetRotation();
    const FVector Pillow=BedHead+FVector(0,0,BreathingOffset);
    Body->SetWorldLocationAndRotation(Pillow-Rotation.RotateVector(HeadInMesh*Body->GetComponentScale()),Rotation);
}

void ARiverfrontCharacter::LeaveBed()
{
    if (!bLyingOnBed) return;
    bLyingOnBed=false;
    auto* Body=GetAvatarMesh();
    for (int32 LOD:BedShoeLODs) Body->ShowMaterialSection(Body->GetMaterialIndex(TEXT("shoe")),INDEX_NONE,true,LOD);
    BedShoeLODs.Reset();
    Body->SetRelativeTransform(BeforeBed);
    SetFittingPose(false);
}

void ARiverfrontCharacter::SetFittingPose(bool bFitting)
{
    auto* Movement = GetCharacterMovement();
    if (!bFitting)
    {
        EndChangingPose();
        if (!bInFittingPose) return;
        bInFittingPose = false;
        Movement->bOrientRotationToMovement = bOrientBeforeFitting;
        if (bBlueprintAvatarActive) ReceiveAvatarFittingPose(false);
        if (LocomotionBlend)
            StartResidentGait();
        else if (!bBlueprintAvatarActive)
            GetMesh()->SetPlayRate(1.f);
        return;
    }
    FinishTraversal(true);
    if (!bInFittingPose) bOrientBeforeFitting = Movement->bOrientRotationToMovement;
    bInFittingPose = true;
    Movement->StopMovementImmediately();
    ConsumeMovementInputVector();
    Movement->bOrientRotationToMovement = false;
    if (bBlueprintAvatarActive)
    {
        ReceiveAvatarFittingPose(true);
        if (!AuthoredMotionBody) return;
    }
    UAnimSequence* Idle = AuthoredMotionBody ? AuthoredAvatarMotion->FittingIdle.Get()
        : AnimationClips.IsValidIndex(0) ? AnimationClips[0].Get() : nullptr;
    if (!Idle) return;

    // Menus can pause the world before the next animation tick. Evaluate a
    // neutral stance now so the fitting view cannot inherit a frozen footstep.
    auto* Body = GetAvatarMesh();
    ActiveSequence = Idle;
    Body->PlayAnimation(ActiveSequence, true);
    Body->SetPosition(ActiveSequence->GetPlayLength() * .25f, false);
    Body->SetPlayRate(0.f);
    Body->TickAnimation(0.f, false);
    Body->RefreshBoneTransforms();
    Body->UpdateChildTransforms();
    Body->MarkRenderDynamicDataDirty();
}

void ARiverfrontCharacter::UpdateWatchAttachment()
{
    Wristwatch->SetVisibility(false);
    if (WardrobeAccessory != TEXT("watch") || !Wristwatch->GetStaticMesh()) return;
    auto* Body = GetAvatarMesh();
    const USkeletalMesh* Model = Body->GetSkeletalMeshAsset();
    if (!Model) return;
    const FReferenceSkeleton& Skeleton = Model->GetRefSkeleton();
    int32 Wrist = Skeleton.FindBoneIndex(TEXT("Handl"));
    if (Wrist == INDEX_NONE) Wrist = Skeleton.FindBoneIndex(TEXT("Hand_L"));
    if (Wrist == INDEX_NONE) return;
    const int32 Elbow = Skeleton.GetParentIndex(Wrist);
    if (Elbow == INDEX_NONE) return;
    // Derive the bracelet's rotation in the bone's local reference frame so it
    // stays around the wrist throughout locomotion on either supported rig.
    const FTransform Hand = Skeleton.GetBoneAbsoluteTransform(Wrist);
    const FVector Axis = (Hand.GetLocation() - Skeleton.GetBoneAbsoluteTransform(Elbow).GetLocation()).GetSafeNormal();
    const FVector Outside = Body->GetComponentTransform().InverseTransformVectorNoScale(-GetActorRightVector());
    const FTransform Fit(FRotationMatrix::MakeFromZX(Axis, Outside).ToQuat(), Hand.GetLocation()-Axis*2.7);
    Wristwatch->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Skeleton.GetBoneName(Wrist));
    Wristwatch->SetRelativeTransform(Fit.GetRelativeTransform(Hand));
    Wristwatch->SetVisibility(true);
}

bool ARiverfrontCharacter::GetWardrobeShirtColor(FLinearColor& Out) const
{
    const int32 Slot = GetMesh()->GetMaterialIndex(TEXT("top_teal_woven"));
    auto* MID = Slot != INDEX_NONE ? Cast<UMaterialInstanceDynamic>(GetMesh()->GetMaterial(Slot)) : nullptr;
    if (!MID) return false;
    Out = MID->K2_GetVectorParameterValue(TEXT("BaseColorFactor"));
    return true;
}

bool ARiverfrontCharacter::HasWristwatchVisible() const
{
    return Wristwatch->IsVisible() && Wristwatch->GetStaticMesh() && !Wristwatch->GetAttachSocketName().IsNone();
}

void ARiverfrontCharacter::UpdateSwimPose(float DeltaSeconds,bool InWater)
{
    if (!InWater && !bSwimPose) return;
    auto* Body=GetAvatarMesh();
    if (!Body || Body!=GetMesh()) return;
    if (!bSwimPose)
    {
        DryMeshTransform=Body->GetRelativeTransform();
        bSwimPose=true;
        if (auto* Anim=Body->GetAnimInstance()) Anim->Montage_Stop(.18f);
        bWasAirborne=bPlayedFall=false;
    }
    const float Cruise=FMath::Clamp(float(GetVelocity().Size2D()/145.),0.f,1.f);
    const float Target=InWater?FMath::Lerp(12.f,68.f,Cruise):0.f;
    SwimPitch=FMath::FInterpConstantTo(SwimPitch,Target,DeltaSeconds,120.f);
    const FQuat Lean(FVector::RightVector,FMath::DegreesToRadians(SwimPitch));
    // Rotate the swimmer about the hips rather than the mesh origin under the feet.
    const FVector Pivot(0,0,88.);
    Body->SetRelativeLocationAndRotation(DryMeshTransform.GetLocation()+Pivot-Lean.RotateVector(Pivot),
        Lean*DryMeshTransform.GetRotation());
    if (!InWater && SwimPitch<=0.f) { Body->SetRelativeTransform(DryMeshTransform); bSwimPose=false; }
}

void ARiverfrontCharacter::BeginSeatExit(const FVector& Seat,const FVector& Landing,const FVector& Out)
{
    Tags.Add(TEXT("DisplacedDriver"));
    ExitStart=Seat; ExitStart.Z=FMath::Max(Seat.Z,Landing.Z-28.);
    ExitLanding=Landing; ExitDirection=Out; ExitElapsed=0.f;
    SetActorEnableCollision(false);
    GetCharacterMovement()->bRunPhysicsWithNoController=true;
    GetCharacterMovement()->DisableMovement();
    SetActorLocationAndRotation(ExitStart,Out.Rotation());
    SetLifeSpan(24.f);
}

void ARiverfrontCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (ExitElapsed>=0.f)
    {
        ExitElapsed+=DeltaSeconds;
        const float T=FMath::Min(ExitElapsed/.38f,1.f);
        SetActorLocation(FMath::Lerp(ExitStart,ExitLanding,T)+FVector(0,0,39.*FMath::Sin(T*PI)));
        GetMesh()->SetRelativeRotation(FRotator(0,GetMesh()->GetRelativeRotation().Yaw,26.f*FMath::Sin(T*PI)));
        if (T>=1.f)
        {
            ExitElapsed=-1.f;
            SetActorEnableCollision(true);
            GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            LaunchCharacter(ExitDirection*230.+FVector(0,0,140.),true,true);
        }
        return;
    }
    if (bLyingOnBed) { BedTime+=DeltaSeconds; UpdateBedPose(); return; }
    if (bInFittingPose || bDrowned) return;
    const bool Swimming=IsPlayerControlled() && GetCharacterMovement()->IsSwimming();
    UpdateSwimPose(DeltaSeconds,Swimming);
    if (Swimming)
    {
        const double Depth=SabarmatiDistrict::SurfaceZ-GetActorLocation().Z;
        // Tread water with the face above the surface until the player holds Dive.
        const float Vertical=bDiving ? -1.f : FMath::Clamp(float((Depth-38.)/45.),-.7f,1.f);
        AddMovementInput(FVector::UpVector,Vertical);
        Breath=Depth>65. ? FMath::Max(0.f,Breath-DeltaSeconds) : FMath::Min(16.f,Breath+3.f*DeltaSeconds);
        if (Breath<=0.f)
        {
            bDrowned=true;
            if (auto* PC=Cast<ARiverfrontPlayerController>(Controller)) PC->Drowned();
        }
        return;
    }
    Breath=FMath::Min(16.f,Breath+3.f*DeltaSeconds);
    UpdateTraversal();
    const float FlatSpeed = GetVelocity().Size2D();
    const auto* Body = GetAvatarMesh();
    UpdateAirAnimation();
    if (!IsHidden() && FlatSpeed > 12.f && GetCharacterMovement()->IsMovingOnGround())
    {
        bool bContact = false;
        FVector Foot = GetActorLocation() - FVector(0,0,87);
        const auto* Resident = Cast<URiverfrontLocomotionInstance>(Body->GetAnimInstance());
        if (!AuthoredMotionBody && LocomotionBlend && (Resident || Body->GetSingleNodeInstance()))
        {
            // Unreal advances blend spaces in normalized time. The clips strike
            // left at 0 and right at 0.5 throughout the walk-to-run transition.
            const float Phase = Resident ? Resident->GetGaitPhase() : Body->GetPosition();
            const int32 ContactHalf = Phase < .5f ? 0 : 1;
            bContact = LastContactHalf != INDEX_NONE && ContactHalf != LastContactHalf;
            LastContactHalf = ContactHalf;
            if (bContact) Foot = Body->GetSocketLocation(ContactHalf ? TEXT("Footr") : TEXT("Footl"));
        }
        else
        {
            StepTravel += FlatSpeed * DeltaSeconds;
            const float StepLength = FMath::Lerp(69.f,112.f,FMath::Clamp(FlatSpeed/600.f,0.f,1.f));
            bContact = StepTravel >= StepLength;
            if (bContact) StepTravel = FMath::Fmod(StepTravel,StepLength);
        }
        if (bContact)
        {
            ++StepIndex;
            USoundWave* Step = StepIndex % 3 == 0 ? GritStep.Get() : StoneStep.Get();
            if (Step) UGameplayStatics::PlaySoundAtLocation(this,Step,Foot,.18f+FMath::Min(FlatSpeed/1600.f,.3f),.94f+(StepIndex%4)*.042f);
        }
    }
    else
    {
        LastContactHalf = INDEX_NONE;
        StepTravel = 0.f;
    }
    if (bBlueprintAvatarActive) return;
    if (bUsesImportedMesh)
    {
        if (LocomotionBlend)
        {
            if (auto* Player = GetMesh()->GetSingleNodeInstance())
                Player->SetBlendSpacePosition(FVector(FlatSpeed,0,0));
            return;
        }
        const int32 State = FlatSpeed < 9 ? 0 : FlatSpeed < (WalkSpeed + SprintSpeed) * .5f ? 1 : 2;
        UAnimSequence* Sequence = AnimationClips[State].Get();
        if (!Sequence)
        {
            Sequence = AnimationClips[0].Get();
        }
        if (Sequence && Sequence != ActiveSequence.Get())
        {
            ActiveSequence = Sequence;
            GetMesh()->PlayAnimation(Sequence, true);
        }
        if (Sequence)
        {
            const float Rate = State == 0 ? 1.f
                : FMath::Clamp(static_cast<float>(FlatSpeed / (State == 1 ? WalkSpeed : SprintSpeed)), .65f, 1.35f);
            GetMesh()->SetPlayRate(Rate);
        }
        return;
    }

    if (FlatSpeed > 9)
    {
        GaitPhase = FMath::Fmod(GaitPhase + DeltaSeconds * (3.0 + FlatSpeed * .020), 2.0 * UE_PI);
    }
    const double Swing = GetCharacterMovement()->IsFalling() ? 13.0
        : FMath::Sin(GaitPhase) * FMath::Min(38.0, FlatSpeed * .085);
    const auto Animate = [DeltaSeconds](USceneComponent* Joint, double Angle)
    {
        Joint->SetRelativeRotation(FMath::RInterpTo(Joint->GetRelativeRotation(),
            FRotator(Angle, 0, 0), DeltaSeconds, 18.f));
    };
    Animate(LeftLeg, Swing);
    Animate(RightLeg, -Swing);
    Animate(LeftArm, -Swing * .62);
    Animate(RightArm, Swing * .62);
}

void ARiverfrontCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("Forward"), this, &ARiverfrontCharacter::MoveForward);
    Input->BindAxis(TEXT("Side"), this, &ARiverfrontCharacter::MoveSide);
    Input->BindAxis(TEXT("LookYaw"), this, &APawn::AddControllerYawInput);
    Input->BindAxis(TEXT("LookPitch"), this, &APawn::AddControllerPitchInput);
    Input->BindAction(TEXT("BrakeOrJump"), IE_Pressed, this, &ARiverfrontCharacter::JumpOrTraverse);
    Input->BindAction(TEXT("BrakeOrJump"), IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction(TEXT("Sprint"), IE_Pressed, this, &ARiverfrontCharacter::Sprint);
    Input->BindAction(TEXT("Sprint"), IE_Released, this, &ARiverfrontCharacter::StopSprint);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &ARiverfrontCharacter::EnterCar);
    Input->BindAction(TEXT("VehicleDoor"),IE_Pressed,this,&ARiverfrontCharacter::OpenCarDoor);
    Input->BindAction(TEXT("Reset"), IE_Pressed, this, &ARiverfrontCharacter::ResetWalk);
    Input->BindAction(TEXT("DebugStats"), IE_Pressed, this, &ARiverfrontCharacter::ToggleStats);
}

void ARiverfrontCharacter::JumpOrTraverse()
{
    if (TraversalSource || bInFittingPose || GetCharacterMovement()->IsSwimming()) return;
    if (bBlueprintAvatarActive && TryAvatarTraversal()) return;
    if (TryStreetTraversal()) return;
    Jump();
}

void ARiverfrontCharacter::MoveForward(float Amount)
{
    if (Controller && !TraversalSource)
    {
        AddMovementInput(FRotator(0, Controller->GetControlRotation().Yaw, 0).Vector(), Amount);
    }
}

void ARiverfrontCharacter::MoveSide(float Amount)
{
    if (Controller && !TraversalSource)
    {
        AddMovementInput(FRotator(0, Controller->GetControlRotation().Yaw + 90, 0).Vector(), Amount);
    }
}

FString ARiverfrontCharacter::GetWaterHint() const
{
    if (bDrowned) return TEXT("Drowned  ·  Returning to Flat 2B");
    if (!GetCharacterMovement()->IsSwimming()) return {};
    return FString::Printf(TEXT("Swimming   ·   Hold sprint to dive   ·   Air %d%%"),FMath::RoundToInt(Breath*6.25f));
}

void ARiverfrontCharacter::Sprint()
{
    bDiving=true;
    GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ARiverfrontCharacter::StopSprint()
{
    bDiving=false;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ARiverfrontCharacter::OpenCarDoor()
{
    if (auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>())
        if (auto* Car=Mode->FindEnterableCar(*this)) Car->OperateDoor(GetActorLocation());
}

void ARiverfrontCharacter::EnterCar()
{
    if (TraversalSource) return;
    const ARiverfrontGameMode* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    if (ARiverfrontCar* Available = Mode ? Mode->FindEnterableCar(*this) : nullptr)
    {
        if (Available->Enter(*this)) return;
    }
    if (ARiverfrontTraffic* Traffic = Mode ? Mode->GetTraffic() : nullptr)
        if (Traffic->EnterVehicle(*this)) return;
    if (auto* Driver=Cast<ARiverfrontPlayerController>(GetController())) Driver->InteractWithPlace();
}

void ARiverfrontCharacter::SetInCar(bool bInCar)
{
    FinishTraversal(true);
    SetActorHiddenInGame(bInCar);
    if (AuthoredMotionBody && AuthoredMotionBody->GetOwner() != this)
        AuthoredMotionBody->GetOwner()->SetActorHiddenInGame(bInCar);
    if (bBlueprintAvatarActive) ReceiveAvatarInCar(bInCar);
    SetActorEnableCollision(!bInCar);
    SetActorTickEnabled(!bInCar);
    GetCharacterMovement()->StopMovementImmediately();
    StopSprint();
    StopJumping();
    if (bInCar)
    {
        GetCharacterMovement()->DisableMovement();
    }
    else
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
}

void ARiverfrontCharacter::ResetWalk()
{
    FinishTraversal(true);
    GetCharacterMovement()->StopMovementImmediately();
    const ARiverfrontGameMode* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    const FVector Spawn = Mode ? Mode->GetWalkSpawn() : ARiverfrontWorld::SpawnPoint;
    const double Yaw = Mode ? Mode->GetSpawnYaw() : ARiverfrontWorld::SpawnYaw;
    for (TActorIterator<ARiverfrontWorld> It(GetWorld()); It; ++It) It->PrepareCityTravel(Spawn);
    if (TeleportTo(Spawn, FRotator(0, Yaw, 0)))
    {
        if (Controller)
        {
            Controller->SetControlRotation(FRotator(-7.0, Yaw, 0.0));
        }
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
}

void ARiverfrontCharacter::ToggleStats()
{
    if (APlayerController* Player = Cast<APlayerController>(Controller))
    {
        Player->ConsoleCommand(TEXT("stat unit"));
    }
}
