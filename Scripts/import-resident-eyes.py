"""Import the projected CC0 ocular colour, iris mask and the corneal dome normal for Metal."""
import os
from pathlib import Path
import unreal as ue

source = Path(os.environ["SABARMATI_RESIDENT_HAIR_SOURCE"])
folder = "/Game/Sabarmati/Riverfront/People/Wardrobe"
assets = ue.EditorAssetLibrary
edit = ue.MaterialEditingLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()


def import_eye_texture(name, normal=False):
    task = ue.AssetImportTask()
    task.set_editor_property("filename", str(source / (name + ".png")))
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    tools.import_asset_tasks([task])
    tex = assets.load_asset(folder + "/" + name)
    if not isinstance(tex, ue.Texture2D) or tex.blueprint_get_size_x() != 2048 or tex.blueprint_get_size_y() != 2048:
        raise RuntimeError(name)
    if normal:
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("compression_no_alpha", False)
    if not assets.save_loaded_asset(tex, False):
        raise RuntimeError(tex.get_path_name())
    return tex


colour = import_eye_texture("T_RiverEyes")
dome = import_eye_texture("T_ResidentCornea_N", normal=True)
name = "M_RiverEyes"
path = folder + "/" + name
mat = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,folder,ue.Material,ue.MaterialFactoryNew())
edit.delete_all_material_expressions(mat)
mat.set_editor_property("used_with_skeletal_mesh", True)
mat.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_EYE)
mat.set_editor_property("use_material_attributes", True)


def node(kind, **props):
    item = edit.create_material_expression(mat, kind)
    for key, val in props.items():
        item.set_editor_property(key,val)
    return item


def link(first, output, second, pin):
    if not edit.connect_material_expressions(first,output,second,pin):
        raise RuntimeError(f"Could not connect {second} {pin}")


uv = node(ue.MaterialExpressionTextureCoordinate, u_tiling=1/.068, v_tiling=1/.038)
offset = node(ue.MaterialExpressionConstant2Vector,r=-.752/.068,g=-.038/.038)
chart = node(ue.MaterialExpressionAdd)
link(uv,"",chart,"A")
link(offset,"",chart,"B")
albedo = node(ue.MaterialExpressionTextureSample, texture=colour)
normal = node(ue.MaterialExpressionTextureSample, texture=dome, sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
link(chart,"",albedo,"UVs")
link(chart,"",normal,"UVs")
rough = node(ue.MaterialExpressionLinearInterpolate, const_a=.21, const_b=.085)
link(albedo,"A",rough,"Alpha")
attrs = node(ue.MaterialExpressionMakeMaterialAttributes)
link(albedo,"RGB",attrs,"BaseColor")
link(normal,"RGB",attrs,"Normal")
link(rough,"",attrs,"Roughness")
link(node(ue.MaterialExpressionConstant,r=.39),"",attrs,"Specular")
# For the native Eye shading model, these two shared custom-data channels
# are labelled Iris Mask and Iris Distance on the material output.
link(albedo,"A",attrs,"ClearCoat")
link(node(ue.MaterialExpressionConstant,r=.22),"",attrs,"ClearCoatRoughness")
if not edit.connect_material_property(attrs,"",ue.MaterialProperty.MP_MATERIAL_ATTRIBUTES):
    raise RuntimeError("Could not bind the Eye shading attributes")
edit.layout_material_expressions(mat)
edit.recompile_material(mat)
if not assets.save_loaded_asset(mat,False):
    raise RuntimeError(name)
ue.log("RESIDENT EYE VERIFIED native Eye shading with iris mask, corneal normal and tear-film roughness")
