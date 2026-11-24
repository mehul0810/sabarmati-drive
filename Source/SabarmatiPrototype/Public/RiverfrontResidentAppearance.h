#pragma once

#include "CoreMinimal.h"
#include "RiverfrontResidentAppearance.generated.h"

class USkeletalMesh;

UENUM(BlueprintType)
enum class ERiverfrontGender : uint8
{
    Male,
    Female
};

namespace RiverfrontResident
{
    SABARMATIPROTOTYPE_API USkeletalMesh* LoadOutfitMesh(ERiverfrontGender Gender, bool bKurta);
}
