#include "RiverfrontPedestrian.h"

#include "RiverfrontCharacter.h"
#include "RiverfrontPlayerController.h"
#include "Engine/World.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

void ARiverfrontPedestrian::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    OutLocation = GetActorLocation() + FVector(0., 0., 145.);
    OutRotation = FRotator(0., GetActorRotation().Yaw - MeshYawToForward, 0.);
}

void ARiverfrontPedestrian::EnablePlayerAwareness()
{
    if (Awareness) return;
    Awareness = NewObject<UAIPerceptionComponent>(this, TEXT("ResidentAwareness"));
    AddInstanceComponent(Awareness);
    auto* Sight = NewObject<UAISenseConfig_Sight>(Awareness);
    Sight->SightRadius = 450.f;
    Sight->LoseSightRadius = 550.f;
    Sight->PeripheralVisionAngleDegrees = 95.f;
    Sight->DetectionByAffiliation.bDetectEnemies = true;
    Sight->DetectionByAffiliation.bDetectFriendlies = true;
    Sight->DetectionByAffiliation.bDetectNeutrals = true;
    Awareness->ConfigureSense(*Sight);
    auto* Hearing = NewObject<UAISenseConfig_Hearing>(Awareness);
    Hearing->HearingRange = 400.f;
    Hearing->DetectionByAffiliation.bDetectEnemies = true;
    Hearing->DetectionByAffiliation.bDetectFriendlies = true;
    Hearing->DetectionByAffiliation.bDetectNeutrals = true;
    Awareness->ConfigureSense(*Hearing);
    Awareness->SetDominantSense(UAISense_Sight::StaticClass());
    Awareness->OnTargetPerceptionUpdated.AddDynamic(this, &ARiverfrontPedestrian::OnPlayerPerception);
    Awareness->RegisterComponent();
}

void ARiverfrontPedestrian::OnPlayerPerception(AActor* Actor, FAIStimulus Stimulus)
{
    const auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || Actor != PC->GetPawn()) return;
    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
        bPlayerVisible = Stimulus.WasSuccessfullySensed();
    else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>()
        && Stimulus.WasSuccessfullySensed() && Stimulus.Tag == TEXT("PlayerFootstep"))
        LastPlayerFootstep = GetWorld()->GetTimeSeconds();
}

bool ARiverfrontPedestrian::TickPlayerAwareness(float Delta)
{
    auto* PC = Cast<ARiverfrontPlayerController>(GetWorld()->GetFirstPlayerController());
    const auto* Player = PC ? Cast<ARiverfrontCharacter>(PC->GetPawn()) : nullptr;
    const bool bAvailable = Player && !Player->IsHidden() && !PC->IsShowingMenu() && !PC->IsWaking();
    const double PlayerDistance = bAvailable ? FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) : 10000.;

    const auto ForEachPerson = [this](auto&& Action)
    {
        if (Family.IsEmpty()) Action(this);
        else for (const auto& Member : Family) if (auto* Person = Member.Get()) Action(Person);
    };

    if (ReactionLeft > 0.)
    {
        ReactionLeft = bAvailable && PlayerDistance < 450. ? FMath::Max(0., ReactionLeft - Delta) : 0.;
        ForEachPerson([&](ARiverfrontPedestrian* Person)
        {
            Person->UpdateGait(Delta);
            if (ReactionLeft > 0.) Person->LookTowards(Player->GetActorLocation(), Delta);
            else Person->SetMoving(Person->bResumeMoving);
        });
        return true;
    }

    if (PlayerDistance > 350.) bNeedsSpace = false;
    const bool bRecentlyHeard = GetWorld()->GetTimeSeconds() - LastPlayerFootstep < 1.3;
    const bool bClose = bAvailable && PlayerDistance < 225.
        && FMath::Abs(Player->GetActorLocation().Z - GetActorLocation().Z) < 170.;
    if (!bNeedsSpace && bClose && !IsCrossingStreet() && (bPlayerVisible || bRecentlyHeard))
        AttentionTime += Delta;
    else
        AttentionTime = FMath::Max(0., AttentionTime - 2. * Delta);
    if (AttentionTime < 2.5) return false;

    AttentionTime = 0.;
    bNeedsSpace = true;
    ReactionLeft = 2.8;
    ++ReactionCount;
    ForEachPerson([](ARiverfrontPedestrian* Person)
    {
        Person->bResumeMoving = Person->bMoving;
        Person->SetMoving(false);
    });
    PC->NotifyResidentReaction(*this, ReactionCount);
    UE_LOG(LogTemp, Display, TEXT("Sabarmati resident: %s noticed the player, reaction %d."), *GetName(), ReactionCount);
    return true;
}
