# Playing the riverfront district

The default start opens a resizable 1600×900 window on the M1 Max. Both **Play Sabarmati Drive.command** and the fitted Retina option leave room for the title bar and window controls. Click **Start**, walk inland to the parked hatch and press **E** beside a door. Use **Esc → Quit Game** or **Quit Game** on the title to close it. The local Development app can also run directly from Finder.

| Milestone | Playable result |
|---|---|
| 1. Audit and benchmark | The original system inventory, priorities and fixed-camera Metal measurements are in the [rendering audit](Riverfront-rendering.md). The current harness captures five fixed views and one normal-camera drive, checks every chase frame for ground contact and validates actual PNG dimensions. |
| 2. Riverfront district | A 1.66 km two-bank corridor connects the Atal pedestrian crossing, road bridges at Sardar and Ellis, the market, Flower Park and the event grounds. Enter Nadi Chai and Kagaz Print House, follow the cross-passage or walk around the arcade. |
| 3. Player vehicle | The unbadged right-hand-drive hatch has two operable front doors, four wheel probes, gears, wet and dry grip, serviceable damage, signals, horn and a dashboard driven by actual speed and RPM. Chase, bonnet and cockpit cameras blend during play. A crossover is parked nearby. |
| 4. Roads, buildings and vegetation | Scanned asphalt, tar repairs, puddles, drains, kerbs and worn markings share the driving surface. Terracotta screens, pipes, cables, storefronts, balconies and projected upper-floor rooms dress the frontage. Two credited tree species supply the close canopy. |
| 5. Traffic and people | Twelve types of unbadged traffic model run on connected, left-hand lanes. Vehicles follow, brake, yield at crossings and react to emergency vehicles. The riverfront has pedestrian circuits, signal-controlled crossings patrons visiting the shop counters, and seated and phone-holding residents beside promenade benches. |
| 6. Weather, lighting and sound | The district supports day, golden hour, night, a moving sun, haze, monsoon, wetness and wind. Local lighting includes lamps, windows, signals and vehicle beams. Thirteen original local sound assets cover the river, street, shops, rain and vehicles. |
| 7. Interaction and exploration | Waypoints, a minimap, discoveries, shop counters, a refuel/repair stop and a four-gate road challenge use the offline player profile. The title and pause menus expose graphics, sound and control settings. |
| 8. City pipeline | An opt-in Development preview populates existing mapped streets on Ashram Road, CG Road and the SG Highway service road. It uses 1,164 additional mapped exterior plots, six new traffic circuits and the reusable materials. The three preview streets are available with `-SabarmatiCity -SabarmatiStreetPreview`. |

## Playtest evidence on the Apple M1 Max

The source game and local standalone app run on Metal SM5. Tests make isolated local profiles. The original player save is shared between ordinary development sessions. The district review exercised both front doors, all three cameras, four contact probes, acceleration, gear change, braking, reverse, a signal crossing, a shop visit and monsoon conditions. Independent lower-promenade walks cleared all four portals at Sardar and Ellis. The actual on-foot path from the door to the tea counter covered 9.53 m. An actual drive through all four gates finished in 15.85 s. Refueling and repair succeeded, and two visits plus the best time survived a profile reload.

| Run | Drawable | Measured traversal | Wall-frame results |
|---|---|---|---|
| Development Editor game, 20 minutes on AC | 1600×900 | 74,328 frames in 1,200.005 s, 28 loops. Walked 1,761.56 m and drove 1,776.79 m. | Median 16.12 ms, p95 16.55 ms, p99 16.68 ms, worst 68.05 ms. First and last full-minute p95 were 16.59 and 16.54 ms. |
| Previous standalone app, 20 minutes on AC during development work | 1600×900 | 68,655 frames in 1,200.01 s and 28 loops. Walked 1,761.00 m and drove 5,392.88 m. | Median 16.23 ms, p95 19.02 ms, p99 57.15 ms and worst 536.61 ms. The Mac also ran native compilations during this pass. |
| Earlier short standard night capture | 1600×900 | 334 chase frames in 5.376 s | p95 16.58 ms |
| Earlier short fitted Retina night capture | 3280×1845 | 292 chase frames in 5.399 s | p95 20.45 ms |

The short pair used one built revision, TAA, 100% primary and secondary render scale, dynamic resolution off, VSync off and `t.MaxFPS=0`. The project has Unreal frame-rate smoothing enabled. The 4K option requested a ceiling of 3840×2160 and fitted the actual screen. These samples describe the short drive at the two observed sizes. An M2 Air was unavailable. The editable checkout and cooked app use distinct Unreal run modes.

## Reproduce

```bash
bash Scripts/benchmark-riverfront.sh night --no-build
bash Scripts/run-mac.sh --game --no-build --4k -- -SabarmatiBenchmark -SabarmatiTime=night
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiSystemsReview
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiSystemsReview -SabarmatiEndurance=20
bash Scripts/run-mac.sh --game --no-build -- -SabarmatiCity -SabarmatiNeighbourhoodReview
```

The windowed benchmark writes screenshots, raw frames, settings and the drive result under `Saved/RenderingBenchmarks`. A systems run writes its evidence under `Saved/SystemsReviews`. The city review writes per-street photographs, nearby plot counts and physical traversal results under `Saved/StreetReviews`. [Mac setup](../MAC-SETUP.md) · [Map provenance](../SourceAssets/AHMEDABAD-CITY-SOURCES.md) · [Asset registry](../SourceAssets/asset-registry.json)

## Visual scope

Gameplay captures show the warm riverfront, readable shop signs, furnished ground floors, differentiated vegetation and a recognizable bridge from street level. Distant skylines, many vehicle bodies and the exterior masses in the city preview are visibly simplified. Only the two furnished shops have public interiors. District-wide production-quality art, full-city individual architecture, independent visual approval against the concept and sustained full-resolution 4K performance remain open. The three named streets serve as a data and traversal preview within the existing city footprint.
