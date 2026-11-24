"""Import the fitted female resident meshes against the shipped outfit rigs in a scratch project."""
import os
from pathlib import Path
import unreal as ue

ROOT = "/Game/Sabarmati/Riverfront/People"
TARGET = ROOT + "/Wardrobe/Female"
SOURCE = Path(os.environ["SABARMATI_FEMALE_SOURCE"])
library = ue.EditorAssetLibrary
manager = ue.InterchangeManager.get_interchange_manager_scripted()


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
    if {str(slot.material_slot_name) for slot in slots} != set(source_materials):
        raise RuntimeError(f"Material slots do not match {reference_path}")
    face_path = ROOT + "/Hero/Female/M_FemaleSkin"
    face = library.load_asset(face_path)
    if not isinstance(face, ue.MaterialInterface):
        raise RuntimeError("Import the female skin before the skeletal meshes")
    source_materials.update(skin=face, lips=face)
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
    ue.log(f"SABARMATI FEMALE VERIFIED {path} skeleton={skeleton.get_path_name()} physics={physics.get_path_name()} slots={len(slots)}")


import_outfit("SK_FemaleEveryday", "Everyday/SK_RiverEveryday")
import_outfit("SK_FemaleKurta", "Tailored/SK_FittedKurta")
