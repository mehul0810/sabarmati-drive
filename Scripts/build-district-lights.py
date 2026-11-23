"""Build the two original, texture-free SM5 materials used by district lighting.
Run inside Unreal's Python commandlet. Rebuilding touches these packages only.
"""
import unreal as ue

ROOT = "/Game/Sabarmati/Riverfront"
library = ue.MaterialEditingLibrary


def material(name):
    path = f"{ROOT}/{name}"
    existing = ue.load_asset(path) if ue.EditorAssetLibrary.does_asset_exist(path) else None
    if existing is None:
        existing = ue.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, ue.Material, ue.MaterialFactoryNew())
    if not isinstance(existing, ue.Material):
        raise RuntimeError(f"Expected a material at {path}")
    library.delete_all_material_expressions(existing)
    return existing


def node(mat, name, x, y):
    return library.create_material_expression(mat, getattr(ue, "MaterialExpression" + name), x, y)


def scalar(mat, value, y):
    n = node(mat, "Constant", -300, y)
    n.set_editor_property("r", value)
    return n


def rgb(mat, values, y):
    n = node(mat, "Constant3Vector", -310, y)
    n.set_editor_property("constant", ue.LinearColor(*values, 1.0))
    return n


def output(n, name):
    if not library.connect_material_property(n, "", getattr(ue.MaterialProperty, "MP_" + name)):
        raise RuntimeError(f"Could not connect {name}")


def save(mat):
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    library.layout_material_expressions(mat)
    library.recompile_material(mat)
    if not ue.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {mat.get_path_name()}")


lens = material("M_PracticalLens")
output(rgb(lens, (.55, .50, .38), -120), "BASE_COLOR")
output(scalar(lens, .37, 220), "ROUGHNESS")
strength = node(lens, "ScalarParameter", -510, 60)
strength.set_editor_property("parameter_name", "Glow")
strength.set_editor_property("default_value", 0.0)
warm = node(lens, "Multiply", -70, 80)
library.connect_material_expressions(strength, "", warm, "A")
tint = node(lens, "VectorParameter", -500, 0)
tint.set_editor_property("parameter_name", "LensTint")
tint.set_editor_property("default_value", ue.LinearColor(1.0, .66, .34, 1.0))
library.connect_material_expressions(tint, "", warm, "B")
output(warm, "EMISSIVE_COLOR")
save(lens)

glass = material("M_OccupiedWindow")
output(rgb(glass, (.028, .041, .052), -140), "BASE_COLOR")
output(scalar(glass, .23, 310), "ROUGHNESS")
output(scalar(glass, .48, 390), "SPECULAR")
pattern = node(glass, "Custom", -20, 70)
pattern.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT3)
ports = []
for key in ("UV", "Seed"):
    port = ue.CustomInput()
    port.set_editor_property("input_name", key)
    ports.append(port)
pattern.set_editor_property("inputs", ports)
library.connect_material_expressions(node(glass, "TextureCoordinate", -410, 0), "", pattern, "UV")
library.connect_material_expressions(node(glass, "PerInstanceRandom", -410, 140), "", pattern, "Seed")
pattern.set_editor_property("code", """
float occupied = smoothstep(.48, .50, Seed);
float2 uv = saturate(UV);
float side = .54 + .46 * smoothstep(.02, .16, min(uv.x, 1.-uv.x));
float shade = smoothstep(.017,.06, abs(uv.y - (.24 + .43 * frac(Seed * 17.))));
float curtain = .76 + .24 * cos(uv.x * 24. + Seed * 31.);
float3 room = lerp(float3(.80,.34,.11), float3(.65,.56,.38), frac(Seed*43.));
return room * occupied * side * (shade*.42+.58) * curtain * (.28+.82*frac(Seed*7.)) + float3(.004,.007,.014);
""")
output(pattern, "EMISSIVE_COLOR")
save(glass)
ue.log("SABARMATI DISTRICT LIGHT MATERIALS SAVED")
