"""Import both redistributable clothed residents and their independent walk, idle and jog clips."""
import os
from pathlib import Path
import unreal as ue

src=Path(os.environ["SABARMATI_PEOPLE_SOURCE"]);library=ue.EditorAssetLibrary
root="/Game/Sabarmati/Riverfront/People"
for name,dest in (("SK_RiverResident",root),("SK_RiverResidentKurta",root+"/Kurta")):
    if library.does_asset_exist(dest+"/"+name):
        for key in (name,"A_Walk","A_WalkAlt","A_Idle","A_Jog"):
            if not library.does_asset_exist(dest+"/"+key):raise RuntimeError(dest+"/"+key)
        continue
    task=ue.AssetImportTask()
    task.set_editor_property("filename",str(src/(name+".glb")))
    task.set_editor_property("destination_path",dest)
    task.set_editor_property("automated",True);task.set_editor_property("save",True)
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    objects=task.get_objects()
    models=[o for o in objects if isinstance(o,ue.SkeletalMesh)]
    clips=[o for o in objects if isinstance(o,ue.AnimSequence)]
    if len(models)!=1 or len(clips)!=4:raise RuntimeError(f"{name}: {len(models)} meshes/{len(clips)} clips")
    for obj,key in [(models[0],name)]+[(o,next(k for k in ("A_WalkAlt","A_Walk","A_Idle","A_Jog") if k in o.get_name())) for o in clips]:
        path=dest+"/"+key
        if path!=obj.get_path_name().split(".")[0] and not library.rename_asset(obj.get_path_name(),path):raise RuntimeError(path)
        if not library.save_asset(path,False):raise RuntimeError(path)
# Seated passengers, benches and phone users share these nine original
# material instances through UE's instanced static mesh components.
instanced = ue.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
editing = ue.MaterialEditingLibrary
for surface in ("bottom_denim", "hair", "iris", "lips", "sclera", "shoe", "skin", "top_seams", "top_teal_woven"):
    path = root + "/SK_RiverResident/Materials/" + surface
    material = library.load_asset(path)
    if not isinstance(material, ue.MaterialInstanceConstant):
        raise RuntimeError(path)
    if not (editing.has_material_usage_override(material, instanced) and editing.has_material_usage(material, instanced)):
        editing.set_material_usage_override(material, instanced, True, True)
        editing.update_material_instance(material)
        if not library.save_loaded_asset(material, False):
            raise RuntimeError("Could not save instancing support for " + path)
    if not (editing.has_material_usage_override(material, instanced) and editing.has_material_usage(material, instanced)):
        raise RuntimeError("Missing instancing support on " + path)
    ue.log("SABARMATI INSTANCE SURFACE VERIFIED " + path)
ue.log("SABARMATI PUBLIC RESIDENTS SAVED")
