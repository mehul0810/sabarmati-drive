#pragma once

#include "CoreMinimal.h"
#include "RiverfrontResidentAppearance.generated.h"

class USkeletalMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ERiverfrontGender : uint8
{
    Male,
    Female
};

namespace RiverfrontResident
{
    SABARMATIPROTOTYPE_API UMaterialInterface* LoadSkinMaterial(ERiverfrontGender Gender);
    SABARMATIPROTOTYPE_API USkeletalMesh* LoadOutfitMesh(ERiverfrontGender Gender, bool bKurta);
}
