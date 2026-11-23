"""Build an offline, sky-only dome. The separate directional light supplies surface shadows."""
import os
from pathlib import Path
import unreal as ue
folder="/Game/Sabarmati/Riverfront"
source=Path(os.environ["SABARMATI_SKY_SOURCE"])
tools=ue.AssetToolsHelpers.get_asset_tools(); lib=ue.MaterialEditingLibrary; assets=ue.EditorAssetLibrary
name="M_DistrictSky"
mat=assets.load_asset(folder+"/"+name) if assets.does_asset_exist(folder+"/"+name) else tools.create_asset(name,folder,ue.Material,ue.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
for k,v in (("two_sided",True),("is_sky",True),("shading_model",ue.MaterialShadingModel.MSM_UNLIT)):
    mat.set_editor_property(k,v)
def node(kind,**props):
    n=lib.create_material_expression(mat,getattr(ue,"MaterialExpression"+kind),0,0)
    for k,v in props.items(): n.set_editor_property(k,v)
    return n
def link(a,b,pin,out=""):
    if not lib.connect_material_expressions(a,out,b,pin): raise RuntimeError(pin)
def parameter(name,value):
    return node("ScalarParameter",parameter_name=name,default_value=value)
uv=node("Custom",code="float3 d=normalize(-V); return float2(frac((atan2(d.y,d.x)-Az)/6.2831853+.5), clamp(acos(clamp(d.z,-1.,1.))/3.14159265, .002,.495));",output_type=ue.CustomMaterialOutputType.CMOT_FLOAT2)
pins=[]
for key in ("V","Az"):
    p=ue.CustomInput();p.set_editor_property("input_name",key);pins.append(p)
uv.set_editor_property("inputs",pins)
link(node("CameraVectorWS"),uv,"V");link(parameter("SolarAzimuth",0.),uv,"Az")
layers=[]
for texname in ("T_SabarmatiEvening","T_SabarmatiClouds"):
    task=ue.AssetImportTask()
    for k,v in (("filename",str(source/(texname+".png"))),("destination_path",folder+"/Textures"),("destination_name",texname),("automated",True),("replace_existing",True),("save",True)):
        task.set_editor_property(k,v)
    tools.import_asset_tasks([task])
    tex=assets.load_asset(folder+"/Textures/"+texname)
    if not isinstance(tex,ue.Texture2D): raise RuntimeError(texname)
    n=node("TextureSample",texture=tex); link(uv,n,"UVs"); layers.append(n)
mix=node("LinearInterpolate"); link(layers[0],mix,"A","RGB"); link(layers[1],mix,"B","RGB"); link(parameter("DayMix",0.),mix,"Alpha")
tint=node("VectorParameter",parameter_name="SkyTint",default_value=ue.LinearColor(1,.94,.88,1))
final=node("Multiply"); link(mix,final,"A"); link(tint,final,"B")
if not lib.connect_material_property(final,"",ue.MaterialProperty.MP_EMISSIVE_COLOR): raise RuntimeError("Sky output")
lib.layout_material_expressions(mat); lib.recompile_material(mat)
if not assets.save_loaded_asset(mat,False): raise RuntimeError(name)
ue.log("SABARMATI PHOTO SKIES SAVED")
