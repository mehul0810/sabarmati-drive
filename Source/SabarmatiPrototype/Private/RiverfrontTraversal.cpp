#include "RiverfrontCharacter.h"
#include "RiverfrontLocomotionInstance.h"

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
}

bool ARiverfrontCharacter::TryStreetTraversal()
{
    auto* Move = GetCharacterMovement();
    auto* Anim = Cast<URiverfrontLocomotionInstance>(GetMesh()->GetAnimInstance());
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
    const double Rise = Top.ImpactPoint.Z - Feet.Z;
    if (Rise < 60. || Rise > 145.) return false;

    // A narrow rail has clear air on the far side. A solid ledge needs a full
    // standing capsule and support under both feet on the upper surface.
    double BackEdge = 0.;
    for (const double Depth : {13., 21., 33., 46., 59.})
    {
        FHitResult Sample;
        if (!FloorAt(Sample, Front + Direction * Depth, Rise + 8., Rise - 6.))
        {
            BackEdge = Depth;
            break;
        }
    }
    const bool bWall = BackEdge == 0.;
    if ((bWall && Rise < 86.) || (!bWall && (Rise > 104. || BackEdge > 46.))) return false;
    auto* Clip = bWall ? WallMantle.Get() : FenceVault.Get();
    if (!Clip) return false;

    const FVector Destination = Front + Direction * (Radius + (bWall ? 26. : BackEdge + 21.));
    FHitResult Landing;
    if (!FloorAt(Landing, Destination, bWall ? Rise + 8. : 26., bWall ? Rise - 7. : -38.)) return false;
    for (const double Side : {-1., 1.})
    {
        FHitResult Support;
        if (!FloorAt(Support, Destination + Across * (Side * Radius * .7), Rise + 9., -40.) ||
            FMath::Abs(Support.ImpactPoint.Z - Landing.ImpactPoint.Z) > 8.) return false;
    }
    const double Advance = (Destination - Feet).Dot(Direction);
    const double LandHeight = Landing.ImpactPoint.Z - Feet.Z + 2.5;
    const double Approach = FMath::Max(0., FaceDistance - Radius - 4.);
    const double Apex = Rise + 9.;
    const TArray<FTraversalKey, TInlineAllocator<8>> Keys = bWall
        ? TArray<FTraversalKey, TInlineAllocator<8>>{{0.f, 0., 0.}, {.16f, Approach*.61, 8.},
            {.33f, Approach, Rise*.48}, {.57f, Approach, Apex}, {.84f, Advance-11., Apex}, {1.f, Advance, LandHeight}}
        : TArray<FTraversalKey, TInlineAllocator<8>>{{0.f, 0., 0.}, {.18f, Approach*.78, Rise*.16},
            {.30f, Approach, Rise*.43}, {.45f, Approach, Apex}, {.65f, Advance-19., Apex},
            {.82f, Advance-4., Rise*.49}, {1.f, Advance, LandHeight}};
    const FVector Target = Start + Direction * Advance + FVector::UpVector * LandHeight;
    auto* Curve = NewObject<UCurveVector>(this);
    for (int32 I = 0; I < Keys.Num(); ++I)
    {
        const auto& K = Keys[I];
        for (const int32 Axis : {0, 2})
        {
            const bool bVertical = Axis == 2;
            const auto Value = [bVertical](const FTraversalKey& Point){ return bVertical ? Point.Height : Point.Distance; };
            const double Total = bVertical ? LandHeight : Advance;
            auto& Channel = Curve->FloatCurves[Axis];
            FRichCurveKey& Key = Channel.GetKey(Channel.AddKey(K.Time, Value(K) - K.Time * Total));
            // Keep the absolute path monotonic through each clearance gate,
            // with a continuous velocity as we leave the ground and touch down.
            double Slope = 0.;
            if (I > 0 && I + 1 < Keys.Num())
            {
                const double In = (Value(K)-Value(Keys[I-1]))/(K.Time-Keys[I-1].Time);
                const double Out = (Value(Keys[I+1])-Value(K))/(Keys[I+1].Time-K.Time);
                if (In*Out > 0.) Slope = 2. * In * Out / (In + Out);
            }
            Key.InterpMode = RCIM_Cubic;
            Key.TangentMode = RCTM_User;
            Key.ArriveTangent = Key.LeaveTangent = Slope - Total;
        }
    }

    // Preflight the same curve and capsule that Character Movement will sweep.
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    const FRotator Facing = Direction.Rotation();
    FVector Previous = Start;
    for (int32 Step = 1; Step <= 32; ++Step)
    {
        const float T = Step / 32.f;
        const FVector Next = FMath::Lerp(Start, Target, T) + Facing.RotateVector(Curve->GetVectorValue(T));
        FHitResult Block;
        if (World->SweepSingleByChannel(Block, Previous, Next, FQuat::Identity, Capsule->GetCollisionObjectType(),
            Shape, Query, Responses) && (Block.bStartPenetrating || Block.Time < .999f)) return false;
        Previous = Next;
    }

    StopJumping();
    ConsumeMovementInputVector();
    const float Duration = Clip->GetPlayLength();
    if (!Anim->PlaySlotAnimationAsDynamicMontage(Clip, TEXT("DefaultSlot"), .085f, .12f)) return false;
    bMantling = bWall;
    TraversalEdge = Top.ImpactPoint;
    TraversalFacing = Direction;
    bOrientBeforeTraversal = Move->bOrientRotationToMovement;
    Move->StopMovementImmediately();
    Move->bOrientRotationToMovement = false;
    SetActorRotation(Facing);
    Move->SetMovementMode(MOVE_Flying);
    auto Motion = MakeShared<FRootMotionSource_MoveToForce>();
    Motion->InstanceName = TEXT("StreetTraversal");
    Motion->Priority = 100;
    Motion->Duration = Duration;
    Motion->StartLocation = Start;
    Motion->TargetLocation = Target;
    Motion->PathOffsetCurve = Curve;
    Motion->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
    TraversalSource = Move->ApplyRootMotionSource(Motion);
    return true;
}

void ARiverfrontCharacter::UpdateTraversal()
{
    if (!TraversalSource) return;
    auto* Move = GetCharacterMovement();
    const auto Motion = StaticCastSharedPtr<FRootMotionSource_MoveToForce>(Move->GetRootMotionSourceByID(TraversalSource));
    if (!Motion || Motion->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
    {
        FinishTraversal(false);
        return;
    }
    const float T = FMath::Clamp(Motion->GetTime() / Motion->Duration, 0.f, 1.f);
    const FVector Expected = FMath::Lerp(Motion->StartLocation, Motion->TargetLocation, T) + Motion->GetPathOffsetInWorldSpace(T);
    if (!Move->IsFlying() || FVector::DistSquared(Expected, GetActorLocation()) > FMath::Square(30.))
        FinishTraversal(true);
}

void ARiverfrontCharacter::FinishTraversal(bool bInterrupted)
{
    if (!TraversalSource) return;
    auto* Move = GetCharacterMovement();
    Move->RemoveRootMotionSourceByID(TraversalSource);
    TraversalSource = 0;
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
        if (auto* Anim = GetMesh()->GetAnimInstance())
            Anim->StopSlotAnimation(.14f, TEXT("DefaultSlot"));
}

float ARiverfrontCharacter::GetTraversalHandContact(bool bRight, FVector& OutPalm, FVector& OutElbow) const
{
    if (!TraversalSource || (!bMantling && bRight)) return 0.f;
    const auto Motion = GetCharacterMovement()->GetRootMotionSourceByID(TraversalSource);
    if (!Motion) return 0.f;
    const float T = Motion->GetTime() / Motion->Duration;
    const float Reach = FMath::SmoothStep(.10f, bMantling ? .25f : .21f, T);
    const float Release = FMath::SmoothStep(bMantling ? .35f : .27f, bMantling ? .51f : .43f, T);
    const FVector Across = FVector::CrossProduct(FVector::UpVector, TraversalFacing) * (bRight ? 1. : -1.);
    OutPalm = TraversalEdge - TraversalFacing * 2. + Across * 17. + FVector::UpVector * 3.;
    OutElbow = GetActorLocation() + Across * 52. - TraversalFacing * 8. + FVector::UpVector * 6.;
    return Reach * (1.f - Release);
}
