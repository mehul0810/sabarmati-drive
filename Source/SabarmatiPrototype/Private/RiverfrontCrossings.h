#pragma once

#include "CoreMinimal.h"

// Centimetres in the straightened riverfront corridor. The deck meets the existing
// bank road at grade and clears a standing person on the lower promenade.
namespace RiverfrontCrossings
{
constexpr double CentreY = -10960.;
constexpr double WalkZ = -84.;
inline double DeckZ(double WorldY)
{
    const double Offset = FMath::Abs(WorldY - CentreY);
    return 158. * FMath::Clamp((14760. - Offset) / 1280., 0., 1.);
}
}
