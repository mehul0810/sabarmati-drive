# Playable resident skin

Both fictional residents use hm08 UVs. Each complexion combines two generic MakeHuman skin atlases and brow hair from the eyebrow009 accessory. The source materials and eyebrow OBJ identify Data Collection AB, Joel Palmius and Jonas Hauquier as the copyright holders at their September 2020 CC0 release. The upstream catalogue records their CC0 release. [MakeHuman system asset catalogue](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html) · [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

| Upstream asset | Use in the game |
| --- | --- |
| `skins/young_asian_male/young_lightskinned_male_diffuse3.png` | Source pigmentation, lips, nose and fine skin colour |
| `skins/young_african_male/young_darkskinned_male_diffuse.png` | Blended complexion and skin variation |
| `skins/young_asian_female/young_lightskinned_female_diffuse3.png` and `skins/young_african_female/young_darkskinned_female_diffuse.png` | Pigmentation, facial features, fine relief and surface variation on the female face and hands |
| `eyebrows/eyebrow009/eyebrow009.obj` and `eyebrow009.png` | Individual brow hairs projected onto the skin atlas |
| hm08 `base.obj` | Coordinates for the brows, shaved shadow and facial roughness |

The complexion combines the two skins in a 57:43 blend. The baker adds subtle shaved shadow and regional pigmentation and roughness. Separate seeded relief for each torso varies pore size, depth and orientation. The relief is projected in body space across the chest, abdomen and back and baked into the tangent-space normal map. The body and face come from the fictional resident.

## Rebuilding the maps

Extract the official CC0 system pack and use the same base OBJ used by the district resident generator. The baker verifies the SHA-256 of each source before writing. The game reads the imported Unreal assets offline. [System asset ZIP](https://files.makehumancommunity.org/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip) · [hm08 base OBJ](https://github.com/makehumancommunity/makehuman/blob/master/makehuman/data/3dobjs/base.obj)

Run this command from the repository with NumPy and Pillow available. The reference bake used NumPy 2.4.6 and Pillow 13.0.

```sh
python3 Scripts/prepare-hero-skin.py /path/to/unpacked/system-assets /path/to/base.obj /tmp/sabarmati-hero-skin-source --save-torso-master
```

To rebuild the female maps, add `--preset female` and choose a separate output directory. For the Unreal import, point `SABARMATI_HERO_SKIN_SOURCE` to that directory and set `SABARMATI_SKIN_PRESET=female` when running `Scripts/import-hero-skin.py`. Both skin materials use the shared Unreal subsurface profile. The female material lives at `Hero/Female/M_FemaleSkin`. The shader applies the player's saved complexion to the face, lips and hands.

The optional 16384 × 16384 torso height PNG stores 16-bit relief as an authoring file. The game uses a 4096 × 4096 tangent-space DirectX normal, a 2048 × 2048 sRGB colour texture and a 1024 × 1024 linear roughness texture. All three use the resident’s native UV0 and Unreal’s character texture groups. The Unreal material and imported textures live under `Content/Sabarmati/Riverfront/People/Hero`.

## Shared facial sculpt

Both playable outfits use the same authored adult face. The standard hm08 target in `SourceAssets/river-resident-adult.target` contains vertex offsets in the MakeHuman base model's native units. The shape broadens the mandibular angle, gives the chin slightly more projection and adds volume over the cheekbones. A separate local adjustment brings the upper and lower lip into contact at rest. It moves each lip by at most 0.77 mm.

The lower-face blend uses MakeHuman's generic CC0 targets. Their headers credit Manuel Bastioni in 2014, followed by Data Collection AB, Joel Palmius and Jonas Hauquier in 2020. The table records the weights before a smooth spatial mask protects the eyes, mouth, ears, scalp and neck. Lip closure was authored on the original mouth topology after the blend. [Head targets](https://github.com/makehumancommunity/makehuman/tree/master/makehuman/data/targets/head) · [Chin targets](https://github.com/makehumancommunity/makehuman/tree/master/makehuman/data/targets/chin) · [Cheek targets](https://github.com/makehumancommunity/makehuman/tree/master/makehuman/data/targets/cheek)

| Source target | Weight |
| --- | --- |
| `head-square` | 0.47 |
| `chin-bones-incr` | 0.29 |
| `chin-width-incr` | 0.53 |
| `chin-prominent-incr` | 0.51 |
| `l-cheek-bones-incr` and `r-cheek-bones-incr` | 0.27 and 0.25 |
| `l-cheek-volume-incr` and `r-cheek-volume-incr` | 0.26 and 0.24 |

Apply the committed sculpt after generating the original everyday and fitted-kurta GLBs, before their standard Unreal imports. The tool transfers the same offsets and normal corrections to each duplicate skin vertex at UV or material boundaries. It adjusts the two skin sections and leaves the wardrobe geometry, rig, animation and UVs intact. Point each Unreal importer at the finished GLB and its usual companion artwork.

```sh
python3 Scripts/sculpt-resident-face.py /path/to/base.obj neutral/SK_RiverEveryday.glb finished/SK_RiverEveryday.glb
python3 Scripts/sculpt-resident-face.py /path/to/base.obj neutral/SK_RiverTailoredKurta.glb finished/SK_RiverTailoredKurta.glb
```
