#include "RiverfrontPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "RiverfrontCharacter.h"
#include "RiverfrontPedestrian.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
enum class EAwarenessStep : uint8 { Approach, Observe, Resume, Follow, FamilyPause, FamilyResume, WaitForCrossing, Crossing };
struct FAwarenessReview
{
    EAwarenessStep Step = EAwarenessStep::Follow;
    double Since = FPlatformTime::Seconds();
    TWeakObjectPtr<ARiverfrontPedestrian> Visitor, Leader;
    TArray<TWeakObjectPtr<ARiverfrontPedestrian>> Family;
    TArray<FVector> FamilyPositions;
    FVector Start = FVector::ZeroVector;
    bool bHeardFootstep = false;
    bool bVisitorPicture = false, bFamilyPicture = false;
    int32 ReactionsBeforeCrossing = 0;
};
}

void ARiverfrontPlayerController::RunAwarenessReview()
{
    auto State = MakeShared<FAwarenessReview>();
    for (TActorIterator<ARiverfrontPedestrian> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("SightseeingVisitor"))) State->Visitor = *It;
        if (It->IsFamilyMember())
        {
            State->Family.Add(*It);
            if (It->HasPlayerAwareness()) State->Leader = *It;
        }
    }
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this,State](float)
    {
        auto* Walker = Cast<ARiverfrontCharacter>(GetPawn());
        auto* Visitor = State->Visitor.Get();
        auto* Leader = State->Leader.Get();
        const double Now = FPlatformTime::Seconds(), Age = Now - State->Since;
        const auto Next = [&](EAwarenessStep Step) { State->Step = Step; State->Since = Now; };
        const auto Fail = [&](const TCHAR* Reason)
        {
            UE_LOG(LogTemp, Error, TEXT("Sabarmati awareness review failed: step=%d %s"), int32(State->Step), Reason);
            FPlatformMisc::RequestExitWithStatus(false, 1); return false;
        };
        if (!Walker || !Visitor || !Leader || State->Family.Num() != 3) return Fail(TEXT("player, visitor or family missing"));
        if (Age > (State->Step == EAwarenessStep::WaitForCrossing ? 110. : 30.)) return Fail(TEXT("timed out"));
        FVector Eyes; FRotator View;
        switch (State->Step)
        {
        case EAwarenessStep::Approach:
            Visitor->GetActorEyesViewPoint(Eyes, View);
            Walker->SetActorLocation(Visitor->GetActorLocation() + View.Vector() * 160. + FVector(0,0,92), false, nullptr, ETeleportType::TeleportPhysics);
            Walker->GetCharacterMovement()->StopMovementImmediately();
            SetControlRotation((Visitor->GetActorLocation() - Walker->GetActorLocation()).Rotation());
            if (Visitor->IsReactingToPlayer())
            {
                if (!Visitor->HasSeenPlayer() || Age < 2.) return Fail(TEXT("sight or sustained attention missing"));
                State->Start = Visitor->GetActorLocation();
                Next(EAwarenessStep::Observe);
            }
            break;
        case EAwarenessStep::Observe:
            if (Age < 1.)
            {
                if (FVector::Dist2D(Visitor->GetActorLocation(), State->Start) > 3.) return Fail(TEXT("visitor did not pause"));
                if (Age > .5 && !State->bVisitorPicture)
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("resident-reaction.png")), false, false, false, FIntRect(), true);
                    State->bVisitorPicture = true;
                }
                return true;
            }
            if (Age < 4.) return true;
            if (Visitor->PlayerReactions() != 1) return Fail(TEXT("reaction repeated without space"));
            Walker->SetActorLocation(Visitor->GetActorLocation() + FVector(0,800,92), false, nullptr, ETeleportType::TeleportPhysics);
            State->Start = Visitor->GetActorLocation();
            Next(EAwarenessStep::Resume);
            break;
        case EAwarenessStep::Resume:
            if (FVector::Dist2D(Visitor->GetActorLocation(), State->Start) < 35.) return true;
            UE_LOG(LogTemp, Display, TEXT("Sabarmati awareness review: visitor saw player, paused and resumed."));
            FPlatformMisc::RequestExitWithStatus(false, 0);
            return false;
        case EAwarenessStep::Follow:
            if (Leader->IsCrossingStreet()) return true;
            Leader->GetActorEyesViewPoint(Eyes, View);
            Walker->SetActorLocation(Leader->GetActorLocation() - View.Vector() * 160. + FVector(0,0,92), false, nullptr, ETeleportType::TeleportPhysics);
            Walker->AddMovementInput(View.Vector());
            SetControlRotation((Leader->GetActorLocation() - Walker->GetActorLocation()).Rotation());
            State->bHeardFootstep |= Leader->HeardPlayerAt() > 0.;
            if (Leader->IsReactingToPlayer())
            {
                if (!State->bHeardFootstep) return Fail(TEXT("no player footstep reached family hearing"));
                for (const auto& Person : State->Family)
                    if (Person.IsValid()) State->FamilyPositions.Add(Person->GetActorLocation());
                Next(EAwarenessStep::FamilyPause);
            }
            break;
        case EAwarenessStep::FamilyPause:
            if (Age < 1.)
            {
                for (int32 I=0; I<State->Family.Num(); ++I)
                    if (!State->Family[I].IsValid() || FVector::Dist2D(State->Family[I]->GetActorLocation(), State->FamilyPositions[I]) > 3.)
                        return Fail(TEXT("family did not pause together"));
                if (Age > .5 && !State->bFamilyPicture)
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("family-reaction.png")), false, false, false, FIntRect(), true);
                    State->bFamilyPicture = true;
                }
                return true;
            }
            Walker->SetActorLocation(Leader->GetActorLocation() + FVector(0,800,92), false, nullptr, ETeleportType::TeleportPhysics);
            Walker->GetCharacterMovement()->StopMovementImmediately();
            State->Start = Leader->GetActorLocation();
            Next(EAwarenessStep::FamilyResume);
            break;
        case EAwarenessStep::FamilyResume:
            if (FVector::Dist2D(Leader->GetActorLocation(), State->Start) < 35.) return true;
            UE_LOG(LogTemp, Display, TEXT("Sabarmati awareness review: family heard player footsteps, paused together and resumed."));
            Next(EAwarenessStep::WaitForCrossing);
            break;
        case EAwarenessStep::WaitForCrossing:
            Walker->SetActorLocation(Leader->GetActorLocation() + FVector(0,1800,92), false, nullptr, ETeleportType::TeleportPhysics);
            Walker->GetCharacterMovement()->StopMovementImmediately();
            if (Leader->IsCrossingStreet())
            {
                State->Start = Leader->GetActorLocation();
                State->ReactionsBeforeCrossing = Leader->PlayerReactions();
                Next(EAwarenessStep::Crossing);
            }
            break;
        case EAwarenessStep::Crossing:
            Leader->GetActorEyesViewPoint(Eyes, View);
            Walker->SetActorLocation(Leader->GetActorLocation() - View.Vector() * 160. + FVector(0,0,92), false, nullptr, ETeleportType::TeleportPhysics);
            Walker->AddMovementInput(View.Vector());
            if (Leader->IsReactingToPlayer() || Leader->PlayerReactions() != State->ReactionsBeforeCrossing)
                return Fail(TEXT("family stopped to react in a crossing"));
            if (Age < 3.1) return true;
            if (FVector::Dist2D(Leader->GetActorLocation(), State->Start) < 100.) return Fail(TEXT("family did not continue crossing"));
            UE_LOG(LogTemp, Display, TEXT("Sabarmati awareness review: family continued crossing despite nearby footsteps."));
            Walker->SetActorLocation(Leader->GetActorLocation() + FVector(0,1800,92), false, nullptr, ETeleportType::TeleportPhysics);
            Walker->GetCharacterMovement()->StopMovementImmediately();
            Next(EAwarenessStep::Approach);
            break;
        }
        return true;
    }));
}
#endif
