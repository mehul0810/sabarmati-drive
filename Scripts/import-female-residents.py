"""Import the fitted female resident meshes against the shipped outfit rigs in a scratch project."""
import os
from pathlib import Path
import unreal as ue

ROOT = "/Game/Sabarmati/Riverfront/People"
TEE = os.environ.get("SABARMATI_TEE_SOURCE")
LOWER = os.environ.get("SABARMATI_LOWER_SOURCE")
FOUNDATION = os.environ.get("SABARMATI_FOUNDATION_SOURCE")
TARGET = ROOT + ("/Wardrobe/Jersey" if TEE else "/Wardrobe/Foundation" if FOUNDATION or LOWER else "/Wardrobe/Female")
SOURCE = Path(TEE or LOWER or FOUNDATION or os.environ["SABARMATI_FEMALE_SOURCE"])
library = ue.EditorAssetLibrary
manager = ue.InterchangeManager.get_interchange_manager_scripted()


def make_jersey_material():
    tools=ue.AssetToolsHelpers.get_asset_tools()
    edit=ue.MaterialEditingLibrary
    path=TARGET+"/M_Jersey"
    material=tools.create_asset("M_Jersey",TARGET,ue.Material,ue.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Expected a clean staging folder for "+path)
    material.set_editor_property("used_with_skeletal_mesh",True)
    material.set_editor_property("two_sided",True)
    def connect(a,output,b,pin):
        if not edit.connect_material_expressions(a,output,b,pin): raise RuntimeError(pin)
    def bind(expr,pin,prop):
        if not edit.connect_material_property(expr,pin,prop): raise RuntimeError(str(prop))
    for name,is_normal in (("T_JerseyKnit",False),("T_JerseyKnit_N",True)):
        task=ue.AssetImportTask()
        task.filename=str(SOURCE/(name+".png"))
        task.destination_path=TARGET
        task.automated=True
        tools.import_asset_tasks([task])
        tex=library.load_asset(TARGET+"/"+name)
        if is_normal:
            tex.set_editor_property("compression_settings",ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb",not is_normal)
        if not library.save_loaded_asset(tex,False): raise RuntimeError(name)
        sample=edit.create_material_expression(material,ue.MaterialExpressionTextureSample)
        sample.set_editor_property("texture",tex)
        if is_normal:
            sample.set_editor_property("sampler_type",ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            bind(sample,"RGB",ue.MaterialProperty.MP_NORMAL)
        else:
            dye=edit.create_material_expression(material,ue.MaterialExpressionVectorParameter)
            dye.set_editor_property("parameter_name","BaseColorFactor")
            dye.set_editor_property("default_value",ue.LinearColor(1,1,1,1))
            product=edit.create_material_expression(material,ue.MaterialExpressionMultiply)
            connect(sample,"RGB",product,"A")
            connect(dye,"",product,"B")
            bind(product,"",ue.MaterialProperty.MP_BASE_COLOR)
    rough=edit.create_material_expression(material,ue.MaterialExpressionConstant)
    rough.set_editor_property("r",.82)
    bind(rough,"",ue.MaterialProperty.MP_ROUGHNESS)
    edit.layout_material_expressions(material)
    edit.recompile_material(material)
    if not library.save_loaded_asset(material,False): raise RuntimeError(path)
    return material


def make_twill_material():
    # Retain the tailored waistband, fly, pocket stitching and fold normals from the trouser UVs.
    edit=ue.MaterialEditingLibrary
    path=TARGET+"/M_ChinoTwill"
    mat=library.duplicate_asset(ROOT+"/Wardrobe/Everyday/M_EverydayFabric",path)
    if not mat:
        raise RuntimeError("Expected a clean staging folder for "+path)
    base=edit.get_material_property_input_node(mat,ue.MaterialProperty.MP_BASE_COLOR)
    weave=next(n for n in edit.get_inputs_for_material_expression(mat,base) if isinstance(n,ue.MaterialExpressionTextureSample))
    soften=edit.create_material_expression(mat,ue.MaterialExpressionLinearInterpolate)
    soften.set_editor_property("const_b",.78)
    soften.set_editor_property("const_alpha",.40)
    if not edit.connect_material_expressions(weave,"RGB",soften,"A") or not edit.connect_material_expressions(soften,"",base,"A"):
        raise RuntimeError("Could not tint the cotton twill")
    rough=edit.get_material_property_input_node(mat,ue.MaterialProperty.MP_ROUGHNESS)
    rough.set_editor_property("r",.86)
    edit.layout_material_expressions(mat)
    edit.recompile_material(mat)
    if not library.save_loaded_asset(mat,False): raise RuntimeError(path)


def import_outfit(name, reference_path):
    path = TARGET + "/" + name
    if library.does_asset_exist(path):
        raise RuntimeError("Import in a clean scratch project before promoting " + path)
    reference = library.load_asset(ROOT + "/Wardrobe/" + reference_path)
    if not isinstance(reference, ue.SkeletalMesh):
        raise RuntimeError("Missing original outfit " + reference_path)
    skeleton = reference.get_editor_property("skeleton")
    physics = reference.get_editor_property("physics_asset")
    if not skeleton or not physics:
        raise RuntimeError("The corresponding rig must include a skeleton and physics asset")

    pipeline = ue.InterchangeGenericAssetsPipeline()
    pipeline.common_skeletal_meshes_and_animations_properties.skeleton = skeleton
    pipeline.material_pipeline.import_materials = False
    pipeline.animation_pipeline.import_animations = False
    pipeline.mesh_pipeline.create_physics_asset = False
    params = ue.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines = [ue.SoftObjectPath(pipeline.get_path_name())]
    staging = TARGET + "/Import" + name
    if not manager.import_asset(staging, manager.create_source_data(str(SOURCE / (name + ".glb"))), params):
        raise RuntimeError("Interchange could not import " + name)
    imported = [p for p in library.list_assets(staging, recursive=True) if isinstance(library.load_asset(p), ue.SkeletalMesh)]
    if len(imported) != 1 or not library.rename_asset(imported[0], path):
        raise RuntimeError(f"Unexpected import result for {name}: {imported}")

    model = library.load_asset(path)
    source_materials = {str(slot.material_slot_name): slot.material_interface for slot in reference.materials}
    slots = list(model.materials)
    if LOWER:
        cotton = library.load_asset(ROOT+"/Wardrobe/M_ResidentCotton")
        leather=source_materials["shoe"]
        source_materials = dict.fromkeys(("leg_skin","bare_lower_skin","midriff_skin","foot_skin_l","foot_skin_r"),source_materials["skin"])
        source_materials.update(cotton_sock_l=cotton,cotton_sock_r=cotton,street_shoe_l=leather,street_shoe_r=leather)
        source_materials.update(underwear_knit=cotton,underwear_band=cotton)
    elif TEE:
        source_materials.pop("top_teal_woven")
        source_materials.update(jersey_knit=jersey_material,jersey_binding=jersey_material)
    elif FOUNDATION:
        source_materials = {k:v for k,v in source_materials.items() if k in ("skin","lips","hair","iris","sclera","bottom_denim","shoe")}
        if name == "SK_FemaleFoundation":
            cotton = library.load_asset(ROOT+"/Wardrobe/M_ResidentCotton")
            source_materials.update(foundation_knit=cotton, foundation_elastic=cotton)
    if {str(slot.material_slot_name) for slot in slots} != set(source_materials):
        raise RuntimeError(f"{name} has {[str(s.material_slot_name) for s in slots]}, expected {list(source_materials)}")
    face_path = ROOT + ("/Hero/M_HeroSkin" if name.startswith("SK_Male") else "/Hero/Female/M_FemaleSkin")
    face = library.load_asset(face_path)
    if not isinstance(face, ue.MaterialInterface):
        raise RuntimeError("Import the female skin before the skeletal meshes")
    if LOWER: source_materials.update(dict.fromkeys(("leg_skin","bare_lower_skin","midriff_skin","foot_skin_l","foot_skin_r"),face))
    else: source_materials.update(skin=face, lips=face)
    for slot in slots:
        parent = source_materials[str(slot.material_slot_name)]
        if not parent:
            raise RuntimeError(f"Missing native material on {name} {slot.material_slot_name}")
        slot.set_editor_property("material_interface", parent)
    model.set_editor_property("materials", slots)
    model.set_editor_property("physics_asset", physics)
    if model.get_editor_property("skeleton") != skeleton or model.get_editor_property("physics_asset") != physics:
        raise RuntimeError("Unexpected native rig on " + name)
    if not library.save_loaded_asset(model, False):
        raise RuntimeError("Could not save " + name)
    ue.log(f"SABARMATI RESIDENT VERIFIED {path} skeleton={skeleton.get_path_name()} physics={physics.get_path_name()} slots={len(slots)}")


if TEE:
    make_twill_material()
    jersey_material=make_jersey_material()
    import_outfit("SK_MaleCrewTee","Everyday/SK_RiverEveryday")
    import_outfit("SK_FemaleCrewTee","Female/SK_FemaleEveryday")
elif LOWER:
    import_outfit("SK_MaleLowerBody","Everyday/SK_RiverEveryday")
    import_outfit("SK_FemaleLowerBody","Female/SK_FemaleEveryday")
elif FOUNDATION:
    import_outfit("SK_MaleFoundation", "Everyday/SK_RiverEveryday")
    import_outfit("SK_FemaleFoundation", "Female/SK_FemaleEveryday")
else:
    import_outfit("SK_FemaleEveryday", "Everyday/SK_RiverEveryday")
    import_outfit("SK_FemaleKurta", "Tailored/SK_FittedKurta")
