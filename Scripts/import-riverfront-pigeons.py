"""Import the CC BY pigeon from SABARMATI_PIGEON_SOURCE with Unreal Interchange."""
import os
from pathlib import Path

import unreal as ue


FOLDER = "/Game/Sabarmati/Riverfront/Wildlife/Pigeon"
CLIPS = ("Idle", "Peck", "Fly", "Glide")


def import_pigeon():
    source = Path(os.environ["SABARMATI_PIGEON_SOURCE"]).resolve(strict=True)
    assets = ue.EditorAssetLibrary
    skeletals = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    if assets.does_directory_exist(FOLDER) and assets.list_assets(FOLDER):
        raise RuntimeError("Import into a clean pigeon asset folder")

    task = ue.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("automated", True)
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_objects()
    for obj in imported:
        ue.log(f"SABARMATI PIGEON IMPORT {obj.get_class().get_name()} {obj.get_path_name()}")
    meshes = [obj for obj in imported if isinstance(obj, ue.SkeletalMesh)]
    if len(meshes) != 1:
        raise RuntimeError(f"Expected one skeletal mesh, got {len(meshes)}")
    mesh = meshes[0]
    for clip in CLIPS:
        matches = [obj for obj in imported if isinstance(obj, ue.AnimSequence)
                   and obj.get_name().lower().removeprefix(source.stem.lower()).lstrip("_") == clip.lower()]
        if len(matches) != 1 or matches[0].get_editor_property("skeleton") != mesh.get_editor_property("skeleton"):
            raise RuntimeError(f"Missing compatible pigeon animation: {clip}")
        if not assets.rename_loaded_asset(matches[0], f"{FOLDER}/A_Pigeon{clip}"):
            raise RuntimeError(f"Could not rename pigeon animation: {clip}")

    if not assets.rename_loaded_asset(mesh, f"{FOLDER}/SK_Pigeon"):
        raise RuntimeError("Could not name the imported pigeon mesh")
    physics = mesh.get_editor_property("physics_asset")
    skeletals.assign_physics_asset(mesh, None)
    for obj in imported:
        if obj == physics or (isinstance(obj, ue.AnimSequence) and not obj.get_name().startswith("A_Pigeon")):
            assets.delete_loaded_asset(obj)

    if not skeletals.regenerate_lod(mesh, new_lod_count=3):
        raise RuntimeError("Could not generate the three pigeon mesh levels")
    counts = [skeletals.get_num_verts(mesh, level) for level in range(3)]
    if not counts[0] > counts[1] > counts[2] > 0:
        raise RuntimeError(f"Ineffective pigeon mesh reductions: {counts}")
    if not assets.save_directory(FOLDER, only_if_is_dirty=False):
        raise RuntimeError("Could not save pigeon assets")
    ue.log(f"SABARMATI PIGEON READY vertices_by_lod={counts}")


import_pigeon()
