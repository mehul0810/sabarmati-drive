"""Create the unbadged hatchback's native paint, glass and rubber materials in Unreal Editor."""
import unreal as ue


FOLDER = "/Game/Sabarmati/Riverfront/Vehicles/Coachwork"
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
MATERIALS = ue.MaterialEditingLibrary
LIBRARY = ue.EditorAssetLibrary


def node(material, expression, **values):
    result = MATERIALS.create_material_expression(material, expression)
    for name, value in values.items():
        result.set_editor_property(name, value)
    return result


def output(expression, name):
    if not MATERIALS.connect_material_property(expression, "", getattr(ue.MaterialProperty, "MP_" + name)):
        raise RuntimeError(f"Cannot connect {name} on {expression.get_outer().get_name()}")


def make(name):
    path = f"{FOLDER}/{name}"
    material = LIBRARY.load_asset(path) if LIBRARY.does_asset_exist(path) else TOOLS.create_asset(
        name, FOLDER, ue.Material, ue.MaterialFactoryNew())
    if not isinstance(material, ue.Material):
        raise RuntimeError(path)
    MATERIALS.delete_all_material_expressions(material)
    return material


def finish(material, color, roughness, metallic):
    if color is not None:
        output(node(material, ue.MaterialExpressionConstant3Vector, constant=ue.LinearColor(*color, 1)), "BASE_COLOR")
    output(node(material, ue.MaterialExpressionConstant, r=roughness), "ROUGHNESS")
    output(node(material, ue.MaterialExpressionConstant, r=metallic), "METALLIC")


def save(material):
    MATERIALS.layout_material_expressions(material)
    MATERIALS.recompile_material(material)
    if not LIBRARY.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(material.get_path_name())


paint = make("M_HatchPaint")
paint.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_CLEAR_COAT)
finish(paint, None, .32, .32)
output(node(paint, ue.MaterialExpressionVectorParameter, parameter_name="BodyColor",
            default_value=ue.LinearColor(.050, .107, .095, 1)), "BASE_COLOR")
save(paint)

glass = make("M_HatchGlazing")
glass.set_editor_property("blend_mode", ue.BlendMode.BLEND_TRANSLUCENT)
glass.set_editor_property("translucency_lighting_mode", ue.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
glass.set_editor_property("two_sided", True)
finish(glass, (.019, .026, .029), .11, 0)
fresnel = node(glass, ue.MaterialExpressionFresnel, exponent=4.4, base_reflect_fraction=.11)
opacity = node(glass, ue.MaterialExpressionMultiply, const_b=.72)
if not MATERIALS.connect_material_expressions(fresnel, "", opacity, "A"):
    raise RuntimeError("Cannot connect the windscreen Fresnel input")
output(opacity, "OPACITY")
save(glass)

rubber = make("M_HatchTyre")
finish(rubber, (.032, .029, .025), .82, 0)
output(node(rubber, ue.MaterialExpressionConstant, r=.33), "SPECULAR")
save(rubber)

ue.log("SABARMATI HATCH FINISHES SAVED")
