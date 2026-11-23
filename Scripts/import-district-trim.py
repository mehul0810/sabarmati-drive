"""Import the original traffic rider and hatch trim with already-cooked public materials."""
import os
from pathlib import Path
import unreal as ue
src=Path(os.environ["SABARMATI_TRIM_SOURCE"])
assets=ue.EditorAssetLibrary;root="/Game/Sabarmati/Riverfront";tools=ue.AssetToolsHelpers.get_asset_tools()
items=(("SM_SeatedResident",root+"/People"),("SM_HatchTrim",root+"/Vehicles/Coachwork"))
if (src/"SM_PhoneResident.glb").is_file():
    items+=(("SM_PhoneResident",root+"/People"),)
for name,folder in items:
    path=folder+"/"+name
    if not assets.does_asset_exist(path):
        manager=ue.InterchangeManager.get_interchange_manager_scripted();pipeline=ue.InterchangeGenericAssetsPipeline()
        pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials",False)
        args=ue.ImportAssetParameters();args.set_editor_property("is_automated",True);args.set_editor_property("override_pipelines",[ue.SoftObjectPath(pipeline.get_path_name())])
        if not manager.import_asset(folder,manager.create_source_data(str(src/(name+".glb"))),args):raise RuntimeError(path)
    mesh=assets.load_asset(path)
    if not isinstance(mesh,ue.StaticMesh):raise RuntimeError(path)
    for idx,slot in enumerate(mesh.get_editor_property("static_materials")):
        key=str(slot.get_editor_property("material_slot_name")).lower()
        if key.startswith("phone_"):
            base=root+"/Vehicles/Fleet/MI_Fleet_"+("panel" if key=="phone_screen" else "iron")
        elif folder.endswith("/People"):
            base=root+"/People/SK_RiverResident/Materials/"+key
        else:
            base=root+"/Vehicles/Fleet/MI_Fleet_"+key
        mat=assets.load_asset(base)
        if not mat:raise RuntimeError(base)
        mesh.set_material(idx,mat)
    sub=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    opts=ue.StaticMeshReductionOptions(auto_compute_lod_screen_size=False,reduction_settings=[ue.StaticMeshReductionSettings(percent_triangles=f,screen_size=s) for f,s in ((1.,1.),(.55,.15),(.21,.05))])
    if sub.set_lods(mesh,opts)!=3:raise RuntimeError("LOD "+name)
    if not assets.save_loaded_asset(mesh,False):raise RuntimeError(path)
ue.log("SABARMATI RIDER AND COACHWORK TRIM SAVED")
