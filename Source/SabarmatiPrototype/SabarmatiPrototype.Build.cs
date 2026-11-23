using UnrealBuildTool;

public class SabarmatiPrototype : ModuleRules
{
    public SabarmatiPrototype(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "ProceduralMeshComponent", "RenderCore", "RHI" });
    }
}
