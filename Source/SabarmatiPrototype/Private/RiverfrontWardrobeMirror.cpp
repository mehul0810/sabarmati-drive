#include "RiverfrontPlayerController.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontFirstMorningWorld.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

bool ARiverfrontPlayerController::OpenWardrobeMirror()
{
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    auto* Glass=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
    if (!Walker || !Glass) return false;
    const FVector Centre=FRiverfrontFirstMorningWorld::MirrorCentre();
    const FVector2D Size=FRiverfrontFirstMorningWorld::MirrorSize();
    const FVector Eye=Centre+FVector(78,-225,32);
    const FRotator Aim=(Centre+FVector(-48,0,-15)-Eye).Rotation();
    auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,Aim);
    if (!Camera) return false;
    BeforeFittingRotation=Walker->GetActorRotation();
    BeforeMirrorLocation=Walker->GetActorLocation();
    FVector Mark=Centre+FVector(-30,-100,0);
    Mark.Z=BeforeMirrorLocation.Z;
    if (!Walker->TeleportTo(Mark,FRotator(0,90,0)))
    {
        Camera->Destroy();
        return false;
    }
    Walker->SetFittingPose(true);
    Camera->GetCameraComponent()->SetFieldOfView(62.f);
    LifeCamera=Camera;
    FullOutfitView=Camera->GetActorTransform();

    auto* Target=NewObject<UTextureRenderTarget2D>(Camera);
    Target->RenderTargetFormat=RTF_RGBA8_SRGB;
    Target->ClearColor=FLinearColor::Black;
    Target->InitAutoFormat(256,512);
    auto* Material=UMaterialInstanceDynamic::Create(Glass,Camera);
    Material->SetTextureParameterValue(TEXT("SlateUI"),Target);
    Material->SetVectorParameterValue(TEXT("TintColorAndOpacity"),FLinearColor::White);
    auto* Surface=NewObject<UProceduralMeshComponent>(Camera);
    Camera->AddInstanceComponent(Surface);
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->SetCanEverAffectNavigation(false);
    Surface->SetCastShadow(false);
    Surface->RegisterComponent();
    Surface->SetWorldLocation(Centre);
    const double X=Size.X*.5, Z=Size.Y*.5;
    Surface->CreateMeshSection_LinearColor(0,{{-X,0,Z},{X,0,Z},{X,0,-Z},{-X,0,-Z}},
        {0,1,2,0,2,3}, {}, {{0,0},{1,0},{1,1},{0,1}}, {}, {}, false);
    Surface->SetMaterial(0,Material);

    auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);
    Camera->AddInstanceComponent(Capture);
    Capture->bCaptureEveryFrame=false;
    Capture->bCaptureOnMovement=false;
    Capture->CaptureSource=SCS_FinalColorLDR;
    Capture->TextureTarget=Target;
    Capture->HiddenComponents.Add(Surface);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetScreenSpaceReflections(false);
    Capture->RegisterComponent();
    const double Distance=Centre.Y-Eye.Y;
    Capture->SetWorldLocationAndRotation(FVector(Eye.X,Centre.Y+Distance,Eye.Z),FRotator(0,-90,0));
    // The glass fills an off-axis frustum. Its near plane excludes the wall behind the mirror.
    Capture->bUseCustomProjectionMatrix=true;
    Capture->CustomProjectionMatrix=FMatrix(
        FPlane(2.*Distance/Size.X,0,0,0),FPlane(0,2.*Distance/Size.Y,0,0),
        FPlane(2.*(Eye.X-Centre.X)/Size.X,2.*(Eye.Z-Centre.Z)/Size.Y,0,1),FPlane(0,0,Distance+.4,0));
    Capture->bOverride_CustomNearClippingPlane=true;
    Capture->CustomNearClippingPlane=Distance+.4;
    MirrorCapture=Capture;
    bMirrorPortrait=false;
    MirrorUpdates=0;
    LastMirrorFrame=MAX_uint64;
    SetViewTarget(Camera);
    if (PlayerCameraManager) PlayerCameraManager->UpdateCamera(0.f);
    RefreshWardrobeMirror();
    return true;
}

void ARiverfrontPlayerController::RefreshWardrobeMirror()
{
    if (MirrorCapture.IsValid() && !bMirrorPortrait && LastMirrorFrame!=GFrameCounter)
    {
        // Coalesce mesh and pose changes so the next draw sees the final state.
        LastMirrorFrame=GFrameCounter;
        MirrorCapture->CaptureSceneDeferred();
        ++MirrorUpdates;
    }
}
