#include "RiverfrontLocomotionInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "GameFramework/Pawn.h"

namespace
{
struct FRiverfrontLocomotionProxy : FAnimInstanceProxy
{
    explicit FRiverfrontLocomotionProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Action.SlotName = TEXT("DefaultSlot");
        Action.bAlwaysUpdateSourcePose = true;
        Action.Source.SetLinkNode(&Ground);
    }

    FAnimNode_BlendSpacePlayer_Standalone Ground;
    FAnimNode_Slot Action;

    virtual FAnimNode_Base* GetCustomRootNode() override { return &Action; }

    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
    {
        OutNodes.Add(&Action);
        OutNodes.Add(&Ground);
    }

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const APawn* Pawn = Instance->TryGetPawnOwner();
        Ground.SetPosition(FVector(Pawn ? Pawn->GetVelocity().Size2D() : 0., 0., 0.));
    }
};
}

void URiverfrontLocomotionInstance::SetGait(UBlendSpace* Gait)
{
    GetProxyOnGameThread<FRiverfrontLocomotionProxy>().Ground.SetBlendSpace(Gait);
}

float URiverfrontLocomotionInstance::GetGaitPhase() const
{
    return GetProxyOnGameThread<FRiverfrontLocomotionProxy>().Ground.GetAccumulatedTime();
}

FAnimInstanceProxy* URiverfrontLocomotionInstance::CreateAnimInstanceProxy()
{
    return new FRiverfrontLocomotionProxy(this);
}

void URiverfrontLocomotionInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
    delete Proxy;
}
