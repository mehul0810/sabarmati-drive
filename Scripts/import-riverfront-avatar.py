"""Import the project's fictional source mesh and native animation clips."""
from pathlib import Path
import unreal as ue

root = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
dest = "/Game/Sabarmati/Avatar"
task = ue.AssetImportTask()
task.set_editor_property("filename", str(root / "Saved/LookSources/SK_Sabarmati.glb"))
task.set_editor_property("destination_path", dest)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
objects = task.get_objects()
for obj in objects:
    ue.log(f"AVATAR IMPORTED {obj.get_class().get_name()} {obj.get_path_name()}")
meshes = [o for o in objects if isinstance(o, ue.SkeletalMesh)]
anims = [o for o in objects if isinstance(o, ue.AnimSequence)]
if len(meshes) != 1 or len(anims) < 3:
    raise RuntimeError(f"Expected a single skeletal mesh and at least three animations: saw {len(meshes)}, {len(anims)}")

for obj, name in [(meshes[0], "SK_Sabarmati")] + [
    (o, next(n for n in ("A_WalkAlt", "A_Walk", "A_Idle", "A_Jog") if n in o.get_name())) for o in anims
]:
    path = dest + "/" + name
    if obj.get_path_name().split(".")[0] != path and not ue.EditorAssetLibrary.rename_asset(obj.get_path_name(), path):
        raise RuntimeError(f"Cannot move {obj.get_path_name()} to {path}")
    loaded = ue.EditorAssetLibrary.load_asset(path)
    if not loaded or not ue.EditorAssetLibrary.save_loaded_asset(loaded, only_if_is_dirty=False):
        raise RuntimeError(path)
    ue.log(f"AVATAR SAVED {path}")
