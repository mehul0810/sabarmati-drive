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
The modeled structure uses the documented 300 m span and 10–14 m width. The
walkable central deck occupies 285 m; its terminal details meet the fictional
bank layout. The west landing references the Flower Park side. All trusses,
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
collision bounds. Door access is still the prototype interaction, and both
vehicles use the existing arcade steering and vehicle physics.

Rebuild the GLB with `python3 Scripts/prepare-riverfront-crossover.py`. Import
the asset with `SABARMATI_IMPORT_CROSSOVER=1` set when running the existing
`Scripts/import-riverfront-hatch.py` editor commandlet.


## Walkable arcade and street materials

The arcade is an original brick-and-plaster design with separate structural wings, galleries, terracotta sun-breakers, rooftop stair rooms and a public cross-passage. The flat roofs, differences in building length and space between buildings were studied in the [Atal west-bank satellite view](https://www.arcgis.com/apps/mapviewer/index.html?basemap=satellite&center=72.572,23.017&level=18). The [SRFDCL street description](https://sabarmatiriverfront.com/streets.html) informed the footpath and roadside drainage treatment. Satellite imagery is reference-only.

The road uses ambientCG’s [Asphalt 033](https://ambientcg.com/view?id=Asphalt033) photogrammetry at its 2.5 m capture scale. [Bricks 051](https://ambientcg.com/view?id=Bricks051) is a 1.25 m brick scan. [Plaster 001](https://ambientcg.com/view?id=Plaster001) is a procedural stucco surface. All three are [CC0](https://docs.ambientcg.com/license/). The weathered stripe and glazing shaders are project-authored. To rebuild this set, place the 1K-JPG archives in `Saved/LookSources` as `Asphalt033.zip`, `Bricks051.zip` and `Plaster001.zip`, and run the existing look commandlet with `SABARMATI_LOOK_SCOPE=street`. `SABARMATI_LOOK_SOURCES` can point to a separate archive directory.
