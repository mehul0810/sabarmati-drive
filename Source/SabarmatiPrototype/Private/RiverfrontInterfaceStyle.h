#pragma once

#include "Math/Color.h"

namespace RiverfrontInterface
{
inline const FLinearColor Ink(FColor(13,20,27));
inline const FLinearColor Surface(FColor(23,33,42));
inline const FLinearColor Text(FColor(245,242,233));
inline const FLinearColor Muted(FColor(179,191,199));
inline const FLinearColor Accent(FColor(248,190,112));
inline const FLinearColor Error(FColor(248,120,88));
inline const FLinearColor Line=Muted.CopyWithNewOpacity(.29f);
inline const FLinearColor Selection=Accent.CopyWithNewOpacity(.18f);
}
