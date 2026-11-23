"""Offline dual-dial instrument shader with real speed, gear and engine RPM parameters."""
import unreal as ue
root="/Game/Sabarmati/Riverfront/Vehicles/Coachwork";name="M_RiverInstruments"
ed=ue.MaterialEditingLibrary;assets=ue.EditorAssetLibrary
m=assets.load_asset(root+"/"+name) if assets.does_asset_exist(root+"/"+name) else ue.AssetToolsHelpers.get_asset_tools().create_asset(name,root,ue.Material,ue.MaterialFactoryNew())
ed.delete_all_material_expressions(m);m.set_editor_property("two_sided",True);m.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
def node(kind,**props):
 n=ed.create_material_expression(m,getattr(ue,"MaterialExpression"+kind),0,0)
 for k,v in props.items():n.set_editor_property(k,v)
 return n
screen=node("Custom",output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3,code="""
struct Ink {
float Stroke(float2 p,float2 a,float2 b,float width) {float2 v=b-a;float t=saturate(dot(p-a,v)/max(1e-6,dot(v,v)));return 1.-smoothstep(width,width+.016,length(p-a-t*v));}
float digit(float2 p,int n) {
    int mask=n==0?63:n==1?6:n==2?91:n==3?79:n==4?102:n==5?109:n==6?125:n==7?7:n==8?127:111;
    float v=0.;
    if(mask&1) v+=Stroke(p,float2(-.26,.48),float2(.26,.48),.048);
    if(mask&2) v+=Stroke(p,float2(.30,.45),float2(.30,.05),.048);
    if(mask&4) v+=Stroke(p,float2(.30,-.04),float2(.30,-.44),.048);
    if(mask&8) v+=Stroke(p,float2(-.26,-.48),float2(.26,-.48),.048);
    if(mask&16) v+=Stroke(p,float2(-.30,-.04),float2(-.30,-.44),.048);
    if(mask&32) v+=Stroke(p,float2(-.30,.45),float2(-.30,.05),.048);
    if(mask&64) v+=Stroke(p,float2(-.26,0),float2(.26,0),.048);
    return saturate(v);
}
}; Ink ink;
float2 p=float2((UV.x-.5)*2.8,(.5-UV.y));float3 color=float3(.004,.008,.010);
float2 g=p-float2(p.x<0.?-.72:.72,0);float r=length(g);
float rim=1.-smoothstep(.004,.014,abs(r-.420));color+=rim*float3(.18,.21,.20);
float a=atan2(g.x,g.y);float arc=2.33;
float value=p.x<0.?saturate(Speed/180.):saturate(RPM/7000.);
float visible=step(abs(a),arc);float graduation=abs(frac((a+arc)*36./(2.*arc)+.5)-.5);
float tick=(1.-smoothstep(.04,.10,graduation))*step(.321,r)*step(r,.387)*visible;
color+=tick*float3(.53,.57,.53);
float angle=lerp(-arc,arc,value);float2 dir=float2(sin(angle),cos(angle));
float needle=ink.Stroke(g,-dir*.052,dir*.335,.012);color+=needle*float3(.95,.23,.043);
color+=float3(.12,.14,.13)*(1.-smoothstep(.035,.060,r));
float2 t=(p-float2(0,.035))*6.5;
int v=(int)round(max(0.,Speed));
if(v>=100) color+=ink.digit(t-float2(-.86,0),v/100)*float3(.60,.79,.70);
if(v>=10) color+=ink.digit(t,(v/10)%10)*float3(.60,.79,.70);
color+=ink.digit(t-float2(.86,0),v%10)*float3(.60,.79,.70);
color+=ink.digit((p-float2(0,-.20))*9.,(int)abs(Gear))*float3(.69,.49,.24);
return color;
""")
inputs={"UV":node("TextureCoordinate")}
for k,v in (("Speed",0.),("RPM",820.),("Gear",1.)):inputs[k]=node("ScalarParameter",parameter_name=k,default_value=v)
pins=[]
for key in inputs:
 pin=ue.CustomInput();pin.set_editor_property("input_name",key);pins.append(pin)
screen.set_editor_property("inputs",pins)
for key,n in inputs.items():
 if not ed.connect_material_expressions(n,"",screen,key):raise RuntimeError(key)
if not ed.connect_material_property(screen,"",ue.MaterialProperty.MP_EMISSIVE_COLOR):raise RuntimeError(name)
ed.layout_material_expressions(m);ed.recompile_material(m)
if not assets.save_loaded_asset(m,False):raise RuntimeError(name)
ue.log("SABARMATI DASHBOARD SAVED")
