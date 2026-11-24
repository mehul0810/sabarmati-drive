#if WITH_EDITOR

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontWorld.h"

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
}

#endif
