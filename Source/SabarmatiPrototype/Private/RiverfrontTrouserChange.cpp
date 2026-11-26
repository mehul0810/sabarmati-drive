#include "RiverfrontCharacter.h"
#include "RiverfrontLifeProfile.h"
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

void ARiverfrontCharacter::SetBraFabricVisible(bool bVisible)
{
    auto* Body=GetMesh();
    const int32 Skin=Body->GetMaterialIndex(TEXT("skin"));
    if (Skin==INDEX_NONE) return;
    // The bra panels share the original torso topology, normals and skin UV seams.
    for (FName Name : {FName(TEXT("foundation_knit")),FName(TEXT("foundation_elastic"))})
    {
        const int32 Slot=Body->GetMaterialIndex(Name);
        if (Slot==INDEX_NONE) continue;
        UMaterialInterface* Surface=Body->GetMaterial(Skin);
        if (bVisible)
        {
            auto* Fabric=Cast<UMaterialInstanceDynamic>(Body->GetMaterial(Slot));
            if (!Fabric || Fabric->Parent!=CottonMaterial.Get()) Fabric=Body->CreateDynamicMaterialInstance(Slot,CottonMaterial);
            if (!Fabric) continue;
            Fabric->SetVectorParameterValue(TEXT("BaseColorFactor"),FLinearColor(Name==TEXT("foundation_knit") ? FColor(47,60,68) : FColor(32,42,46)));
            Surface=Fabric;
        }
        Body->SetMaterial(Slot,Surface);
        if (ChangingMesh) ChangingMesh->SetMaterial(Slot,Surface);
    }
}

void ARiverfrontCharacter::SetLowerBodyVisible(bool bVisible,bool bMovingBriefs)
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
        const bool bBriefs=bWearingBriefs && !bMovingBriefs;
        for (FName Name : {FName(TEXT("leg_skin")),FName(TEXT("bare_lower_skin")),FName(TEXT("midriff_skin"))})
            LowerBody->SetMaterialByName(Name,Body->GetMaterial(Body->GetMaterialIndex(TEXT("skin"))));
        for (FName Name : {FName(TEXT("leg_skin")),FName(TEXT("underwear_knit")),FName(TEXT("underwear_band"))})
            ShowSection(*LowerBody,Name,bBriefs);
        ShowSection(*LowerBody,TEXT("bare_lower_skin"),!bBriefs);
        ShowSection(*LowerBody,TEXT("midriff_skin"),!bBriefs && WardrobeOutfit!=RiverfrontLife::BaseLayer);
        LowerBody->SetLeaderPoseComponent(ChangingMesh ? static_cast<USkinnedMeshComponent*>(ChangingMesh.Get()) : Body);
    }
    ShowSection(*Body,TEXT("bottom_denim"),!bVisible);
    if (ChangingMesh) ShowSection(*ChangingMesh,TEXT("bottom_denim"),!bVisible);
    if (LowerBody) LowerBody->SetVisibility(bVisible);
}

UPoseableMeshComponent* ARiverfrontCharacter::CreateGarmentDisplay(FName Item)
{
    const bool bBriefs=Item==RiverfrontLife::Briefs, bBra=Item==RiverfrontLife::SportsBra;
    auto* Source=bBriefs ? LowerBody.Get() : GetMesh();
    if (!Source || !Source->GetSkeletalMeshAsset()) return nullptr;
    const FName Panel=bBra ? TEXT("foundation_knit") : bBriefs ? TEXT("underwear_knit") : TEXT("bottom_denim");
    const FName Band=bBra ? FName(TEXT("foundation_elastic")) : bBriefs ? FName(TEXT("underwear_band")) : FName();
    const int32 MainSlot=Source->GetMaterialIndex(Panel);
    if (MainSlot==INDEX_NONE) return nullptr;
    auto* Garment=NewObject<UPoseableMeshComponent>(this);
    AddInstanceComponent(Garment);
    Garment->SetupAttachment(Source);
    Garment->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
    Garment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Garment->SetCanEverAffectNavigation(false);
    Garment->RegisterComponent();
    for (int32 Slot=0;Slot<Source->GetNumMaterials();++Slot)
    {
        const FName Name=Source->GetSkeletalMeshAsset()->GetMaterials()[Slot].MaterialSlotName;
        if (Name!=Panel && (Band.IsNone() || Name!=Band))
        {
            for (int32 LOD=0;LOD<Source->GetNumLODs();++LOD) Garment->ShowMaterialSection(Slot,INDEX_NONE,false,LOD);
            continue;
        }
        if (bBriefs || bBra)
        {
            const FColor Color=bBra ? (Name==Band ? FColor(32,42,46) : FColor(47,60,68))
                : (Name==Band ? FColor(41,47,52) : FColor(61,73,81));
            if (auto* Fabric=Garment->CreateDynamicMaterialInstance(Slot,CottonMaterial))
                Fabric->SetVectorParameterValue(TEXT("BaseColorFactor"),FLinearColor(Color));
        }
        else
        {
            auto* Worn=Source->GetMaterial(Slot);
            auto* Instance=Cast<UMaterialInstanceDynamic>(Worn);
            if (auto* Material=Garment->CreateDynamicMaterialInstance(Slot,Instance ? Instance->Parent.Get() : Worn))
            {
                const FLinearColor Dye(Item==TEXT("denim-washed") ? FColor(121,149,173) : FColor(53,62,73));
                Material->SetVectorParameterValue(TEXT("BaseColorFactor"),Dye);
                Material->SetVectorParameterValue(TEXT("TrouserDye"),Dye);
            }
        }
    }
    Garment->CopyPoseFromSkeletalComponent(GetMesh());
    Garment->RefreshBoneTransforms();
    return Garment;
}

FTransform ARiverfrontCharacter::FoldedGarmentTransform(const USkinnedMeshComponent& Wearer,FName Item) const
{
    auto* Body=GetMesh();
    const FVector Palm=Wearer.GetSocketLocation(TEXT("Handr"));
    const FVector Grip=Palm+(Palm-Wearer.GetSocketLocation(TEXT("Elbowr"))).GetSafeNormal()*7.;
    const FVector Anchor=Body->GetSocketTransform(Item==RiverfrontLife::SportsBra ? TEXT("Chest") : TEXT("Pelvis"),RTS_Component).GetLocation();
    FTransform Fold(Body->GetComponentQuat(),FVector::ZeroVector,Body->GetComponentScale()*FVector(.40,.43,.32));
    Fold.SetLocation(Grip-Fold.TransformVector(Anchor));
    return Fold;
}

void ARiverfrontCharacter::PlaceHeldGarment()
{
    if (!HeldProp) return;
    const USkinnedMeshComponent* Wearer=ChangingMesh ? static_cast<USkinnedMeshComponent*>(ChangingMesh.Get()) : GetMesh();
    HeldProp->SetWorldTransform(FoldedGarmentTransform(*Wearer,HeldPropItem));
}

void ARiverfrontCharacter::PoseChangingTrousers(FName Item,float Undress)
{
    auto* Body=GetMesh();
    if (!PreparePosedMesh()) return;
    if (!MovingGarment)
    {
        MovingGarment=CreateGarmentDisplay(Item);
        if (!MovingGarment) return;
        MovingGarment->ComponentTags.Add(TEXT("WardrobeMovingJeans"));
        SetLowerBodyVisible(true,Item==RiverfrontLife::Briefs);
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
    const double KneeBend=Ease(.47f,.62f)*(1.-Ease(.75f,.98f));
    Hips.AddToTranslation(-Forward*2.*Crouch-(Up*17.+Forward*12.)*KneeBend);
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
        MovingGarment->SetBoneTransformByName(Joint,ClothBone,EBoneSpaces::ComponentSpace);
    }
    MovingGarment->RefreshBoneTransforms();
    MovingGarment->MarkRenderDynamicDataDirty();
    const FTransform Standing=Body->GetComponentTransform();
    FTransform Bunched=Standing;
    Bunched.SetScale3D(Standing.GetScale3D()*GatherScale);
    Bunched.SetLocation(Standing.TransformPosition(Ankle)-Bunched.TransformVector(Ankle));
    FTransform Placed;
    Placed.Blend(Bunched,FoldedGarmentTransform(*ChangingMesh,Item),Pickup);
    MovingGarment->SetWorldTransform(Placed);
}

void ARiverfrontCharacter::PoseChangingSportsBra(float Undress)
{
    if (!PreparePosedMesh()) return;
    if (!MovingGarment)
    {
        MovingGarment=CreateGarmentDisplay(RiverfrontLife::SportsBra);
        if (!MovingGarment) return;
        MovingGarment->ComponentTags.Add(TEXT("WardrobeMovingBra"));
        ShowHeldWardrobeItem(NAME_None);
        SetBraFabricVisible(false);
    }
    const float Overhead=FMath::SmoothStep(0.f,.56f,Undress)*(1.f-FMath::SmoothStep(.65f,.98f,Undress));
    const float Hand=FMath::SmoothStep(.59f,1.f,Undress);
    PoseChangingSleeves(FMath::Lerp(Overhead,.69f,Hand),1.f-Hand);
    const FTransform Worn=GetMesh()->GetComponentTransform();
    FTransform Lifted=Worn;
    const FVector Center=GetMesh()->GetSocketTransform(TEXT("Chest"),RTS_Component).GetLocation();
    const float Rise=FMath::SmoothStep(.04f,.65f,Undress);
    const FVector Scale(1.+.12*Overhead,1.+.16*Overhead,1.-.51*Rise);
    Lifted.SetScale3D(Worn.GetScale3D()*Scale);
    Lifted.SetLocation(Worn.TransformPosition(Center)-Lifted.TransformVector(Center)
        +FVector::UpVector*Rise*56.+GetActorForwardVector()*Rise*10.);
    FTransform Placed;
    Placed.Blend(Lifted,FoldedGarmentTransform(*ChangingMesh,RiverfrontLife::SportsBra),Hand);
    MovingGarment->SetWorldTransform(Placed);
}
