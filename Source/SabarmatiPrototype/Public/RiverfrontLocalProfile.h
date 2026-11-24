#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RiverfrontResidentAppearance.h"
#include "RiverfrontLocalProfile.generated.h"

/** Single-device profile. The game does not ask for credentials or communicate with a server. */
UCLASS()
class SABARMATIPROTOTYPE_API URiverfrontLocalProfile : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchema = 2;
    static FString CleanDisplayName(const FString& Input);

    UPROPERTY(SaveGame)
    FString DisplayName;

    UPROPERTY(SaveGame) TArray<FName> VisitedSites;
    UPROPERTY(SaveGame) float BestRiverRunSeconds=0.f;

    UPROPERTY(SaveGame) int64 CashRupees = 0;
    UPROPERTY(SaveGame) int64 BankRupees = 0;
    UPROPERTY(SaveGame) TArray<FName> OwnedItems;
    UPROPERTY(SaveGame) FName EquippedOutfit;
    UPROPERTY(SaveGame) FName EquippedAccessory;
    // Tagged saves written before the avatar selector inherit the original resident.
    UPROPERTY(SaveGame) ERiverfrontGender AvatarGender = ERiverfrontGender::Male;
    UPROPERTY(SaveGame) FName AppearanceTone;
    UPROPERTY(SaveGame) bool bCroppedHair = false;
    UPROPERTY(SaveGame) FString LastReceipt;
    UPROPERTY(SaveGame) bool bFirstMorningSeen = false;
    UPROPERTY(SaveGame) bool bHasLifeCheckpoint = false;
    UPROPERTY(SaveGame) FName CheckpointDistrict;
    UPROPERTY(SaveGame) FVector CheckpointLocation = FVector::ZeroVector;
    UPROPERTY(SaveGame) float CheckpointYaw = 0.f;
    UPROPERTY(SaveGame) bool bPrintDeliveryActive = false;
    UPROPERTY(SaveGame) int32 CompletedPrintDeliveries = 0;

    // A legacy save can omit fields equal to its original class defaults.
    UPROPERTY(SaveGame)
    int32 Schema = 1;
};
