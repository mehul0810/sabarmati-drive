#include "RiverfrontPlayerController.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontWorld.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "TimerManager.h"
#include "Tracks/MovieScene3DTransformTrack.h"

namespace
{
constexpr int32 FramesPerSecond = 30;
constexpr int32 LastFrame = 70;

void AddViewKey(UMovieScene3DTransformSection& Section, int32 Frame, const FTransform& View)
{
    const FVector Position = View.GetLocation();
    const FRotator Angles = View.Rotator();
    const double Values[] = { Position.X, Position.Y, Position.Z, Angles.Roll, Angles.Pitch, Angles.Yaw, 1., 1., 1. };
    const auto Channels = Section.GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
    for (int32 I = 0; I < FMath::Min(9, Channels.Num()); ++I)
        Channels[I]->AddCubicKey(Frame, Values[I]);
}
}

void ARiverfrontPlayerController::PlayWakeup()
{
    if (TActorIterator<ARiverfrontWorld> District(GetWorld()); District) District->StartMorningClock();
    const FTransform From = FRiverfrontFirstMorningWorld::WakeStartView();
    ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(From.GetLocation(), From.Rotator());
    if (!Camera) return;
    Camera->GetCameraComponent()->SetFieldOfView(68.f);
    auto* Sequence = NewObject<ULevelSequence>(this);
    Sequence->Initialize();
    UMovieScene* Scene = Sequence->GetMovieScene();
    Scene->SetTickResolutionDirectly(FFrameRate(FramesPerSecond, 1));
    Scene->SetDisplayRate(FFrameRate(FramesPerSecond, 1));
    Scene->SetPlaybackRange(0, LastFrame+1);
    const FGuid CameraId = Scene->AddPossessable(TEXT("Bedside view"), Camera->GetClass());
    Sequence->BindPossessableObject(CameraId, *Camera, GetWorld());
    auto* Track = Scene->AddTrack<UMovieScene3DTransformTrack>(CameraId);
    auto* Section = CastChecked<UMovieScene3DTransformSection>(Track->CreateNewSection());
    Section->SetRange(TRange<FFrameNumber>::All());
    AddViewKey(*Section, 0, From);
    AddViewKey(*Section, 15, From);
    AddViewKey(*Section, LastFrame, FRiverfrontFirstMorningWorld::WakeEndView());
    Track->AddSection(*Section);

    FMovieSceneSequencePlaybackSettings Settings;
    Settings.bPauseAtEnd = true;
    ALevelSequenceActor* Actor = nullptr;
    auto* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, Actor);
    if (!Player) { Camera->Destroy(); return; }
    Actor->SetBinding(FMovieSceneObjectBindingID(CameraId), {Camera});
    LifeCamera = Camera;
    WakeupSequence = Actor;
    bWaking = true;
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    SetViewTarget(Camera);
    if (PlayerCameraManager)
        PlayerCameraManager->StartCameraFade(1.f, 0.f, .65f, FLinearColor::Black, false);
    Player->Play();
    GetWorldTimerManager().SetTimer(WakeupTimer, this, &ARiverfrontPlayerController::FinishWakeup, float(LastFrame)/FramesPerSecond, false);
}

void ARiverfrontPlayerController::FinishWakeup()
{
    if (!bWaking) return;
    GetWorldTimerManager().ClearTimer(WakeupTimer);
    bWaking=false;
    bHasExperiencedWake=true;
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    if (WakeupSequence.IsValid())
    {
        if (auto* Player = WakeupSequence->GetSequencePlayer()) Player->Stop();
        WakeupSequence->Destroy();
        WakeupSequence.Reset();
    }
    if (GetPawn()) SetViewTargetWithBlend(GetPawn(), .32f, VTBlend_EaseInOut, 2.f, true);
    if (LifeCamera.IsValid()) LifeCamera->SetLifeSpan(.6f);
    LifeCamera.Reset();
    SaveLifeCheckpoint();
    Notice(TEXT("Kesar Apartments · Flat 2B    E  wardrobe    I  wallet"));
}
