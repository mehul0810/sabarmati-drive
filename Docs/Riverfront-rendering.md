# Riverfront rendering milestone

## Technical audit

The local target is the existing UE 5.8.4 C++ Development game on Apple Silicon. Its native Metal SM5 renderer, two offline maps and offline player profile are the baseline. The installed arm64 and Metal compilers passed the project setup probe. The source, configs, content inventory, data conversion and import scripts, tests, packaging rules and both Mac launchers were reviewed before changing the renderer. The following describes the starting checkout, including the local arcade and road pass.

| System | Starting status | Evidence and priority |
|---|---|---|
| Launch, profile and menus | Implemented and working | The windowed Mac launchers and Slate title/pause/quit controls work. The local save gates play and survives map travel. Splash activity is real, but the loader exposes no quantified progress. |
| Game loop and controls | Implemented and working | `RiverfrontGameMode` builds the district, creates the player and spawns parked vehicles. The on-foot character uses movement physics and a collision-tested spring arm. The car uses possession and checks space to exit. |
| Geography and world streaming | Partially implemented | OSM-derived offline centre lines, water and building masses cover the city. The binary contains 1,892 tiles of 600 m, with at most 25 active. Tile geometry and collision are built on the game thread. Elevations and building heights are approximations. |
| Riverfront architecture and roads | Implemented but visually inadequate | Two banks, Atal, road bridges, stepped promenades, a furnished arcade, street markings and photo-scanned surfaces exist. Repeated bays, broad empty spaces, and missing shop interiors are visible from normal play height. First priority is the route at the west landing. |
| Vehicle models and handling | Partially implemented | The original hatch and crossover have separate wheels, a chase camera, entry/exit, steering, braking and collision. The chassis follows a ground trace and swept box. Suspension, tire slip, cockpit camera, functional doors and independent traffic are later vehicle work. |
| Vegetation and pedestrians | Partially implemented | A rights-cleared scanned tree uses a reduced mesh and masked leaf cards. Riverfront pedestrians follow validated circuits with different starts and speeds. Distance culling exists. Closely repeated tree crowns and local-only skeletal assets limit variety. There is no pedestrian intersection decision system. |
| Traffic, interactions and audio | Missing for the wider simulation | The player can enter either parked car. Inspected runtime code has no network-following traffic, signal phases, routed navigation, spatial engine/tire/footstep assets, or shop transactions. |
| Lighting, reflections and post-processing | Partially implemented | The starting setup has one fixed directional light, a captured sky, height fog, TAA and screen-space reflections. Fixtures are geometry alone. There is no reproducible night view. Opaque glass and water shaders approximate reflections. This is the immediate visual priority. |
| Art creation, caching and rights | Partially implemented | Blender/glTF preparation and Unreal imports, source credits, instanced surface batches and engine texture streaming exist. Some meshes load synchronously by path. Local playtest avatar assets are excluded from public Git. Additional categories still require cleared meshes, collision and LOD validation. |
| Validation and distribution | Partially implemented | Python checks cover topology, bindings, map data and the avatar format. Existing native review modes test movement, map travel, collisions and selected views. Two of sixteen lower-promenade probes missed in the prior native route check and need a location-level investigation. A repeatable frame-time comparison is the immediate engineering priority. Development packaging is scripted. Shipping, signing and a sustained fanless-device test remain open. |

## Rendering choices and expansion gate

Start with the established SM5 renderer, conventional directional shadows, PBR, TAA and screen-space reflections. Epic lists TSR and software Lumen on M1-class hardware. Software Lumen requires a separate distance-field, shader and performance evaluation for this world. Nanite and Virtual Shadow Maps start at M2-class hardware on Mac. SSR can only reflect visible screen content. Preserve the desktop-fit window and compare alternatives at the same drawable size before changing the default. [Epic’s Mac rendering requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine?application_version=5.8)

Priorities in order are repeatable lighting and frame timings (high impact, modest implementation cost), a richer walkable frontage and vehicle close-up (high art cost), then tested asset LODs and traffic (simulation and frame-time cost). Keep the current city footprint during this milestone. Review the normal player camera, a close vehicle, a traversable street and a fixed bridge view. Keep the first district open for review until its visual targets, collision gaps and sustained performance targets are met.

## Reproducing the benchmark

Run `bash Scripts/benchmark-riverfront.sh all` from the checkout. The runner builds the native Development module once, then opens four standalone windows in sequence. Select `reference`, `day`, `golden` or `night` to run one condition. Each session uses an isolated QA profile. Results live under `Saved/RenderingBenchmarks/<run>/`. The runner checks each PNG’s dimensions, the input and road-contact records, and the frame samples before printing a summary. Use `python3 Scripts/report-rendering-benchmark.py <run-directory>` to read a past run.

Each condition captures the west Atal landing, shopfront and parked hatch from identical positions and fields of view. After three seconds of warm-up, each camera supplies 180 frames. The fourth capture comes from the normal chase camera. The test takes possession of the parked hatch, accelerates using the mapped W input, then checks speed, distance and road contact at 45 m. It also cycles all three lighting states using the mapped T input and verifies the number of active headlight beams. PNG capture time is excluded from the samples. Raw wall, game-thread, render-thread and GPU times are recorded separately.

The first recorded baseline used the checkout’s original scene and lighting. The repeatable reference run in the table below keeps the final geometry, materials and render resolution and restores the earlier sun, sky and fog. This isolates the lighting comparison. The full run keeps the display windowed, both resolution scales at 100%, dynamic resolution and VSync disabled, and the project’s usual frame smoothing enabled.

## Measured on Apple M1 Max

UE 5.8.4, Metal SM5, 32-core GPU, 32 GB unified memory, 1568 × 882. These are short, warm Development runs. The fixed-view figure is the highest 95th-percentile wall time among the three fixed cameras. GPU frame timing is the engine’s Metal counter and can include waiting.

| Condition | Distance driven | Chase median | Chase P95 | Chase GPU median | Highest fixed-view P95 |
|---|---:|---:|---:|---:|---:|
| Previous lighting, TAA | 45.26 m | 16.61 ms | 17.44 ms | 15.94 ms | 17.37 ms |
| Day, TAA | 45.23 m | 16.54 ms | 17.60 ms | 15.82 ms | 17.54 ms |
| Golden hour, TAA | 45.11 m | 16.62 ms | 17.45 ms | 15.68 ms | 17.47 ms |
| Night, TAA | 45.18 m | 16.61 ms | 17.44 ms | 15.86 ms | 17.57 ms |
| Golden hour, TSR at native scale | 45.14 m | 16.69 ms | 17.39 ms | 16.03 ms | 17.70 ms |

The golden-hour drive measured similarly with both antialiasing methods. Keep TAA as the default pending a lower-resolution and sustained-load comparison. To repeat the TSR trial after building, run `bash Scripts/run-mac.sh --no-build -- -SabarmatiBenchmark -SabarmatiTime=golden '-ini:Engine:[/Script/Engine.RendererSettings]:r.AntiAliasingMethod=4'`. The results record method 4 for TSR and method 2 for TAA. These measurements cover this window size and device only. Sustained M2 Air performance, Retina/4K frame time and packaged performance still need measurements.

## What the captures show

- **Street and vehicles.** Low sun gives the arcade columns and kerbs readable shadows. Daylight reveals brick and plaster, and the chase camera shows worn markings and patched asphalt. At night, individual rooms glow, the footpaths have local pools of light and two dipped beams illuminate the road ahead of the occupied hatch. The game fits 197 fixtures across the district and reuses up to 14 nearby lights. Headlights switched off in the daylight states in every playable-mode run.
- **Compared with the approved art.** The gameplay shares the warm light and a recognizable bridge silhouette. The concept has layers of riverside vegetation and buildings, more detailed car paint and glazing, and richer reflections. The gameplay still shows a shallow skyline, repeated bays, opaque vehicle glass and sparse night streets. The next art pass should concentrate on this frontage, its planting, shop interiors and the close vehicle model.
- **On-foot verification.** Fresh sessions completed all six legs through the passage and around the building in both maps. The riverfront route measured 101.9 m and the larger city route measured 289.9 m. The character stayed grounded, the open passage cleared the movement capsule, and a separate wall trace hit the solid facade.
- **Acceptance.** The audit, reproducible timing baseline, native screenshots and input checks complete the first milestone. The photorealistic district remains in progress. Outside the arcade, two lower-promenade floor probes still need a clearance check near the stacked bridge crossing. The broader city, traffic simulation, wet weather and audio remain later milestones.
