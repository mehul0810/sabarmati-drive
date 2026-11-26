# Playable resident physique

The everyday and fitted-kurta meshes share one adult athletic body profile. The authoring tool samples muscle displacement from the full fictional hm08 skin and transfers it continuously through each outfit. The chest gains side-to-side and front-to-back volume. The deltoids, upper arms, thighs and calves gain volume around the existing limb axes. The waist stays at its existing dimensions. A protected area around the head, wrist attachment and feet keeps their original mesh coordinates.

The stock source is MakeHuman's average-weight adult male muscle target. Its header records the September 2020 CC0 release and credits Manuel Bastioni, Data Collection AB, Joel Palmius and Jonas Hauquier. The upstream Git blob is `b7c039c7f463fe1f1721b9ab269c7248d9c4f2f7`. Two copyright-comment lines in the checked-in copy have their trailing spaces removed. The tool accepts both byte representations and verifies the geometry data. [MakeHuman muscle target](https://github.com/makehumancommunity/makehuman/blob/master/makehuman/data/targets/macrodetails/universal-male-young-maxmuscle-averageweight.target) · [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

## Offline rebuild

Apply the resident's facial sculpt first, then fit the complete dressed mesh. The physique step uses Python and NumPy, the original hm08 OBJ, and the included muscle target. Its normal transform follows the same deformation as the vertex positions. Unreal uses the existing skeleton, garments' skin weights and material UVs.

```sh
python3 Scripts/fit-resident-physique.py /path/to/base.obj faced/SK_RiverEveryday.glb finished/SK_RiverEveryday.glb
python3 Scripts/fit-resident-physique.py /path/to/base.obj faced/SK_RiverTailoredKurta.glb finished/SK_RiverTailoredKurta.glb
```

Use the corresponding everyday and kurta Unreal importers with the finished GLBs. See [facial sculpt instructions](RESIDENT-SKIN.md).

## Fit measurements

These are plane intersections through the reference skin in its A-pose. Each entry gives width × front-to-back depth in centimetres. Limb planes run perpendicular to the corresponding long bone. The shoulder-level plane includes the deltoids and proximal upper arms.

| Section | Original | Athletic |
| --- | --- | --- |
| Shoulder level | 46.9 × 15.5 | 52.1 × 17.4 |
| Chest | 30.2 × 18.9 | 32.4 × 21.3 |
| Waist | 29.7 × 18.5 | 29.7 × 18.5 |
| Upper arm | 7.9 × 8.8 | 8.3 × 10.2 |
| Thigh | 14.6 × 16.8 | 16.7 × 19.2 |
| Calf | 10.6 × 11.4 | 11.5 × 12.5 |

## Undressed upper bodies

The wardrobe uses two fitted meshes on the everyday skeleton. The male mesh exposes the athletic torso. The female mesh includes an opaque knit bralette with an elastic band. Both reuse the original denim, shoes, facial sculpt, hair attachment and skin maps. Only the selected mesh is drawn.

To rebuild, use the hm08 OBJ and the official clothing pack listed in [the skin and source notes](RESIDENT-SKIN.md). With NumPy and Pillow available, run the following command. The output GLBs are temporary import inputs.

```sh
python3 Scripts/make-foundation-bodies.py /path/to/base.obj /path/to/system-assets /tmp/sabarmati-foundation
```

In a scratch Unreal project containing the shipped People assets, set `SABARMATI_FOUNDATION_SOURCE` to the GLB output directory and run `Scripts/import-female-residents.py`. Promote the two verified skeletal meshes from `Wardrobe/Foundation`.

## Short-sleeved jersey

Both jersey presets fit the CC0 MakeHuman `male_casualsuit06` sewn shirt to the resident skeleton and body targets. The unprinted knit uses its normal and occlusion atlases. The ordinary jeans and shoes remain separate wardrobe slots. To regenerate, run `Scripts/make-foundation-bodies.py BASE_OBJ SYSTEM_ASSETS OUTPUT --tee` and set `SABARMATI_TEE_SOURCE` to that output for `Scripts/import-female-residents.py` in a scratch project. [MakeHuman CC0 system asset pack](https://files.makehumancommunity.org/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip)

## Western wardrobe and body contours

The fine knit, quilted jacket, formal suit, denim shorts, court trainers and lace-up boots use the CC0 patterns `male_casualsuit02`, `male_casualsuit05`, `male_elegantsuit01`, `female_casualsuit02`, `shoes05` and `shoes03`, respectively. The knit uses the stock occlusion map as a neutral, unprinted fabric. Sutra's tank, trunks and crew socks were cut from the existing hm08 body and garment shell. The two fitted bodies use their own subtle collarbone, chest, abdominal, knee and calf contours on the existing skeleton. [MakeHuman system assets and CC0 credits](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html)

The exposed adult skin uses the CC0 female topology by spamrakuen (asset 3396) and CC0 male detail by ukiyoe (asset 3223). The female topology shares the original waist and ankle borders. The male detail is fitted over the original skin with a recessed border and UVs projected from the nearby skin. These details are baked into the exposed skin section, which the wardrobe hides while an inner or outer bottom covers it. [MakeHuman contribution index and licenses](https://files.makehumancommunity.org/sites/default/files/assets.json)

Save the anatomy sources under their asset IDs as `3396.obj`, `3396.proxy`, `3223.obj` and `3223.mhclo`. Run `Scripts/make-foundation-bodies.py BASE_OBJ SYSTEM_ASSETS OUTPUT --western --anatomy ANATOMY_DIR`. In a scratch Unreal project with the shipped People assets, move the four old skeletal meshes out of `Wardrobe/Foundation` before importing. Set `SABARMATI_WESTERN_SOURCE` to `OUTPUT` and run `Scripts/import-female-residents.py`. Promote the skeletal meshes and their materials from `Wardrobe/Western` and the four revised meshes from `Wardrobe/Foundation` together with the wardrobe code.
