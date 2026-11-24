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

### Unreal and macOS implementation choices

Use the systems shipped with Unreal Engine 5.8.4 for gameplay and interface work. For Mac-specific integration, use Unreal’s platform layer where it covers the operation, then a documented public Apple API where the launcher or platform needs one. The M1 Max and the packaged arm64 build are the reference for feature support and performance.

| Upcoming work | Engine or platform support to use |
|---|---|
| HUD, phone, wardrobe and shop screens | Build on the current Slate menus. Use UMG/Slate widgets for layout, keyboard focus, input and DPI scaling. |
| Avatar, clothing and apartment | Use Unreal skeletal meshes, Animation Blueprints and Character Movement for dressing and locomotion. Use Sequencer for the first wake-up and hand control back to the player. |
| Solo pedestrians and families | Use the Navigation System, AI path following and StateTree for routes and activities. Author the family’s destination, waiting and regrouping rules as gameplay. |
| Purchases, wardrobe and offline progress | Use Unreal Data Assets for items and prices and the existing `USaveGame` slot for persistence. Game rules own the rupee balances, purchases, and migration of existing player progress. |
| Mac rendering, windows and storage | Use Unreal’s Metal renderer and game window settings. The launcher uses AppKit’s `NSScreen` for the usable desktop and Foundation’s `FileManager` for the user’s Application Support directory. |

Check each newly adopted module or plugin in a packaged offline build on the M1 Max, including keyboard focus, window fitting and existing saves where relevant. [Epic’s Mac feature matrix](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine?application_version=5.8) · [Apple’s Application Support directory](https://developer.apple.com/documentation/foundation/filemanager/searchpathdirectory/applicationsupportdirectory)

### Local riverfront art verification

Open **Play Sabarmati Drive.command** for a standard window or **Play Sabarmati Drive 4K.command** for a Retina window fitted to the desktop. After packaging, you can also double-click `~/Applications/Sabarmati Drive.app` in Finder. All three open the same offline profile on your Mac.

For a fresh profile, enter a display name and choose **Start**. You wake in Flat 2B at Kesar Apartments with ₹1,00,000 cash and ₹5,00,000 in the bank. Walk down the apartment stairs to **Sutra Cloth House**, then cross the street to the furnished interiors of **Kagaz Print House** and **Nadi Chai**. On a later launch, **Continue** restores your last saved checkpoint and keeps your balances and clothing. **Explore Ahmedabad** in the title or pause menu opens the wider city map. **Quit Game** closes the game from either menu. [First-morning play guide](Docs/First-morning.md) · [City map sources](SourceAssets/AHMEDABAD-CITY-SOURCES.md)

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

### Game Animation Sample apartment playtest

Once the licensed sample content and its dependencies are available locally, run the editor in Standalone Play with a separate `-UserDir`. Open `/Engine/Maps/Entry?game=<sample GameMode generated class path>?SabarmatiSampleApartment` in the play-world console using the `open` command. Replace the placeholder with the full class path ending in `_C` from the installed sample. The editor builds the riverfront and places the Player Start in Flat 2B before the sample game mode spawns its character. Verify the possessed pawn, walk down the stairs, and test the low vault and physical recovery in play.

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

The shared profile lives at `~/Library/Application Support/Sabarmati Drive/Saved/SaveGames/SabarmatiPlayerProfileV1.sav`. When that file is absent, a shortcut can seed it from an existing checkout save. A direct Finder launch can migrate a valid older app save. Each keeps the original file. Later launches use the shared copy.

Run `bash Scripts/run-mac.sh --standalone` for the cooked game in a fitted window, or add `--4k` for Retina. The packaged app runs without Unreal Editor. Use `-UserDir=/absolute/path/to/test-folder` to keep a packaged review’s saves separate. Summarize a completed run with `python3 Scripts/report-district-endurance.py /path/to/Saved/SystemsReviews/RUN_ID --minutes 20`. [District and benchmark evidence](Docs/Riverfront-district.md)

For a sustained test through the normal fitted launcher, run `bash Scripts/benchmark-standalone-mac.sh 20`. The script records individual wall times, in-game focus and a separate macOS window-focus sample every 15 seconds. An optional final `--4k` selects the fitted Retina preset. Each run has a separate save and evidence directory under `Saved/StandaloneBenchmarks`.

### Copying the standalone app

After packaging, use `bash Scripts/archive-local-mac.sh` for a local ZIP under `Saved/MacPackage`. The script copies the bundle without Finder attributes, verifies its ad-hoc signature in the clean copy and tests the ZIP. Play the installed app from your user Applications folder. Packaging updates that copy in place. Keep the ZIP compressed when playing on this Mac. This archive is for local Development testing.
