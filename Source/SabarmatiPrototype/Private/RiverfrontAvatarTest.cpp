#if WITH_DEV_AUTOMATION_TESTS

#include "RiverfrontAvatarMotion.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontLocalProfile.h"
#include "RiverfrontLocomotionInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSabarmatiAuthoredAvatar,
    "Sabarmati.Player.AuthoredAvatar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSabarmatiAuthoredAvatar::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated game world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    auto* Character = World->SpawnActor<ARiverfrontCharacter>();
    auto* Model = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/People/SK_RiverResident.SK_RiverResident"));
    auto* OptIn = FindFProperty<FBoolProperty>(ARiverfrontCharacter::StaticClass(), TEXT("bUseBlueprintAvatar"));
    if (!TestNotNull(TEXT("Player"), Character) || !TestNotNull(TEXT("Public fixture mesh"), Model) ||
        !TestNotNull(TEXT("Blueprint class-default opt-in"), OptIn)) return false;

    // Exercise the inherited mesh exactly as a subclass that supplies an animation graph would.
    OptIn->SetPropertyValue_InContainer(Character, true);
    auto* Body = Character->GetMesh();
    Body->SetSkeletalMesh(Model);
    Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    Character->DispatchBeginPlay();
    Character->SetFittingPose(true);
    auto* Profile = NewObject<URiverfrontLocalProfile>();
    Profile->AvatarGender = ERiverfrontGender::Female;
    Profile->EquippedOutfit = TEXT("kurta");
    Profile->EquippedAccessory = TEXT("watch");
    Profile->AppearanceTone = TEXT("warm");
    Character->ApplyProfileAppearance(*Profile);
    TestEqual(TEXT("Authored avatar can read its selected gender"), Character->GetAvatarGender(), ERiverfrontGender::Female);
    Character->SetCroppedHair(true);
    Character->SetFittingPose(false);
    Character->Tick(.016f);
    TestTrue(TEXT("Appearance preview retains the authored mesh"), Body->GetSkeletalMeshAsset() == Model);
    TestTrue(TEXT("Fitting and locomotion retain animation Blueprint mode"),
        Body->GetAnimationMode() == EAnimationMode::AnimationBlueprint);
    TestEqual(TEXT("Outfit selection delivered to the avatar"), Character->GetWardrobeOutfit(), FName(TEXT("kurta")));
    TestEqual(TEXT("Accessory selection delivered to the avatar"), Character->GetWardrobeAccessory(), FName(TEXT("watch")));
    TestFalse(TEXT("Hair choices require a supporting assembly"), Character->SupportsPublicHaircut());
    Character->SetInCar(true);
    TestTrue(TEXT("Entering a vehicle hides the player"), Character->IsHidden());
    Character->SetInCar(false);
    TestFalse(TEXT("Leaving a vehicle reveals the player"), Character->IsHidden());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSabarmatiAssembledAvatarMotion,
    "Sabarmati.Player.AssembledAvatarMotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSabarmatiAssembledAvatarMotion::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated game world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Player = World->SpawnActor<ARiverfrontCharacter>();
    auto* Motion = NewObject<URiverfrontAvatarMotion>(Player);
    const FString People = TEXT("/Game/Sabarmati/Riverfront/People/");
    auto* Model = LoadObject<USkeletalMesh>(nullptr, *(People + TEXT("SK_RiverResident.SK_RiverResident")));
    if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Fixture rig"), Model)) return false;
    Motion->Gait = LoadObject<UBlendSpace1D>(nullptr, *(People + TEXT("BS_ResidentGait.BS_ResidentGait")));
    Motion->FittingIdle = LoadObject<UAnimSequence>(nullptr, *(People + TEXT("A_Idle.A_Idle")));
    const auto LoadMotion = [&](const TCHAR* Name)
    {
        return LoadObject<UAnimSequence>(nullptr, *(People + TEXT("Motion/Everyday/") + Name + TEXT(".") + Name));
    };
    Motion->Takeoff = LoadMotion(TEXT("MM_Jump"));
    Motion->Fall = LoadMotion(TEXT("MM_Fall_Loop"));
    Motion->Landing = LoadMotion(TEXT("MM_Land"));
    Motion->FenceVault = LoadMotion(TEXT("A_FenceVault"));
    Motion->WallMantle = LoadMotion(TEXT("A_WallMantle"));

    auto* Slot = NewObject<UChildActorComponent>(Player);
    Player->AddInstanceComponent(Slot);
    Slot->SetupAttachment(Player->GetRootComponent());
    Slot->SetChildActorClass(ASkeletalMeshActor::StaticClass());
    Slot->RegisterComponent();
    auto* Assembly = Cast<ASkeletalMeshActor>(Slot->GetChildActor());
    if (!TestNotNull(TEXT("Child avatar assembly"), Assembly)) return false;
    auto* Body = Assembly->GetSkeletalMeshComponent();
    Body->SetSkeletalMesh(Model);
    Body->ComponentTags.Add(TEXT("RiverfrontAvatarBody"));
    const auto* OptIn = FindFProperty<FBoolProperty>(ARiverfrontCharacter::StaticClass(), TEXT("bUseBlueprintAvatar"));
    const auto* MotionProperty = FindFProperty<FObjectProperty>(ARiverfrontCharacter::StaticClass(), TEXT("AuthoredAvatarMotion"));
    if (!TestNotNull(TEXT("Appearance opt-in"), OptIn) || !TestNotNull(TEXT("Motion property"), MotionProperty)) return false;
    OptIn->SetPropertyValue_InContainer(Player, true);
    MotionProperty->SetObjectPropertyValue_InContainer(Player, Motion);
    Player->DispatchBeginPlay();

    TestEqual(TEXT("The child body drives native animation"), Player->GetAvatarMesh(), Body);
    TestFalse(TEXT("Only the player capsule owns collision"), Assembly->GetActorEnableCollision());
    TestNotNull(TEXT("The child uses the native gait and montage graph"),
        Cast<URiverfrontLocomotionInstance>(Body->GetAnimInstance()));
    auto* Profile = NewObject<URiverfrontLocalProfile>();
    Profile->EquippedOutfit = TEXT("shirt-indigo");
    Profile->EquippedAccessory = TEXT("watch");
    Player->ApplyProfileAppearance(*Profile);
    TestEqual(TEXT("The saved outfit reaches the assembly"), Player->GetWardrobeOutfit(), FName(TEXT("shirt-indigo")));
    TestTrue(TEXT("The saved wristwatch follows the child rig"), Player->HasWristwatchVisible());
    Player->SetFittingPose(true);
    TestEqual(TEXT("The assembled body enters fitting pose"), Body->GetAnimationMode(), EAnimationMode::AnimationSingleNode);
    Player->SetFittingPose(false);
    TestNotNull(TEXT("Fitting restores locomotion"), Cast<URiverfrontLocomotionInstance>(Body->GetAnimInstance()));
    Player->SetInCar(true);
    TestTrue(TEXT("Car possession hides the assembly"), Assembly->IsHidden());
    Player->SetInCar(false);
    TestFalse(TEXT("Exiting the car reveals the assembly"), Assembly->IsHidden());
    return true;
}

#endif
