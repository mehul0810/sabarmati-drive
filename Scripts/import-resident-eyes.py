"""Update the resident's eye chart while preserving the existing eye material and UVs."""
import os
from pathlib import Path
import unreal as ue

source = Path(os.environ["SABARMATI_RESIDENT_HAIR_SOURCE"]) / "T_RiverEyes.png"
folder = "/Game/Sabarmati/Riverfront/People/Wardrobe"
task = ue.AssetImportTask()
task.set_editor_property("filename", str(source))
task.set_editor_property("destination_path", folder)
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex = ue.EditorAssetLibrary.load_asset(folder + "/T_RiverEyes")
if not isinstance(tex, ue.Texture2D) or tex.blueprint_get_size_x() != 2048 or tex.blueprint_get_size_y() != 2048:
    raise RuntimeError("Expected the 2048 x 2048 resident eye chart")
ue.log("RESIDENT BROWN EYE CHART VERIFIED " + tex.get_path_name())
