# Playing the riverfront district

This report records the district milestone at game revision `7a55918`. [The first-morning release](First-morning.md) adds the apartment, clothing shop, wardrobe, visitors and paid work.

The regular launcher opens a resizable window up to 1600×900. It measures the available desktop on each launch. Both **Play Sabarmati Drive.command** and the fitted Retina option leave room for the title bar and window controls. Click **Start**, walk inland to the parked hatch and press **E** beside a door. Use **Esc → Quit Game** or **Quit Game** on the title to close it. Use `bash Scripts/run-mac.sh --standalone` to open the cooked app with the same desktop fit and existing local player profile.

| Milestone | Playable result |
|---|---|
| 1. Audit and benchmark | The original system inventory, priorities and fixed-camera Metal measurements are in the [rendering audit](Riverfront-rendering.md). The current harness captures seven fixed views and one normal-camera drive, checks every chase frame for ground contact and validates actual PNG dimensions. |
| 2. Riverfront district | A 1.66 km two-bank corridor connects the Atal pedestrian crossing, road bridges at Sardar and Ellis, the market, Flower Park and the event grounds. Enter Nadi Chai and Kagaz Print House, follow the cross-passage or walk around the arcade. |
| 3. Player vehicle | The unbadged right-hand-drive hatch has two operable front doors, four wheel probes, gears, wet and dry grip, serviceable damage, signals, horn and a dashboard driven by actual speed and RPM. Chase, bonnet and cockpit cameras blend during play. A crossover is parked nearby. |
| 4. Roads, buildings and vegetation | Scanned asphalt, tar repairs, puddles, drains, kerbs and worn markings share the driving surface. Terracotta screens, pipes, cables, storefronts, balconies and projected upper-floor rooms dress the frontage. Side and rear elevations carry shaded stair windows, stone sills, rainwater pipes and service doors. Scanned masonry darkens near damp ground. Two credited tree models supply the close canopy. Another 298 scanned trees share a spatially culled instance batch along the outer banks and parks. |
| 5. Traffic and people | Twelve types of unbadged traffic model run on connected, left-hand lanes. Vehicles follow, brake, yield at crossings and react to emergency vehicles. The riverfront has pedestrian circuits, signal-controlled crossings, patrons visiting the shop counters, and seated and phone-holding residents beside promenade benches. |
| 6. Weather, lighting and sound | The district supports day, golden hour, night, a moving sun, haze, monsoon, wetness and wind. Local lighting includes lamps, windows, signals and vehicle beams. Thirteen original local sound assets cover the river, street, shops, rain and vehicles. |
| 7. Interaction and exploration | Waypoints, a minimap, discoveries, shop counters, a refuel/repair stop and a four-gate road challenge use the offline player profile. The title and pause menus expose graphics, sound and control settings. |
| 8. City pipeline | An opt-in Development preview populates existing mapped streets on Ashram Road, CG Road and the SG Highway service road. It uses 1,164 additional mapped exterior plots, six new traffic circuits and the reusable materials. The three preview streets are available with `-SabarmatiCity -SabarmatiStreetPreview`. |

These images come from the self-contained Mac build with the public character and vehicle assets. [Riverfront gameplay](captures/riverfront-district.jpg) · [Mapped street visits](captures/ahmedabad-streets.jpg)

## Playtest evidence on the Apple M1 Max

The actual character walked 9.49 m through the tea-house door to the counter and 57.84 m around all four sides of a second riverfront building. The character stayed grounded through all four Sardar and Ellis underpasses. Separate perimeter walks covered 101.9 m around the main riverfront arcade and 289.9 m in the city. Both front doors, the three driving cameras, four wheel contacts, braking, reverse and monsoon weather were exercised in the standalone app. The live run observed 23 traffic vehicles, 7 stops, a pedestrian crossing and 2 visits to the shop counters.

The four-gate River Run finished in 15.82 s. The driver refuelled and repaired the hatch. Two shop visits and the best time survived reloading the isolated profile. The visible Quit Game buttons were verified through keyboard navigation on the title and after entering play and pressing Esc. Each activation closed its game process. Normal source-game and packaged launches share the Application Support profile seeded from the existing checkout save. The checkout copy is preserved. The ordinary standalone launch reached its title in a 1600×900 window, and the original and active saves matched byte for byte.

The following captures and timings use game revision `7a55918`, UE 5.8.4, Metal SM5 on the Apple M1 Max. The short drives used the same lighting, TAA, 100% primary and secondary render scale, dynamic resolution off, VSync off, `t.MaxFPS=0` and `t.IdleWhenNotForeground=0`. Both programs recorded Unreal frame smoothing enabled. The standalone run measures the complete walking and driving loop. The 4K launcher fits the available desktop within a 3840×2160 ceiling. Only the M1 Max was available.

| Run | Actual drawable | Measured traversal | Wall-frame results |
|---|---|---|---|
| Standard night drive | 1568×882 | 327 chase frames in 5.398 s, 45.12 m | Median 16.63 ms, p95 16.89 ms, p99 17.05 ms. |
| Fitted Retina night drive | 3152×1773 | 168 chase frames in 5.413 s, 45.29 m | Median 31.32 ms, p95 45.97 ms, p99 48.76 ms. |
| Fitted standalone app, 20 minutes on AC | 1568×882 | 73,404 frames in 1,200.013 s and 28 loops. Walked 1,761.59 m and drove 5,356.62 m. | Median 16.25 ms, p95 16.84 ms, p99 17.72 ms, worst 397.78 ms. |

The first and last full-minute p95 were 17.47 and 16.43 ms. 59 frames exceeded 33.33 ms and 3 exceeded 100 ms. Unreal reported 71,585 foreground frames at p95 16.82 ms and 1,819 background frames at p95 20.05 ms. macOS independently reported the game in front on 81 of 84 observations with one running instance. The AC supply was checked before and after the run.

Earlier standalone measurements show the variability on this shared Mac. Revision `3a8da6a` completed 28 loops at 1600×900 with 32,650 frames, p95 67.31 ms and 397 frames above 100 ms. Its fitted 1568×882 repeat recorded 71,922 frames, p95 19.59 ms and seven frames over 100 ms. The worst render-thread stall was 1.07 s in that repeat. A separate Development Editor run at 1600×900 recorded 74,328 frames and p95 16.55 ms over 20 minutes.

The two latest short raw recordings are under `Saved/RenderingBenchmarks/20261123-150746-F9D98C15` and `Saved/RenderingBenchmarks/20261123-150852-F7080E53`. The standalone record, focus samples and power readings are under `Saved/StandaloneBenchmarks/579F1A9E-9134-460C-8B79-3DBF378F7879` on the test Mac.

## Mapped street traversal

The final standalone review arrived on a tagged sidewalk at each street, walked its free length using the normal character and kept its visible face aligned with movement. The regular camera and public character were used. Tree trunks respond to the orbit-camera probe. On returning to Atal, the same run entered the parked hatch, travelled in the stopped car, drove 22.08 m on Ashram Road with all four contacts and exited.

| Street | Mapped plots within 150 m | Grounded sidewalk walk | Moving frames and p95 | Synchronous street transfer |
|---|---:|---:|---:|---:|
| Ashram Road | 10 | 8.01 m | 216, 21.36 ms | 36.48 ms |
| CG Road | 48 | 8.02 m | 216, 21.38 ms | 49.57 ms |
| SG Highway service road | 26 | 8.02 m | 216, 21.53 ms | 77.32 ms |

Each transfer time covers the actual streaming preparation and actor placement. The moving frame sample begins after three seconds to allow adjacent tiles to settle. It measures a short stretch at the arrival site.

## Reproduce

```bash
bash Scripts/benchmark-riverfront.sh night --no-build
bash Scripts/run-mac.sh --game --no-build --4k -- -SabarmatiBenchmark -SabarmatiTime=night
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiSystemsReview
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiSystemsReview -SabarmatiEndurance=20
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiCity -SabarmatiNeighbourhoodReview
bash Scripts/benchmark-standalone-mac.sh 20
```

The windowed benchmark writes screenshots, raw frames, settings and the drive result under `Saved/RenderingBenchmarks`. A systems run writes its evidence under `Saved/SystemsReviews`. The city review writes per-street photographs, nearby plot counts and physical traversal results under `Saved/StreetReviews`. [Mac setup](../MAC-SETUP.md) · [Map provenance](../SourceAssets/AHMEDABAD-CITY-SOURCES.md) · [Asset registry](../SourceAssets/asset-registry.json)

## Visual scope

Gameplay captures show the warm riverfront, readable shop signs, furnished ground floors, differentiated vegetation and a recognizable bridge from street level. Distant skylines, many vehicle bodies and the exterior masses in the city preview are visibly simplified. Only the two furnished shops have public interiors. District-wide production-quality art, full-city individual architecture, independent visual approval against the concept and sustained full-resolution 4K performance remain open. The three named streets serve as a data and traversal preview within the existing city footprint.
