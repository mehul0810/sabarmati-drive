#include "RiverfrontPlayerController.h"
#if !UE_BUILD_SHIPPING
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontTraffic.h"
#include "RiverfrontWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"

void ARiverfrontPlayerController::RunImpactReview()
{
    auto* Walker=Cast<ARiverfrontCharacter>(GetPawn());
    auto* Mode=GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    const bool Chaos=FParse::Param(FCommandLine::Get(),TEXT("SabarmatiHatchImpact"));
    const bool Minor=FParse::Param(FCommandLine::Get(),TEXT("SabarmatiMinorImpact"));
    struct FRun
    {
        int32 Step=0;
        double Since=0,TopKph=0;
        TWeakObjectPtr<ARiverfrontCar> Car;
        TWeakObjectPtr<ARiverfrontRoadAgent> Sport;
        TWeakObjectPtr<UInstancedStaticMeshComponent> Trees;
        FVector Tree=FVector::ZeroVector;
        int32 Count=0;
    };
    auto Run=MakeShared<FRun>();
    const auto Fail=[](const TCHAR* Reason)
    {
        UE_LOG(LogTemp,Error,TEXT("Sabarmati impact review failed: %s"),Reason);
        FPlatformMisc::RequestExitWithStatus(false,1);return false;
    };
    if (!Walker || !Mode || Mode->IsCityMode()) {Fail(TEXT("riverfront required"));return;}
    for (TActorIterator<ARiverfrontRoadAgent> It(GetWorld());It;++It)
        if (It->Tags.Contains(TEXT("Kerbside")) && It->IsSport()) {Run->Sport=*It;break;}
    if (!Run->Sport.IsValid()) {Fail(TEXT("parked sport coupe"));return;}
    Run->Since=GetWorld()->GetTimeSeconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Walker,Mode,Run,Fail,Chaos,Minor](float)
    {
        const double Now=GetWorld()->GetTimeSeconds();
        const double Age=Now-Run->Since;
        const auto Key=[&](FKey K,EInputEvent Event){InputKey(FInputKeyEventArgs(GetWorld()->GetGameViewport()->Viewport,
            FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64()));};
        const auto Next=[&](){++Run->Step;Run->Since=Now;};
        if (Age>22.) {UE_LOG(LogTemp,Warning,TEXT("impact stage %d top %.1f"),Run->Step,Run->TopKph);return Fail(TEXT("timeout"));}
        if (Run->Step==0)
        {
            for (TActorIterator<ARiverfrontRoadAgent> It(GetWorld());It;++It)
            {
                if (It->Tags.Contains(TEXT("Kerbside")) || It->IsHidden()) continue;
                const FVector Door=It->GetActorTransform().TransformPosition(FVector(It->GetAccessX(),It->HalfWidth+78.,90.-It->TyreRadius));
                if (Walker->TeleportTo(Door,It->GetActorRotation()) && Mode->GetTraffic()->FindEnterableVehicle(*Walker)==*It)
                { Key(EKeys::E,IE_Pressed); Next();break; }
            }
        }
        else if (Run->Step==1 && Age>.15)
        {
            Key(EKeys::E,IE_Released);
            auto* Car=Cast<ARiverfrontCar>(GetPawn());
            auto* Seat=Car?Car->FindComponentByClass<UPoseableMeshComponent>():nullptr;
            bool Ejected=false;
            for (TActorIterator<ARiverfrontCharacter> It(GetWorld());It;++It) Ejected |= It->ActorHasTag(TEXT("DisplacedDriver"));
            if (!Ejected || !Seat || Seat->GetSkinnedAsset()!=Walker->GetAvatarMesh()->GetSkeletalMeshAsset())
                return Fail(TEXT("ejected NPC or player seat mesh"));
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("traffic-takeover.png")),false,false);
            UE_LOG(LogTemp,Display,TEXT("Sabarmati impact review: E takeover, displaced NPC and matching player mesh."));
            Next();
        }
        else if (Run->Step==2 && Age>.18)
        {
            Key(EKeys::E,IE_Released);
            if (GetPawn()!=Walker)
            {
                if (Age>1.5) return Fail(TEXT("exit borrowed vehicle"));
                Key(EKeys::E,IE_Pressed); return true;
            }
            TInlineComponentArray<UInstancedStaticMeshComponent*> Components(Mode->GetDistrict());
            for (auto* Trees:Components)
            {
                if (!Trees->ComponentHasTag(TEXT("StreetTrunk"))) continue;
                for (int32 I=0;I<Trees->GetInstanceCount();++I)
                {
                    FTransform Tree;
                    Trees->GetInstanceTransform(I,Tree,true);
                    const FVector P=Tree.GetLocation();
                    if (P.X < -33500. || P.X > -8500. || FMath::Abs(P.Y-3423.)>55.) continue;
                    const FVector D=FVector(2200.,-820.,0).GetSafeNormal();
                    const FVector Approach=FVector(P.X,P.Y,78.)-D*2500.;
                    const FRotator Heading=D.Rotation();
                    if (GetWorld()->SweepTestByChannel(Approach,Approach+D*2000.,Heading.Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(210,96,47)))) continue;
                    FHitResult First;
                    if (!GetWorld()->SweepSingleByChannel(First,Approach,FVector(P.X,P.Y,78),Heading.Quaternion(),ECC_Pawn,
                        FCollisionShape::MakeBox(FVector(215,98,50))) || First.GetComponent()!=Trees || First.Item!=I) continue;
                    const FVector Start=Minor ? FVector(P.X,P.Y,78.)-D*700. : Approach;
                    FVector Door;
                    if (Chaos)
                    {
                        auto* Car=Mode->GetCar();
                        Car->SetActorLocationAndRotation(Start,Heading,false,nullptr,ETeleportType::TeleportPhysics);
                        Door=Car->GetActorTransform().TransformPosition(FVector(0,164.,26.));
                    }
                    else
                    {
                        auto* Agent=Run->Sport.Get();
                        Agent->SetActorLocationAndRotation(FVector(Start.X,Start.Y,Agent->TyreRadius+6.),Heading);
                        Door=Agent->GetActorTransform().TransformPosition(FVector(Agent->GetAccessX(),Agent->HalfWidth+74.,90.-Agent->TyreRadius));
                    }
                    if (!Walker->TeleportTo(Door,Heading)) continue;
                    Run->Tree=P;Run->Trees=Trees;Run->Count=Trees->GetInstanceCount();
                    Key(EKeys::E,IE_Pressed);Next();return true;
                }
            }
            return Fail(TEXT("clear approach to an existing tree"));
        }
        else if (Run->Step==3 && Age>.25)
        {
            Key(EKeys::E,IE_Released);
            Run->Car=Cast<ARiverfrontCar>(GetPawn());
            if (!Run->Car.IsValid()) return Fail(TEXT("enter test vehicle"));
            Key(EKeys::W,IE_Pressed);Next();
        }
        else if (Run->Step==4)
        {
            if (Minor)
            {
                if (!Run->Car.IsValid()) return Fail(TEXT("minor impact destroyed the car"));
                Run->TopKph=FMath::Max(Run->TopKph,double(Run->Car->GetSpeedKph()));
                if (Run->Car->GetCondition()>=1.f) return true;
                Key(EKeys::W,IE_Released);
                if (GetPawn()!=Run->Car.Get() || !Run->Trees.IsValid() || Run->Trees->GetInstanceCount()!=Run->Count || Run->TopKph>=39.6)
                    return Fail(TEXT("minor impact driver, tree or impact speed"));
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("collision-minor.png")),false,false);
                UE_LOG(LogTemp,Display,TEXT("Sabarmati minor impact: %s peak %.1f km/h, condition %.0f%%, driver and tree retained."),
                    Chaos?TEXT("Chaos hatch"):TEXT("parked coupe"),Run->TopKph,100.f*Run->Car->GetCondition());
                Next();
                return true;
            }
            if (Run->Car.IsValid()) {Run->TopKph=FMath::Max(Run->TopKph,double(Run->Car->GetSpeedKph()));return true;}
            Key(EKeys::W,IE_Released);
            UE_LOG(LogTemp,Display,TEXT("Sabarmati impact review: pawn=%s hidden=%d tree=%s count=%d expected=%d"),
                *GetNameSafe(GetPawn()),Walker->IsHidden(),*Run->Tree.ToCompactString(),
                Run->Trees.IsValid()?Run->Trees->GetInstanceCount():-1,Run->Count-1);
            if (GetPawn()!=Walker || Walker->IsHidden() || !Run->Trees.IsValid() || Run->Trees->GetInstanceCount()!=Run->Count-1)
                return Fail(TEXT("wreck, walkable player or broken tree"));
            int32 VisibleWrecks=0,VisibleWheels=0;
            for (TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
            {
                const UStaticMeshComponent* Shell=It->GetStaticMeshComponent();
                if (It->ActorHasTag(TEXT("SabarmatiWreck")) && Shell && Shell->GetStaticMesh() && Shell->IsVisible())
                {
                    ++VisibleWrecks;
                    for (const auto* Part:TInlineComponentArray<UStaticMeshComponent*>(*It))
                        if (Part->ComponentHasTag(TEXT("WreckWheel")) && Part->GetStaticMesh() && Part->IsVisible()) ++VisibleWheels;
                }
            }
            if (VisibleWrecks!=1 || VisibleWheels!=4) return Fail(TEXT("visible wreck shell and wheels"));
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("collision-wreck.png")),false,false);
            UE_LOG(LogTemp,Display,TEXT("Sabarmati impact review: %s peak %.1f km/h, tree and car removed, player back on foot."),Chaos?TEXT("Chaos hatch"):TEXT("parked coupe"),Run->TopKph);
            Next();
        }
        else if (Run->Step==5 && Age>.5) {FPlatformMisc::RequestExit(false);return false;}
        return true;
    }));
}
#endif
