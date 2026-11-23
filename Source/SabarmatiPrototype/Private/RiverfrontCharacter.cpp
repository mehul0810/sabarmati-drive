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
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
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
    if (!FPackageName::DoesPackageExist(Package))
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

    // A cooked Unreal Skeletal Mesh, not a runtime read of the raw GLB.
    if (USkeletalMesh* ImportedMesh = LoadOptionalAvatarAsset<USkeletalMesh>(TEXT("SK_Sabarmati")))
    {
        GetMesh()->SetSkeletalMesh(ImportedMesh);
        GetMesh()->SetRelativeRotation(FRotator(0,RiverfrontResident::YawToActorForward(*ImportedMesh),0));
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        PreviewRig->SetVisibility(false, true);
        PreviewRig->SetHiddenInGame(true, true);
        bUsesImportedMesh = true;
        for (const TCHAR* Name : {TEXT("A_Idle"), TEXT("A_Walk"), TEXT("A_Jog")})
        {
            UAnimSequence* Sequence = LoadOptionalAvatarAsset<UAnimSequence>(Name);
            AnimationClips.Add(Sequence && Sequence->GetSkeleton() == ImportedMesh->GetSkeleton()
                ? Sequence : nullptr);
        }
        const FBoxSphereBounds Bounds = ImportedMesh->GetBounds();
        UE_LOG(LogTemp, Display, TEXT("Sabarmati avatar: skeletal mesh loaded, bounds %s/%s; clips idle=%d walk=%d jog=%d."),
            *Bounds.Origin.ToCompactString(), *Bounds.BoxExtent.ToCompactString(), IsValid(AnimationClips[0].Get()),
            IsValid(AnimationClips[1].Get()), IsValid(AnimationClips[2].Get()));
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

void ARiverfrontCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
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
    Input->BindAction(TEXT("BrakeOrJump"), IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction(TEXT("BrakeOrJump"), IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction(TEXT("Sprint"), IE_Pressed, this, &ARiverfrontCharacter::Sprint);
    Input->BindAction(TEXT("Sprint"), IE_Released, this, &ARiverfrontCharacter::StopSprint);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &ARiverfrontCharacter::EnterCar);
    Input->BindAction(TEXT("VehicleDoor"),IE_Pressed,this,&ARiverfrontCharacter::OpenCarDoor);
    Input->BindAction(TEXT("Reset"), IE_Pressed, this, &ARiverfrontCharacter::ResetWalk);
    Input->BindAction(TEXT("DebugStats"), IE_Pressed, this, &ARiverfrontCharacter::ToggleStats);
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
