#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RiverfrontGameMode.generated.h"

class ARiverfrontCar;
class ARiverfrontCharacter;
class ARiverfrontWorld;
class APlayerStart;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARiverfrontGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    ARiverfrontCar* GetCar() const { return Car; }
    ARiverfrontCar* FindEnterableCar(const ARiverfrontCharacter& Character) const;
    bool IsCityMode() const { return bCityBuilt; }
    bool CityFailed() const { return bCityRequested && !bCityBuilt; }
    FVector GetWalkSpawn() const { return WalkSpawn; }
    double GetSpawnYaw() const { return SpawnYaw; }

private:
    FVector WalkSpawn = FVector::ZeroVector;
    double SpawnYaw = 0.0;
    bool bCityRequested = false;
    bool bCityBuilt = false;

    UPROPERTY()
    TObjectPtr<APlayerStart> RiverStart;
    UPROPERTY()
    TObjectPtr<ARiverfrontCar> Car;
    UPROPERTY()
    TObjectPtr<ARiverfrontCar> Crossover;

    ARiverfrontCar* SpawnCrossover(ARiverfrontWorld& WorldActor, const FVector& Position, const FRotator& Facing);
};
