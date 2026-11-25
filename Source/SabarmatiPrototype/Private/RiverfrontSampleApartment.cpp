#if WITH_EDITOR

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/PackageName.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontWorld.h"

namespace
{
void PrepareSampleFence(UWorld& World)
{
    constexpr ECollisionChannel Traversable = ECC_GameTraceChannel1;
    if (UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Traversable) != TEXT("Traversable"))
    {
        UE_LOG(LogTemp, Warning, TEXT("Sample fence needs the Game Animation Sample's Traversable channel."));
        return;
    }

    UClass* LedgeClass = LoadClass<AActor>(nullptr, TEXT("/Game/Levels/LevelPrototyping/LevelBlock_Traversable.LevelBlock_Traversable_C"));
    if (!LedgeClass) return;

    // The sample's 100 cm block starts at its bottom corner. Match its ledges to the real rail.
    const FBox Bounds = FRiverfrontFirstMorningWorld::FenceTraversalBounds();
    const FTransform Placement(FQuat::Identity, Bounds.Min, Bounds.GetSize() / 100.);
    AActor* Ledge = World.SpawnActorDeferred<AActor>(LedgeClass, Placement, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Ledge) return;
    Ledge->FinishSpawning(Placement);
    UStaticMeshComponent* Mesh = Ledge->FindComponentByClass<UStaticMeshComponent>();
    if (!Mesh)
    {
        Ledge->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("Sample fence could not find the ledge mesh."));
        return;
    }

    // The existing fence supplies visuals and physical collision. Only traversal traces need the ledge actor.
    Ledge->SetActorHiddenInGame(true);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(Traversable, ECR_Block);
    UE_LOG(LogTemp, Display, TEXT("Sabarmati sample fence: traversal ledges aligned to the riverfront rail."));
}

void PrepareSampleHuman(UWorld& World)
{
    const TWeakObjectPtr<UWorld> PreviewWorld(&World);
    const double Started = FPlatformTime::Seconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([PreviewWorld, Started](float)
    {
        UWorld* World = PreviewWorld.Get();
        if (!World) return false;
        APlayerController* Controller = World->GetFirstPlayerController();
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        if (!World->HasBegunPlay() || !Pawn)
        {
            if (FPlatformTime::Seconds() - Started < 120.) return true;
            UE_LOG(LogTemp, Error, TEXT("Sample human preview could not find a possessed pawn."));
            return false;
        }

        const FString Package = TEXT("/Game/MetaHumans/Kellan/BP_Kellan");
        UClass* HumanClass = FPackageName::DoesPackageExist(Package)
            ? LoadClass<AActor>(nullptr, TEXT("/Game/MetaHumans/Kellan/BP_Kellan.BP_Kellan_C")) : nullptr;
        TInlineComponentArray<UChildActorComponent*> Children;
        Pawn->GetComponents(Children);
        auto* Visual = Children.FindByPredicate([](const UChildActorComponent* Child)
        {
            return Child && Child->ComponentHasTag(TEXT("VisualOverride"));
        });
        auto* Driver = Visual ? Cast<USkeletalMeshComponent>((*Visual)->GetAttachParent()) : nullptr;
        if (!HumanClass || !Visual || !Driver)
        {
            UE_LOG(LogTemp, Error, TEXT("Sample human preview needs Kellan and the sample visual override."));
            return false;
        }

        UClass* PreviousClass = (*Visual)->GetChildActorClass();
        (*Visual)->SetChildActorClass(HumanClass);
        AActor* Human = (*Visual)->GetChildActor();
        TInlineComponentArray<USkeletalMeshComponent*> Parts;
        if (Human) Human->GetComponents(Parts);
        const bool bHasRetargetBody = Parts.ContainsByPredicate([](USkeletalMeshComponent* Part)
        {
            return Part && Part->ComponentHasTag(TEXT("RTG_UEFN_to_Metahuman_nrw")) &&
                Part->GetSkeletalMeshAsset() && Part->GetAnimClass();
        });
        if (!bHasRetargetBody)
        {
            (*Visual)->SetChildActorClass(PreviousClass);
            UE_LOG(LogTemp, Error, TEXT("Sample human preview could not find Kellan's retargeted body."));
            return false;
        }
        Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Driver->SetVisibility(false, false);
        PrepareSampleFence(*World);
        UE_LOG(LogTemp, Display, TEXT("Sabarmati sample human preview: Kellan is driven by %s."),
            *Pawn->GetClass()->GetPathName());
        return false;
    }), .1f);
}
}

void PrepareRiverfrontSampleApartment(const FActorsInitializedParams& Params)
{
    UWorld* World = Params.World;
    if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone ||
        !World->URL.HasOption(TEXT("SabarmatiSampleApartment")))
    {
        return;
    }
    // Requiring an exact class match avoids silently testing Unreal's fallback game mode.
    const FString RequestedMode = World->URL.GetOption(TEXT("game="), TEXT(""));
    const AGameModeBase* Mode = World->GetAuthGameMode();
    if (!Mode || Mode->IsA<ARiverfrontGameMode>() || RequestedMode.IsEmpty() ||
        Mode->GetClass()->GetPathName() != RequestedMode)
    {
        UE_LOG(LogTemp, Error, TEXT("Flat 2B playtest requires ?game= with the sample game mode's full class path. Loaded: %s"),
            *GetPathNameSafe(Mode ? Mode->GetClass() : nullptr));
        return;
    }

    APlayerStart* Start = nullptr;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        if (Start)
        {
            UE_LOG(LogTemp, Error, TEXT("Use an empty test level with at most one Player Start."));
            return;
        }
        Start = *It;
    }
    TActorIterator<ARiverfrontWorld> Districts(World);
    ARiverfrontWorld* District = Districts ? *Districts : World->SpawnActor<ARiverfrontWorld>();
    if (!IsValid(District) || District->IsCityMode())
    {
        UE_LOG(LogTemp, Error, TEXT("Could not prepare the riverfront in the test level."));
        return;
    }
    District->Build();
    const FVector Home = FRiverfrontFirstMorningWorld::HomeStart();
    const FRotator Facing(0, FRiverfrontFirstMorningWorld::HomeYaw(), 0);
    FHitResult Floor;
    if (!World->LineTraceSingleByChannel(Floor, Home, Home - FVector(0, 0, 140), ECC_Visibility)
        || Floor.GetActor() != District)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find Flat 2B's floor in the test level."));
        return;
    }

    // The sample spawns its pawn here with its original input, camera and mover simulation.
    if (Start)
    {
        Start->GetRootComponent()->SetMobility(EComponentMobility::Movable);
        if (!Start->SetActorLocationAndRotation(Home, Facing))
        {
            UE_LOG(LogTemp, Error, TEXT("Could not position the test level's Player Start in Flat 2B."));
            return;
        }
    }
    else
    {
        Start = World->SpawnActor<APlayerStart>(Home, Facing);
        if (!Start)
        {
            UE_LOG(LogTemp, Error, TEXT("Could not spawn the Player Start in Flat 2B."));
            return;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati sample apartment staged: mode=%s start=%s"),
        *RequestedMode, *Start->GetActorLocation().ToCompactString());
    if (World->URL.HasOption(TEXT("SabarmatiSampleHuman"))) PrepareSampleHuman(*World);
}

#endif
