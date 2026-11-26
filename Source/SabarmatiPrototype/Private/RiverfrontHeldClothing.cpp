#include "RiverfrontCharacter.h"
#include "RiverfrontLifeProfile.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
// A folded top pinched at the shoulder. Two thicknesses turn around the lower hem.
void MakeFoldedTop(UProceduralMeshComponent& Cloth, bool bKurta)
{
    constexpr int32 Columns=13, Rows=9;
    TArray<FVector> Points, Normals;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    TArray<FProcMeshTangent> Tangents;
    const double Length=bKurta?36.:27.;
    const auto Surface=[Length](double U,double V,int32 Side)
    {
        const double Across=2.*U-1., Fold=FMath::Cos(5.*PI*U+V*.55);
        const double Width=2.+11.*FMath::Sqrt(V);
        return FVector((1.+2.2*V)*Fold+Side*(.85+.8*V), Across*Width+3.*V,
            -V*Length+(1.7*Fold+2.6*Across)*V+Side*1.6*V);
    };
    for (int32 Side=0;Side<2;++Side)
        for (int32 Row=0;Row<Rows;++Row)
            for (int32 Col=0;Col<Columns;++Col)
            {
                const double V=double(Row)/(Rows-1), U=double(Col)/(Columns-1);
                Points.Add(Surface(U,V,Side));
                UVs.Emplace(U,V);
            }
    const auto Quad=[&](int32 A,int32 B,int32 C,int32 D){Triangles.Append({A,B,C,A,C,D});};
    constexpr int32 Back=Rows*Columns;
    for (int32 Y=0;Y<Rows-1;++Y)
        for (int32 X=0;X<Columns-1;++X)
        {
            const int32 A=Y*Columns+X, B=A+1, C=B+Columns, D=A+Columns;
            Quad(A,D,C,B);
            Quad(A+Back,B+Back,C+Back,D+Back);
            if (!X) Quad(A,A+Back,D+Back,D);
            if (X==Columns-2) Quad(B,C,C+Back,B+Back);
            if (!Y) Quad(A,B,B+Back,A+Back);
            if (Y==Rows-2) Quad(D,D+Back,C+Back,C);
        }
    const auto FinishSection=[&](int32 Section)
    {
        UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Points,Triangles,UVs,Normals,Tangents);
        Cloth.CreateMeshSection_LinearColor(Section,Points,Triangles,Normals,UVs,{},Tangents,false);
        Points.Reset(); Triangles.Reset(); UVs.Reset(); Normals.Reset(); Tangents.Reset();
    };
    FinishSection(0);

    // Sew the collar and placket onto the outer fold. Sample the same draped surface
    // at every row so the placket tracks the folds all the way down the button-down.
    const auto Stitch=[&](double U,double V,double Rise)
    {
        Points.Add(Surface(U,V,1)+FVector(Rise,0,0));
        UVs.Emplace(U,V);
    };
    const double End=bKurta?.50:.94;
    for (int32 Row=0;Row<=8;++Row)
    {
        const double V=FMath::Lerp(.19,End,Row/8.);
        Stitch(.46,V,.30); Stitch(.54,V,.30);
        if (Row) Quad(Row*2-2,Row*2-1,Row*2+1,Row*2);
    }
    for (double Side : {-1.,1.})
    {
        const int32 Start=Points.Num();
        Stitch(.5,.18,.44); Stitch(.5+Side*.26,.115,.35);
        Stitch(.5+Side*.28,bKurta?.19:.24,.56);
        Stitch(.5+Side*.13,bKurta?.23:.37,.68);
        Quad(Start,Start+1,Start+2,Start+3);
    }
    FinishSection(1);

    for (int32 Button=0;Button<(bKurta?2:4);++Button)
    {
        const int32 Centre=Points.Num();
        const FVector ButtonCentre=Surface(.50,.29+Button*.16,1)+FVector(.57,0,0);
        Points.Add(ButtonCentre); UVs.Emplace(.5,.5);
        constexpr int32 Sides=8;
        for (int32 I=0;I<Sides;++I)
        {
            const double Angle=2.*PI*I/Sides, Y=FMath::Cos(Angle), Z=FMath::Sin(Angle);
            Points.Add(ButtonCentre+FVector(-.08,.37*Y,.37*Z));
            UVs.Emplace(.5+.5*Y,.5+.5*Z);
        }
        for (int32 I=0;I<Sides;++I)
            Triangles.Append({Centre,Centre+1+I,Centre+1+(I+1)%Sides});
    }
    FinishSection(2);
}
}

bool ARiverfrontCharacter::HasHeldWardrobeProp(FName Id) const
{
    return HeldProp && HeldProp->IsVisible() && HeldPropItem==Id && !HeldProp->GetAttachSocketName().IsNone();
}

void ARiverfrontCharacter::PoseHeldWardrobeItem()
{
    if (!HeldProp || !bInFittingPose) return;
    PoseChangingSleeves(.69f,0.f);
    // Gravity pulls the loose ends down while the hand lifts the folded item into view.
    HeldProp->SetWorldRotation(GetActorRotation());
}

void ARiverfrontCharacter::ShowHeldWardrobeItem(FName Id)
{
    auto* Body=GetAvatarMesh();
    if (HeldProp && HeldPropItem==Id && HeldProp->GetAttachParent()==Body) return;
    if (HeldProp) { HeldProp->DestroyComponent(); HeldProp=nullptr; }
    HeldPropItem=NAME_None;
    if (Id.IsNone() || !Body || !Body->GetSkeletalMeshAsset()) return;
    const FName Hand=Body->DoesSocketExist(TEXT("Handr")) ? TEXT("Handr") : TEXT("hand_r");
    if (!Body->DoesSocketExist(Hand)) return;
    const auto* Item=RiverfrontLife::Catalog().FindByPredicate([Id](const auto& Entry){return Entry.Id==Id;});
    if (!Item) return;
    if (Item->Slot==ERiverfrontWearSlot::Accessory)
    {
        if (!Wristwatch->GetStaticMesh()) return;
        auto* Watch=NewObject<UStaticMeshComponent>(this);
        Watch->SetStaticMesh(Wristwatch->GetStaticMesh());
        HeldProp=Watch;
    }
    else
    {
        auto* Cloth=NewObject<UProceduralMeshComponent>(this);
        MakeFoldedTop(*Cloth,Id==TEXT("kurta"));
        if (auto* Fabric=CottonMaterial ? UMaterialInstanceDynamic::Create(CottonMaterial,Cloth) : nullptr)
        {
            Fabric->SetVectorParameterValue(TEXT("BaseColorFactor"),Item->Tint);
            Cloth->SetMaterial(0,Fabric);
            auto* Trim=UMaterialInstanceDynamic::Create(CottonMaterial,Cloth);
            Trim->SetVectorParameterValue(TEXT("BaseColorFactor"),Item->Tint*.72f);
            Cloth->SetMaterial(1,Trim);
            auto* Buttons=UMaterialInstanceDynamic::Create(CottonMaterial,Cloth);
            Buttons->SetVectorParameterValue(TEXT("BaseColorFactor"),FLinearColor(.50,.40,.27));
            Cloth->SetMaterial(2,Buttons);
        }
        HeldProp=Cloth;
    }
    AddInstanceComponent(HeldProp);
    HeldProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeldProp->SetCanEverAffectNavigation(false);
    HeldProp->SetupAttachment(Body,Hand);
    HeldProp->RegisterComponent();
    // Build the grip in the reference hand frame, independent of the current animation.
    const auto& Skeleton=Body->GetSkeletalMeshAsset()->GetRefSkeleton();
    const int32 Index=Skeleton.FindBoneIndex(Hand);
    if (Index!=INDEX_NONE)
    {
        const FTransform Joint=Skeleton.GetBoneAbsoluteTransform(Index);
        const FVector Wrist=Joint.GetLocation();
        const FVector Elbow=Skeleton.GetBoneAbsoluteTransform(Skeleton.GetParentIndex(Index)).GetLocation();
        const FQuat LocalAxes=Body->GetRelativeRotation().Quaternion().Inverse();
        const FTransform Grip(LocalAxes,Wrist+(Wrist-Elbow).GetSafeNormal()*7.);
        HeldProp->SetRelativeTransform(Grip.GetRelativeTransform(Joint));
    }
    HeldPropItem=Id;
}
