#pragma once
#include "Engine/EngineTypes.h"
#include "GameFramework/HUD.h"
#include "RiverfrontHUD.generated.h"

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;

private:
    TArray<FCanvasUVTri> HUDTriangles;
    float WelcomeUntil = -1.f;
};
