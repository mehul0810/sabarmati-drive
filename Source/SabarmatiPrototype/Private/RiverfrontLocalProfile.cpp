#include "RiverfrontLocalProfile.h"

FString URiverfrontLocalProfile::CleanDisplayName(const FString& Input)
{
    FString Name = Input.TrimStartAndEnd();
    if (Name.Len() < 2 || Name.Len() > 24)
    {
        return FString();
    }
    for (const TCHAR Character : Name)
    {
        // Keep international display names while excluding control characters and line breaks.
        const uint32 CodePoint = static_cast<uint32>(Character);
        if (CodePoint < 32 || (CodePoint >= 127 && CodePoint <= 159) ||
            CodePoint == 0x2028 || CodePoint == 0x2029 ||
            (CodePoint >= 0x202A && CodePoint <= 0x202E) ||
            (CodePoint >= 0x2066 && CodePoint <= 0x2069))
        {
            return FString();
        }
    }
    return Name;
}
