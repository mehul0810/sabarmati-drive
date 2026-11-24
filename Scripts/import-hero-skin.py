"""Build the playable resident's skin from native hm08 UV0 atlases in Unreal Editor."""
import os
from pathlib import Path

import unreal as ue


source = Path(os.environ["SABARMATI_HERO_SKIN_SOURCE"])
folder = "/Game/Sabarmati/Riverfront/People/Hero"
assets = ue.EditorAssetLibrary
asset_tools = ue.AssetToolsHelpers.get_asset_tools()
edit = ue.MaterialEditingLibrary


def get_or_create(name, kind, factory):
    path = f"{folder}/{name}"
    asset = (assets.load_asset(path) if assets.does_asset_exist(path)
             else asset_tools.create_asset(name, folder, kind, factory))
    if not isinstance(asset, kind):
        raise RuntimeError(f"Could not create {path}")
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError(f"Could not save {asset.get_path_name()}")


def import_map(name, compression, srgb, group):
    file = source / f"{name}.png"
    if not file.is_file():
        raise FileNotFoundError(file)
    task = ue.AssetImportTask()
    task.set_editor_property("filename", str(file))
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])
    texture = assets.load_asset(f"{folder}/{name}")
    if not isinstance(texture, ue.Texture2D):
        raise RuntimeError(f"Could not import {file}")
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("lod_group", group)
    save(texture)
    return texture


color = import_map("T_HeroSkin_Color", ue.TextureCompressionSettings.TC_DEFAULT, True,
                   ue.TextureGroup.TEXTUREGROUP_CHARACTER)
normal = import_map("T_HeroSkin_Normal", ue.TextureCompressionSettings.TC_NORMALMAP, False,
                    ue.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
roughness = import_map("T_HeroSkin_Roughness", ue.TextureCompressionSettings.TC_MASKS, False,
                       ue.TextureGroup.TEXTUREGROUP_CHARACTER_SPECULAR)

profile = get_or_create("SSP_HeroSkin", ue.SubsurfaceProfile, ue.SubsurfaceProfileFactory())
settings = profile.get_editor_property("settings")
# Representative default cheek (158, 120, 98 sRGB), in linear colour.
settings.set_editor_property("surface_albedo", ue.LinearColor(.342, .188, .122, 1))
# UE's built-in Burley model takes a mean free path in centimetres.
settings.set_editor_property("mean_free_path_distance", .19)
settings.set_editor_property("mean_free_path_color", ue.LinearColor(1, .45, .27, 1))
profile.set_editor_property("settings", settings)
save(profile)

material = get_or_create("M_HeroSkin", ue.Material, ue.MaterialFactoryNew())
material.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_SUBSURFACE_PROFILE)
material.set_editor_property("subsurface_profile", profile)
material.set_editor_property("used_with_skeletal_mesh", True)
edit.delete_all_material_expressions(material)


def expression(kind, **properties):
    node = edit.create_material_expression(material, kind)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node


def connect(start, pin, end, target):
    if not edit.connect_material_expressions(start, pin, end, target):
        raise RuntimeError(f"Could not connect {pin} to {target}")


def output(node, pin, prop):
    if not edit.connect_material_property(node, pin, prop):
        raise RuntimeError(f"Could not connect {prop}")


# Mesh UV0 already addresses the matching hm08 face, hands, body and lips.
albedo = expression(ue.MaterialExpressionTextureSample, texture=color)
tint = expression(ue.MaterialExpressionVectorParameter, parameter_name="BaseColorFactor",
                  default_value=ue.LinearColor(1, 1, 1, 1))
tinted = expression(ue.MaterialExpressionMultiply)
connect(albedo, "RGB", tinted, "A")
connect(tint, "", tinted, "B")
output(tinted, "", ue.MaterialProperty.MP_BASE_COLOR)
pores = expression(ue.MaterialExpressionTextureSample, texture=normal,
                   sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
output(pores, "RGB", ue.MaterialProperty.MP_NORMAL)
oil = expression(ue.MaterialExpressionTextureSample, texture=roughness,
                 sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
output(oil, "R", ue.MaterialProperty.MP_ROUGHNESS)
output(expression(ue.MaterialExpressionConstant, r=.35), "", ue.MaterialProperty.MP_SPECULAR)
edit.layout_material_expressions(material)
edit.recompile_material(material)
save(material)
for prop in (ue.MaterialProperty.MP_BASE_COLOR, ue.MaterialProperty.MP_NORMAL,
             ue.MaterialProperty.MP_ROUGHNESS, ue.MaterialProperty.MP_SPECULAR):
    if not edit.get_material_property_input_node(material, prop):
        raise RuntimeError(f"Missing material connection for {prop}")
if (material.get_editor_property("subsurface_profile") != profile
        or material.get_editor_property("shading_model") != ue.MaterialShadingModel.MSM_SUBSURFACE_PROFILE
        or not material.get_editor_property("used_with_skeletal_mesh")):
    raise RuntimeError("Unexpected hero skin lighting model, profile or skeletal mesh usage")
ue.log(f"SABARMATI HERO SKIN SAVED {material.get_path_name()} with {profile.get_path_name()}")
