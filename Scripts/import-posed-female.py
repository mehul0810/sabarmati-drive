"""Import the female bench, phone and driver poses after baking the female everyday GLB."""
import os
from pathlib import Path
import unreal as ue

source = Path(os.environ["SABARMATI_POSE_SOURCE"])
people = "/Game/Sabarmati/Riverfront/People"
folder = people + "/Wardrobe/Female/Poses"
assets = ue.EditorAssetLibrary
editing = ue.MaterialEditingLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
instancing = ue.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
fabric = people + "/Wardrobe/Everyday/M_EverydayFabric"
leather = people + "/Wardrobe/Everyday/M_EverydayLeather"


def surface(label, parent_path, tint=None):
    path = folder + "/MI_Posed" + label
    material = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        "MI_Posed" + label, folder, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
    parent = assets.load_asset(parent_path)
    if not parent or not material:
        raise RuntimeError(path + " / " + parent_path)
    editing.set_material_instance_parent(material, parent)
    if tint:
        editing.set_material_instance_vector_parameter_value(material, "BaseColorFactor", ue.LinearColor(*tint))
    editing.set_material_usage_override(material, instancing, True, True)
    editing.update_material_instance(material)
    if not assets.save_loaded_asset(material, False) or not editing.has_material_usage(material, instancing):
        raise RuntimeError("Could not enable instancing for " + path)
    return material


skin = surface("FemaleSkin", people + "/Hero/Female/M_FemaleSkin")
eyes = surface("Eyes", people + "/Wardrobe/M_RiverEyes")
materials = {
    "skin": skin, "lips": skin, "iris": eyes, "sclera": eyes,
    "top_teal_woven": surface("SageCotton", fabric, (.33, .42, .30, 1)),
    "bottom_denim": surface("DarkDenim", fabric, (.08, .12, .17, 1)),
    "shoe": surface("Leather", leather, (.30, .15, .07, 1)),
}
phone_source = assets.load_asset(people + "/SM_PhoneResident")
for slot in phone_source.get_editor_property("static_materials"):
    name = str(slot.material_slot_name)
    if name in {"hair", "phone_case", "phone_screen"}:
        materials[name] = slot.material_interface

for name in ("SM_FemaleSeatedResident", "SM_FemalePhoneResident"):
    path = folder + "/" + name
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    pipeline = ue.InterchangeGenericAssetsPipeline()
    pipeline.material_pipeline.import_materials = False
    params = ue.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines = [ue.SoftObjectPath(pipeline.get_path_name())]
    if not manager.import_asset(folder, manager.create_source_data(str(source / (name + ".glb"))), params):
        raise RuntimeError("Interchange failed for " + name)
    model = assets.load_asset(path)
    if not isinstance(model, ue.StaticMesh):
        raise RuntimeError(path)
    slots = model.get_editor_property("static_materials")
    for index, slot in enumerate(slots):
        parent = materials[str(slot.material_slot_name)]
        if not parent:
            raise RuntimeError(f"{name}: missing surface {slot.material_slot_name}")
        model.set_material(index, parent)
    if not assets.save_loaded_asset(model, False):
        raise RuntimeError(path)
    ue.log(f"SABARMATI FEMALE POSE VERIFIED {name}: {len(slots)} surfaces")
