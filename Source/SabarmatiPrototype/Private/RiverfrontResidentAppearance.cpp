#include "RiverfrontResidentAppearance.h"

#include "Engine/SkeletalMesh.h"

USkeletalMesh* RiverfrontResident::LoadOutfitMesh(ERiverfrontGender Gender, bool bKurta)
{
    const TCHAR* Path;
    switch (Gender)
    {
    case ERiverfrontGender::Male:
        Path = bKurta
            ? TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Tailored/SK_FittedKurta.SK_FittedKurta")
            : TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Everyday/SK_RiverEveryday.SK_RiverEveryday");
        break;
    case ERiverfrontGender::Female:
        Path = bKurta
            ? TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Female/SK_FemaleKurta.SK_FemaleKurta")
            : TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Female/SK_FemaleEveryday.SK_FemaleEveryday");
        break;
    default:
        return nullptr;
    }
    return LoadObject<USkeletalMesh>(nullptr, Path);
}
