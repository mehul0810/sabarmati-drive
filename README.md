# Sabarmati Drive | Unreal Engine 5 Mac prototype

Sabarmati Drive is a native Apple Silicon free-roam Unreal prototype. It has a detailed, hand-built riverfront with both banks, Atal pedestrian bridge and separate road crossings, plus a larger offline Ahmedabad mode built from [OpenStreetMap](https://www.openstreetmap.org/copyright). Both share the same offline profile, controls and interface. The city’s road geometry is map-derived; terrain, building heights and façades are approximations, not a survey or a photoreal reconstruction. See [map sources and attribution](SourceAssets/AHMEDABAD-CITY-SOURCES.md) and [art sources](SourceAssets/RIVERFRONT-SOURCES.md).

## Start on an Apple Silicon Mac

1. Install **Unreal Engine 5.8** from Epic Games Launcher and a compatible full **Xcode**. Open Xcode once to finish installing its SDK and components. See [Epic’s version-specific macOS and Xcode requirements](https://dev.epicgames.com/documentation/unreal-engine/macos-development-requirements-for-unreal-engine) before installing. Epic lists macOS 14.5 and Xcode 26.0 as minimums for UE 5.8 and recommends Xcode 26.1.1. Xcode 27 on this Mac still needs a real UE build check.
2. Open the extracted `Sabarmati-Drive-UE58-Mac` folder and open Terminal there. Use a native Apple Silicon terminal, not a shell translated by Rosetta.
3. Build the C++ project and launch a resizable game window up to 1600×900:

   ```bash
   bash Scripts/run-mac.sh
   ```

   If Epic is installed outside the usual directory, set its root (the folder containing `Engine`):

   ```bash
   UE_ROOT="/your/path/to/UE_5.8" bash Scripts/run-mac.sh
   ```

Use `bash Scripts/run-mac.sh --editor` for editing and avatar import. The engine Entry map may look empty in the editor; the district appears when you press **Play**. Click the viewport once if needed to capture the pointer, then look around without mouse drag. Click **Stop** (or use the keyboard shortcut shown by Unreal) to release the editor viewport. After a successful build you can launch without compiling again with `bash Scripts/run-mac.sh --no-build`. The first C++ build and shader preparation can take time.

After building, **Play Sabarmati Drive.command** opens the riverfront in a titled window. **Play Sabarmati Drive 4K.command** opens Ahmedabad City in a titled Retina window with 100% render scale, up to 3840×2160 backing pixels. Both presets fit the primary display’s available desktop and leave room for the window controls.

## Riverfront lighting

The regular window opens the riverfront at golden hour. Press **T** during free roam to cycle through night, daylight and golden hour. Streetlights cast pools on the footpaths and carriageways. Selected apartment windows light up, and the occupied car switches its dipped beams on at night.

Run `bash Scripts/benchmark-riverfront.sh all` for the fixed-camera comparison and a 45 m drive under each lighting condition. Each run writes screenshots and raw frame timings under `Saved/RenderingBenchmarks`. [Technical audit and benchmark](Docs/Riverfront-rendering.md)

## Controls

| On foot | In the car |
|---|---|
| WASD move; mouse orbit; Shift jog; Space jump | W accelerate; S brake/reverse; A/D steer; mouse orbit; Space brake |
| E enters the parked car when near either door | E exits when stopped and there is room next to the car |
| R resets to the promenade start | R returns the car to where it started |
| F3 toggles Unreal frame timings | F3 toggles Unreal frame timings |

The parked car is on the street above the starting promenade. Move inland first (turn right from the spawn view) and then a few metres along the road. Head along the road toward the warm-colored bridge masts and turn toward the river to cross. The car uses bounded arcade driving and swept chassis collision, not suspension or crash-damage physics.

## Optional local avatar import

`SourceAssets/fictional-rigged.glb` may be supplied in a local checkout. The prototype rig uses one skin, 15 joints and four named clips: **Idle**, **Walk**, **WalkAlt** and **Jog**. The prototype rig is not distributed in the public repository. The raw GLB is not loaded in-game. Out of the box, the UE source uses a simple animated fallback figure. To use a locally provided, appropriately licensed avatar:

1. In the UE editor, create **Content/Sabarmati/Avatar**. In the Content Browser, import the GLB as an asset into that folder (do not use **Import Into Level**). In the [Interchange importer](https://dev.epicgames.com/documentation/unreal-engine/interchange-import-reference-in-unreal-engine), enable **Import Skeletal Meshes** and **Import Animations**. Enable **Combine Skeletal Meshes**, if displayed, so the skinned body/clothes/shoes use one skeletal mesh and one skeleton. Save imported materials and textures.
2. Rename the imported skeletal mesh **SK_Sabarmati**, and its Idle, Walk and Jog animation assets **A_Idle**, **A_Walk** and **A_Jog**, all directly in that folder. The mesh and three clips must reference the same Skeleton asset. The optional `WalkAlt` clip is not loaded by default. If your Unreal glTF importer does not bring the skinned parts or clips together, re-export the fictional source rig from Blender as skeletal FBX with baked animation clips and import against the same Skeleton; do not use the fallback as proof of a successful import.
3. Preview orientation (UE forward is +X), height (~1.8 m), ground contact, clothes/shoes and foot movement in the editor, then press Play. If necessary correct import transforms before saving. The prototype uses simple speed-based clip selection, not production foot IK, turning animations, motion matching, or retargeting.
4. For a distributable build using the avatar, add **/Game/Sabarmati/Avatar** in Project Settings → Packaging → Additional Asset Directories to Cook. Those assets are found by name only and must be explicitly cooked.

## Run a native smoke test

The default is Metal SM5 and conventional dynamic lighting, with Nanite, Virtual Shadow Maps, hardware ray tracing and Lumen disabled. This configuration aims to run on an M1 Max while leaving room to tune the M2 Air. It is not a measured performance claim.

- Build/launch on the **M1 Max** first. Check the map loads at the promenade; inspect lighting, river/banks, street silhouettes and HUD at 1600×900. Walk, orbit without drag, jog, jump, and reset. Cross the bridge on foot; check the sidewalks are continuous. If using the imported avatar, inspect the idle/walk/jog changes, shoe/foot alignment and foot contact from the side, not just standing still.
- Drive onto the bridge and across, check low-speed and reverse steering, wheel spin, rail blocking, braking, exit from either side and refusal when moving or neither exit is clear. Confirm the avatar is visible after exiting and both R resets return to the start.
- Run a **20-minute repeatable pass on the fanless M2 Air**, starting after shader compilation. Begin in a 1600×900 window (record the actual render/drawable resolution from the HUD and note power mode and any changed resolution scale). Repeat the same walk/drive/bridge loop. Compare cold and warm frame timing, not just a momentary average; aim for sustained 30 fps (~33.3 ms/frame) with no recurring long spikes. Reduce the window to 1280×720 and re-test if needed. Capture timings, device and settings rather than assuming the same M1 Max result.

   ```bash
   MTL_HUD_ENABLED=1 bash Scripts/run-mac.sh --game --no-build
   ```

   [Apple’s Metal HUD instructions](https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance) cover the HUD variable and metrics. Use F3 for Unreal's game-thread, draw and GPU breakdown.

## Optional local Development app

```bash
bash Scripts/run-mac.sh --package
```

This requests a local arm64 cook and package under `Saved/MacPackage`. It does not notarize, distribute, or submit the app. After packaging, start the resulting `.app` in Finder and run the smoke test again. A packaged build may differ from editor behavior.

## Source checks and limits

```bash
bash -n Scripts/run-mac.sh
python3 Tests/check_source.py
```

These checks validate project metadata, runtime inputs, renderer flags, a world layout sanity check, and the included glTF skin/animation data. They do not compile Unreal headers or prove native rendering, input, cooking, packaging, or frame rates. The development game was built and run on Apple Silicon with Unreal 5.8.4; packaged and other-device verification are separate.

### Start and pause

The native launcher shows a **Loading game…** window before the title screen. It has no percentage because startup does not expose a reliable progress estimate. Click **Start** to enter free roam after creating or loading a local player profile.

The title uses generated environmental concept artwork as a backdrop; it is not a gameplay capture.

The startup artwork is `Content/Splash/Splash.png`. Regenerate it on macOS with `swift Scripts/make-mac-splash.swift Content/Splash/Splash.png`.
During play, **Esc** freezes the simulation and releases the pointer. Click
**Resume** or press **Esc** again to resume and recapture the pointer. **Quit Game** closes the game from either the title or pause menu. For normal
keyboard QA use a standalone game window; Unreal Editor can intercept Esc in PIE.

Across the street from the starting cars, a brick-and-plaster block has a shaded colonnade, a public cross-passage and a paved circuit around the back. In **Explore Ahmedabad**, a longer version occupies the mapped plot beside the starting road. Both routes are accessible on foot. The road surface uses scanned asphalt, and the central streets include worn markings, flush drains and kerbs. See [the material sources](SourceAssets/RIVERFRONT-SOURCES.md) and [city map notes](SourceAssets/AHMEDABAD-CITY-SOURCES.md).
