#pragma once

#include "Engine/SkeletalMesh.h"

namespace RiverfrontResident
{
// The public CC0 and optional local rigs were authored with opposite forward axes.
inline double YawToActorForward(const USkeletalMesh& Model)
{
    return Model.GetPathName().StartsWith(TEXT("/Game/Sabarmati/Riverfront/People/")) ? -90. : 90.;
}
}
