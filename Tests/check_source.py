"""Cheap repository checks only; Unreal compilation and playtesting run on macOS."""

import configparser
import json
import re
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "Source" / "SabarmatiPrototype"
PRIVATE = SOURCE / "Private"


class SourceTests(unittest.TestCase):
    def test_project_wiring(self):
        project = json.loads((ROOT / "SabarmatiPrototype.uproject").read_text())
        self.assertEqual(project["EngineAssociation"], "5.8")
        self.assertEqual(project["Modules"][0]["Name"], "SabarmatiPrototype")
        for filename, target_type in (
            ("SabarmatiPrototype.Target.cs", "Game"),
            ("SabarmatiPrototypeEditor.Target.cs", "Editor"),
        ):
            target = (ROOT / "Source" / filename).read_text()
            self.assertIn(f"Type = TargetType.{target_type};", target)
            self.assertIn('ExtraModuleNames.Add("SabarmatiPrototype")', target)

        for header in (SOURCE / "Public").glob("*.h"):
            data = header.read_text()
            if re.search(r"\bU(?:CLASS|STRUCT|ENUM|INTERFACE|DELEGATE)\s*\(", data):
                self.assertIn(f'#include "{header.stem}.generated.h"', data)
            if re.search(r"\bU(?:CLASS|STRUCT|INTERFACE)\s*\(", data):
                self.assertIn("GENERATED_BODY()", data)

    def test_map_and_renderer(self):
        config = configparser.ConfigParser(strict=False)  # Unreal allows repeated +Array entries.
        config.read(ROOT / "Config" / "DefaultEngine.ini")
        maps = config["/Script/EngineSettings.GameMapsSettings"]
        self.assertEqual(maps["GameDefaultMap"], "/Engine/Maps/Entry")
        self.assertEqual(maps["GlobalDefaultGameMode"], "/Script/SabarmatiPrototype.RiverfrontGameMode")
        renderer = config["/Script/Engine.RendererSettings"]
        self.assertEqual(renderer.getint("r.DynamicGlobalIlluminationMethod"), 0)
        self.assertFalse(renderer.getboolean("r.RayTracing"))
        self.assertEqual(renderer.getint("r.Shadow.Virtual.Enable"), 0)
        self.assertFalse(renderer.getboolean("r.Nanite.ProjectEnabled"))

    def test_bindings_have_config(self):
        config = (ROOT / "Config" / "DefaultInput.ini").read_text()
        axes = set(re.findall(r'AxisName="(\w+)"', config))
        actions = set(re.findall(r'ActionName="(\w+)"', config))
        self.assertIn("CapturePermanently_IncludingInitialMouseDown", config)
        for file in ("RiverfrontCharacter.cpp", "RiverfrontCar.cpp", "RiverfrontPlayerController.cpp"):
            code = (PRIVATE / file).read_text()
            self.assertTrue(set(re.findall(r'BindAxis\(TEXT\("(\w+)"\)', code)) <= axes)
            self.assertTrue(set(re.findall(r'BindAction\(TEXT\("(\w+)"\)', code)) <= actions)
            if file != "RiverfrontPlayerController.cpp":
                self.assertIn('BindAction(TEXT("Interact")', code)
        # The Unreal Editor can serialize the same mapping with explicit default modifiers.
        self.assertRegex(config, r'(?m)^\+ActionMappings=\(ActionName="PauseDrive",(?:b(?:Shift|Ctrl|Alt|Cmd)=False,)*Key=Escape\)$')
        menu = (PRIVATE / "RiverfrontPlayerController.cpp").read_text()
        for route in ('EScreen::Title', 'EScreen::Paused', 'SetPause(true)', 'SetPause(false)',
                      'FInputModeUIOnly', 'FInputModeGameOnly', 'EKeys::Escape'):
            self.assertIn(route, menu)

    def test_bank_geometry_connects(self):
        source = (PRIVATE / "RiverfrontWorld.cpp").read_text()
        def get_y_extent(geometry):
            match = re.search(geometry, source)
            self.assertIsNotNone(match)
            center, width = map(float, match.groups())
            return center - width / 2, center + width / 2

        near, far = get_y_extent(r"FVector\(0, BankY\(Bank, (\d+)\), -23\), FVector\(15500, (\d+), 50\)")
        road_near, road_far = get_y_extent(r"FVector\(0, BankY\(Bank, (\d+)\), -28\), FVector\(15500, (\d+), 56\)")
        channel = float(re.search(r"ChannelWidth = ([0-9.]+)", source).group(1))
        water_edge = float(re.search(r"NearWaterEdge = ([0-9.]+)", source).group(1))
        west_abutment = float(re.search(r"BridgeHalfLength = ([0-9.]+) - ChannelCenter", source).group(1))
        channel_centre = water_edge - channel * .5
        east_abutment = 2 * channel_centre - west_abutment
        self.assertEqual(channel, 26300.0)
        self.assertIn("DeckLength = 2.0 * BridgeHalfLength", source)
        self.assertIn("FVector(BridgeX, ChannelCenter, -20), FVector(720, DeckLength, 40)", source)
        self.assertAlmostEqual(west_abutment, road_near)
        self.assertAlmostEqual(east_abutment, 2 * channel_centre - road_near)
        self.assertIn("const double RailEnd = BridgeHalfLength - 170.0;", source)
        self.assertIn("2.0 * ChannelCenter", source)
        self.assertLessEqual(road_near, far)
        spawn = re.search(r"SpawnPoint\(-?\d+, (\d+), \d+\)", source)
        car = re.search(r"CarPosition\(-?\d+, (\d+), \d+\)", source)
        self.assertIsNotNone(spawn)
        self.assertIsNotNone(car)
        self.assertTrue(near < float(spawn.group(1)) < far)
        self.assertTrue(road_near < float(car.group(1)) < road_far)

    def test_atal_keeps_pedestrian_and_road_routes_separate(self):
        world = (PRIVATE / "RiverfrontWorld.cpp").read_text()
        source = (PRIVATE / "RiverfrontAtalBridge.cpp").read_text()
        header = (SOURCE / "Public" / "RiverfrontWorld.h").read_text()
        project = json.loads((ROOT / "SabarmatiPrototype.uproject").read_text())
        self.assertTrue(any(plugin["Name"] == "ProceduralMeshComponent" and plugin["Enabled"]
                            for plugin in project["Plugins"]))

        def numeric(text, field):
            return float(re.search(rf"{field} = (-?[\d.]+)", text).group(1))

        self.assertIn("BridgeX = RiverfrontCrossings::CentralRoadX", world)
        bridge_x = numeric((PRIVATE / "RiverfrontCrossings.h").read_text(), "CentralRoadX")
        atal_x = numeric(header, "AtalX")
        centre = numeric(source, "MidY")
        half_length = numeric(source, "HalfLength")
        self.assertAlmostEqual(2 * half_length, 28500)
        self.assertLess(centre + half_length + 180, 4090 - 510)
        self.assertGreater(centre - half_length - 180, -26010 + 510)
        self.assertEqual(2 * numeric(header, "AtalEndHalfWidth"), 1000)
        self.assertGreater(abs(bridge_x - atal_x),
                           numeric(world, "BridgeSide") + numeric(header, "AtalEndHalfWidth") + 200)
        self.assertIn("3580.0 - ChannelCenter", world)
        self.assertIn("BuildAtalBridge();", world)

    def test_offline_player_profile_gates_gameplay(self):
        source = (PRIVATE / "RiverfrontPlayerController.cpp").read_text()
        storage = (SOURCE / "Public" / "RiverfrontLocalProfile.h").read_text()
        self.assertIn("public USaveGame", storage)
        life = (PRIVATE / "RiverfrontLifeProfile.cpp").read_text()
        self.assertIn("RiverfrontLife::Create(ProfileSlot, Clean, Gender)", source)
        self.assertIn("RiverfrontLife::Load(ProfileSlot)", source)
        self.assertIn("UGameplayStatics::SaveGameToSlot", life)
        self.assertIn("UGameplayStatics::LoadGameFromSlot", life)
        self.assertNotIn("FHttpModule", life)
        self.assertIn("Screen == EScreen::Title && !HasLocalProfile()", source)
        self.assertIn('SabarmatiQA-', source)
        self.assertNotIn("FHttpModule", source)
        self.assertNotIn("Password", storage)

    def test_game_launchers_use_a_fitted_window(self):
        launch = (ROOT / "Scripts" / "run-mac.sh").read_text()
        self.assertIn('launcher=("$editor" "$project" -game)', launch)
        self.assertIn('launcher=(/usr/bin/open)', launch)
        self.assertIn('if [[ "$review" == true || "$explicit_data" == true ]]; then', launch)
        self.assertIn('launcher+=("$sabarmati_app" --args)', launch)
        self.assertIn('exec "${launcher[@]}" -windowed "-ResX=$width" "-ResY=$height"', launch)
        self.assertNotIn("-fullscreen", launch)
        self.assertIn("screen.visibleFrame", launch)
        self.assertIn("screen.backingScaleFactor", launch)
        self.assertIn("-Sabarmati4K", launch)
        self.assertIn("FullscreenMode=2", (ROOT / "Config/DefaultGameUserSettings.ini").read_text())
        self.assertNotIn("-ForceRes", launch)
        source = (PRIVATE / "RiverfrontPlayerController.cpp").read_text()
        self.assertIn("r.SecondaryScreenPercentage.GameViewport", source)

    def test_glb_skin_and_motion(self):
        path = ROOT / "SourceAssets" / "fictional-rigged.glb"
        if not path.is_file():
            self.skipTest("Optional local avatar rig is not in the public checkout")
        with path.open("rb") as file:
            magic, version, length, json_length, chunk_type = struct.unpack("<4sIII4s", file.read(20))
            self.assertEqual((magic, version, length, chunk_type), (b"glTF", 2, path.stat().st_size, b"JSON"))
            gltf = json.loads(file.read(json_length))
        self.assertEqual(len(gltf["skins"]), 1)
        self.assertEqual(len(gltf["skins"][0]["joints"]), 15)
        self.assertEqual({item["name"] for item in gltf["animations"]}, {"Idle", "Walk", "WalkAlt", "Jog"})
        self.assertTrue(all(asset["channels"] for asset in gltf["animations"]))
        skinned_mesh_nodes = [node for node in gltf["nodes"] if "skin" in node and "mesh" in node]
        self.assertTrue(skinned_mesh_nodes)


if __name__ == "__main__":
    unittest.main(verbosity=2)
