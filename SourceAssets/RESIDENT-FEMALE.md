# Female resident character art

The female resident is a fictional adult with two fitted outfits and a distinct facial sculpt. Both outfits use the original hm08 topology and the same wardrobe, hair and accessory attachment locations as the first resident. The everyday outfit reuses the everyday rig. The kurta reuses the kurta rig. Clothing, exposed skin, lips, and their shading normals are fitted together. The eyes, shoes and scalp attachment area retain the original geometry.

## Art sources

The bundled `river-resident-female.target` averages three paired female-minus-male adult hm08 targets from MakeHuman. Equal weights give one change in body shape across the three source pairs. Source filenames are authoring metadata. Characters have no ancestry attribute. All six targets have CC0 notices crediting Manuel Bastioni, Data Collection AB, Joel Palmius and Jonas Hauquier. The authoring script verifies their Git blob digests when regenerating the bundled target. [MakeHuman macrodetails](https://github.com/makehumancommunity/makehuman/tree/master/makehuman/data/targets/macrodetails) · [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The fit removes the macros’ stature difference, then shapes the pelvis, shoulders, shirt front, jaw and cheeks around the existing joints. Both skin and lips use the female subsurface material. Clothing, wristwatch, and shoes retain the source wardrobe UVs. [Original wardrobe and rig sources](RIVERFRONT-SOURCES.md) · [Athletic base body](RESIDENT-PHYSIQUE.md)

## Rebuild from the dressed source meshes

Use the final athletic everyday and tailored-kurta GLBs, the original hm08 base OBJ, Python and NumPy. The delivered Unreal assets also work in an offline installation.

```sh
python3 Scripts/fit-resident-female.py /path/to/base.obj \
  --outfit /path/to/athletic/SK_RiverEveryday.glb /tmp/ResidentArt/SK_FemaleEveryday.glb \
  --outfit /path/to/athletic/SK_RiverTailoredKurta.glb /tmp/ResidentArt/SK_FemaleKurta.glb
```

To regenerate the bundled target, provide the directory containing the six original MakeHuman targets using `--build-target /path/to/macrodetails`. The everyday input SHA-256 is `d1f987a63c2f72944c12372ff928cc331658d78421b12e607a0ec340c6366e0d`. The tailored-kurta input SHA-256 is `362f4e5e293a74d5167b106ef07d132ae29ca403c133f1dfb418a924776efc79`.

Set `SABARMATI_FEMALE_SOURCE` to the output directory. Run `Scripts/import-female-residents.py` through Unreal's Python commandlet in an isolated content project containing the two original dressed meshes, their skeletons and physics assets, materials and the female skin. The importer checks both rigs and all sections before saving the two meshes under `/Game/Sabarmati/Riverfront/People/Wardrobe/Female`.

## Geometry checks

Width and front-to-back depth below are centimetres measured at the same bind-pose heights through the underlying body. Each row describes these two specific fictional characters.

| Cross-section | Original athletic resident | Female resident |
| --- | --- | --- |
| Shoulder level | 52.1 × 17.4 | 48.1 × 17.1 |
| Chest | 31.3 × 21.5 | 27.1 × 24.7 |
| Waist | 29.7 × 18.5 | 27.5 × 17.4 |
| Hips | 36.2 × 22.3 | 39.9 × 21.8 |
| Jaw | 10.0 × 16.5 | 9.3 × 16.1 |

Vertices, normals and skin weights were checked in idle and at left and right swing during walking and running. All 12,276 sampled triangles around the shoulder, elbow and knee kept their orientation relative to the original clothed mesh. The trousers cleared the kurta hem throughout 100 walk and 100 run samples. The skeletal meshes have the same bone hierarchy, skin weights, indices and UVs as their corresponding originals. Each outfit moves 2,541 head skin vertices and 477 lip vertices by more than 1 mm.
