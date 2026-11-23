#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RiverfrontLocalProfile.generated.h"

/** Single-device profile. The game does not ask for credentials or communicate with a server. */
UCLASS()
class SABARMATIPROTOTYPE_API URiverfrontLocalProfile : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchema = 1;
    static FString CleanDisplayName(const FString& Input);

    UPROPERTY(SaveGame)
    FString DisplayName;

    UPROPERTY(SaveGame) TArray<FName> VisitedSites;
    UPROPERTY(SaveGame) float BestRiverRunSeconds=0.f;

    UPROPERTY(SaveGame)
    int32 Schema = CurrentSchema;
};
