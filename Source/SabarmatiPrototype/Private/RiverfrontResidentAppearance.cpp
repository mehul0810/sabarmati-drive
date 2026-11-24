#include "RiverfrontResidentAppearance.h"

#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"

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

UMaterialInterface* RiverfrontResident::LoadSkinMaterial(ERiverfrontGender Gender)
{
    return LoadObject<UMaterialInterface>(nullptr, Gender == ERiverfrontGender::Female
        ? TEXT("/Game/Sabarmati/Riverfront/People/Hero/Female/M_FemaleSkin.M_FemaleSkin")
        : TEXT("/Game/Sabarmati/Riverfront/People/Hero/M_HeroSkin.M_HeroSkin"));
}
