#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RiverfrontLocalProfile.h"
#include "RiverfrontLifeProfile.generated.h"

UENUM()
enum class ERiverfrontWearSlot : uint8 { Top, Bottom, Accessory };

USTRUCT()
struct FRiverfrontWearable
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName Id;
    UPROPERTY(EditAnywhere) FString Label;
    UPROPERTY(EditAnywhere) int32 PriceRupees = 0;
    UPROPERTY(EditAnywhere) ERiverfrontWearSlot Slot = ERiverfrontWearSlot::Top;
    UPROPERTY(EditAnywhere) FLinearColor Tint = FLinearColor::White;
};

// The packaged catalogue is authored in DefaultGame.ini using Unreal property serialization.
UCLASS(Config=Game)
class SABARMATIPROTOTYPE_API URiverfrontWardrobeCatalog : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Config, Category="Merchandise")
    TArray<FRiverfrontWearable> Items;
};

namespace RiverfrontLife
{
    const TArray<FRiverfrontWearable>& Catalog();
    bool Exists(const FString& Slot);
    URiverfrontLocalProfile* Load(const FString& Slot);
    bool Create(const FString& Slot, const FString& DisplayName, ERiverfrontGender Gender = ERiverfrontGender::Male);
    bool MergeExploration(const FString& Slot, const FString& DisplayName, const TArray<FName>& Visits, float BestTime);
    inline const FName StarterBottom(TEXT("denim-dark"));
    inline const FName Underwear(TEXT("underwear"));
    inline const FName BaseLayer(TEXT("fitting-base"));
    enum class EDressingAction : uint8 { TakeOff, Take, PutOn, PutBack };
    FText Buy(const FString& Slot, FName Item, bool bPayFromBank);
    FText Dress(const FString& Slot, EDressingAction Action, FName Item);
    FText SetAvatarGender(const FString& Slot, ERiverfrontGender Gender);
    FText SetAppearanceTone(const FString& Slot, FName Tone);
    FText SetCroppedHair(const FString& Slot, bool bCropped);
    bool SaveCheckpoint(const FString& Slot, FName District, FVector Location, float Yaw, bool bFinishedWakeup = true);
    FText StartPrintDelivery(const FString& Slot);
    FText CompletePrintDelivery(const FString& Slot);
}
