"""Bake the empty-room capture into two view-dependent window finishes for day and night."""
import os
from pathlib import Path
import unreal as ue
root="/Game/Sabarmati/Riverfront"
ed=ue.MaterialEditingLibrary; assets=ue.EditorAssetLibrary; tools=ue.AssetToolsHelpers.get_asset_tools()
src=Path(os.environ["SABARMATI_ROOM_SOURCE"])
tpath=root+"/Textures/T_RiverRoom"
if not assets.does_asset_exist(tpath):
    task=ue.AssetImportTask();task.set_editor_property("filename",str(src));task.set_editor_property("destination_path",root+"/Textures")
    task.set_editor_property("destination_name","T_RiverRoom");task.set_editor_property("automated",True);task.set_editor_property("save",True)
    tools.import_asset_tasks([task])
texture=assets.load_asset(tpath)
if not isinstance(texture,ue.Texture2D):raise RuntimeError(tpath)
def material(name,kind,factory):
    return assets.load_asset(root+"/"+name) if assets.does_asset_exist(root+"/"+name) else tools.create_asset(name,root,kind,factory)
m=material("M_DistrictWindow",ue.Material,ue.MaterialFactoryNew());ed.delete_all_material_expressions(m)
m.set_editor_property("used_with_instanced_static_meshes",True)
def node(kind,**props):
    n=ed.create_material_expression(m,getattr(ue,"MaterialExpression"+kind),0,0)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def link(a,b,k,out=""):
    if not ed.connect_material_expressions(a,out,b,k):raise RuntimeError(k)
def prop(n,name):
    if not ed.connect_material_property(n,"",getattr(ue.MaterialProperty,"MP_"+name)):raise RuntimeError(name)
def custom(code,out,inputs):
    n=node("Custom",code=code,output_type=getattr(ue.CustomMaterialOutputType,"CMOT_"+out))
    pins=[]
    for k,a in inputs.items():
        p=ue.CustomInput();p.set_editor_property("input_name",k);pins.append(p)
    n.set_editor_property("inputs",pins)
    for k,a in inputs.items():link(a,n,k)
    return n
uv=node("TextureCoordinate");seed=node("PerInstanceRandom");v=node("CameraVectorWS");normal=node("PixelNormalWS")
proj=custom("""
float3 tangent=normalize(float3(-N.y,N.x,0.)+1.e-5);
float3 eye=float3(-dot(V,tangent),-V.z,max(.12,abs(dot(V,N))));
float3 origin=float3((UV.x-.5)*2.,(.5-UV.y)*1.44,-1.);
float3 inv=1./(eye+1.e-5);
float3 a=(-float3(1.,.72,1.)-origin)*inv, b=(float3(1.,.72,1.)-origin)*inv;
float3 t=max(a,b);float3 p=normalize(origin+eye*max(.001,min(t.x,min(t.y,t.z))));
return float2(frac(.5+atan2(p.x,p.z)/6.2831853+floor(S*4.)*.25),acos(clamp(p.y,-1.,1.))/3.14159265);
""","FLOAT2",{"UV":uv,"V":v,"N":normal,"S":seed})
photo=node("TextureSample",texture=texture);link(proj,photo,"UVs")
night=node("ScalarParameter",parameter_name="Night",default_value=0.)
surface=custom("""
float angle=pow(1.-abs(dot(V,N)),3.);
float3 glass=lerp(float3(.021,.034,.041),float3(.13,.18,.21),angle);
return glass+Pic*.19*lerp(1.,.32,Night);
""","FLOAT3",{"V":v,"N":normal,"Pic":photo,"Night":night})
prop(surface,"BASE_COLOR");prop(node("Constant",r=.17),"ROUGHNESS");prop(node("Constant",r=.52),"SPECULAR")
lamp=custom("""
float occupied=smoothstep(.38,.40,Seed);
float lamp=(.34+.50*frac(Seed*7.))*occupied;
float3 hue=lerp(float3(.95,.57,.29),float3(.83,.83,.65),frac(Seed*17.));
float blind=Seed<.24 && UV.y<.40?(.32+.22*smoothstep(.22,.46,frac(UV.y*28.))):1.;
return Pic*lerp(.048,lamp+.003,Night)*lerp(float3(.84,.89,.90),hue,Night)*blind;
""","FLOAT3",{"UV":uv,"Seed":seed,"Pic":photo,"Night":night})
prop(lamp,"EMISSIVE_COLOR");ed.layout_material_expressions(m);ed.recompile_material(m);assets.save_loaded_asset(m,False)
mi=material("MI_DistrictWindowNight",ue.MaterialInstanceConstant,ue.MaterialInstanceConstantFactoryNew())
ed.set_material_instance_parent(mi,m);ed.set_material_instance_scalar_parameter_value(mi,"Night",1.);ed.update_material_instance(mi);assets.save_loaded_asset(mi,False)
ue.log("SABARMATI PROJECTED WINDOWS SAVED")
