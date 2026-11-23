"""Layer dry earth, gravel and worn grass over the credited concrete aggregate for city plots."""
import unreal as ue

lib = ue.EditorAssetLibrary
edit = ue.MaterialEditingLibrary
root = "/Game/Sabarmati/Riverfront"
name = root + "/M_StreetGround"
mat = lib.load_asset(name) if lib.does_asset_exist(name) else lib.duplicate_asset(root + "/M_Concrete", name)
if not isinstance(mat, ue.Material):
    raise RuntimeError(name)
nodes = edit.get_material_expressions(mat)
if not any(isinstance(n, ue.MaterialExpressionCustom) and n.get_editor_property("desc") == "Street ground pigment" for n in nodes):
    prop = ue.MaterialProperty.MP_BASE_COLOR
    old = edit.get_material_property_input_node(mat, prop)
    old_output = edit.get_material_property_input_node_output_name(mat, prop)
    if not old:
        raise RuntimeError("The original surface has no aggregate color")
    custom = edit.create_material_expression(mat, ue.MaterialExpressionCustom)
    custom.set_editor_property("desc", "Street ground pigment")
    custom.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("code", """
float2 p=P.xy/480., i=floor(p), f=frac(p); f=f*f*(3.-2.*f);
float4 h=frac(sin(dot(i,float2(26.31,71.91))+float4(0.,26.31,71.91,98.22))*32518.4);
float n=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);
float grain=dot(Base,float3(.299,.587,.114));
float3 soil=lerp(float3(.19,.128,.073),float3(.42,.35,.235),n);
float grass=smoothstep(.58,.85,n+.12*sin(P.x*.013)*sin(P.y*.011));
return max(.01,grain)*lerp(soil,float3(.15,.22,.10),grass);
""")
    pins = []
    for pin_name in ("Base", "P"):
        pin = ue.CustomInput()
        pin.set_editor_property("input_name", pin_name)
        pins.append(pin)
    custom.set_editor_property("inputs", pins)
    pos = edit.create_material_expression(mat, ue.MaterialExpressionWorldPosition)
    if not (edit.connect_material_expressions(old, old_output, custom, "Base") and
            edit.connect_material_expressions(pos, "", custom, "P") and
            edit.connect_material_property(custom, "", prop)):
        raise RuntimeError("Could not connect city ground pigment")
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    edit.layout_material_expressions(mat)
    edit.recompile_material(mat)
    if not lib.save_loaded_asset(mat, False):
        raise RuntimeError(name)
ue.log("SABARMATI MAPPED STREET GROUND SAVED")
