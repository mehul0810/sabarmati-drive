"""Create a rain-responsive copy of the credited asphalt, preserving the dry graph at zero wetness."""
from pathlib import Path
import unreal as ue

root="/Game/Sabarmati/Riverfront"
assets=ue.EditorAssetLibrary
lib=ue.MaterialEditingLibrary
source=root+"/M_Asphalt"
dest=root+"/M_DistrictRoad"
puddle_code="""
float2 p=P.xy/225.;
float2 i=floor(p), f=frac(p); f=f*f*(3.-2.*f);
// Hash the same integer grid corner identically from either neighbouring cell.
// Adding floating-point offsets after the dot product made shared corners
// disagree at distant world coordinates, leaving rectangular wetness seams.
int2 cell=(int2)i;
uint4 x=uint(cell.x)+uint4(0u,1u,0u,1u);
uint4 y=uint(cell.y)+uint4(0u,0u,1u,1u);
uint4 bits=(x*0x9e3779b9u)^(y*0x85ebca6bu);
bits^=bits>>16; bits*=0x7feb352du; bits^=bits>>15;
bits*=0x846ca68bu; bits^=bits>>16;
float4 h=float4(bits & 0x00ffffffu)*(1.0/16777215.0);
float n=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);
// Damp asphalt keeps some aggregate texture; shallow, softly edged patches
// become smoother without making whole sections mirror-flat.
float pool=smoothstep(.40,.78,n+.045*sin(P.x*.021+P.y*.009));
return saturate(W)*(.28+.44*pool);
"""
if assets.does_asset_exist(dest):
    mat=assets.load_asset(dest)
else:
    mat=assets.duplicate_asset(source,dest)
if not isinstance(mat,ue.Material): raise RuntimeError(dest)
already=any(isinstance(n,ue.MaterialExpressionScalarParameter) and str(n.get_editor_property("parameter_name"))=="Wetness" for n in lib.get_material_expressions(mat))
if not already:
    def new(kind,**props):
        e=lib.create_material_expression(mat,getattr(ue,"MaterialExpression"+kind),0,0)
        for k,v in props.items(): e.set_editor_property(k,v)
        return e
    def connect(a,b,pin,out=""):
        if not lib.connect_material_expressions(a,out,b,pin): raise RuntimeError(pin)
    wet=new("ScalarParameter",parameter_name="Wetness",default_value=0.)
    pos=new("WorldPosition")
    puddle=new("Custom",output_type=ue.CustomMaterialOutputType.CMOT_FLOAT1,code=puddle_code)
    pins=[]
    for name in ("P","W"):
        v=ue.CustomInput(); v.set_editor_property("input_name",name); pins.append(v)
    puddle.set_editor_property("inputs",pins); connect(pos,puddle,"P"); connect(wet,puddle,"W")
    for prop_name in ("BASE_COLOR","ROUGHNESS","NORMAL"):
        prop=getattr(ue.MaterialProperty,"MP_"+prop_name)
        old=lib.get_material_property_input_node(mat,prop)
        if not old: raise RuntimeError("Dry asphalt has no "+prop_name)
        out=lib.get_material_property_input_node_output_name(mat,prop)
        blend=new("LinearInterpolate")
        if prop_name=="BASE_COLOR":
            dark=new("Multiply",const_b=.43); connect(old,dark,"A",out); target=dark
        elif prop_name=="ROUGHNESS": target=new("Constant",r=.085)
        else: target=new("Constant3Vector",constant=ue.LinearColor(0,0,1,1))
        connect(old,blend,"A",out); connect(target,blend,"B"); connect(puddle,blend,"Alpha")
        if not lib.connect_material_property(blend,"",prop): raise RuntimeError(prop_name)
    mat.set_editor_property("used_with_instanced_static_meshes",True)
    lib.layout_material_expressions(mat); lib.recompile_material(mat)
    if not assets.save_loaded_asset(mat,False): raise RuntimeError(dest)
# Update the rain mask on existing materials too; rerunning this script should
# preserve the material graph and change only the moisture response.
puddles=[n for n in lib.get_material_expressions(mat)
         if isinstance(n,ue.MaterialExpressionCustom) and
         [str(pin.get_editor_property("input_name")) for pin in n.get_editor_property("inputs")]==["P","W"]]
if len(puddles)!=1: raise RuntimeError("Expected one wetness mask in "+dest)
if puddles[0].get_editor_property("code")!=puddle_code:
    puddles[0].set_editor_property("code",puddle_code)
    lib.recompile_material(mat)
    if not assets.save_loaded_asset(mat,False): raise RuntimeError(dest)
# Tar-filled transverse cuts, eroded edges and small aggregate variation survive close ground-level views.
# The pattern is filtered as it recedes. It follows world scale across adjacent road sections.
expressions=lib.get_material_expressions(mat)
if not any(isinstance(n,ue.MaterialExpressionScalarParameter) and str(n.get_editor_property("parameter_name"))=="RepairContrast" for n in expressions):
    position=lib.create_material_expression(mat,ue.MaterialExpressionWorldPosition,0,0)
    strength=lib.create_material_expression(mat,ue.MaterialExpressionScalarParameter,0,0)
    strength.set_editor_property("parameter_name","RepairContrast");strength.set_editor_property("default_value",.92)
    finish=lib.create_material_expression(mat,ue.MaterialExpressionCustom,0,0)
    finish.set_editor_property("output_type",ue.CustomMaterialOutputType.CMOT_FLOAT3)
    finish.set_editor_property("code","""
float2 a=P.xy;float2 cell=floor((a+float2(210.,73.))/float2(1640.,870.));
float2 u=frac((a+float2(210.,73.))/float2(1640.,870.));
float4 rand=frac(sin(dot(cell,float2(45.37,27.92))+float4(1.,7.1,9.3,16.6))*13758.5);
float jag=sin(a.y*.062+rand.x*21.)*1.4+sin(a.y*.012+a.x*.005)*5.;
float dx=abs((u.x-(.32+.39*rand.y))*1640.+jag)-(.7+rand.y*1.7);
float dy=abs((u.y-(.18+.34*rand.z))*870.+sin(a.x*.034)*1.5)-1.4;
float aa=clamp(max(length(ddx(a)),length(ddy(a))),.48,6.);
float cut=(1.-smoothstep(-aa,aa,dx))*smoothstep(.12,.18,u.y)*(1.-smoothstep(.49,.57,u.y));
float branch=(1.-smoothstep(-aa,aa,dy))*smoothstep(.29,.33,u.x)*(1.-smoothstep(.54,.62,u.x));
float seam=saturate(cut+branch*.65)*smoothstep(.23,.42,rand.x)*K;
float2 c=(u-float2(.53,.63))*float2(1640.,870.);
float patch=max(abs(c.x)-79.-rand.y*63.,abs(c.y)-34.-rand.z*28.)+sin(a.x*.065+a.y*.08)*1.8;
float inside=(1.-smoothstep(-aa,aa,patch))*step(.68,rand.w);
float border=(1.-smoothstep(.3,2.8+aa,abs(patch)))*step(.68,rand.w);
return lerp(Base*(1.-inside*.16)+float3(.004,.003,.001)*inside,Base*.41+float3(.002,.001,.001),saturate(seam+border*.46));
""")
    ports=[]
    for name in ("Base","P","K"):
        pin=ue.CustomInput();pin.set_editor_property("input_name",name);ports.append(pin)
    finish.set_editor_property("inputs",ports)
    prop=ue.MaterialProperty.MP_BASE_COLOR;old=lib.get_material_property_input_node(mat,prop)
    output=lib.get_material_property_input_node_output_name(mat,prop)
    for n,channel,key in ((old,output,"Base"),(position,"","P"),(strength,"","K")):
        if not lib.connect_material_expressions(n,channel,finish,key):raise RuntimeError(key)
    if not lib.connect_material_property(finish,"",prop):raise RuntimeError("road repair output")
    lib.layout_material_expressions(mat);lib.recompile_material(mat)
    if not assets.save_loaded_asset(mat,False):raise RuntimeError(dest)
# The dry and wet graphs contain the same two scanned wearing courses. Update the
# shared asphalt node in place so existing material and instance references survive.
shader_path = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())) / "Scripts/riverfront-asphalt.hlsl"
road_code = shader_path.read_text()
for path in (source, dest):
    road_material = assets.load_asset(path)
    matching = [expression for expression in lib.get_material_expressions(road_material)
                if isinstance(expression, ue.MaterialExpressionCustom) and
                [str(pin.get_editor_property("input_name")) for pin in expression.get_editor_property("inputs")]
                == ["Fine", "Broad", "UV", "P", "Old"]]
    if len(matching) != 1:
        raise RuntimeError("Expected one two-scan asphalt node in " + path)
    if matching[0].get_editor_property("code") != road_code:
        matching[0].set_editor_property("code", road_code)
        lib.recompile_material(road_material)
        if not assets.save_loaded_asset(road_material, False):
            raise RuntimeError("Could not save " + path)
ue.log("SABARMATI RAIN ROAD SAVED")
