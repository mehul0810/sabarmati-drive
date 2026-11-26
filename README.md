# Sabarmati Drive | Unreal Engine 5 Mac prototype

Sabarmati Drive is a native Apple Silicon free-roam Unreal prototype. It has a detailed, hand-built riverfront with both banks, Atal pedestrian bridge and separate road crossings, plus a larger offline Ahmedabad mode built from [OpenStreetMap](https://www.openstreetmap.org/copyright). Both share the same offline profile, controls and interface. The city’s road geometry is map-derived; terrain, building heights and façades are approximations, not a survey or a photoreal reconstruction. See [map sources and attribution](SourceAssets/AHMEDABAD-CITY-SOURCES.md) and [art sources](SourceAssets/RIVERFRONT-SOURCES.md).

## Start on an Apple Silicon Mac

1. Install **Unreal Engine 5.8** from Epic Games Launcher and a compatible full **Xcode**. Open Xcode once to finish installing its SDK and components. See [Epic’s version-specific macOS and Xcode requirements](https://dev.epicgames.com/documentation/unreal-engine/macos-development-requirements-for-unreal-engine) before installing. Epic lists macOS 14.5 and Xcode 26.0 as minimums for UE 5.8 and recommends Xcode 26.1.1. The M1 Max development machine also builds this project with Xcode 27.
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

Open **Play Sabarmati Drive.command** for the standard window or **Play Sabarmati Drive 4K.command** for a Retina window at 100% render scale, up to 3840×2160 backing pixels. Both shortcuts fit the available desktop and select the packaged game when available. You can also open `~/Applications/Sabarmati Drive.app` directly in Finder. **Finder and both shortcuts use the same offline profile** on your Mac, including your wallet, wardrobe and checkpoint.

On your first visit, enter a local display name, choose **Male** or **Female**, create the profile, and choose **Start**. You wake at 07:15 in Flat 2B, Kesar Apartments, with ₹1,00,000 in cash and ₹5,00,000 in the bank. On later visits, **Continue** restores your last saved checkpoint. If you saved while driving, you resume on foot beside the last safe vehicle position. **Quit Game** is on the title screen and the **Esc** menu. [Play through the first morning](Docs/First-morning.md) · [Riverfront gameplay screenshots](Docs/captures/riverfront-district.jpg)

For a development playtest with an optional local avatar, run `bash Scripts/run-mac.sh --game --no-build`. To open the packaged game from Terminal, run `bash Scripts/run-mac.sh --standalone`.

## Riverfront lighting

The riverfront title overlooks the district at golden hour. A player’s first morning starts at 07:15 in Kesar Apartments. Press **T** during free roam to cycle through night, daylight and golden hour. Streetlights cast pools on the footpaths and carriageways. Selected apartment windows light up, and the occupied car switches its dipped beams on at night.

Run `bash Scripts/benchmark-riverfront.sh all` for the fixed-camera comparison and a 45 m drive under each lighting condition. Each run writes screenshots and raw frame timings under `Saved/RenderingBenchmarks`. [Technical audit and benchmark](Docs/Riverfront-rendering.md)

## Controls

| Action | Control |
|---|---|
| Walk, look, run, jump, vault or climb | WASD, mouse, left Shift, Space |
| Enter the nearest available car or leave a stopped car | E |
| Drive, steer, brake or reverse | W, A/D, S. Space applies the handbrake. |
| Change chase, cockpit and bonnet views | C |
| Open the hatch's front doors, signal, use hazards, sound the horn | O, Z/X, B, H |
| Use the wardrobe, browse Sutra Cloth House, greet a pedestrian or interact at a shop | E when the prompt appears |
| Open your wallet and most recent receipt | I |
| Refuel and repair the stopped vehicle at River Motor Works | F |
| Select a waypoint, toggle the map, start a driving challenge | N, M, G |
| Change time, weather and automatic clock | T, P, Y |
| Show controls, frame statistics or the pause menu | F1, F3, Esc |
| Reset to the start | R |

You can walk from the wardrobe in Flat 2B down the apartment stairs to street level and enter three furnished shops. **Sutra Cloth House** is on the ground floor. Across the road, **Kagaz Print House** offers a delivery to the counter at **Nadi Chai**. A public passage runs behind these two shops. The hatch and crossover are parked by the Atal promenade. Cross Atal on foot or use the road bridges to drive between the banks.

Tap **Space** in the open to jump. From the street, face the low iron fence or the raised stone platform beside Kesar and tap **Space** within a step. The avatar vaults the fence with one hand or climbs onto the platform with both hands.

The circular street map sits at the lower left. Use **N** to cycle destinations and **M** to hide or show navigation. Enter a vehicle to see speed, gear, engine speed, fuel and indicators. Press **F1** for the controls. Pause with **Esc** to return to the title and settings controls over the live scene.

## Optional local avatar import

The offline game includes two fictional adult residents. At the wardrobe in Flat 2B, select **Male** or **Female** to change your avatar on an existing save. Both can wear cotton shirts, crew-neck tees, kurtas, jeans and chinos, change hairstyle and complexion, and wear the watch. At Sutra, walk into the curtained fitting room and press **E**. Remove the worn layer, take a garment from the shelf, then put it on. **Esc** restores your entry outfit. To leave in a new outfit, buy the garments and choose **Wear this outfit out**. **Inspect face** moves the camera close and **View full outfit** returns to the fitting view. Your purchases, wallet and checkpoint carry over. The two presets also appear among walkers, visitors and families. [Male physique and sources](SourceAssets/RESIDENT-PHYSIQUE.md) · [Female physique and sources](SourceAssets/RESIDENT-FEMALE.md)

The public resident walks at 145 cm/s and runs at 335 cm/s with Shift. A native Unreal 1D Blend Space uses the character's speed to blend planted steps and the Third Person template's arm motion. Character Movement sweeps the capsule during traversal, and Unreal's two-bone IK moves the wrists toward the detected edge.

For a local rig comparison, the editor supports a separately supplied and appropriately licensed `SourceAssets/fictional-rigged.glb` with one skin, 15 joints and Idle, Walk, WalkAlt and Jog clips.

1. In the UE editor, create **Content/Sabarmati/Avatar**. In the Content Browser, import the GLB as an asset into that folder (do not use **Import Into Level**). In the [Interchange importer](https://dev.epicgames.com/documentation/unreal-engine/interchange-import-reference-in-unreal-engine), enable **Import Skeletal Meshes** and **Import Animations**. Enable **Combine Skeletal Meshes**, if displayed, so the skinned body/clothes/shoes use one skeletal mesh and one skeleton. Save imported materials and textures.
2. Rename the imported skeletal mesh **SK_Sabarmati**, and its Idle, Walk and Jog animation assets **A_Idle**, **A_Walk** and **A_Jog**, all directly in that folder. The mesh and three clips must reference the same Skeleton asset. The optional `WalkAlt` clip is not loaded by default. If your Unreal glTF importer does not bring the skinned parts or clips together, re-export the fictional source rig from Blender as skeletal FBX with baked animation clips and import against the same Skeleton; do not use the fallback as proof of a successful import.
3. Preview the mesh facing +X and check its standing height, foot contact and deformation. To play with the private rig, run `bash Scripts/run-mac.sh --game -- -SabarmatiPrivateAvatar`. A configured Blueprint avatar keeps its own animation graph. The private skeletal-mesh fallback plays the imported clips.
4. The public cook excludes this local folder automatically. Both clothed public residents ship in the Riverfront content directory.

## Run a native smoke test

For a repeatable player check in a Development build, pass `-SabarmatiGaitReview` to exercise walking and Shift-running on level paving. Add `-SabarmatiJumpReview -SabarmatiTraversalReview` to test the open jump, fence and stone platform. Use `-SabarmatiGaitFrames` to record a side view or `-SabarmatiGaitKurta` to test the fitted kurta. `-SabarmatiMorningReview -SabarmatiBodyReview` photographs both fitted tops using a disposable shop profile. The wardrobe review adds the remove, take, put-back and put-on checks at the apartment mirror. These checks create temporary QA profiles. An explicit `-UserDir=/path/to/qa` keeps captures under that directory’s `Saved` folder.

The default is Metal SM5 and conventional dynamic lighting, with Nanite, Virtual Shadow Maps, hardware ray tracing and Lumen disabled. This configuration aims to run on an M1 Max while leaving room to tune the M2 Air. It is not a measured performance claim.

- Build/launch on the **M1 Max** first. Check the first wake-up or saved checkpoint, then walk to the promenade and inspect lighting, river/banks, street silhouettes and HUD at 1600×900. Walk, orbit without drag, jog, jump, and reset. Cross the bridge on foot; check the sidewalks are continuous. If using the imported avatar, inspect the idle/walk/jog changes, shoe/foot alignment and foot contact from the side, not just standing still.
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

The command cooks and ad-hoc signs a self-contained Apple Silicon Development app. Each successful build replaces the game in your user Applications folder. Double-click `~/Applications/Sabarmati Drive.app` in Finder. It reads its local cooked containers and starts in a window. Distribution signing and notarization are separate.

## Source checks and limits

```bash
bash -n Scripts/run-mac.sh
python3 -m unittest discover -s Tests -p 'check_*.py'
```

These checks validate project metadata, runtime inputs, renderer flags, a world layout sanity check, and the included glTF skin/animation data. They do not compile Unreal headers or prove native rendering, input, cooking, packaging, or frame rates. The Development Editor game and cooked standalone game were both exercised on the M1 Max. [District validation](Docs/Riverfront-district.md)

### Start and pause

The native launcher opens a splash while the engine starts. The subsequent map loader shows six milestones as roads, buildings, lighting, vehicles and traffic become available. Click **Start** for the first morning in Flat 2B or **Continue** to resume from the last saved location. Both use the local offline player profile. [Playing the first morning](Docs/First-morning.md)

The title uses generated environmental concept artwork as a backdrop; it is not a gameplay capture.

The startup artwork is `Content/Splash/Splash.png`. Regenerate it on macOS with `swift Scripts/make-mac-splash.swift Content/Splash/Splash.png`.
During play, **Esc** freezes the simulation and releases the pointer. Click
**Resume** or press **Esc** again to resume and recapture the pointer. **Quit Game** closes the game from either the title or pause menu. For normal
keyboard QA use a standalone game window; Unreal Editor can intercept Esc in PIE.

Across the street from the starting cars, a brick-and-plaster block has a shaded colonnade, a public cross-passage and a paved circuit around the back. In **Explore Ahmedabad**, a longer version occupies the mapped plot beside the starting road. Both routes are accessible on foot. The road surface uses scanned asphalt, and the central streets include worn markings, flush drains and kerbs. See [the material sources](SourceAssets/RIVERFRONT-SOURCES.md) and [city map notes](SourceAssets/AHMEDABAD-CITY-SOURCES.md).

**Explore Ahmedabad** opens a 1 km × 1 km district around the Atal west approach. Press **N** to select a local destination and **V** to travel. The home riverfront also has a 1 km × 1 km play boundary. Look for streetside tea, vegetable and fabric carts, shade trees and parked vehicles. Approach a stopped vehicle and press **E** to drive. The sport coupe and motorcycle have higher speed limits. A direct impact can wreck the car.

Ghat steps lead down to the river. Use the walking controls to swim, hold **Sprint** to dive and release it to surface. Running out of air returns you to Flat 2B with your possessions.
