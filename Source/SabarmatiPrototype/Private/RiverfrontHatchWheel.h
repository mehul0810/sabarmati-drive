#pragma once

#include "ChaosVehicleWheel.h"
#include "RiverfrontHatchWheel.generated.h"

UCLASS()
class URiverfrontHatchWheel : public UChaosVehicleWheel
{
    GENERATED_BODY()
public:
    URiverfrontHatchWheel();
};

UCLASS()
class URiverfrontHatchRearWheel : public URiverfrontHatchWheel
{
    GENERATED_BODY()
public:
    URiverfrontHatchRearWheel();
};
