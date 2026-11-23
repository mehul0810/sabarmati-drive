"""Generate original district surfaces, signage and automotive finishes in the Unreal editor."""
from pathlib import Path
import os
import unreal as ue

root = "/Game/Sabarmati/Riverfront"
art = Path(os.environ.get("SABARMATI_SIGN_DIR", str(Path(ue.Paths.project_saved_dir()) / "DistrictArt")))
lib = ue.MaterialEditingLibrary
assets = ue.EditorAssetLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()

def node(mat, kind, **props):
    n = lib.create_material_expression(mat, getattr(ue, "MaterialExpression" + kind), 0, 0)
    for key, val in props.items():
        n.set_editor_property(key, val)
    return n

def link(a, b, pin, out=""):
    if not lib.connect_material_expressions(a, out, b, pin):
        raise RuntimeError(f"Could not connect {pin}")

def output(n, key, pin=""):
    if not lib.connect_material_property(n, pin, getattr(ue.MaterialProperty, "MP_" + key)):
        raise RuntimeError(key)

def make(name):
    path = f"{root}/{name}"
    m = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name, root, ue.Material, ue.MaterialFactoryNew())
    if not isinstance(m, ue.Material):
        raise RuntimeError(path)
    lib.delete_all_material_expressions(m)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m

def save(mat):
    lib.layout_material_expressions(mat)
    lib.recompile_material(mat)
    if not assets.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(mat.get_path_name())

def number(m, value):
    return node(m, "Constant", r=value)

def color(m, values):
    return node(m,"Constant3Vector",constant=ue.LinearColor(*values,1.0))

def custom(m, coords, code, scalar=False):
    expr = node(m,"Custom",code=code, output_type=ue.CustomMaterialOutputType.CMOT_FLOAT1 if scalar else ue.CustomMaterialOutputType.CMOT_FLOAT3)
    pin = ue.CustomInput()
    pin.set_editor_property("input_name","P")
    expr.set_editor_property("inputs",[pin])
    link(coords,expr,"P")
    return expr

m = make("M_DistrictGlass")
m.set_editor_property("blend_mode",ue.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("translucency_lighting_mode",ue.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
m.set_editor_property("two_sided",True)
output(color(m,(.24,.35,.36)),"BASE_COLOR")
output(number(m,.09),"ROUGHNESS")
output(number(m,.60),"SPECULAR")
fresnel = node(m,"Fresnel", exponent=3.2, base_reflect_fraction=.11)
scale = node(m,"Multiply", const_b=.66)
link(fresnel,scale,"A")
offset = node(m,"Add",const_b=.09)
link(scale,offset,"A")
output(offset,"OPACITY")
save(m)

m = make("M_DistrictCarPaint")
m.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_CLEAR_COAT)
output(node(m,"VectorParameter",parameter_name="BodyColor",default_value=ue.LinearColor(.12,.19,.16,1.)),"BASE_COLOR")
# UE's clear-coat shading model supplies coat weight 1.0 and coat roughness 0.1.
for key,val in (("METALLIC",.65),("ROUGHNESS",.32)):
    output(number(m,val),key)
save(m)

for name,code,rough,metal in (
    ("Timber","float g=sin(P.x*.10+P.y*.017+sin(P.z*.055)*2.+sin(P.x*.023)*3.); float pore=sin(P.x*1.8+sin(P.y*.09))*sin(P.z*1.55); return lerp(float3(.115,.054,.024),float3(.255,.130,.056),.5+.32*g+.08*pore);",.53,0.),
    ("Brass","float t=.5+.5*sin(P.z*7.+sin(P.x*3.)); return lerp(float3(.43,.26,.085),float3(.56,.365,.16),.16+.13*t);",.28,.86),
    ("Ceramic","float t=sin(P.x*7.)*sin(P.y*5.7); return float3(.66,.60,.45)*(.95+.025*t);",.18,.04),
    ("Canvas","float band=smoothstep(.5,.56,frac(P.x/34.)); float thread=sin(P.x*4.)*sin(P.y*4.+P.z*3.); return lerp(float3(.46,.31,.17),float3(.14,.20,.17),band)*(.91+.065*thread);",.82,0.)):
    m=make("M_District"+name)
    output(custom(m,node(m,"WorldPosition"),code),"BASE_COLOR")
    output(number(m,rough),"ROUGHNESS"); output(number(m,metal),"METALLIC")
    save(m)

for material_name,filename in (("M_NadiChaiSign","T_NadiChai"),("M_KagazPressSign","T_KagazPrint")):
    if not (art/(filename+".png")).is_file():
        raise RuntimeError(str(art/(filename+".png")))
    task=ue.AssetImportTask()
    for key,val in (("filename",str(art/(filename+".png"))),("destination_path",root+"/Textures"),("destination_name",filename),("automated",True),("replace_existing",True),("save",True)):
        task.set_editor_property(key,val)
    tools.import_asset_tasks([task])
    tex=assets.load_asset(root+"/Textures/"+filename)
    if not isinstance(tex,ue.Texture2D): raise RuntimeError(filename)
    m=make(material_name)
    output(node(m,"TextureSample",texture=tex),"BASE_COLOR","RGB")
    output(number(m,.54),"ROUGHNESS")
    save(m)
ue.log("SABARMATI DISTRICT FINISHES SAVED")
