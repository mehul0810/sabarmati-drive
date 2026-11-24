#include "RiverfrontLocomotionInstance.h"
#include "RiverfrontCharacter.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "BoneControllers/AnimNode_TwoBoneIK.h"

namespace
{
struct FRiverfrontLocomotionProxy : FAnimInstanceProxy
{
    explicit FRiverfrontLocomotionProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Action.SlotName = TEXT("DefaultSlot");
        Action.bAlwaysUpdateSourcePose = true;
        Action.Source.SetLinkNode(&Ground);
        ToComponent.LocalPose.SetLinkNode(&Action);
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto& Hand = Hands[Index];
            Hand.IKBone.BoneName = Index ? TEXT("Handr") : TEXT("Handl");
            Hand.EffectorLocationSpace = BCS_WorldSpace;
            Hand.JointTargetLocationSpace = BCS_WorldSpace;
            Hand.bAllowStretching = false;
            Hand.Alpha = 0.f;
        }
        Hands[0].ComponentPose.SetLinkNode(&ToComponent);
        Hands[1].ComponentPose.SetLinkNode(&Hands[0]);
        ToLocal.ComponentPose.SetLinkNode(&Hands[1]);
    }

    FAnimNode_BlendSpacePlayer_Standalone Ground;
    FAnimNode_Slot Action;
    FAnimNode_ConvertLocalToComponentSpace ToComponent;
    FAnimNode_TwoBoneIK Hands[2];
    FAnimNode_ConvertComponentToLocalSpace ToLocal;

    virtual FAnimNode_Base* GetCustomRootNode() override { return &ToLocal; }

    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
    {
        OutNodes.Add(&Action);
        OutNodes.Add(&Ground);
        OutNodes.Add(&ToComponent);
        OutNodes.Add(&Hands[0]);
        OutNodes.Add(&Hands[1]);
        OutNodes.Add(&ToLocal);
    }

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const APawn* Pawn = Instance->TryGetPawnOwner();
        Ground.SetPosition(FVector(Pawn ? Pawn->GetVelocity().Size2D() : 0., 0., 0.));
        const auto* Resident = Cast<ARiverfrontCharacter>(Pawn);
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto& Hand = Hands[Index];
            Hand.Alpha = Resident ? Resident->GetTraversalHandContact(Index != 0,
                Hand.EffectorLocation, Hand.JointTargetLocation) : 0.f;
        }
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
