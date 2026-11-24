#if WITH_DEV_AUTOMATION_TESTS

#include "RiverfrontCharacter.h"
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
    Character->ApplyWardrobeLook(TEXT("kurta"), TEXT("watch"), TEXT("warm"));
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

#endif
