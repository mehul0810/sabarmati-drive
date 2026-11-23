# First Apple Silicon playtest

Start with the existing UE 5.8 project and a Development build. The browser game and
the delivered source archive are separate and unchanged. This setup does not install
anything or switch the system's default developer tools.

1. Run `bash Scripts/check-mac-setup.sh`. It checks disk space, Unreal, full Xcode,
   ARM64 compilation and the Metal shader compiler.
2. With Unreal and Metal available, build and run from the project directory:
   `DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer bash Scripts/run-mac.sh`.
   Set `UE_ROOT` too if Unreal is not in its default location. This use of
   `DEVELOPER_DIR` leaves the Mac's global Xcode selection unchanged.
3. Verify walking and mouse look on one bank, then enter and drive the car across
   the bridge, stop, exit and reset. Next, import your optional local Blender avatar
   using the README and repeat the movement check.
4. After the in-editor playtest, run the launcher with `--package` for the local
   Development app, verify its executable is ARM64, then launch that app independently.
   The local package is ad-hoc signed. It has no Developer ID distribution signature or notarization.

On the M1 Max, Xcode 27 compiled and ran an ARM64 C++ probe, the Metal
Toolchain compiled a shader to AIR, and Unreal 5.8.4 linked the project editor
module as ARM64. The first launch may spend several minutes validating Epic's
libraries and compiling shaders. The same machine also ran the cooked, self-contained Development app. Check free space before cooking.
If the Unreal build fails early, compare with a minimal UE 5.8 C++ project using
the same toolchain before changing gameplay code. Leave global developer settings
and other projects untouched. See [Epic's macOS requirements](https://dev.epicgames.com/documentation/unreal-engine/macos-development-requirements-for-unreal-engine)
and [Apple's developer-directory documentation](https://developer.apple.com/documentation/xcode/configuring-command-line-tools-settings).

### Local riverfront art verification

Use **Play Sabarmati Drive.command** for the riverfront in a resizable window. The **4K** launcher opens the city in a Retina window fitted to the desktop. Use **Quit Game** on the title screen or the **Esc** menu to close the standalone game. On the
first run, choose a local display name to unlock Start. The profile is a
local Unreal save on this installation; no email, password or server is used.
Start opens the current map. Choose **Explore Ahmedabad** from the title or
pause screen to load the offline city map, then press **Start**. The same local
profile works in both maps. The city map's sources and fidelity limits are
documented in [AHMEDABAD-CITY-SOURCES.md](SourceAssets/AHMEDABAD-CITY-SOURCES.md).
Mouse look starts at 30% of the old sensitivity. Change it in **Start** or **Esc**;
the chosen value is saved when you enter or resume the game.

For the wider two-bank route and the named destinations, see
[the corridor guide](SourceAssets/RIVERFRONT-CORRIDOR.md). Atal is for walking.
Sardar and the concrete approaches at Ellis are vehicle crossings. On the lower
promenade, use the stairs near these crossings to continue on the upper walk.
Animated pedestrians are ambient and do not block player movement.

An opt-in Development build run with `-SabarmatiArtReview` captures the title, starting
character view, a view of the street, pause, Atal Bridge, Flower Park, Ellis
Bridge, and an Atal pedestrian in `Saved/ArtReview`. It logs road and foot
surface probes, route-join clearance, crowd movement and sensitivity testing.
Add `-SabarmatiCityReview` to also exercise a profile-gated City map switch
and capture a real road. QA uses separate `SabarmatiQA-*.sav` files, never the
player's `SabarmatiPlayerProfileV1.sav` save. The flags inject test inputs;
they do not replace hardware playtesting.

For the street-level check, launch with `-SabarmatiArtReview -SabarmatiWalkReview`. Add `-SabarmatiCity` for the city plot. The review captures the normal player camera facing the arcade, then walks the character through its passage and around the entire perimeter using collision and movement physics. The log reports completed legs, distance and ground contact. The captures are in `Saved/ArtReview`.

### Fixed-camera rendering benchmark

Run `bash Scripts/benchmark-riverfront.sh all` in Terminal. Use `night`, `golden`, `day` or `reference` to measure one condition. Add `--no-build` after the condition to reuse an up-to-date editor module. Each native game window closes after its checks. The shell reports the run directory, verifies the captured dimensions and prints frame timing percentiles. The regular launcher opens a separate playable session. [Protocol and acceptance status](Docs/Riverfront-rendering.md)

### District, driving and offline city checks

Run the following from the checkout after the native build. The review flags create isolated QA saves. The normal launcher continues to use the player's offline profile.

```bash
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiSystemsReview
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiSystemsReview -SabarmatiEndurance=20
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiCity -SabarmatiNeighbourhoodReview
bash Scripts/run-mac.sh --package
```

For the interactive preview of Ashram Road, CG Road and the SG Highway service road, launch `bash Scripts/run-mac.sh --game --no-build -- -SabarmatiCity -SabarmatiStreetPreview`. Select a street with **N** and travel with **V** on foot or in a stopped car. The Atal west approach is a return stop. The ordinary city starts in the original central area. [Mapped areas and data sources](SourceAssets/AHMEDABAD-CITY-SOURCES.md)

The standalone executable accepts the same review flags. Use `-UserDir=/absolute/path/to/test-folder` to put a packaged review's Saved directory in a separate location. Summarize a completed run with `python3 Scripts/report-district-endurance.py /path/to/Saved/SystemsReviews/RUN_ID`. [District and benchmark evidence](Docs/Riverfront-district.md)
