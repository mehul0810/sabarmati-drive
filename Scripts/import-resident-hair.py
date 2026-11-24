"""Import the fitted CC0 hair and its 2K masked strand atlases using UE Interchange."""
import os
from pathlib import Path
import unreal as ue

folder = "/Game/Sabarmati/Riverfront/People/Wardrobe/Hair"
source = Path(os.environ["SABARMATI_RESIDENT_HAIR_SOURCE"])
assets = ue.EditorAssetLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
edit = ue.MaterialEditingLibrary
coverage = .30


def save(asset):
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Failed to save " + asset.get_path_name())


def material(style):
    name = "T_Resident" + style
    task = ue.AssetImportTask()
    task.set_editor_property("filename", str(source / (name + ".png")))
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    tools.import_asset_tasks([task])
    tex = assets.load_asset(folder + "/" + name)
    if not isinstance(tex, ue.Texture2D):
        raise RuntimeError("Missing strand texture " + name)
    tex.set_editor_property("do_scale_mips_for_alpha_coverage", True)
    tex.set_editor_property("alpha_coverage_thresholds", ue.Vector4(0, 0, 0, coverage))
    save(tex)

    name = "M_Resident" + style
    path = folder + "/" + name
    mat = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        name, folder, ue.Material, ue.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", ue.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    if style == "Sweep":
        mat.set_editor_property("dither_opacity_mask", True)
    mat.set_editor_property("opacity_mask_clip_value", coverage)
    edit.delete_all_material_expressions(mat)
    strand = edit.create_material_expression(mat, ue.MaterialExpressionTextureSample)
    strand.set_editor_property("texture", tex)
    roughness = edit.create_material_expression(mat, ue.MaterialExpressionConstant)
    roughness.set_editor_property("r", .66 if style == "Sweep" else .51)
    specular = edit.create_material_expression(mat, ue.MaterialExpressionConstant)
    specular.set_editor_property("r", .23 if style == "Sweep" else .34)
    for node, output, pin in [(strand, "RGB", ue.MaterialProperty.MP_BASE_COLOR),
                              (strand, "A", ue.MaterialProperty.MP_OPACITY_MASK),
                              (roughness, "", ue.MaterialProperty.MP_ROUGHNESS),
                              (specular, "", ue.MaterialProperty.MP_SPECULAR)]:
        if not edit.connect_material_property(node, output, pin):
            raise RuntimeError(f"{name}: {pin}")
    if style == "Sweep":
        # The outward normals are shared by both sides of each card.
        sign = edit.create_material_expression(mat, ue.MaterialExpressionTwoSidedSign)
        normal = edit.create_material_expression(mat, ue.MaterialExpressionConstant3Vector)
        normal.set_editor_property("constant", ue.LinearColor(0,0,1,1))
        shared = edit.create_material_expression(mat, ue.MaterialExpressionMultiply)
        if not (edit.connect_material_expressions(sign, "", shared, "A") and
                edit.connect_material_expressions(normal, "", shared, "B") and
                edit.connect_material_property(shared, "", ue.MaterialProperty.MP_NORMAL)):
            raise RuntimeError("Unable to assign the hair volume normals")
    edit.layout_material_expressions(mat)
    edit.recompile_material(mat)
    save(mat)
    return mat


def import_cut(style):
    finish = material(style)
    name = "SM_Resident" + style
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    pipeline = ue.InterchangeGenericAssetsPipeline()
    pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials", False)
    # Retain the full hairline on M1 Metal; Nanite's simplified fallback
    # can collapse the overlapping masked cards into a solid silhouette.
    pipeline.get_editor_property("mesh_pipeline").set_editor_property("build_nanite", False)
    params = ue.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("override_pipelines", [ue.SoftObjectPath(pipeline.get_path_name())])
    if not manager.import_asset(folder, manager.create_source_data(str(source / (name + ".glb"))), params):
        raise RuntimeError(name)
    mesh = assets.load_asset(folder + "/" + name)
    if not isinstance(mesh, ue.StaticMesh):
        raise RuntimeError(name + " failed to import")
    if mesh.get_editor_property("nanite_settings").get_editor_property("enabled"):
        raise RuntimeError(name + " unexpectedly requires Nanite")
    slots = {str(s.get_editor_property("material_slot_name")): i
             for i, s in enumerate(mesh.get_editor_property("static_materials"))}
    if set(slots) != {"scalp", "strands"} or mesh.get_num_sections(0) != 2:
        raise RuntimeError(f"Invalid hair or scalp sections: {slots}")
    mesh.set_material(slots["strands"], finish)
    # The component binds this slot to the actual wearer's skin MID at runtime.
    mesh.set_material(slots["scalp"], assets.load_asset("/Engine/EngineMaterials/DefaultMaterial"))
    save(mesh)
    ue.log(f"RESIDENT HAIR VERIFIED {name}: {mesh.get_num_triangles(0)} triangles, {slots}, {finish.get_path_name()}")


assets.make_directory(folder)
for hairstyle in ["Sweep", "Crop"]:
    import_cut(hairstyle)
