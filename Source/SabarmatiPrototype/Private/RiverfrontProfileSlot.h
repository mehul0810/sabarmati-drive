#pragma once

#include "CoreMinimal.h"

class URiverfrontLocalProfile;

struct FRiverfrontProfileSlot
{
    FString Name;
    // Unset uses Unreal's normal slot. A direct Mac launch supplies its shared SaveGames folder.
    TOptional<FString> DirectSaveDirectory;

    static FRiverfrontProfileSlot ForLaunch(const FString& Slot);
    bool Exists() const;
    URiverfrontLocalProfile* Load() const;
    URiverfrontLocalProfile* Read(bool* bFromLegacyDirectory = nullptr) const;
    bool Write(URiverfrontLocalProfile& Profile) const;
};
