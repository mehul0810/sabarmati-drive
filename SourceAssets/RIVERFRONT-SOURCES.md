# Riverfront material sources

The project uses the 1K JPEG color, DirectX tangent normal and roughness maps from:
- AmbientCG [Asphalt 019](https://ambientcg.com/view?id=Asphalt019)
- AmbientCG [Paving Stones 034](https://ambientcg.com/view?id=PavingStones034)
- AmbientCG [Concrete 034](https://ambientcg.com/view?id=Concrete034)

AmbientCG distributes these materials under CC0 1.0. Runtime assets are under
`Content/Sabarmati/Riverfront`. The editor-only `Scripts/build-riverfront-look.py`
rebuilds them with those named ZIP files under `Saved/LookSources`. The procedural
river shader is project-authored. These materials do not depict or reproduce
map-service imagery or verify the geometry against street surveys.

Close-range trees use Poly Haven's [Tree Small 02](https://polyhaven.com/a/tree_small_02)
by Rico Cilliers, under [CC0](https://polyhaven.com/license). Kept complete leaf islands
but thinned their count before importing, preserving the original bark and foliage
maps. The reduced source is prepared by `Scripts/prepare-riverfront-tree.py` and
imported by `Scripts/import-riverfront-tree.py`. Distant trees still use placeholders;
the high-detail mesh is not yet used as an all-city tree template.

The drivable hatchback uses the project's existing right-hand-drive adaptation of
[tevy's Five-Door Hatchback](https://tevy.dev/models/hatchback) (CC0 1.0). The car's
gameplay collision remains the original pawn's box, independent of the imported
parts. The 715,744-byte source is `Sabarmati-Right-Hand-Drive-Hatchback.glb`;
`Scripts/prepare-riverfront-hatch.py` preserves the adapted steering position,
bakes the non-rotating parts, and isolates the rolling wheel meshes. Run from root:
`python3 Scripts/prepare-riverfront-hatch.py SourceAssets/Sabarmati-Right-Hand-Drive-Hatchback.glb Saved/LookSources/hatch`.
Then run `Scripts/import-riverfront-hatch.py` as an Unreal editor Python commandlet.

The hinged cockpit separates the steering wheel at its original column pivot.
Run `Scripts/prepare-riverfront-doors.py` with the right-hand-drive source GLB,
an output directory and `--split-steering`. Use that directory for
`SABARMATI_DOOR_SOURCE` when running `Scripts/import-riverfront-doors.py` in
Unreal Editor. The wheel uses the original CC0 surfaces and turns with the
front axle.

## Realism pass: two-bank street and crossing

Public dimensions and form were referenced from the SRFDCL [master plan](https://www.sabarmatiriverfront.com/srfdcl-master-plan.html) (263 m channel), [promenade](https://www.sabarmatiriverfront.com/river-promenade.html) (two levels), and [street overview](https://sabarmatiriverfront.com/streets.html). The deck, private streetscape, facades, and scenery are original, simplified geometry, not surveys or literal recreations of named buildings or bridges. No photographs or map-service imagery were included.

The surface variants use the previously credited AmbientCG Concrete 034 maps, orientation-aware world-space projection, and color/roughness treatments. Glazing and painted rail surfaces are project-authored. To update only this subset, run the original look commandlet with `SABARMATI_LOOK_SCOPE=facades`.

The runtime broadleaf canopy samples the existing Poly Haven tree's leaf distribution
and replaces the sparse imported leaf clusters with 7,708 folded leaf meshes.
The bank strip uses this foliage at a bounded number of positions; separate
editor-generated LODs are optional and are not assumed in the preview.
Reproduce with `python3 Scripts/prepare-riverfront-broadleaf.py` and run
`Scripts/import-riverfront-tree.py` with `SABARMATI_TREE_NAME=SM_RiverfrontBroadleaf`.

The latest playable avatar uses the fictional, already-local `fictional-rigged.glb`.
`Scripts/prepare-riverfront-avatar.py` combines its same-skeleton meshes and
`Scripts/import-riverfront-avatar.py` imports the skeleton, four sequences, and
materials. There is no photographed face or real-person identity. The local
placeholder asset has not been independently cleared for public distribution.

The foliage cards use the matching Poly Haven diffuse and 1K PNG alpha map on
masked instanced planes. Alpha transfer verified against the provider's MD5
`63199ba87a9e8928424bd8c3b8017bd0`. The original candidate that replaced
leaf geometry but had no visible canopy was not counted as a foliage fix.

## Atal Bridge west landing and pedestrian crossing

Reference: [SRFDCL Atal Bridge and photos](https://www.sabarmatiriverfront.com/atal-bridge.html)
and [Gujarat Tourism dimensions](https://gujarattourism.com/central-zone/ahmedabad/atal-bridge.html).
The published bridge is 300 m long and 10–14 m wide. The game fits its deck, trusses and canopy to the 285 m clear span between the modeled promenade abutments. The west landing references the Flower Park side. All trusses,
sails, bollards, planters, and deck features were modeled for the project.
The photos were viewed as references only, not imported. Neither bank is a
survey, and the separate crossing for cars is still a generic prototype.

## Unbadged 4.35 m crossover

This original project variation uses the same CC0 hatch source, right-hand-drive
cabin and independently rotating wheels. The taller roof and hood, wider
greenhouse, longer bumper profiles and warm metallic finish come from a
project-authored continuous deformation of the source body and all attached
panels; no real manufacturer geometry, badges, textures or product identity
were imported. The 2.60 m wheelbase and 1.645 m overall roof height are in the
range of compact Indian crossovers (see the dimensions section of
[Hyundai's Creta owner's manual](https://org1.hyundai.com/content/dam/hyundai/in/en/data/connect-to-service/owners-manual/new/Cretasep2020-Jan2024.pdf)
for class-level reference only). Mirrors are included in the 2.096 m bounds.
The second vehicle spawns only on checked flat pavement and uses its own
collision bounds. Both cars use four wheel contact probes, surface-dependent grip, body inertia, swept chassis collision and automatic gears. The hatch also has independently hinged front doors.

Rebuild the GLB with `python3 Scripts/prepare-riverfront-crossover.py`. Import
the asset with `SABARMATI_IMPORT_CROSSOVER=1` set when running the existing
`Scripts/import-riverfront-hatch.py` editor commandlet.


## Walkable arcade and street materials

The arcade is an original brick-and-plaster design with separate structural wings, galleries, terracotta sun-breakers, rooftop stair rooms and a public cross-passage. The flat roofs, differences in building length and space between buildings were studied in the [Atal west-bank satellite view](https://www.arcgis.com/apps/mapviewer/index.html?basemap=satellite&center=72.572,23.017&level=18). The [SRFDCL street description](https://sabarmatiriverfront.com/streets.html) informed the footpath and roadside drainage treatment. Satellite imagery is reference-only.

The road uses ambientCG’s [Asphalt 033](https://ambientcg.com/view?id=Asphalt033) photogrammetry at its 2.5 m capture scale. [Bricks 051](https://ambientcg.com/view?id=Bricks051) is a 1.25 m brick scan. [Plaster 001](https://ambientcg.com/view?id=Plaster001) is a procedural stucco surface. All three are [CC0](https://docs.ambientcg.com/license/). The weathered stripe and glazing shaders are project-authored. To rebuild this set, place the 1K-JPG archives in `Saved/LookSources` as `Asphalt033.zip`, `Bricks051.zip` and `Plaster001.zip`, and run the existing look commandlet with `SABARMATI_LOOK_SCOPE=street`. `SABARMATI_LOOK_SOURCES` can point to a separate archive directory.

## District skies and residents

The offline sky textures use the two empty sky panoramas. The preparation script checks the original HDR digests, places their sun directions on the scene azimuth, and produces mipmapped textures. [Kloofendal 48d Partly Cloudy Pure Sky](https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky) · [Industrial Sunset 02 Pure Sky](https://polyhaven.com/a/industrial_sunset_02_puresky) · [Poly Haven CC0](https://polyhaven.com/license)

The public pedestrians use the generic MakeHuman hm08 mesh. The preparation script selects the visible skin and garments, builds two clothed fictional characters and supplies original idle, walk, alternate walk and jog clips. The regular player can use the optional local playtest avatar. A public build uses the dressed resident automatically. [MakeHuman core asset license](https://www.makehumancommunity.org/about/license.html) · [CC0 notice in the source OBJ](https://github.com/makehumancommunity/makehuman/blob/master/makehuman/data/3dobjs/base.obj)

The shop signs, furnishings, service station, small-vehicle fleet and synthesized sounds are project-authored. The two opening front doors use the credited hatchback geometry. Road routing uses the existing offline ODbL street network. [SRFDCL promenade](https://www.sabarmatiriverfront.com/river-promenade.html) · [HCP Ravivari Market](https://hcp.co.in/urbanism/71) · [OSM copyright and attribution](https://www.openstreetmap.org/copyright)

## Promenade pigeons

The animated **Pigeon** model and textures are by [vinish.dev](https://vinish.dev/free-3d-models/birds),
under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The project
imports the original GLB, keeps four clips, removes the unused physics asset,
and generates three skeletal mesh detail levels. The promenade placement and
flight paths are original project work.

To rebuild, point `SABARMATI_PIGEON_SOURCE` at the creator's Pigeon GLB and run
`Scripts/import-riverfront-pigeons.py` with Unreal Editor's
`-ExecutePythonScript` option in a project without the imported pigeon assets.
The reviewed GLB has SHA-256
`872acf11c18735c5841f752db0ff09e237e5b227afd84d14f9b86805dd9889d9`.

The playable resident's everyday button-front shirt and jeans use MakeHuman's `male_casualsuit03` geometry, and the slip-ons use `shoes01`. The asset pack provides the geometry, fitting information and surface maps under CC0. The project fit them to the resident skeleton and prepared the fabric for the saved sand and indigo dyes. [MakeHuman system assets and license](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html)

## Canopy, glazing, instruments and street sound

The park's larger trees use the CC0 Jacaranda by Rico Cilliers and Rob Tuytel. The project converted its trunk, branch cards and masked leaf atlas for the Metal renderer. The mesh has 78,352 triangles at LOD 0 and three imported or reduced LODs. Texture archives and the source GLTF are development inputs. [Jacaranda Tree](https://polyhaven.com/a/jacaranda_tree) · [Poly Haven license](https://polyhaven.com/license)

Upper-floor windows use a cropped, tone-mapped interior panorama in a view-dependent room shader. The photo was licensed separately from the locally modeled, enterable rooms on the ground floor. [Small Empty Room 4 by Sergej Majboroda](https://polyhaven.com/a/small_empty_room_4) · [Poly Haven license](https://polyhaven.com/license)

The center-mounted instrument panel, headlamp internals, grille and street utilities are project-authored. The dashboard receives actual road speed, engine RPM and gear. A seated version of the public clothed resident rides the two- and three-wheelers. Thirteen original synthesized clips cover vehicles, footfalls, shop bell, weather, river, distant traffic and market voices. The market voices contain no identifiable speech or recordings.

The phone users, bench occupants and traffic passengers use seated and standing poses baked from both fictional residents. For the female variant, run `Scripts/prepare-seated-resident.py` on the dressed female everyday GLB with `--female` for the seated pose and `--female --phone` for the standing pose. Set `SABARMATI_POSE_SOURCE` to that output directory when running `Scripts/import-posed-female.py` in the Unreal Editor. The static meshes reuse the female skin, eyes, fabric, leather and handset materials through native material instances. The 70 × 143 mm phone is original geometry. The mapped streets share the Jacaranda canopy and a new dry ground shader derived from the credited Concrete 034 aggregate. These props and materials follow local art direction.

## Riverfront service elevations and asphalt repairs

The original service elevations add shaded stair lights, stone sills, rear service doors and rainwater pipes to the existing building footprints. The three masonry materials keep their credited scan maps. A world-space moisture mask changes colour and roughness close to ground level. The road blends the credited Asphalt 033 and 019 materials, with worn wheel tracks, dusty shoulders and irregular resurfacing cuts at metre scale. The two new fixed camera views capture a public side lane and a low view over a cast access cover. The editor can rebuild these adjustments with `Scripts/build-wet-road.py` and `Scripts/build-facade-weathering.py`.

The wet-road mask hashes shared grid corners consistently to avoid square seams at distant world coordinates. Damp asphalt retains some surface texture and the zero-wetness material remains unchanged.

## Playable resident hair

The swept and cropped wardrobe cuts use the MakeHuman system hairstyles `short01` and `short04`. The swept style uses outward-facing volume normals and temporal coverage of the source strand mask at the hairline. Their CC0 mesh bindings fit the existing hm08 head, and the original 2048-pixel strand masks retain the shape of the hairline. The restored scalp uses the wearer's current skin material. Run `Scripts/make-resident-hair.py` with the hm08 OBJ, the extracted system pack and an output directory. Set `SABARMATI_RESIDENT_HAIR_SOURCE` to that directory for `Scripts/import-resident-hair.py`. [MakeHuman system assets and CC0 credits](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html)

The 2048-pixel playable and close-resident eye chart uses the CC0 MakeHuman system `eyes/materials/brown_eye.png`. `Scripts/prepare-resident-eyes.py` samples each iris and its adjoining sclera onto the existing public mesh UVs at a 12 mm iris diameter. `Scripts/import-resident-eyes.py` reads the result from `SABARMATI_RESIDENT_HAIR_SOURCE` and binds the atlas, iris mask and corneal dome normal to Unreal's Eye shading model. [MakeHuman system assets and CC0 credits](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html)

## Kesar street, vehicle and movement pass

The terrace utilities, terracotta screen, stair nosings and roadside drainage details are original. They follow the existing fictional Kesar block and use the credited scanned stone and asphalt surfaces. The new hatch paint, glass, tyre material and invisible Chaos physics chassis are project-authored. `Scripts/build-hatch-finishes.py`, `Scripts/make-hatch-physics.py` and `Scripts/import-hatch-physics.py` reproduce the Unreal assets. The rendered original hatch adaptation continues to use the CC0 coachwork credited above.

The resident jump, fall and landing sequences derive from the Third Person template's installed `Mannequins/Anims/Unarmed/Jump` examples. `Scripts/retarget-resident-jump.py` uses Unreal's IK Retargeter to bake the three clips onto the Everyday and Kurta resident skeletons. The runtime moves the capsule with Character Movement and blends the baked poses over the ground gait. [Unreal Engine EULA, section 5(b)](https://www.unrealengine.com/eula/unreal)

`Scripts/bake-resident-gaits.py` reads arm and wrist directions from the installed template's forward unarmed walk and jog. It retains the original resident's planted leg tracks and bakes the gait for both resident skeletons. The fence and wall poses, courtyard geometry, and district traffic bodies are original project assets. [Unreal Engine EULA, section 5(b)](https://www.unrealengine.com/eula/unreal)
