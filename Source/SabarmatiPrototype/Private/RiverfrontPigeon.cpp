#include "RiverfrontPigeon.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "RiverfrontWorld.h"

namespace
{
constexpr double FlightSeconds = 4.7;
constexpr double VisibleDistance = 2750.;
constexpr float GroundTickInterval = .16f;

FVector FlightOffset(double Phase, const FVector& Direction)
{
    const double Forward = 390. * Phase + 3150. * FMath::Square(Phase);
    const double Height = 220. * FMath::SmoothStep(0., .30, Phase) + 680. * FMath::Square(Phase);
    return Direction * Forward + FVector(0., 0., Height);
}

bool ClearDeparture(UWorld& World, const FVector& Home, const FVector& Direction)
{
    const FCollisionObjectQueryParams StaticGeometry(ECC_WorldStatic);
    const FCollisionQueryParams Probe(SCENE_QUERY_STAT(RiverfrontPigeonDeparture), false);
    const FCollisionShape Body = FCollisionShape::MakeSphere(25.f);
    const FVector Centre = Home + FVector(0., 0., 30.);
    FVector Previous = Centre;
    for (int32 Point = 1; Point <= 12; ++Point)
    {
        const FVector Next = Centre + FlightOffset(Point / 12., Direction);
        if (World.SweepTestByObjectType(Previous, Next, FQuat::Identity, StaticGeometry, Body, Probe))
            return false;
        Previous = Next;
    }
    return true;
}
}

ARiverfrontPigeon::ARiverfrontPigeon()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = GroundTickInterval;
    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Bird"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetCullDistance(VisibleDistance);
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Mesh->bEnableUpdateRateOptimizations = true;
}

void ARiverfrontPigeon::SpawnPromenadeFlock(ARiverfrontWorld& District)
{
    const FString Folder = TEXT("/Game/Sabarmati/Riverfront/Wildlife/Pigeon/");
    if (!FPackageName::DoesPackageExist(Folder + TEXT("SK_Pigeon"))) return;
    USkeletalMesh* Model = LoadObject<USkeletalMesh>(nullptr, *(Folder + TEXT("SK_Pigeon.SK_Pigeon")));
    if (!Model) return;
    const auto LoadClip = [&Folder, Model](const TCHAR* Action) -> UAnimSequence*
    {
        const FString Name = FString(TEXT("A_Pigeon")) + Action;
        auto* Clip = LoadObject<UAnimSequence>(nullptr, *(Folder + Name + TEXT(".") + Name));
        return Clip && Clip->GetSkeleton() == Model->GetSkeleton() ? Clip : nullptr;
    };
    UAnimSequence* IdleClip = LoadClip(TEXT("Idle"));
    UAnimSequence* PeckClip = LoadClip(TEXT("Peck"));
    UAnimSequence* FlightClip = LoadClip(TEXT("Fly"));
    UAnimSequence* GlideClip = LoadClip(TEXT("Glide"));
    if (!IdleClip || !PeckClip || !FlightClip || !GlideClip)
    {
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati wildlife: the pigeon mesh needs its four animation clips."));
        return;
    }

    // One small group on this open section of upper promenade. Its path is
    // checked against the actual deck and rails before the birds are placed.
    const FVector Spots[] = {{-4020., 3295., 2.}, {-4126., 3230., 2.}, {-4210., 3324., 2.}};
    UWorld& World = *District.GetWorld();
    const FCollisionObjectQueryParams StaticGeometry(ECC_WorldStatic);
    const FCollisionQueryParams Probe(SCENE_QUERY_STAT(RiverfrontPigeonGround), false);
    const FVector MeshForward = Model->GetComposedRefPoseMatrix(TEXT("Head")).GetOrigin()
        - Model->GetComposedRefPoseMatrix(TEXT("Hips")).GetOrigin();
    int32 Spawned = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Spots); ++Index)
    {
        const FVector Spot = Spots[Index];
        FHitResult Floor;
        if (!World.LineTraceSingleByObjectType(Floor, Spot + FVector(0., 0., 35.), Spot - FVector(0., 0., 15.),
                StaticGeometry, Probe) || Floor.ImpactNormal.Z < .92 || FMath::Abs(Floor.ImpactPoint.Z - Spot.Z) > 3.)
            continue;
        const FVector Direction = FVector(.30 * (Index - 1), -1., 0.).GetSafeNormal2D();
        const FVector HomePoint = Floor.ImpactPoint + FVector(0., 0., .3);
        if (!ClearDeparture(World, HomePoint, Direction)) continue;

        auto* Pigeon = World.SpawnActor<ARiverfrontPigeon>(HomePoint, FRotator::ZeroRotator);
        if (!Pigeon) continue;
        Pigeon->Mesh->SetSkeletalMesh(Model);
        Pigeon->Idle = IdleClip;
        Pigeon->Peck = PeckClip;
        Pigeon->Fly = FlightClip;
        Pigeon->Glide = GlideClip;
        Pigeon->Home = HomePoint;
        Pigeon->FlightDirection = Direction;
        Pigeon->ForwardYawOffset = -MeshForward.Rotation().Yaw;
        Pigeon->HomeYaw = 65. + 109. * Index + Pigeon->ForwardYawOffset;
        Pigeon->AnimationRate = .91f + .07f * Index;
        Pigeon->ReturnToGround();
        Pigeon->Mesh->SetPosition(IdleClip->GetPlayLength() * (.17f + .29f * Index), false);
        ++Spawned;
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati wildlife: %d pigeons on the Atal promenade."), Spawned);
}

void ARiverfrontPigeon::Play(UAnimSequence* Clip, bool bLoop)
{
    Mesh->PlayAnimation(Clip, bLoop);
    Mesh->SetPlayRate(AnimationRate);
}

void ARiverfrontPigeon::ReturnToGround()
{
    Activity = EActivity::Ground;
    SetActorLocationAndRotation(Home, FRotator(0., HomeYaw, 0.));
    SetActorHiddenInGame(false);
    SetActorTickInterval(GroundTickInterval);
    NextPeck = GetWorld()->GetTimeSeconds() + 2. + AnimationRate * 2.1;
    Play(Idle, true);
}

void ARiverfrontPigeon::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now = GetWorld()->GetTimeSeconds();
    if (Activity == EActivity::Flapping || Activity == EActivity::Gliding)
    {
        const double Phase = FMath::Clamp((Now - FlightStart) / FlightSeconds, 0., 1.);
        const double TargetYaw = FlightDirection.Rotation().Yaw + ForwardYawOffset;
        const double Yaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, 470. * DeltaSeconds);
        SetActorLocationAndRotation(Home + FlightOffset(Phase, FlightDirection), FRotator(0., Yaw, 0.));
        if (Activity == EActivity::Flapping && Phase > .60)
        {
            Activity = EActivity::Gliding;
            Play(Glide, true);
        }
        if (Phase >= 1.)
        {
            Activity = EActivity::Away;
            ReturnAfter = Now + 12. / AnimationRate;
            SetActorHiddenInGame(true);
            SetActorTickInterval(.6f);
        }
        return;
    }

    const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    const APawn* Player = Controller ? Controller->GetPawn() : nullptr;
    if (!Player) return;
    const FVector ToPlayer = Player->GetActorLocation() - Home;
    if (Activity == EActivity::Away)
    {
        // Repopulate beyond draw distance, so a waiting player never sees a respawn.
        FVector Eye;
        FRotator View;
        Controller->GetPlayerViewPoint(Eye, View);
        if (Now >= ReturnAfter && FVector::DistSquared(Eye, Home) > FMath::Square(VisibleDistance + 360.))
            ReturnToGround();
        return;
    }
    if (ToPlayer.SizeSquared() > FMath::Square(VisibleDistance + 500.)) return;
    if (FMath::Abs(ToPlayer.Z) < 210. && ToPlayer.SizeSquared2D() < FMath::Square(195. + 32. * AnimationRate))
    {
        Activity = EActivity::Flapping;
        FlightStart = Now;
        SetActorTickInterval(0.f);
        Play(Fly, true);
        return;
    }
    if (Now >= NextPeck)
    {
        const bool bPecking = Activity == EActivity::Pecking;
        Activity = bPecking ? EActivity::Ground : EActivity::Pecking;
        Play(bPecking ? Idle.Get() : Peck.Get(), bPecking);
        NextPeck = Now + (bPecking ? 4. * AnimationRate : Peck->GetPlayLength() / AnimationRate);
    }
}
