"""Import the openable CC0 hatch components and match their material slots to the existing car."""
from pathlib import Path
import os
import unreal as ue

folder="/Game/Sabarmati/Riverfront/Vehicles"
source=Path(os.environ["SABARMATI_DOOR_SOURCE"])
assets=ue.EditorAssetLibrary
original=assets.load_asset(folder+"/SM_SabarmatiHatch")
slots={str(s.get_editor_property("material_slot_name")).lower():s.get_editor_property("material_interface") for s in original.get_editor_property("static_materials")}
for name in ("SM_HatchCabin","SM_HatchDoorL","SM_HatchDoorR"):
    path=folder+"/Coachwork/"+name
    if assets.does_asset_exist(path): mesh=assets.load_asset(path)
    else:
        task=ue.AssetImportTask()
        for key,value in (("filename",str(source/(name+".glb"))),("destination_name",name),("destination_path",folder+"/Coachwork"),("automated",True),("save",True)):
            task.set_editor_property(key,value)
        ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        choices=[a for a in task.get_objects() if isinstance(a,ue.StaticMesh)]
        if len(choices)!=1: raise RuntimeError(name)
        mesh=choices[0]
        if mesh.get_path_name().split(".")[0]!=path:
            if not assets.rename_asset(mesh.get_path_name(),path): raise RuntimeError(path)
            mesh=assets.load_asset(path)
    for i,s in enumerate(mesh.get_editor_property("static_materials")):
        key=str(s.get_editor_property("material_slot_name")).lower()
        if key in slots: mesh.set_material(i,slots[key])
    sub=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if sub:
        opts=ue.StaticMeshReductionOptions(auto_compute_lod_screen_size=False,
            reduction_settings=[ue.StaticMeshReductionSettings(percent_triangles=f,screen_size=s)
                for f,s in ((1.,1.),(.55,.24),(.24,.095))])
        if sub.set_lods(mesh,opts)!=3: raise RuntimeError("LOD "+name)
    if not assets.save_loaded_asset(mesh,False): raise RuntimeError(name)
ue.log("SABARMATI HINGED COACHWORK SAVED")
