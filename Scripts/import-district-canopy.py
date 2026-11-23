"""Import the optimized CC0 tree with shared scanning maps, card opacity, wind and three LODs."""
import os
from pathlib import Path
import unreal as ue

folder="/Game/Sabarmati/Riverfront/Foliage/Jacaranda"
source=Path(os.environ["SABARMATI_CANOPY_SOURCE"])
images=Path(os.environ["SABARMATI_CANOPY_TEXTURES"])
assets=ue.EditorAssetLibrary;edit=ue.MaterialEditingLibrary;tools=ue.AssetToolsHelpers.get_asset_tools()
def get(name):return assets.load_asset(folder+"/"+name)
def tex(file,name,category):
    if not assets.does_asset_exist(folder+"/"+name):
        task=ue.AssetImportTask();task.set_editor_property("filename",str(file));task.set_editor_property("destination_path",folder)
        task.set_editor_property("destination_name",name);task.set_editor_property("automated",True);task.set_editor_property("save",True)
        tools.import_asset_tasks([task])
    t=get(name)
    if not isinstance(t,ue.Texture2D):raise ValueError(name)
    if category!="Color":
        t.set_editor_property("srgb",False)
        t.set_editor_property("compression_settings",ue.TextureCompressionSettings.TC_NORMALMAP if category=="Normal" else ue.TextureCompressionSettings.TC_MASKS)
        if category=="Normal":t.set_editor_property("flip_green_channel",True)
        assets.save_loaded_asset(t,False)
    return t
def node(mat,cls,**props):
    n=edit.create_material_expression(mat,getattr(ue,"MaterialExpression"+cls),0,0)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def prop(expr,channel,rough,mat):
    if not edit.connect_material_property(expr,channel,getattr(ue.MaterialProperty,"MP_"+rough)):raise RuntimeError(mat.get_name()+rough)
def link(a,b,name,out=""):
    if not edit.connect_material_expressions(a,out,b,name):raise RuntimeError(name)
surface={}
for part in ("branches","trunk","leaves"):
    name="M_Jacaranda"+part.title()
    m=get(name) if assets.does_asset_exist(folder+"/"+name) else tools.create_asset(name,folder,ue.Material,ue.MaterialFactoryNew())
    edit.delete_all_material_expressions(m)
    m.set_editor_property("used_with_instanced_static_meshes",True)
    if part=="leaves":
        m.set_editor_property("two_sided",True);m.set_editor_property("blend_mode",ue.BlendMode.BLEND_MASKED)
        m.set_editor_property("opacity_mask_clip_value",.32);m.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    for kind,tag,sampler,channel,property_name in (("diff","Color","COLOR","RGB","BASE_COLOR"),("nor_gl","Normal","NORMAL","RGB","NORMAL"),("arm","Surface","MASKS","G","ROUGHNESS")):
        t=tex(images/f"jacaranda_tree_{part}_{kind}_1k.jpg",f"T_Jacaranda_{part}_{tag}",tag)
        n=node(m,"TextureSample",texture=t,sampler_type=getattr(ue.MaterialSamplerType,"SAMPLERTYPE_"+sampler))
        prop(n,channel,property_name,m)
        if tag=="Surface":prop(n,"R","AMBIENT_OCCLUSION",m)
        if tag=="Color" and part=="leaves":
            tint=node(m,"Multiply");link(n,tint,"A","RGB");tint.set_editor_property("const_b",.37);prop(tint,"","SUBSURFACE_COLOR",m)
    if part=="leaves":
        alpha=tex(images/"leaf_alpha.png","T_Jacaranda_LeafMask","Surface")
        mask=node(m,"TextureSample",texture=alpha,sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_MASKS);prop(mask,"R","OPACITY_MASK",m)
        wind=node(m,"Custom",code="float f=sin(T*1.7+P.x*.004+P.y*.006);float h=saturate((P.z-250.)/470.);return float3((f+.35*sin(T*3.+P.y*.025))*h*W,f*h*W*.53,0.);",output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3)
        pins=[]
        for key in ("T","P","W"):
            pin=ue.CustomInput();pin.set_editor_property("input_name",key);pins.append(pin)
        wind.set_editor_property("inputs",pins)
        link(node(m,"Time"),wind,"T");link(node(m,"WorldPosition"),wind,"P")
        link(node(m,"ScalarParameter",parameter_name="WindCm",default_value=2.8),wind,"W");prop(wind,"","WORLD_POSITION_OFFSET",m)
    edit.layout_material_expressions(m);edit.recompile_material(m)
    if not assets.save_loaded_asset(m,False):raise RuntimeError(name)
    surface["jacaranda_tree_"+part]=m
path=folder+"/SM_Jacaranda"
if not assets.does_asset_exist(path):
    manager=ue.InterchangeManager.get_interchange_manager_scripted(); pipeline=ue.InterchangeGenericAssetsPipeline()
    pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials",False)
    params=ue.ImportAssetParameters();params.set_editor_property("is_automated",True);params.set_editor_property("override_pipelines",[ue.SoftObjectPath(pipeline.get_path_name())])
    if not manager.import_asset(folder,manager.create_source_data(str(source/"SM_Jacaranda.gltf")),params):raise RuntimeError(path)
mesh=assets.load_asset(path)
if not isinstance(mesh,ue.StaticMesh):raise ValueError(path)
for i,slot in enumerate(mesh.get_editor_property("static_materials")):
    mesh.set_material(i,surface[str(slot.get_editor_property("material_slot_name")).lower()])
sub=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
opts=ue.StaticMeshReductionOptions(auto_compute_lod_screen_size=False,reduction_settings=[
    ue.StaticMeshReductionSettings(percent_triangles=f,screen_size=s) for f,s in ((1.,1.),(.40,.25),(.14,.095))])
if sub.set_lods(mesh,opts)!=3:raise RuntimeError("canopy LODs")
if not assets.save_loaded_asset(mesh,False):raise RuntimeError(path)
ue.log("SABARMATI PHOTOGRAPHIC CANOPY SAVED")
