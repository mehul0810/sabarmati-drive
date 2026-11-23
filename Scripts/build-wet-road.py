"""Create a rain-responsive copy of the credited asphalt, preserving the dry graph at zero wetness."""
import unreal as ue

root="/Game/Sabarmati/Riverfront"
assets=ue.EditorAssetLibrary
lib=ue.MaterialEditingLibrary
source=root+"/M_Asphalt"
dest=root+"/M_DistrictRoad"
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
    puddle=new("Custom",output_type=ue.CustomMaterialOutputType.CMOT_FLOAT1,code="""
float2 p=P.xy/225.;
float2 i=floor(p), f=frac(p); f=f*f*(3.-2.*f);
float4 h=frac(sin(dot(i,float2(127.1,311.7))+float4(0.,127.1,311.7,438.8))*43758.54);
float n=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);
return W*(.34+.66*smoothstep(.44,.73,n+.06*sin(P.x*.021+P.y*.009)));
""")
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
ue.log("SABARMATI RAIN ROAD SAVED")
