"""Import the openable CC0 hatch components and match their material slots to the existing car."""
from pathlib import Path
import os
import unreal as ue

folder="/Game/Sabarmati/Riverfront/Vehicles"
source=Path(os.environ["SABARMATI_DOOR_SOURCE"])
assets=ue.EditorAssetLibrary
original=assets.load_asset(folder+"/SM_SabarmatiHatch")
slots={str(s.get_editor_property("material_slot_name")).lower():s.get_editor_property("material_interface") for s in original.get_editor_property("static_materials")}
for name in ("SM_HatchCabin","SM_HatchDoorL","SM_HatchDoorR","SM_HatchCockpit","SM_HatchSteering"):
    if not (source/(name+".glb")).is_file():
        continue
    path=folder+"/Coachwork/"+name
    if assets.does_asset_exist(path): mesh=assets.load_asset(path)
    else:
        manager=ue.InterchangeManager.get_interchange_manager_scripted()
        pipeline=ue.InterchangeGenericAssetsPipeline()
        pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials",False)
        args=ue.ImportAssetParameters();args.set_editor_property("is_automated",True)
        args.set_editor_property("override_pipelines",[ue.SoftObjectPath(pipeline.get_path_name())])
        if not manager.import_asset(folder+"/Coachwork",manager.create_source_data(str(source/(name+".glb"))),args):
            raise RuntimeError(name)
        mesh=assets.load_asset(path)
        if not isinstance(mesh,ue.StaticMesh):raise RuntimeError(path)
    for i,s in enumerate(mesh.get_editor_property("static_materials")):
        key=str(s.get_editor_property("material_slot_name")).lower()
        if key in slots: mesh.set_material(i,slots[key])
    sub=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if not sub:raise RuntimeError("Use the full Unreal Editor with -ExecutePythonScript for mesh LODs.")
    opts=ue.StaticMeshReductionOptions(auto_compute_lod_screen_size=False,
        reduction_settings=[ue.StaticMeshReductionSettings(percent_triangles=f,screen_size=s)
            for f,s in ((1.,1.),(.55,.24),(.24,.095))])
    if sub.set_lods(mesh,opts)!=3: raise RuntimeError("LOD "+name)
    if not assets.save_loaded_asset(mesh,False): raise RuntimeError(name)
ue.log("SABARMATI HINGED COACHWORK SAVED")
