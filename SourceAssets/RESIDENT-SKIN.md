# Playable resident skin

The playable resident uses the existing fictional hm08 mesh. His skin combines two generic MakeHuman skin atlases and the hair detail from the eyebrow009 accessory. The source materials and eyebrow OBJ identify Data Collection AB, Joel Palmius and Jonas Hauquier as the copyright holders at their September 2020 CC0 release. The upstream catalogue lists all three assets under CC0. [MakeHuman system asset catalogue](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html) · [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

| Upstream asset | Use in the game |
| --- | --- |
| `skins/young_asian_male/young_lightskinned_male_diffuse3.png` | Source pigmentation, lips, nose and fine skin colour |
| `skins/young_african_male/young_darkskinned_male_diffuse.png` | Blended complexion and skin variation |
| `eyebrows/eyebrow009/eyebrow009.obj` and `eyebrow009.png` | Individual brow hairs projected onto the skin atlas |
| hm08 `base.obj` | Coordinates for the brows, shaved shadow and facial roughness |

The complexion combines the two skins in a 57:43 blend. The baker adds subtle shaved shadow, a seeded pore normal and regional roughness for the forehead, nose and lips. These last two maps approximate surface response from fine source variation. The face shape comes from the fictional resident. No person-specific portrait, scan or biometric reference was supplied to the baker.

## Rebuilding the maps

Extract the official CC0 system pack and use the same base OBJ used by the district resident generator. The baker verifies the SHA-256 of each source before writing. The game reads the imported Unreal assets offline. [System asset ZIP](https://files.makehumancommunity.org/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip) · [hm08 base OBJ](https://github.com/makehumancommunity/makehuman/blob/master/makehuman/data/3dobjs/base.obj)

Run this command from the repository with NumPy and Pillow available. The reference bake used NumPy 2.4.6 and Pillow 13.0.

```sh
python3 Scripts/prepare-hero-skin.py /path/to/unpacked/system-assets /path/to/base.obj /tmp/sabarmati-hero-skin-source
```

The 2048 × 2048 colour texture uses sRGB. The 2048 × 2048 normal texture is tangent-space DirectX, and the 1024 × 1024 roughness texture is linear grayscale. All three use the resident's native UV0 and the same mapping across the face, lips and hands. The Unreal material and imported textures live under `Content/Sabarmati/Riverfront/People/Hero`.
