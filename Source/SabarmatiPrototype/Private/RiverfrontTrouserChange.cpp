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

struct FFootSections
{
    FName Slots[2];
};

const FFootSections StreetShoe{{TEXT("street_shoe_l"),TEXT("street_shoe_r")}};
const FFootSections CourtShoe{{TEXT("court_shoe_l"),TEXT("court_shoe_r")}};
const FFootSections Boot{{TEXT("zip_boot_l"),TEXT("zip_boot_r")}};
const FFootSections AnkleSock{{TEXT("cotton_sock_l"),TEXT("cotton_sock_r")}};
const FFootSections CrewSock{{TEXT("crew_sock_l"),TEXT("crew_sock_r")}};
const FFootSections SockToe{{TEXT("sock_toe_l"),TEXT("sock_toe_r")}};
const FFootSections BareFoot{{TEXT("foot_skin_l"),TEXT("foot_skin_r")}};
const FFootSections BareAnkle{{TEXT("foot_ankle_l"),TEXT("foot_ankle_r")}};

const FFootSections& FootSections(ERiverfrontGarmentCut Cut,bool bShoes)
{
    if (bShoes)
    {
        if (Cut==ERiverfrontGarmentCut::CourtRunner) return CourtShoe;
        if (Cut==ERiverfrontGarmentCut::ZipBoot) return Boot;
        return StreetShoe;
    }
    return Cut==ERiverfrontGarmentCut::CrewSocks ? CrewSock : AnkleSock;
}

FName TrouserSection(ERiverfrontGarmentCut Cut)
{
    if (Cut==ERiverfrontGarmentCut::Tailored) return TEXT("tailored_trousers");
    if (Cut==ERiverfrontGarmentCut::DenimShorts) return TEXT("denim_shorts");
    return NAME_None;
}

bool IsTrunks(const FRiverfrontWearable* Item)
{
    return Item && Item->Cut==ERiverfrontGarmentCut::Trunks;
}

FLinearColor InnerDye(const FRiverfrontWearable& Item,bool bBand)
{
    if (Item.Id==RiverfrontLife::SportsBra)
        return FLinearColor(bBand ? FColor(32,42,46) : FColor(47,60,68));
    if (Item.Id==RiverfrontLife::Briefs)
        return FLinearColor(bBand ? FColor(41,47,52) : FColor(61,73,81));
    FLinearColor Dye=Item.Tint*(bBand ? .72f : 1.f);
    Dye.A=1.f;
    return Dye;
}

FLinearColor FootDye(const FRiverfrontWearable& Item)
{
    return Item.Id==RiverfrontLife::CottonSocks ? FLinearColor(FColor(211,212,215)) : Item.Tint;
}
}

void ARiverfrontCharacter::DyeWardrobeSection(UMeshComponent& Mesh,FName Slot,const FLinearColor& Dye,UMaterialInterface* Parent)
{
    const int32 Index=Mesh.GetMaterialIndex(Slot);
    if (Index==INDEX_NONE) return;
    auto* Material=Cast<UMaterialInstanceDynamic>(Mesh.GetMaterial(Index));
    if (!Material || (Parent && Material->Parent!=Parent))
    {
        if (!Parent) Parent=Mesh.GetMaterial(Index);
        if (!Parent) return;
        Material=Mesh.CreateDynamicMaterialInstance(Index,Parent);
    }
    if (Material) Material->SetVectorParameterValue(TEXT("BaseColorFactor"),Dye);
}

void ARiverfrontCharacter::SetInnerTopVisible(bool bVisible)
{
    auto* Body=GetMesh();
    const int32 Skin=Body->GetMaterialIndex(TEXT("skin"));
    if (Skin==INDEX_NONE) return;
    const auto* Item=bVisible ? RiverfrontLife::FindItem(WardrobeInnerTop) : nullptr;
    // The fabric panels share the original torso's positions and UV seams.
    for (FName Name : {FName(TEXT("foundation_knit")),FName(TEXT("foundation_elastic"))})
    {
        const int32 Slot=Body->GetMaterialIndex(Name);
        if (Slot==INDEX_NONE) continue;
        if (Item) DyeWardrobeSection(*Body,Name,InnerDye(*Item,Name==TEXT("foundation_elastic")),CottonMaterial);
        else Body->SetMaterial(Slot,Body->GetMaterial(Skin));
        if (ChangingMesh) ChangingMesh->SetMaterial(Slot,Body->GetMaterial(Slot));
    }
}

USkeletalMesh* ARiverfrontCharacter::LoadLowerBodyMesh() const
{
    const TCHAR* Path=AvatarGender==ERiverfrontGender::Female
        ? TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Foundation/SK_FemaleLowerBody.SK_FemaleLowerBody")
        : TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/Foundation/SK_MaleLowerBody.SK_MaleLowerBody");
    return LoadObject<USkeletalMesh>(nullptr,Path);
}

void ARiverfrontCharacter::SetLowerBodyVisible(bool bVisible,bool bMovingBriefs,int32 ShoeMask,int32 SockMask)
{
    auto* Body=GetMesh();
    if (!SupportsClothingChange())
    {
        if (LowerBody) LowerBody->SetVisibility(false);
        return;
    }
    const auto* Bottom=RiverfrontLife::FindItem(WardrobeBottom);
    const FName Trousers=Bottom && !bVisible ? TrouserSection(Bottom->Cut) : NAME_None;
    const bool bShorts=Trousers==TEXT("denim_shorts");
    const FName ShoeId=ShoeMask==INDEX_NONE ? WardrobeShoes : ChangingFootwear;
    const FName SockId=SockMask==INDEX_NONE ? WardrobeSocks : ChangingFootwear;
    const auto* Shoe=RiverfrontLife::FindItem(ShoeId);
    const auto* Sock=RiverfrontLife::FindItem(SockId);
    const bool bSplitFeet=bLyingOnBed || ShoeId!=RiverfrontLife::StreetShoes || SockId!=RiverfrontLife::CottonSocks ||
        ShoeMask!=INDEX_NONE || SockMask!=INDEX_NONE || bShorts;
    const bool bShowLower=bVisible || bSplitFeet || !Trousers.IsNone();
    const int32 Shoes=bLyingOnBed || !Shoe ? 0 : ShoeMask!=INDEX_NONE ? ShoeMask : 3;
    const int32 Socks=!Sock ? 0 : SockMask!=INDEX_NONE ? SockMask : 3;
    if (bShowLower)
    {
        auto* Shape=LoadLowerBodyMesh();
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
        }

        const auto* Brief=bVisible && !bMovingBriefs ? RiverfrontLife::FindItem(WardrobeInnerBottom) : nullptr;
        const bool bTrunks=IsTrunks(Brief);
        UMaterialInterface* Skin=Body->GetMaterial(Body->GetMaterialIndex(TEXT("skin")));
        if (Skin)
            for (FName Name : {FName(TEXT("leg_skin")),FName(TEXT("trunk_skin")),FName(TEXT("shorts_skin")),
                FName(TEXT("bare_lower_skin")),FName(TEXT("midriff_skin")),BareFoot.Slots[0],BareFoot.Slots[1],
                BareAnkle.Slots[0],BareAnkle.Slots[1]})
                LowerBody->SetMaterialByName(Name,Skin);
        ShowSection(*LowerBody,TEXT("leg_skin"),Brief && !bTrunks);
        ShowSection(*LowerBody,TEXT("trunk_skin"),bTrunks);
        ShowSection(*LowerBody,TEXT("shorts_skin"),bShorts);
        ShowSection(*LowerBody,TEXT("bare_lower_skin"),bVisible && !Brief);
        ShowSection(*LowerBody,TEXT("midriff_skin"),(bVisible || bShorts) && WardrobeOutfit!=RiverfrontLife::BaseLayer);
        ShowSection(*LowerBody,TEXT("tailored_trousers"),Trousers==TEXT("tailored_trousers"));
        ShowSection(*LowerBody,TEXT("denim_shorts"),bShorts);
        if (!Trousers.IsNone()) DyeWardrobeSection(*LowerBody,Trousers,Bottom->Tint);

        const FName Fabric=bTrunks ? TEXT("trunk_knit") : TEXT("underwear_knit");
        const FName Band=bTrunks ? TEXT("trunk_band") : TEXT("underwear_band");
        for (FName Slot : {FName(TEXT("underwear_knit")),FName(TEXT("underwear_band")),
            FName(TEXT("trunk_knit")),FName(TEXT("trunk_band"))})
            ShowSection(*LowerBody,Slot,Brief && (Slot==Fabric || Slot==Band));
        if (Brief)
        {
            DyeWardrobeSection(*LowerBody,Fabric,InnerDye(*Brief,false),CottonMaterial);
            DyeWardrobeSection(*LowerBody,Band,InnerDye(*Brief,true),CottonMaterial);
        }

        const auto& ShoeStyle=FootSections(Shoe ? Shoe->Cut : ERiverfrontGarmentCut::Standard,true);
        const auto& SockStyle=FootSections(Sock ? Sock->Cut : ERiverfrontGarmentCut::Standard,false);
        for (int32 Side=0;Side<2;++Side)
        {
            const int32 Bit=1<<Side;
            const bool bHasShoe=bSplitFeet && (Shoes&Bit);
            const bool bHasSock=bSplitFeet && (Socks&Bit);
            for (const auto* Style : {&StreetShoe,&CourtShoe,&Boot})
                ShowSection(*LowerBody,Style->Slots[Side],bHasShoe && Style==&ShoeStyle);
            for (const auto* Style : {&AnkleSock,&CrewSock})
                ShowSection(*LowerBody,Style->Slots[Side],bHasSock && Style==&SockStyle);
            ShowSection(*LowerBody,BareFoot.Slots[Side],bSplitFeet && !bHasSock && !bHasShoe);
            ShowSection(*LowerBody,BareAnkle.Slots[Side],bHasShoe && !bHasSock);
            ShowSection(*LowerBody,SockToe.Slots[Side],bHasSock && !bHasShoe);
            if (bHasShoe)
                DyeWardrobeSection(*LowerBody,ShoeStyle.Slots[Side],ShoeId==RiverfrontLife::StreetShoes ? FLinearColor::White : FootDye(*Shoe));
            if (bHasSock)
            {
                DyeWardrobeSection(*LowerBody,SockStyle.Slots[Side],FootDye(*Sock),CottonMaterial);
                if (!bHasShoe) DyeWardrobeSection(*LowerBody,SockToe.Slots[Side],FootDye(*Sock),CottonMaterial);
            }
        }
        LowerBody->SetLeaderPoseComponent(ChangingMesh ? static_cast<USkinnedMeshComponent*>(ChangingMesh.Get()) : Body);
    }

    for (auto* View : {static_cast<USkinnedMeshComponent*>(Body),static_cast<USkinnedMeshComponent*>(ChangingMesh.Get())})
    {
        if (!View) continue;
        ShowSection(*View,TEXT("bottom_denim"),!bVisible && Trousers.IsNone());
        ShowSection(*View,TEXT("shoe"),!bSplitFeet);
    }
    if (LowerBody) LowerBody->SetVisibility(bShowLower);
}

UPoseableMeshComponent* ARiverfrontCharacter::CreateGarmentDisplay(FName Item,int32 Side)
{
    const auto* Entry=RiverfrontLife::FindItem(Item);
    if (!Entry) return nullptr;
    auto* Body=GetMesh();
    const bool bBriefs=Entry->Slot==ERiverfrontWearSlot::InnerBottom;
    const bool bInnerTop=Entry->Slot==ERiverfrontWearSlot::InnerTop;
    const bool bShoes=Entry->Slot==ERiverfrontWearSlot::Shoes;
    const bool bFootwear=bShoes || Entry->Slot==ERiverfrontWearSlot::Socks;
    const FName Trousers=TrouserSection(Entry->Cut);
    const auto& Footwear=FootSections(Entry->Cut,bShoes);
    USkeletalMesh* Shape=Body->GetSkeletalMeshAsset();
    FName Panel=TEXT("bottom_denim");
    FName Band;
    if (bInnerTop)
    {
        Shape=LoadFoundationMesh(Item);
        Panel=TEXT("foundation_knit");
        Band=TEXT("foundation_elastic");
    }
    else if (bBriefs || bFootwear || !Trousers.IsNone())
    {
        Shape=LoadLowerBodyMesh();
        if (bBriefs)
        {
            Panel=IsTrunks(Entry) ? TEXT("trunk_knit") : TEXT("underwear_knit");
            Band=IsTrunks(Entry) ? TEXT("trunk_band") : TEXT("underwear_band");
        }
        else Panel=Trousers;
    }
    else if (Entry->Slot==ERiverfrontWearSlot::Top)
    {
        Shape=LoadFittedTop(Entry->Cut);
        Panel=TEXT("western_top");
    }
    if (!Shape || !Shape->GetMaterials().ContainsByPredicate([&](const FSkeletalMaterial& Slot)
    {
        return bFootwear ? Slot.MaterialSlotName==Footwear.Slots[0] : Slot.MaterialSlotName==Panel;
    })) return nullptr;

    auto* Garment=NewObject<UPoseableMeshComponent>(this);
    AddInstanceComponent(Garment);
    Garment->SetupAttachment(Body);
    Garment->SetSkinnedAssetAndUpdate(Shape);
    Garment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Garment->SetCanEverAffectNavigation(false);
    Garment->RegisterComponent();
    for (const auto& Slot : Shape->GetMaterials())
    {
        const FName Name=Slot.MaterialSlotName;
        const bool bDisplay=bFootwear ? ((Side!=1 && (Name==Footwear.Slots[0] || (!bShoes && Name==SockToe.Slots[0]))) ||
            (Side!=0 && (Name==Footwear.Slots[1] || (!bShoes && Name==SockToe.Slots[1]))))
            : Name==Panel || (!Band.IsNone() && Name==Band);
        ShowSection(*Garment,Name,bDisplay);
        if (!bDisplay) continue;

        if (bBriefs || bInnerTop)
            DyeWardrobeSection(*Garment,Name,InnerDye(*Entry,Name==Band),CottonMaterial);
        else if (bFootwear)
        {
            if (Item!=RiverfrontLife::StreetShoes)
                DyeWardrobeSection(*Garment,Name,FootDye(*Entry),bShoes ? nullptr : CottonMaterial.Get());
        }
        else
        {
            const bool bChino=Entry->Cut==ERiverfrontGarmentCut::Chino;
            const FLinearColor Dye=Item==TEXT("denim-dark") ? FLinearColor(FColor(53,62,73)) :
                Item==TEXT("denim-washed") ? FLinearColor(FColor(121,149,173)) : Entry->Tint;
            DyeWardrobeSection(*Garment,Name,Dye,bChino ? ChinoFabric() : nullptr);
        }
    }
    Garment->CopyPoseFromSkeletalComponent(Body);
    Garment->RefreshBoneTransforms();
    return Garment;
}

FTransform ARiverfrontCharacter::FoldedGarmentTransform(const USkinnedMeshComponent& Wearer,FName Item) const
{
    auto* Body=GetMesh();
    const FVector Palm=Wearer.GetSocketLocation(TEXT("Handr"));
    const FVector Grip=Palm+(Palm-Wearer.GetSocketLocation(TEXT("Elbowr"))).GetSafeNormal()*7.;
    const auto* Entry=RiverfrontLife::FindItem(Item);
    const bool bFoot=Entry && (Entry->Slot==ERiverfrontWearSlot::Shoes || Entry->Slot==ERiverfrontWearSlot::Socks);
    const bool bChest=Entry && (Entry->Slot==ERiverfrontWearSlot::InnerTop || Entry->Slot==ERiverfrontWearSlot::Top);
    const FVector Anchor=bFoot ? (Body->GetSocketTransform(TEXT("Footl"),RTS_Component).GetLocation()+
        Body->GetSocketTransform(TEXT("Footr"),RTS_Component).GetLocation())*.5 :
        Body->GetSocketTransform(bChest ? TEXT("Chest") : TEXT("Pelvis"),RTS_Component).GetLocation();
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
        const auto* Entry=RiverfrontLife::FindItem(Item);
        SetLowerBodyVisible(true,Entry && Entry->Slot==ERiverfrontWearSlot::InnerBottom);
        ShowHeldWardrobeItem(NAME_None);
    }
    PoseLowerDressingBody(Undress);
    const auto Ease=[Undress](float A,float B){ return FMath::SmoothStep(A,B,Undress); };
    const auto Bone=[this](FName Name){return ChangingMesh->GetBoneTransformByName(Name,EBoneSpaces::ComponentSpace);};
    const double Gather=Ease(.12f,.64f), Pickup=Ease(.68f,.96f);
    const FVector Forward=Body->GetComponentQuat().UnrotateVector(GetActorForwardVector());
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

void ARiverfrontCharacter::PoseChangingInnerTop(FName Item,float Undress)
{
    if (!PreparePosedMesh()) return;
    if (!MovingGarment)
    {
        MovingGarment=CreateGarmentDisplay(Item);
        if (!MovingGarment) return;
        MovingGarment->ComponentTags.Add(TEXT("WardrobeMovingBra"));
        ShowHeldWardrobeItem(NAME_None);
        SetInnerTopVisible(false);
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
    Placed.Blend(Lifted,FoldedGarmentTransform(*ChangingMesh,Item),Hand);
    MovingGarment->SetWorldTransform(Placed);
}

void ARiverfrontCharacter::PoseLowerDressingBody(float Undress,int32 OnlyFoot,double FootLift)
{
    auto* Body=GetMesh();
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
        const double Lift=OnlyFoot==INDEX_NONE ? FMath::Square(FMath::Sin(PI*Step)) : OnlyFoot==Side ? FootLift : 0.;
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
}

void ARiverfrontCharacter::PoseChangingFootwear(FName Item,float Undress)
{
    if (!PreparePosedMesh()) return;
    const int32 Side=Undress<.5f ? 0 : 1;
    if (Side!=ChangingFoot || !MovingGarment)
    {
        if (MovingGarment) { MovingGarment->DestroyComponent(); MovingGarment=nullptr; }
        ChangingFoot=Side;
        const int32 Stationary=Side==0 ? 2 : 0;
        const auto* Entry=RiverfrontLife::FindItem(Item);
        const bool bShoes=Entry && Entry->Slot==ERiverfrontWearSlot::Shoes;
        ChangingFootwear=Item;
        SetLowerBodyVisible(WardrobeBottom==RiverfrontLife::Underwear,false,bShoes ? Stationary : INDEX_NONE,bShoes ? INDEX_NONE : Stationary);
        MovingGarment=CreateGarmentDisplay(Item,Side);
        if (!MovingGarment) return;
        MovingGarment->ComponentTags.Add(TEXT("WardrobeMovingFootwear"));
        ShowHeldWardrobeItem(NAME_None);
    }
    const float Pass=Undress*2.f-Side;
    const float Grip=FMath::SmoothStep(.20f,.84f,Pass);
    const double Bend=FMath::Square(FMath::Sin(PI*Pass));
    PoseLowerDressingBody(.62f*Bend,Side,Bend);
    auto* Body=GetMesh();
    const FName Foot=Side ? TEXT("Footr") : TEXT("Footl");
    const FTransform RestFoot=Body->GetSocketTransform(Foot,RTS_Component);
    const FTransform Worn=RestFoot.Inverse()*ChangingMesh->GetSocketTransform(Foot);
    FTransform Lifted=Worn;
    Lifted.AddToTranslation(GetActorForwardVector()*(Item==RiverfrontLife::StreetShoes ? 17. : 9.)*Grip);
    FTransform Held=FoldedGarmentTransform(*ChangingMesh,Item);
    const FVector Hand=ChangingMesh->GetSocketLocation(Side ? TEXT("Handr") : TEXT("Handl"));
    Held.SetLocation(Hand-Held.TransformVector(RestFoot.GetLocation()));
    FTransform Final;
    Final.Blend(Lifted,Held,Grip);
    MovingGarment->SetWorldTransform(Final);
}
