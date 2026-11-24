#pragma once

#include "Animation/AnimInstance.h"
#include "RiverfrontLocomotionInstance.generated.h"

class UBlendSpace;
class ARiverfrontCharacter;

UCLASS(Transient)
class URiverfrontLocomotionInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    void SetGait(UBlendSpace* Gait, ARiverfrontCharacter* Player);
    float GetGaitPhase() const;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
