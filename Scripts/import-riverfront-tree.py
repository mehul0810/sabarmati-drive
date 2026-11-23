"""Import the reduced CC0 hero tree; keep a conservative count until editor LODs exist."""
from pathlib import Path
import os
import unreal as ue

project = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
dest = "/Game/Sabarmati/Riverfront/Foliage"
name = os.environ.get("SABARMATI_TREE_NAME", "SM_RiverfrontTree")
if name not in {"SM_RiverfrontTree", "SM_RiverfrontBroadleaf"}:
    raise RuntimeError(f"Unsupported tree: {name}")
path = dest + "/" + name
if ue.EditorAssetLibrary.does_asset_exist(path):
    mesh = ue.EditorAssetLibrary.load_asset(path)
else:
    task = ue.AssetImportTask()
    task.set_editor_property("filename", str(project / "Saved/LookSources/tree-small-02" / (name + ".gltf")))
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = [obj for obj in task.get_objects() if isinstance(obj, ue.StaticMesh)]
    if len(imported) != 1:
        raise RuntimeError(f"Expected one imported tree mesh, got {imported}")
    mesh = imported[0]
    if mesh.get_path_name().split(".")[0] != path:
        if not ue.EditorAssetLibrary.rename_asset(mesh.get_path_name(), path):
            raise RuntimeError(f"Cannot rename {mesh.get_path_name()} to {path}")
        mesh = ue.EditorAssetLibrary.load_asset(path)

def leaf_surface():
    asset_path = dest + "/M_Broadleaf"
    mat = ue.EditorAssetLibrary.load_asset(asset_path) if ue.EditorAssetLibrary.does_asset_exist(asset_path) else \
        ue.AssetToolsHelpers.get_asset_tools().create_asset("M_Broadleaf", dest, ue.Material, ue.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("Cannot create the broadleaf material")
    mat.set_editor_property("two_sided", True)
    edit = ue.MaterialEditingLibrary
    edit.delete_all_material_expressions(mat)
    base = edit.create_material_expression(mat, ue.MaterialExpressionConstant3Vector, -400, 0)
    base.set_editor_property("constant", ue.LinearColor(.095, .22, .046, 1.0))
    rough = edit.create_material_expression(mat, ue.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property("r", .72)
    if not edit.connect_material_property(base, "", ue.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("Cannot set leaf base color")
    if not edit.connect_material_property(rough, "", ue.MaterialProperty.MP_ROUGHNESS):
        raise RuntimeError("Cannot set leaf roughness")
    edit.recompile_material(mat)
    ue.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def sprig_surface():
    """Use the proper alpha map for UV-cropped cross-plane branch sprays."""
    def texture(name, filename, mask):
        path = dest + "/" + name
        if ue.EditorAssetLibrary.does_asset_exist(path):
            value = ue.EditorAssetLibrary.load_asset(path)
        else:
            task = ue.AssetImportTask()
            task.set_editor_property("filename", str(project / "Saved/LookSources/tree-small-02/textures" / filename))
            task.set_editor_property("destination_path", dest)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            value = ue.EditorAssetLibrary.load_asset(path)
        if not value:
            raise RuntimeError(f"Cannot load {path}")
        if mask:
            value.set_editor_property("srgb", False)
            value.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
            ue.EditorAssetLibrary.save_loaded_asset(value, only_if_is_dirty=False)
        return value

    path = dest + "/M_LeafSprig"
    mat = ue.EditorAssetLibrary.load_asset(path) if ue.EditorAssetLibrary.does_asset_exist(path) else \
        ue.AssetToolsHelpers.get_asset_tools().create_asset("M_LeafSprig", dest, ue.Material, ue.MaterialFactoryNew())
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("blend_mode", ue.BlendMode.BLEND_MASKED)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    edit = ue.MaterialEditingLibrary
    edit.delete_all_material_expressions(mat)

    def expression(cls, x, y):
        return edit.create_material_expression(mat, getattr(ue, "MaterialExpression" + cls), x, y)

    uv = expression("TextureCoordinate", -850, 0)
    parent = expression("Custom", -620, 20)
    coord = ue.CustomInput()
    coord.set_editor_property("input_name", "UV")
    parent.set_editor_property("inputs", [coord])
    parent.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT2)
    parent.set_editor_property("code", "return UV * float2(.300,.545) + float2(.532,.096);")
    assert edit.connect_material_expressions(uv, "", parent, "UV")

    rgba = expression("TextureSample", -340, -100)
    rgba.set_editor_property("texture", texture("T_BroadleafColor", "tree_small_02_leaves_diff_1k.jpg", False))
    rgba.set_editor_property("sampler_type", ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    opacity = expression("TextureSample", -340, 140)
    opacity.set_editor_property("texture", texture("T_BroadleafOpacity", "tree_small_02_leaves_alpha_1k.png", True))
    opacity.set_editor_property("sampler_type", ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
    for sampler in (rgba, opacity):
        assert edit.connect_material_expressions(parent, "", sampler, "UVs")
    assert edit.connect_material_property(rgba, "RGB", ue.MaterialProperty.MP_BASE_COLOR)
    assert edit.connect_material_property(opacity, "R", ue.MaterialProperty.MP_OPACITY_MASK)
    rough = expression("Constant", -290, 340)
    rough.set_editor_property("r", .76)
    assert edit.connect_material_property(rough, "", ue.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(mat)
    if not ue.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(path)
    ue.log("SABARMATI SPRIG MASK SAVED")

if name == "SM_RiverfrontBroadleaf":
    slots = mesh.get_editor_property("static_materials")
    labels = [str(s.get_editor_property("material_slot_name")) for s in slots]
    ue.log(f"SABARMATI BROADLEAF SLOTS {labels}")
    leaf_ids = [i for i, label in enumerate(labels) if "canopy" in label.lower()]
    if len(leaf_ids) != 1:
        raise RuntimeError(f"Expected one canopy material slot, got {labels}")
    mesh.set_material(leaf_ids[0], leaf_surface())
    sprig_surface()
    # NullRHI commandlets may not instantiate this editor subsystem.
    subsystem = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if subsystem:
        spec = ue.StaticMeshReductionOptions(
            auto_compute_lod_screen_size=False,
            reduction_settings=[
                ue.StaticMeshReductionSettings(percent_triangles=fraction, screen_size=screen)
                for fraction, screen in [(1., 1.), (.34, .16), (.13, .065), (.045, .023)]
            ])
        if subsystem.set_lods(mesh, spec) != 4:
            raise RuntimeError("Broadleaf LOD generation did not return four levels.")
        ue.log("SABARMATI BROADLEAF LODS " + str(subsystem.get_lod_screen_sizes(mesh)))
    else:
        ue.log("SABARMATI BROADLEAF no editor LOD subsystem; keep conservative placement.")

if not ue.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
    raise RuntimeError(f"Cannot save {mesh.get_path_name()}")
ue.log(f"SABARMATI TREE SAVED {mesh.get_path_name()}, bounds {mesh.get_bounds()}")
