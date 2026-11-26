#include "RiverfrontCharacter.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TwoBoneIK.h"

namespace
{
void ShowSection(USkinnedMeshComponent& Mesh, FName Name, bool bShow)
{
    const int32 Slot=Mesh.GetMaterialIndex(Name);
    if (Slot==INDEX_NONE || !Mesh.GetSkinnedAsset()) return;
    for (int32 LOD=0;LOD<Mesh.GetSkinnedAsset()->GetLODNum();++LOD)
        if (Mesh.IsMaterialSectionShown(Slot,LOD)!=bShow) Mesh.ShowMaterialSection(Slot,INDEX_NONE,bShow,LOD);
}
}

void ARiverfrontCharacter::SetLowerBodyVisible(bool bVisible)
{
    auto* Body=GetMesh();
    if (bVisible)
    {
        const TCHAR* Path=AvatarGender==ERiverfrontGender::Female
            ? TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Foundation/SK_FemaleLowerBody.SK_FemaleLowerBody")
            : TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Foundation/SK_MaleLowerBody.SK_MaleLowerBody");
        auto* Shape=LoadObject<USkeletalMesh>(nullptr,Path);
        if (!Shape) return;
        if (!LowerBody)
        {
            LowerBody=NewObject<USkeletalMeshComponent>(this);
            AddInstanceComponent(LowerBody);
            LowerBody->SetupAttachment(Body);
            LowerBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            LowerBody->SetCanEverAffectNavigation(false);
            LowerBody->RegisterComponent();
        }
        if (LowerBody->GetSkeletalMeshAsset()!=Shape)
        {
            LowerBody->EmptyOverrideMaterials();
            LowerBody->SetSkeletalMesh(Shape);
            for (const FName Slot : {FName(TEXT("underwear_knit")),FName(TEXT("underwear_band"))})
                if (auto* Material=LowerBody->CreateDynamicMaterialInstance(LowerBody->GetMaterialIndex(Slot),CottonMaterial))
                    Material->SetVectorParameterValue(TEXT("BaseColorFactor"),
                        FLinearColor(Slot==TEXT("underwear_band") ? FColor(41,47,52) : FColor(61,73,81)));
        }
        LowerBody->SetMaterialByName(TEXT("leg_skin"),Body->GetMaterial(Body->GetMaterialIndex(TEXT("skin"))));
        LowerBody->SetLeaderPoseComponent(ChangingMesh ? static_cast<USkinnedMeshComponent*>(ChangingMesh.Get()) : Body);
    }
    ShowSection(*Body,TEXT("bottom_denim"),!bVisible);
    if (ChangingMesh) ShowSection(*ChangingMesh,TEXT("bottom_denim"),!bVisible);
    if (LowerBody) LowerBody->SetVisibility(bVisible);
}

UPoseableMeshComponent* ARiverfrontCharacter::CreateJeansDisplay(FName Item)
{
    auto* Source=GetMesh();
    const int32 Denim=Source->GetMaterialIndex(TEXT("bottom_denim"));
    if (Denim==INDEX_NONE || !Source->GetSkeletalMeshAsset()) return nullptr;
    auto* Garment=NewObject<UPoseableMeshComponent>(this);
    AddInstanceComponent(Garment);
    Garment->SetupAttachment(Source);
    Garment->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
    Garment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Garment->SetCanEverAffectNavigation(false);
    Garment->RegisterComponent();
    for (int32 Slot=0;Slot<Source->GetNumMaterials();++Slot)
        if (Slot!=Denim)
            for (int32 LOD=0;LOD<Source->GetNumLODs();++LOD)
                Garment->ShowMaterialSection(Slot,INDEX_NONE,false,LOD);
    auto* Worn=Source->GetMaterial(Denim);
    auto* Instance=Cast<UMaterialInstanceDynamic>(Worn);
    if (auto* Material=Garment->CreateDynamicMaterialInstance(Denim,Instance ? Instance->Parent.Get() : Worn))
    {
        const FLinearColor Dye(Item==TEXT("denim-washed") ? FColor(121,149,173) : FColor(53,62,73));
        Material->SetVectorParameterValue(TEXT("BaseColorFactor"),Dye);
        Material->SetVectorParameterValue(TEXT("TrouserDye"),Dye);
    }
    Garment->CopyPoseFromSkeletalComponent(Source);
    Garment->RefreshBoneTransforms();
    return Garment;
}

FTransform ARiverfrontCharacter::FoldedJeansTransform(const USkinnedMeshComponent& Wearer) const
{
    auto* Body=GetMesh();
    const FVector Palm=Wearer.GetSocketLocation(TEXT("Handr"));
    const FVector Grip=Palm+(Palm-Wearer.GetSocketLocation(TEXT("Elbowr"))).GetSafeNormal()*7.;
    const FVector Waist=Body->GetSocketTransform(TEXT("Pelvis"),RTS_Component).GetLocation();
    FTransform Fold(Body->GetComponentQuat(),FVector::ZeroVector,Body->GetComponentScale()*FVector(.40,.43,.32));
    Fold.SetLocation(Grip-Fold.TransformVector(Waist));
    return Fold;
}

void ARiverfrontCharacter::PlaceHeldJeans()
{
    if (!HeldProp) return;
    const USkinnedMeshComponent* Wearer=ChangingMesh ? static_cast<USkinnedMeshComponent*>(ChangingMesh.Get()) : GetMesh();
    HeldProp->SetWorldTransform(FoldedJeansTransform(*Wearer));
}

void ARiverfrontCharacter::PoseChangingTrousers(FName Item,float Undress)
{
    auto* Body=GetMesh();
    if (!PreparePosedMesh()) return;
    if (!MovingJeans)
    {
        MovingJeans=CreateJeansDisplay(Item);
        if (!MovingJeans) return;
        MovingJeans->ComponentTags.Add(TEXT("WardrobeMovingJeans"));
        SetLowerBodyVisible(true);
        ShowHeldWardrobeItem(NAME_None);
    }
    const auto Ease=[Undress](float A,float B){ return FMath::SmoothStep(A,B,Undress); };
    const double Crouch=Ease(.03f,.47f)*(1.-Ease(.75f,.98f));
    const double Gather=Ease(.12f,.64f), Pickup=Ease(.68f,.96f);
    const FVector Forward=Body->GetComponentQuat().UnrotateVector(GetActorForwardVector());
    const FVector Right=(Body->GetSocketTransform(TEXT("Hipr"),RTS_Component).GetLocation()-
        Body->GetSocketTransform(TEXT("Hipl"),RTS_Component).GetLocation()).GetSafeNormal();
    const FVector Up=Body->GetComponentQuat().UnrotateVector(FVector::UpVector);
    if (Pickup>0.) ApplySleevePose(.69f*Pickup,0.f,0.f);
    else ChangingMesh->CopyPoseFromSkeletalComponent(Body);
    const auto Bone=[this](FName Name){return ChangingMesh->GetBoneTransformByName(Name,EBoneSpaces::ComponentSpace);};
    const auto SetBone=[this](FName Name,const FTransform& T){ChangingMesh->SetBoneTransformByName(Name,T,EBoneSpaces::ComponentSpace);};
    auto Hips=Bone(TEXT("Pelvis"));
    // Hinge at the hips while the waistband still surrounds both knees.
    const double KneeBend=Ease(.38f,.56f)*(1.-Ease(.75f,.98f));
    Hips.AddToTranslation(-(Up*8.+Forward*14.)*Crouch-Up*12.*KneeBend);
    SetBone(TEXT("Pelvis"),Hips);
    auto Spine=Bone(TEXT("Spine"));
    Spine.SetRotation(FQuat(Body->GetComponentQuat().UnrotateVector(GetActorRightVector()),FMath::DegreesToRadians(59.*Crouch))*Spine.GetRotation());
    SetBone(TEXT("Spine"),Spine);

    const auto Reach=[&](FName Upper,FName Middle,FName End,const FVector& Target,const FVector& Pole)
    {
        auto A=Bone(Upper),B=Bone(Middle),C=Bone(End);
        AnimationCore::SolveTwoBoneIK(A,B,C,Pole,Target,false,1.,1.);
        SetBone(Upper,A); SetBone(Middle,B);
    };
    for (int32 Side=0;Side<2;++Side)
    {
        const FName Foot=Side ? TEXT("Footr") : TEXT("Footl");
        const FVector Rest=Body->GetSocketTransform(Foot,RTS_Component).GetLocation();
        const float Step=Side ? Ease(.62f,.78f) : Ease(.46f,.65f);
        const double Lift=FMath::Square(FMath::Sin(PI*Step));
        const FVector Target=Rest+(Forward*12.+Up*21.)*Lift;
        if (Crouch>0. || Lift>.001)
        {
            Reach(Side?TEXT("Hipr"):TEXT("Hipl"),Side?TEXT("Kneer"):TEXT("Kneel"),Foot,Target,Rest+Forward*82.+Up*32.);
            auto Ankle=Bone(Foot);
            Ankle.SetRotation(Body->GetSocketTransform(Foot,RTS_Component).GetRotation());
            SetBone(Foot,Ankle);
        }
        const FName Hand=Side?TEXT("Handr"):TEXT("Handl");
        const FVector RestHand=Bone(Hand).GetLocation();
        const FVector Out=Side ? Right : -Right;
        const FVector Waist=Hips.GetLocation()+Out*19.+Forward*12.;
        const FVector Knee=Bone(Side?TEXT("Kneer"):TEXT("Kneel")).GetLocation();
        // A bent knee projects beyond its ankle. Follow its outer seam on the way down.
        const FVector Cuff=Rest+Out*12.+Forward*10.+Up*16.;
        const FVector AlongLeg=FMath::Lerp(Waist,Cuff,Gather)
            +(Knee-Rest).ProjectOnToNormal(Forward)*FMath::Sin(PI*Gather)+Out*5.*FMath::Sin(PI*Gather);
        const double Grip=Ease(0.f,.15f)*(1.-Ease(.76f,1.f));
        const FVector Goal=FMath::Lerp(RestHand,AlongLeg,Grip);
        if (Grip>.001)
            Reach(Side?TEXT("Shoulderr"):TEXT("Shoulderl"),Side?TEXT("Elbowr"):TEXT("Elbowl"),Hand,
                Goal,Goal+Right*(Side?45.:-45.)-Forward*20.);
    }
    ChangingMesh->RefreshBoneTransforms();
    ChangingMesh->RefreshFollowerComponents();

    // The waistband travels from the hips to the cuffs on the actual denim mesh.
    // Feet clear each opening in succession, then the gathered fabric follows the hand.
    const double Release=Ease(.22f,.64f);
    const FVector Ankle=(Body->GetSocketTransform(TEXT("Footl"),RTS_Component).GetLocation()+
        Body->GetSocketTransform(TEXT("Footr"),RTS_Component).GetLocation())*.5;
    const FVector GatherScale(1.+.035*Gather,1.+.035*Gather,1.-.82*Gather);
    const double FollowLeg=FMath::SmoothStep(.01,.15,Gather)*(1.-Ease(.46f,.68f));
    const auto LegAtHeight=[&](bool bRight,double Height)
    {
        const FVector Knee=Bone(bRight?TEXT("Kneer"):TEXT("Kneel")).GetLocation();
        const FVector End=Height>Knee.Z ? Bone(bRight?TEXT("Hipr"):TEXT("Hipl")).GetLocation()
            : Bone(bRight?TEXT("Footr"):TEXT("Footl")).GetLocation();
        const double Span=End.Z-Knee.Z;
        return FMath::IsNearlyZero(Span) ? Knee : FMath::Lerp(Knee,End,FMath::Clamp((Height-Knee.Z)/Span,0.,1.));
    };
    for (FName Joint : {FName(TEXT("Pelvis")),FName(TEXT("Hipl")),FName(TEXT("Hipr")),
        FName(TEXT("Kneel")),FName(TEXT("Kneer")),FName(TEXT("Footl")),FName(TEXT("Footr"))})
    {
        FTransform ClothBone;
        ClothBone.Blend(Bone(Joint),Body->GetSocketTransform(Joint,RTS_Component),Release);
        if (FollowLeg>0.)
        {
            // As the waistband passes the knee, its centre moves forward with the shin.
            const FVector Gathered=Ankle+(ClothBone.GetLocation()-Ankle)*GatherScale;
            const FVector LeftSeam=LegAtHeight(false,Gathered.Z), RightSeam=LegAtHeight(true,Gathered.Z);
            const bool bRight=Joint==TEXT("Hipr") || Joint==TEXT("Kneer") || Joint==TEXT("Footr");
            const FVector Seam=Joint==TEXT("Pelvis") ? (LeftSeam+RightSeam)*.5 : bRight ? RightSeam : LeftSeam;
            ClothBone.AddToTranslation((Seam-Gathered).ProjectOnToNormal(Forward)*FollowLeg/GatherScale.X);
        }
        MovingJeans->SetBoneTransformByName(Joint,ClothBone,EBoneSpaces::ComponentSpace);
    }
    MovingJeans->RefreshBoneTransforms();
    MovingJeans->MarkRenderDynamicDataDirty();
    const FTransform Standing=Body->GetComponentTransform();
    FTransform Bunched=Standing;
    Bunched.SetScale3D(Standing.GetScale3D()*GatherScale);
    Bunched.SetLocation(Standing.TransformPosition(Ankle)-Bunched.TransformVector(Ankle));
    FTransform Placed;
    Placed.Blend(Bunched,FoldedJeansTransform(*ChangingMesh),Pickup);
    MovingJeans->SetWorldTransform(Placed);
}
