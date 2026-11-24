#include "RiverfrontCharacter.h"
#include "RiverfrontLocomotionInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

namespace
{
struct FTraversalKey
{
    float Time;
    double Distance;
    double Height;
};

FRiverfrontTraversalSection MakeTraversalSection(UObject* Outer, TConstArrayView<FTraversalKey> Keys,
    int32 First, int32 Last, const FVector& Origin, const FVector& Forward)
{
    const FTraversalKey& Begin = Keys[First];
    const FTraversalKey& End = Keys[Last];
    FRiverfrontTraversalSection Section;
    Section.Start = Origin + Forward * Begin.Distance + FVector::UpVector * Begin.Height;
    Section.End = Origin + Forward * End.Distance + FVector::UpVector * End.Height;
    Section.From = Begin.Time;
    Section.To = End.Time;
    Section.Offset = NewObject<UCurveVector>(Outer);
    const float Span = Section.To - Section.From;
    for (const int32 Axis : {0, 2})
    {
        const auto Value = [Axis](const FTraversalKey& Key) { return Axis == 0 ? Key.Distance : Key.Height; };
        const double Total = Value(End) - Value(Begin);
        FRichCurve& Channel = Section.Offset->FloatCurves[Axis];
        for (int32 Index = First; Index <= Last; ++Index)
        {
            const auto& Key = Keys[Index];
            const float T = (Key.Time - Section.From) / Span;
            double Slope = 0.;
            if (Index > 0 && Index + 1 < Keys.Num())
            {
                const double In = (Value(Key) - Value(Keys[Index - 1])) / (Key.Time - Keys[Index - 1].Time);
                const double Out = (Value(Keys[Index + 1]) - Value(Key)) / (Keys[Index + 1].Time - Key.Time);
                if (In * Out > 0.) Slope = 2. * In * Out / (In + Out);
            }
            FRichCurveKey& OffsetKey = Channel.GetKey(Channel.AddKey(T, Value(Key) - Value(Begin) - T * Total));
            OffsetKey.InterpMode = RCIM_Cubic;
            OffsetKey.TangentMode = RCTM_User;
            OffsetKey.ArriveTangent = OffsetKey.LeaveTangent = Slope * Span - Total;
        }
    }
    return Section;
}
}

bool ARiverfrontCharacter::TryStreetTraversal()
{
    auto* Move = GetCharacterMovement();
    auto* Anim = Cast<URiverfrontLocomotionInstance>(GetAvatarMesh()->GetAnimInstance());
    if (!Anim || !Move->IsMovingOnGround() || IsHidden() || (!FenceVault && !WallMantle)) return false;

    const auto* Capsule = GetCapsuleComponent();
    const double Radius = Capsule->GetScaledCapsuleRadius();
    const double HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector Start = GetActorLocation();
    const FVector Feet = Start - FVector::UpVector * HalfHeight;
    const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
    const UWorld* World = GetWorld();
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(StreetTraversal), false, this);
    const FCollisionObjectQueryParams Static(ECC_WorldStatic);
    const auto Trace = [&](FHitResult& Hit, const FVector& From, const FVector& To)
    {
        return World->LineTraceSingleByObjectType(Hit, From, To, Static, Query);
    };

    FHitResult Face;
    for (const double Height : {65., 88., 112.})
    {
        const FVector From = Feet + FVector::UpVector * Height;
        FHitResult Hit;
        if (Trace(Hit, From, From + Forward * (Radius + 86.)) && !Hit.bStartPenetrating &&
            Hit.ImpactNormal.Dot(Forward) < -.80 && (!Face.bBlockingHit || Hit.Distance < Face.Distance))
            Face = Hit;
    }
    if (!Face.bBlockingHit) return false;
    const FVector Direction = -Face.ImpactNormal.GetSafeNormal2D();
    const FVector Across = FVector::CrossProduct(FVector::UpVector, Direction);
    const double FaceDistance = (Face.ImpactPoint - Feet).Dot(Direction);
    if (FaceDistance < Radius - 2.) return false;
    const FVector Front = Feet + Direction * FaceDistance;
    const auto FloorAt = [&](FHitResult& Hit, const FVector& At, double High, double Low)
    {
        return Trace(Hit, At + FVector::UpVector * High, At + FVector::UpVector * Low) && Move->IsWalkable(Hit);
    };
    FHitResult Top;
    if (!FloorAt(Top, Front + Direction * 5., 168., 53.) || Top.GetComponent() != Face.GetComponent()) return false;
    const double ObstacleHeight = Top.ImpactPoint.Z - Feet.Z;
    if (ObstacleHeight < 60. || ObstacleHeight > 145.) return false;

    // A narrow rail has clear air on the far side. A solid ledge needs a full
    // standing capsule and support under both feet on the upper surface.
    double BackEdge = 0.;
    for (const double Depth : {13., 21., 33., 46., 59.})
    {
        FHitResult Sample;
        if (!FloorAt(Sample, Front + Direction * Depth, ObstacleHeight + 8., ObstacleHeight - 6.))
        {
            BackEdge = Depth;
            break;
        }
    }
    const bool bWall = BackEdge == 0.;
    if ((bWall && ObstacleHeight < 86.) || (!bWall && (ObstacleHeight > 104. || BackEdge > 46.))) return false;
    auto* Clip = bWall ? WallMantle.Get() : FenceVault.Get();
    if (!Clip) return false;

    const FVector Destination = Front + Direction * (Radius + (bWall ? 26. : BackEdge + 21.));
    FHitResult GroundHit;
    if (!FloorAt(GroundHit, Destination, bWall ? ObstacleHeight + 8. : 26., bWall ? ObstacleHeight - 7. : -38.)) return false;
    for (const double Side : {-1., 1.})
    {
        FHitResult Support;
        if (!FloorAt(Support, Destination + Across * (Side * Radius * .7), ObstacleHeight + 9., -40.) ||
            FMath::Abs(Support.ImpactPoint.Z - GroundHit.ImpactPoint.Z) > 8.) return false;
    }
    const double Advance = (Destination - Feet).Dot(Direction);
    const double LandHeight = GroundHit.ImpactPoint.Z - Feet.Z + 2.5;
    const double Approach = FMath::Max(0., FaceDistance - Radius - 4.);
    const double Apex = ObstacleHeight + 9.;
    const TArray<FTraversalKey, TInlineAllocator<8>> Keys = bWall
        ? TArray<FTraversalKey, TInlineAllocator<8>>{{0.f, 0., 0.}, {.16f, Approach*.61, 8.},
            {.33f, Approach, ObstacleHeight*.48}, {.57f, Approach, Apex},
            {.84f, Advance-11., Apex}, {1.f, Advance, LandHeight}}
        : TArray<FTraversalKey, TInlineAllocator<8>>{{0.f, 0., 0.}, {.18f, Approach*.78, ObstacleHeight*.16},
            {.30f, Approach, ObstacleHeight*.43}, {.45f, Approach, Apex}, {.65f, Advance-19., Apex},
            {.82f, Advance-4., ObstacleHeight*.49}, {1.f, Advance, LandHeight}};
    // Character Movement sweeps straight to each frame's target. Separate the
    // hand plant, lift, crossing and landing so a long frame cannot skip them.
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    const FRotator Facing = Direction.Rotation();
    TArray<FRiverfrontTraversalSection> Sections;
    Sections.Reserve(4);
    FVector Previous = Start;
    int32 First = 0;
    for (const int32 Last : {2, 3, 4, Keys.Num() - 1})
    {
        const auto& Section = Sections.Add_GetRef(MakeTraversalSection(this, Keys, First, Last, Start, Direction));
        for (int32 Step = 1; Step <= 8; ++Step)
        {
            const float T = Step / 8.f;
            const FVector Next = FMath::Lerp(Section.Start, Section.End, T) + Facing.RotateVector(Section.Offset->GetVectorValue(T));
            FHitResult Block;
            if (World->SweepSingleByChannel(Block, Previous, Next, FQuat::Identity, Capsule->GetCollisionObjectType(),
                Shape, Query, Responses) && (Block.bStartPenetrating || Block.Time < .999f)) return false;
            Previous = Next;
        }
        First = Last;
    }

    StopJumping();
    ConsumeMovementInputVector();
    UAnimMontage* Montage = Anim->PlaySlotAnimationAsDynamicMontage(Clip, TEXT("DefaultSlot"), .085f, .12f);
    if (!Montage) return false;
    TraversalSections = MoveTemp(Sections);
    TraversalSection = 0;
    TraversalDuration = Clip->GetPlayLength();
    TraversalMontage = Montage;
    bMantling = bWall;
    TraversalEdge = Top.ImpactPoint;
    TraversalFacing = Direction;
    bOrientBeforeTraversal = Move->bOrientRotationToMovement;
    Move->StopMovementImmediately();
    Move->bOrientRotationToMovement = false;
    SetActorRotation(Facing);
    Move->SetMovementMode(MOVE_Flying);
    StartTraversalSection();
    return true;
}

void ARiverfrontCharacter::StartTraversalSection()
{
    const auto& Section = TraversalSections[TraversalSection];
    auto Motion = MakeShared<FRootMotionSource_MoveToForce>();
    Motion->InstanceName = TEXT("StreetTraversal");
    Motion->Priority = 100;
    Motion->Duration = (Section.To - Section.From) * TraversalDuration;
    Motion->StartLocation = Section.Start;
    Motion->TargetLocation = Section.End;
    Motion->PathOffsetCurve = Section.Offset;
    Motion->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
    TraversalSource = GetCharacterMovement()->ApplyRootMotionSource(Motion);
}

void ARiverfrontCharacter::UpdateTraversal()
{
    if (!TraversalSource) return;
    auto* Move = GetCharacterMovement();
    const auto Motion = StaticCastSharedPtr<FRootMotionSource_MoveToForce>(Move->GetRootMotionSourceByID(TraversalSource));
    const auto& Section = TraversalSections[TraversalSection];
    const bool bFinished = !Motion || Motion->Status.HasFlag(ERootMotionSourceStatusFlags::Finished);
    const float T = bFinished ? 1.f : FMath::Clamp(Motion->GetTime() / Motion->Duration, 0.f, 1.f);
    const FVector Expected = FMath::Lerp(Section.Start, Section.End, T)
        + TraversalFacing.Rotation().RotateVector(Section.Offset->GetVectorValue(T));
    if (!Move->IsFlying() || FVector::DistSquared(Expected, GetActorLocation()) > FMath::Square(bFinished ? 5. : 30.))
    {
        FinishTraversal(true);
        return;
    }
    if (!bFinished) return;
    if (TraversalSection + 1 == TraversalSections.Num())
    {
        FinishTraversal(false);
        return;
    }
    // The animation clock may have advanced further than the capsule during a long frame.
    if (UAnimMontage* Montage = TraversalMontage.Get())
        if (auto* Anim = GetAvatarMesh()->GetAnimInstance(); Anim && Anim->Montage_IsActive(Montage))
            Anim->Montage_SetPosition(Montage, Section.To * TraversalDuration);
    Move->RemoveRootMotionSourceByID(TraversalSource);
    ++TraversalSection;
    StartTraversalSection();
}

void ARiverfrontCharacter::FinishTraversal(bool bInterrupted)
{
    if (!TraversalSource) return;
    auto* Move = GetCharacterMovement();
    Move->RemoveRootMotionSourceByID(TraversalSource);
    TraversalSource = 0;
    TraversalSections.Reset();
    TraversalMontage.Reset();
    Move->bOrientRotationToMovement = bOrientBeforeTraversal;
    Move->StopMovementImmediately();
    if (Move->IsFlying())
    {
        FFindFloorResult Floor;
        Move->FindFloor(GetActorLocation(), Floor, false);
        Move->SetMovementMode(Floor.IsWalkableFloor() && Floor.GetDistanceToFloor() < 8. ? MOVE_Walking : MOVE_Falling);
    }
    bWasAirborne = bPlayedFall = false;
    if (bInterrupted)
        if (auto* Anim = GetAvatarMesh()->GetAnimInstance())
            Anim->StopSlotAnimation(.14f, TEXT("DefaultSlot"));
}

float ARiverfrontCharacter::GetTraversalHandContact(bool bRight, FVector& OutPalm, FVector& OutElbow) const
{
    if (!TraversalSource || (!bMantling && bRight)) return 0.f;
    const auto Motion = GetCharacterMovement()->GetRootMotionSourceByID(TraversalSource);
    if (!Motion) return 0.f;
    const auto& Section = TraversalSections[TraversalSection];
    const float T = FMath::Lerp(Section.From, Section.To, FMath::Clamp(Motion->GetTime() / Motion->Duration, 0.f, 1.f));
    const float Reach = FMath::SmoothStep(.10f, bMantling ? .25f : .21f, T);
    const float Release = FMath::SmoothStep(bMantling ? .35f : .27f, bMantling ? .51f : .43f, T);
    const FVector Across = FVector::CrossProduct(FVector::UpVector, TraversalFacing) * (bRight ? 1. : -1.);
    OutPalm = TraversalEdge - TraversalFacing * 2. + Across * 17. + FVector::UpVector * 3.;
    OutElbow = GetActorLocation() + Across * 52. - TraversalFacing * 8. + FVector::UpVector * 6.;
    return Reach * (1.f - Release);
}
