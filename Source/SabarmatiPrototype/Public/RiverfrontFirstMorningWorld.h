#pragma once

#include "CoreMinimal.h"

class ARiverfrontWorld;

/** Grounded locations in Kesar Apartments and the Sutra Cloth House directly below. */
struct FRiverfrontFirstMorningWorld
{
    static void Build(ARiverfrontWorld& World);
    static FVector HomeStart();
    static FVector WardrobePoint();
    static FVector ShopPoint();
    static FVector StreetPoint();
    static FVector FenceApproach();
    static FVector WallApproach();
    static double HomeYaw() { return -78.; }
    static FTransform WakeStartView();
    static FTransform WakeEndView();
    static TArray<FVector> ExitRoute();
};
