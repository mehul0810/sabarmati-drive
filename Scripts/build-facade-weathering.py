"""Add ground moisture and dust to the existing scanned masonry material graphs."""
import unreal as ue


assets = ue.EditorAssetLibrary
editor = ue.MaterialEditingLibrary
marker = "Sabarmati ground moisture"
mask_code = """
// Stable world coordinates keep the rising damp continuous across wall sections.
float2 p = P.xy / 103.;
float2 cell = floor(p);
float2 f = frac(p);
f = f * f * (3. - 2. * f);
float4 h = frac(sin(dot(cell, float2(127.1, 311.7)) + float4(0., 127.1, 311.7, 438.8)) * 43758.54);
float band = lerp(lerp(h.x, h.y, f.x), lerp(h.z, h.w, f.x), f.y);
float2 film = P.xy / 19.;
float grain = .5 + .5 * sin(film.x + film.y + 1.6 * sin(film.y * .6));
float top = 11. + band * 38. + grain * 5.;
float footing = 1. - smoothstep(top - 12., top + 6., P.z);
float3 face = cross(ddx(P), ddy(P));
float vertical = 1. - smoothstep(.22, .68, abs(face.z) * rsqrt(max(dot(face, face), .000001)));
return saturate(footing * vertical * (.46 + .47 * band));
"""


def connect(left, output, right, input_name):
    if not editor.connect_material_expressions(left, output, right, input_name):
        raise RuntimeError("Could not attach masonry shader input " + input_name)


def apply_material(name):
    material = assets.load_asset("/Game/Sabarmati/Riverfront/" + name)
    if not isinstance(material, ue.Material):
        raise RuntimeError("Missing masonry material " + name)
    existing = [node for node in editor.get_material_expressions(material)
                if isinstance(node, ue.MaterialExpressionCustom) and node.get_editor_property("desc") == marker]
    if existing:
        if existing[0].get_editor_property("code") == mask_code:
            return
        existing[0].set_editor_property("code", mask_code)
    else:
        def node(cls):
            return editor.create_material_expression(material, cls, 0, 0)

        mask = node(ue.MaterialExpressionCustom)
        mask.set_editor_property("desc", marker)
        mask.set_editor_property("code", mask_code)
        mask.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT1)
        pin = ue.CustomInput()
        pin.set_editor_property("input_name", "P")
        mask.set_editor_property("inputs", [pin])
        connect(node(ue.MaterialExpressionWorldPosition), "", mask, "P")
        for channel, multiplier in ((ue.MaterialProperty.MP_BASE_COLOR, .68),
                                    (ue.MaterialProperty.MP_ROUGHNESS, .88)):
            original = editor.get_material_property_input_node(material, channel)
            if original is None:
                raise RuntimeError(name + " is missing its scanned surface input")
            output = editor.get_material_property_input_node_output_name(material, channel)
            weathered = node(ue.MaterialExpressionMultiply)
            weathered.set_editor_property("const_b", multiplier)
            connect(original, output, weathered, "A")
            blend = node(ue.MaterialExpressionLinearInterpolate)
            connect(original, output, blend, "A")
            connect(weathered, "", blend, "B")
            connect(mask, "", blend, "Alpha")
            if not editor.connect_material_property(blend, "", channel):
                raise RuntimeError("Could not attach " + name + " surface output")
    editor.layout_material_expressions(material)
    editor.recompile_material(material)
    if not assets.save_loaded_asset(material, False):
        raise RuntimeError("Could not save " + name)


for material_name in ("M_FacadePlaster", "M_FacadeSandstone", "M_FacadeTerracotta"):
    apply_material(material_name)
ue.log("SABARMATI MASONRY WEATHERING SAVED")
