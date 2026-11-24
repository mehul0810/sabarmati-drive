#include "RiverfrontCharacter.h"
#include "RiverfrontResidentOrientation.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"

#include "Animation/AnimSequence.h"
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
#include "RiverfrontGameMode.h"
#include "RiverfrontPlayerController.h"
#include "RiverfrontWorld.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float WalkSpeed = 228.f;
constexpr float SprintSpeed = 463.f;
const TCHAR* AvatarRoot = TEXT("/Game/Sabarmati/Avatar/");

template <typename Type>
Type* LoadOptionalAvatarAsset(const TCHAR* Name)
{
    FString Package=FString(AvatarRoot)+Name;
    if (FParse::Param(FCommandLine::Get(), TEXT("SabarmatiPublicAvatar")) || !FPackageName::DoesPackageExist(Package))
    {
        const FString PublicName=FCString::Strcmp(Name,TEXT("SK_Sabarmati"))==0?TEXT("SK_RiverResident"):Name;
        Package=FString(TEXT("/Game/Sabarmati/Riverfront/People/"))+PublicName;
        return FPackageName::DoesPackageExist(Package)?LoadObject<Type>(nullptr,*(Package+TEXT(".")+PublicName)):nullptr;
    }
    return FPackageName::DoesPackageExist(Package)
        ? LoadObject<Type>(nullptr, *(Package + TEXT(".") + Name)) : nullptr;
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
    Movement->MaxAcceleration = 1560.f;
    Movement->BrakingDecelerationWalking = 1530.f;
    Movement->JumpZVelocity = 440.f;
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
            ReceiveAvatarAppearance(WardrobeOutfit, WardrobeAccessory, WardrobeTone, bCroppedHair);
            return;
        }
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati avatar %s has no assembled skeletal mesh; loading the riverfront character."),
            *GetClass()->GetPathName());
    }

    // A cooked Unreal Skeletal Mesh, not a runtime read of the raw GLB.
    if (USkeletalMesh* ImportedMesh = LoadOptionalAvatarAsset<USkeletalMesh>(TEXT("SK_Sabarmati")))
    {
        DefaultAvatarMesh = ImportedMesh;
        CottonMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_ResidentCotton.M_ResidentCotton"));
        const bool bPublic = ImportedMesh->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
        if (bPublic)
        {
            Hair->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/SM_RiverSweptHair/StaticMeshes/SM_RiverSweptHair.SM_RiverSweptHair")));
            EyeMaterial = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_RiverEyes.M_RiverEyes"));
            TailoredCloth = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_RiverTailoredCloth.M_RiverTailoredCloth"));
        }
        FaceMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_ResidentSkin.M_ResidentSkin"));
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

void ARiverfrontCharacter::UseWardrobeMesh(USkeletalMesh* Model, bool bKurta)
{
    if (!Model || GetMesh()->GetSkeletalMeshAsset() == Model) return;

    GetMesh()->EmptyOverrideMaterials();
    GetMesh()->SetSkeletalMesh(Model);
    GetMesh()->SetRelativeRotation(FRotator(0, RiverfrontResident::YawToActorForward(*Model), 0));
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    ActiveSequence = nullptr;
    AnimationClips.Reset(3);
    const bool bPublic = Model->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
    UpdateHair();
    const FString ClipFolder = bPublic ? (bKurta ? TEXT("/Game/Sabarmati/Riverfront/People/Kurta/")
        : TEXT("/Game/Sabarmati/Riverfront/People/")) : AvatarRoot;
    for (const TCHAR* Clip : {TEXT("A_Idle"), TEXT("A_Walk"), TEXT("A_Jog")})
    {
        const FString Package = ClipFolder + Clip;
        auto* Sequence = FPackageName::DoesPackageExist(Package)
            ? LoadObject<UAnimSequence>(nullptr, *(Package + TEXT(".") + Clip)) : nullptr;
        AnimationClips.Add(Sequence && Sequence->GetSkeleton() == Model->GetSkeleton() ? Sequence : nullptr);
    }
    if (AnimationClips[0])
    {
        ActiveSequence = AnimationClips[0];
        GetMesh()->PlayAnimation(ActiveSequence, true);
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati avatar: skeletal mesh loaded, %s; clips idle=%d walk=%d jog=%d."),
        *Model->GetName(), IsValid(AnimationClips[0].Get()), IsValid(AnimationClips[1].Get()), IsValid(AnimationClips[2].Get()));
}

bool ARiverfrontCharacter::SupportsPublicHaircut() const
{
    if (bBlueprintAvatarActive) return bSupportsBlueprintHaircuts;
    return DefaultAvatarMesh && Hair->GetStaticMesh() &&
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
        Hair->SetVisibility(!bCroppedHair);
        const int32 HairSlot = GetMesh()->GetMaterialIndex(TEXT("hair"));
        if (HairSlot != INDEX_NONE)
            for (int32 LOD = 0; LOD < GetMesh()->GetNumLODs(); ++LOD)
                GetMesh()->ShowMaterialSection(HairSlot, INDEX_NONE, bCroppedHair, LOD);
    }
}

void ARiverfrontCharacter::ApplyWardrobeLook(FName Outfit, FName Accessory, FName Tone)
{
    WardrobeOutfit = Outfit.IsNone() ? FName(TEXT("cotton-sand")) : Outfit;
    WardrobeAccessory = Accessory;
    WardrobeTone = Tone;
    if (bBlueprintAvatarActive)
    {
        ReceiveAvatarAppearance(WardrobeOutfit, WardrobeAccessory, WardrobeTone, bCroppedHair);
        return;
    }
    if (!DefaultAvatarMesh) return;

    const bool bIndigo = WardrobeOutfit == TEXT("shirt-indigo");
    const bool bKurta = WardrobeOutfit == TEXT("kurta");
    const bool bPublic = DefaultAvatarMesh->GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/"));
    if (bPublic && bKurta && !KurtaMesh)
    {
        KurtaMesh = LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Tailored/SK_FittedKurta.SK_FittedKurta"));
    }
    UseWardrobeMesh(bPublic && bKurta && KurtaMesh ? KurtaMesh.Get() : DefaultAvatarMesh.Get(), bPublic && bKurta);

    // The fabric is UV-mapped on the garment and moves with the animated body.
    const FColor Shirt = bIndigo ? FColor(60, 85, 132) : bKurta ? FColor(135, 63, 48) : FColor(202, 183, 150);
    const FColor Trim = bIndigo ? FColor(39, 54, 79) : bKurta ? FColor(98, 44, 42) : FColor(144, 125, 99);
    const FColor Trousers = bIndigo ? FColor(169, 153, 121) : bKurta ? FColor(209, 201, 178) : FColor(53, 62, 73);
    const FColor Skin = Tone == TEXT("umber") ? FColor(141, 91, 66) : Tone == TEXT("warm") ?
        FColor(205, 150, 107) : FColor(180, 133, 105);

    auto SetSurface = [this, Shirt, Trousers](FName SlotName, UMaterialInterface* Surface, const FColor& Colour)
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
            MID->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(Colour));
        }
    };
    auto* Cloth = bPublic && !bKurta && TailoredCloth ? TailoredCloth.Get() : CottonMaterial.Get();
    SetSurface(TEXT("top_teal_woven"), Cloth, Shirt);
    SetSurface(TEXT("top_seams"), Cloth, Trim);
    SetSurface(TEXT("bottom_denim"), Cloth, Trousers);
    SetSurface(TEXT("scarf"), CottonMaterial, FColor(195, 149, 91));
    if (bPublic)
    {
        const FColor Complexion = Tone == TEXT("umber") ? FColor(116,78,61) : Tone == TEXT("warm")
            ? FColor(174,119,87) : FColor(156,108,82);
        SetSurface(TEXT("skin"), FaceMaterial, Complexion);
        SetSurface(TEXT("hair"), nullptr, FColor(34,25,21));
        const int32 HairSlot = GetMesh()->GetMaterialIndex(TEXT("hair"));
        if (HairSlot != INDEX_NONE)
            if (auto* Material = Cast<UMaterialInstanceDynamic>(GetMesh()->GetMaterial(HairSlot)))
                Material->SetScalarParameterValue(TEXT("RoughnessFactor"), .68f);
        SetSurface(TEXT("lips"), nullptr, FColor(Complexion.R*.86f, Complexion.G*.74f, Complexion.B*.79f));
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
    UpdateWatchAttachment();
    if (bInFittingPose) SetFittingPose(true);
}

void ARiverfrontCharacter::SetFittingPose(bool bFitting)
{
    auto* Movement = GetCharacterMovement();
    if (!bFitting)
    {
        if (!bInFittingPose) return;
        bInFittingPose = false;
        Movement->bOrientRotationToMovement = bOrientBeforeFitting;
        if (bBlueprintAvatarActive) ReceiveAvatarFittingPose(false);
        else GetMesh()->SetPlayRate(1.f);
        return;
    }
    if (!bInFittingPose) bOrientBeforeFitting = Movement->bOrientRotationToMovement;
    bInFittingPose = true;
    Movement->StopMovementImmediately();
    ConsumeMovementInputVector();
    Movement->bOrientRotationToMovement = false;
    if (bBlueprintAvatarActive)
    {
        ReceiveAvatarFittingPose(true);
        return;
    }
    if (!AnimationClips.IsValidIndex(0) || !AnimationClips[0]) return;

    // Menus can pause the world before the next animation tick. Evaluate a
    // neutral stance now so the fitting view cannot inherit a frozen footstep.
    auto* Body = GetMesh();
    ActiveSequence = AnimationClips[0];
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
    const USkeletalMesh* Model = GetMesh()->GetSkeletalMeshAsset();
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
    const FVector Outside = GetMesh()->GetRelativeRotation().UnrotateVector(FVector::LeftVector);
    const FTransform Fit(FRotationMatrix::MakeFromZX(Axis, Outside).ToQuat(), Hand.GetLocation()-Axis*2.7);
    Wristwatch->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Skeleton.GetBoneName(Wrist));
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

void ARiverfrontCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bInFittingPose) return;
    if (!IsHidden() && GetCharacterMovement()->IsMovingOnGround())
    {
        const float Rate=GetVelocity().Size2D();
        StepTravel+=Rate*DeltaSeconds;
        if (StepTravel>=FMath::Lerp(69.f,112.f,FMath::Clamp(Rate/600.f,0.f,1.f)))
        {
            StepTravel=0; ++StepIndex;
            const FVector Foot=GetActorLocation()-FVector(0,0,87);
            USoundWave* Step=StepIndex%3==0?GritStep.Get():StoneStep.Get();
            if (Step) UGameplayStatics::PlaySoundAtLocation(this,Step,Foot,.18f+FMath::Min(Rate/1600.f,.3f),.94f+(StepIndex%4)*.042f);
        }
    }
    if (bBlueprintAvatarActive) return;
    const double FlatSpeed = GetVelocity().Size2D();
    if (bUsesImportedMesh)
    {
        const int32 State = FlatSpeed < 9 ? 0 : FlatSpeed < 315 ? 1 : 2;
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
    if (bBlueprintAvatarActive && TryAvatarTraversal()) return;
    Jump();
}

void ARiverfrontCharacter::MoveForward(float Amount)
{
    if (Controller)
    {
        AddMovementInput(FRotator(0, Controller->GetControlRotation().Yaw, 0).Vector(), Amount);
    }
}

void ARiverfrontCharacter::MoveSide(float Amount)
{
    if (Controller)
    {
        AddMovementInput(FRotator(0, Controller->GetControlRotation().Yaw + 90, 0).Vector(), Amount);
    }
}

void ARiverfrontCharacter::Sprint()
{
    GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ARiverfrontCharacter::StopSprint()
{
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ARiverfrontCharacter::OpenCarDoor()
{
    if (auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>())
        if (auto* Car=Mode->FindEnterableCar(*this)) Car->OperateDoor(GetActorLocation());
}

void ARiverfrontCharacter::EnterCar()
{
    const ARiverfrontGameMode* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    if (ARiverfrontCar* Available = Mode ? Mode->FindEnterableCar(*this) : nullptr)
    {
        Available->Enter(*this);
        return;
    }
    if (auto* Driver=Cast<ARiverfrontPlayerController>(GetController())) Driver->InteractWithPlace();
}

void ARiverfrontCharacter::SetInCar(bool bInCar)
{
    SetActorHiddenInGame(bInCar);
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
