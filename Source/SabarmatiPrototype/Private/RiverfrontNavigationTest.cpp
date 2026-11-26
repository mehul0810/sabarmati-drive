#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RiverfrontNavigation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSabarmatiBridgeGuidance,
    "Sabarmati.Navigation.BridgeGuidance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSabarmatiBridgeGuidance::RunTest(const FString& Parameters)
{
    using RiverfrontNavigation::Waypoint;
    const FVector West(-8500., 4800., 98.), East(43400., -27600., 98.);
    const FVector SameBank(-2000., 6100., 98.);
    TestEqual(TEXT("same-bank destination remains direct"), Waypoint(West,SameBank,false,98.), SameBank);

    const FVector FootEntry=Waypoint(West,East,false,98.);
    TestTrue(TEXT("walking guidance leads to Atal's west approach"),
        FMath::IsNearlyEqual(FootEntry.X,ARiverfrontWorld::AtalX) && FMath::IsNearlyEqual(FootEntry.Y,3410.));
    const FVector FootMid(ARiverfrontWorld::AtalX,RiverfrontCrossings::CentreY,121.);
    const FVector FootExit=Waypoint(FootMid,East,false,98.);
    TestTrue(TEXT("walking guidance stays on the bridge beyond its midpoint"),
        FMath::IsNearlyEqual(FootExit.X,FootMid.X) && FMath::IsNearlyEqual(FootExit.Y,-25330.));
    const FVector ReturnExit=Waypoint(FootMid,West,false,98.);
    TestTrue(TEXT("returning across the bridge points to the west landing"), FMath::IsNearlyEqual(ReturnExit.Y,3410.));

    const FVector CarWest(-37000.,4090.,68.), CarEast(-35000.,-26010.,68.);
    const FVector CarEntry=Waypoint(CarWest,CarEast,true,68.);
    TestTrue(TEXT("car route stays inside the district"),SabarmatiDistrict::Contains(CarEntry,false));
    TestTrue(TEXT("car uses the central road bridge and its southbound lane"),
        CarEntry.X>2800. && CarEntry.X<3150. && FMath::IsNearlyEqual(CarEntry.Y,4090.));
    const FVector CarMid(CarEntry.X,RiverfrontCrossings::CentreY,
        68.+RiverfrontCrossings::DeckZ(RiverfrontCrossings::CentreY));
    const FVector CarExit=Waypoint(CarMid,CarEast,true,68.);
    TestTrue(TEXT("car guidance stays on its lane through the crossing"),
        FMath::IsNearlyEqual(CarExit.X,CarMid.X) && FMath::IsNearlyEqual(CarExit.Y,-26010.));
    TestEqual(TEXT("guidance becomes direct after arriving on the destination bank"),
        Waypoint(FVector(CarMid.X,-26100.,68.),CarEast,true,68.),CarEast);
    return true;
}
#endif
