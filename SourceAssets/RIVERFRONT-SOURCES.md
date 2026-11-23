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

## Canopy, glazing, instruments and street sound

The park's larger trees use the CC0 Jacaranda by Rico Cilliers and Rob Tuytel. The project converted its trunk, branch cards and masked leaf atlas for the Metal renderer. The mesh has 78,352 triangles at LOD 0 and three imported or reduced LODs. Texture archives and the source GLTF are development inputs. [Jacaranda Tree](https://polyhaven.com/a/jacaranda_tree) · [Poly Haven license](https://polyhaven.com/license)

Upper-floor windows use a cropped, tone-mapped interior panorama in a view-dependent room shader. The photo was licensed separately from the locally modeled, enterable rooms on the ground floor. [Small Empty Room 4 by Sergej Majboroda](https://polyhaven.com/a/small_empty_room_4) · [Poly Haven license](https://polyhaven.com/license)

The center-mounted instrument panel, headlamp internals, grille and street utilities are project-authored. The dashboard receives actual road speed, engine RPM and gear. A seated version of the public clothed resident rides the two- and three-wheelers. Thirteen original synthesized clips cover vehicles, footfalls, shop bell, weather, river, distant traffic and market voices. The market voices contain no identifiable speech or recordings.

The phone user and seated residents on promenade benches derive from the same public clothed mesh. The 70 × 143 mm phone is original geometry. The mapped streets share the Jacaranda canopy and a new dry ground shader derived from the credited Concrete 034 aggregate. These props and materials follow local art direction.
