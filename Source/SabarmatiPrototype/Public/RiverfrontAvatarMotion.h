#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RiverfrontAvatarMotion.generated.h"

class UAnimSequence;
class UBlendSpace1D;

/** Retargeted clips for an authored avatar that uses Sabarmati's Character Movement. */
UCLASS(BlueprintType)
class SABARMATIPROTOTYPE_API URiverfrontAvatarMotion : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Ground")
    TObjectPtr<UBlendSpace1D> Gait;

    UPROPERTY(EditAnywhere, Category="Ground")
    TObjectPtr<UAnimSequence> FittingIdle;

    UPROPERTY(EditAnywhere, Category="Air")
    TObjectPtr<UAnimSequence> Takeoff;

    UPROPERTY(EditAnywhere, Category="Air")
    TObjectPtr<UAnimSequence> Fall;

    UPROPERTY(EditAnywhere, Category="Air")
    TObjectPtr<UAnimSequence> Landing;

    UPROPERTY(EditAnywhere, Category="Traversal")
    TObjectPtr<UAnimSequence> FenceVault;

    UPROPERTY(EditAnywhere, Category="Traversal")
    TObjectPtr<UAnimSequence> WallMantle;
};
