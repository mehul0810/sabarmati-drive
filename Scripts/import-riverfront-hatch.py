"""Import local, pivot-safe vehicle GLBs into the existing native game."""
import os
from pathlib import Path
import unreal as ue

source = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())) / "Saved/LookSources/hatch"
folder = "/Game/Sabarmati/Riverfront/Vehicles"
default = ("SM_SabarmatiHatch", "SM_HatchWheelL", "SM_HatchWheelR")
names = ("SM_SabarmatiCrossover",) if os.environ.get("SABARMATI_IMPORT_CROSSOVER") == "1" else default
for name in names:
    asset_path = f"{folder}/{name}"
    if ue.EditorAssetLibrary.does_asset_exist(asset_path):
        mesh = ue.EditorAssetLibrary.load_asset(asset_path)
    else:
        task = ue.AssetImportTask()
        for key, value in {"filename":str(source / f"{name}.glb"),
                           "destination_path":folder, "destination_name":name,
                           "automated":True, "save":True}.items():
            task.set_editor_property(key, value)
        ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        candidates = [x for x in task.get_objects() if isinstance(x,ue.StaticMesh)]
        if len(candidates) != 1:
            raise RuntimeError(f"Expected one mesh for {name}, found {len(candidates)}")
        mesh = candidates[0]
        if mesh.get_path_name().split(".")[0] != asset_path:
            if not ue.EditorAssetLibrary.rename_asset(mesh.get_path_name(),asset_path):
                raise RuntimeError(f"Cannot rename {mesh.get_path_name()} to {asset_path}")
            mesh = ue.EditorAssetLibrary.load_asset(asset_path)
    if not mesh or not ue.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {asset_path}")
    ue.log(f"SABARMATI HATCH ASSET: {mesh.get_path_name()} {mesh.get_bounds()}")
