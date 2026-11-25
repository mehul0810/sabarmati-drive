#include "RiverfrontLocomotionInstance.h"
#include "RiverfrontCharacter.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "BoneControllers/AnimNode_TwoBoneIK.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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
    TWeakObjectPtr<ARiverfrontCharacter> Player;
    float StrokePhase=0.f;

    virtual void Initialize(UAnimInstance* Instance) override
    {
        const auto* Body = Instance->GetOwningComponent();
        const bool bEpicHands = Body && Body->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE;
        Hands[0].IKBone.BoneName = bEpicHands ? TEXT("hand_l") : TEXT("Handl");
        Hands[1].IKBone.BoneName = bEpicHands ? TEXT("hand_r") : TEXT("Handr");
        FAnimInstanceProxy::Initialize(Instance);
    }

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
        const auto* Resident = Player.Get();
        const bool Swimming=Resident && Resident->GetCharacterMovement()->IsSwimming();
        const float Cruise=Resident?FMath::Clamp(float(Resident->GetVelocity().Size2D()/145.),0.f,1.f):0.f;
        Ground.SetPosition(FVector(Swimming ? Cruise*95. : Resident ? Resident->GetVelocity().Size2D() : 0.,0.,0.));
        if (Swimming) StrokePhase=FMath::Fmod(StrokePhase+DeltaSeconds*FMath::Lerp(2.4f,4.5f,Cruise),2.f*PI);
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto& Hand = Hands[Index];
            if (Swimming)
            {
                const float Phase=StrokePhase+Index*PI, Side=Index?1.f:-1.f;
                const auto& T=Resident->GetActorTransform();
                Hand.EffectorLocation=T.TransformPosition(FVector(32.+Cruise*41.f*FMath::Cos(Phase),
                    Side*(34.f+12.f*FMath::Sin(Phase)),16.+14.*FMath::Sin(Phase)));
                Hand.JointTargetLocation=T.TransformPosition(FVector(18.,Side*68.,35.));
                Hand.Alpha=1.f;
                continue;
            }
            Hand.Alpha = Resident ? Resident->GetTraversalHandContact(Index != 0,
                Hand.EffectorLocation, Hand.JointTargetLocation) : 0.f;
        }
    }
};
}

void URiverfrontLocomotionInstance::SetGait(UBlendSpace* Gait, ARiverfrontCharacter* Player)
{
    auto& Proxy = GetProxyOnGameThread<FRiverfrontLocomotionProxy>();
    Proxy.Player = Player;
    Proxy.Ground.SetBlendSpace(Gait);
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
