"""Import the original district fleet with one shared material palette and three mesh LODs."""
import os
from pathlib import Path
import runpy
import unreal as ue

folder="/Game/Sabarmati/Riverfront/Vehicles/Fleet"; src=Path(os.environ["SABARMATI_FLEET_SOURCE"])
assets=ue.EditorAssetLibrary; tools=ue.AssetToolsHelpers.get_asset_tools(); lib=ue.MaterialEditingLibrary
recipe=runpy.run_path(str(Path(__file__).with_name("make-district-fleet.py")))
def asset(name,kind,factory):
    path=folder+"/"+name
    return assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,folder,kind,factory)
base=asset("M_TrafficFinish",ue.Material,ue.MaterialFactoryNew())
lib.delete_all_material_expressions(base)
for prop,param,default in (("BASE_COLOR","Tint",ue.LinearColor(.2,.2,.2,1)),("ROUGHNESS","Rough",.48),("METALLIC","Metal",.35)):
    expr=lib.create_material_expression(base,ue.MaterialExpressionVectorParameter if prop=="BASE_COLOR" else ue.MaterialExpressionScalarParameter,0,0)
    expr.set_editor_property("parameter_name",param);expr.set_editor_property("default_value",default)
    if not lib.connect_material_property(expr,"",getattr(ue.MaterialProperty,"MP_"+prop)):raise RuntimeError(prop)
lib.layout_material_expressions(base);lib.recompile_material(base);assets.save_loaded_asset(base,False)
surfaces={}
for key,tint,rough,metal in recipe["PALETTE"]:
    if key=="glass": surfaces[key]=assets.load_asset("/Game/Sabarmati/Riverfront/M_DistrictGlass");continue
    mat=asset("MI_Fleet_"+key,ue.MaterialInstanceConstant,ue.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mat,base)
    lib.set_material_instance_vector_parameter_value(mat,"Tint",ue.LinearColor(*tint,1))
    lib.set_material_instance_scalar_parameter_value(mat,"Rough",rough)
    lib.set_material_instance_scalar_parameter_value(mat,"Metal",metal)
    lib.update_material_instance(mat);assets.save_loaded_asset(mat,False);surfaces[key]=mat

manager=ue.InterchangeManager.get_interchange_manager_scripted()
pipeline=ue.InterchangeGenericAssetsPipeline()
pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials",False)
params=ue.ImportAssetParameters()
params.set_editor_property("is_automated",True)
params.set_editor_property("override_pipelines",[ue.SoftObjectPath(pipeline.get_path_name())])
for file in sorted(src.glob("SM_Traffic*.glb")):
    path=folder+"/"+file.stem
    if not assets.does_asset_exist(path):
        data=manager.create_source_data(str(file))
        if not manager.import_asset(folder,data,params): raise RuntimeError(file.stem)
    mesh=assets.load_asset(path)
    if not isinstance(mesh,ue.StaticMesh): raise RuntimeError(path)
    for i,slot in enumerate(mesh.get_editor_property("static_materials")):
        key=str(slot.get_editor_property("material_slot_name")).lower()
        if key not in surfaces: raise RuntimeError(f"{file.stem}: unknown surface {key}")
        mesh.set_material(i,surfaces[key])
    sub=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if not sub:raise RuntimeError("Use the full Unreal Editor with -ExecutePythonScript for mesh LODs.")
    spec=ue.StaticMeshReductionOptions(auto_compute_lod_screen_size=False,reduction_settings=[
        ue.StaticMeshReductionSettings(percent_triangles=f,screen_size=s) for f,s in ((1.,1.),(.65,.16),(.35,.055))])
    if sub.set_lods(mesh,spec)!=3:raise RuntimeError("LOD "+file.stem)
    if not assets.save_loaded_asset(mesh,False):raise RuntimeError(path)
ue.log("SABARMATI FLEET SAVED")
