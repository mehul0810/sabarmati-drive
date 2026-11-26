#pragma once

#include "CoreMinimal.h"

class ARiverfrontWorld;

/** Grounded locations in Kesar Apartments and the Sutra Cloth House directly below. */
struct FRiverfrontFirstMorningWorld
{
    static void Build(ARiverfrontWorld& World);
    static FVector HomeStart();
    static void CoverSleeper(ARiverfrontWorld& World,bool Occupied);
    static FVector Bedside();
    static FVector Pillow();
    static FVector WardrobePoint();
    static FVector MirrorCentre();
    static FVector2D MirrorSize();
    static FVector ShopPoint();
    static FVector FittingRoomPoint();
    static FVector ShopMirrorCentre();
    static FVector StreetPoint();
    static FVector FenceApproach();
#if WITH_EDITOR
    static FBox FenceTraversalBounds();
#endif
    static FVector WallApproach();
    static double HomeYaw() { return -78.; }
    static FTransform WakeStartView(bool bShowSleeper=true);
    static FTransform WakeEndView(bool bShowSleeper=true);
    static TArray<FVector> ExitRoute();
};
