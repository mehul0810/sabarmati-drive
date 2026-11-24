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
