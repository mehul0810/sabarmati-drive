#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverfrontPigeon.generated.h"

class ARiverfrontWorld;
class UAnimSequence;
class USkeletalMeshComponent;

/** A small ambient bird on the promenade, animated with imported clips. */
UCLASS()
class ARiverfrontPigeon : public AActor
{
    GENERATED_BODY()

public:
    ARiverfrontPigeon();
    static void SpawnPromenadeFlock(ARiverfrontWorld& District);

protected:
    void Tick(float DeltaSeconds) override;

private:
    enum class EActivity : uint8 { Ground, Pecking, Flapping, Gliding, Away };
    void Play(UAnimSequence* Clip, bool bLoop);
    void ReturnToGround();

    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    UPROPERTY() TObjectPtr<UAnimSequence> Peck;
    UPROPERTY() TObjectPtr<UAnimSequence> Fly;
    UPROPERTY() TObjectPtr<UAnimSequence> Glide;

    EActivity Activity = EActivity::Ground;
    FVector Home = FVector::ZeroVector;
    FVector FlightDirection = FVector::ZeroVector;
    double ForwardYawOffset = 0.;
    double HomeYaw = 0.;
    double FlightStart = 0.;
    double NextPeck = 0.;
    double ReturnAfter = 0.;
    float AnimationRate = 1.f;
};
