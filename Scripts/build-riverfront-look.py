"""Build the editor-only material assets from the verified CC0 source textures."""
from pathlib import Path
import os
from zipfile import ZipFile
import unreal as ue

ROOT = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
SOURCE = Path(os.environ.get("SABARMATI_LOOK_SOURCES", ROOT / "Saved/LookSources"))
DEST = "/Game/Sabarmati/Riverfront"
EDIT = ue.MaterialEditingLibrary

def node(material, cls, x, y):
    return EDIT.create_material_expression(material, getattr(ue, "MaterialExpression" + cls), x, y)

def connect(left, right, name, output=""):
    if not EDIT.connect_material_expressions(left, output, right, name):
        raise RuntimeError(f"Cannot connect {left} to {right}.{name}")

def prop(expression, material_property, output=""):
    if not EDIT.connect_material_property(expression, output, getattr(ue.MaterialProperty, "MP_" + material_property)):
        raise RuntimeError(f"Cannot connect {material_property}")

def scalar(material, value, x, y):
    expr = node(material, "Constant", x, y)
    expr.set_editor_property("r", value)
    return expr

def color(material, rgb, x, y):
    expr = node(material, "Constant3Vector", x, y)
    expr.set_editor_property("constant", ue.LinearColor(*rgb, 1.0))
    return expr

def make_material(name):
    path = f"{DEST}/{name}"
    asset = ue.EditorAssetLibrary.load_asset(path) if ue.EditorAssetLibrary.does_asset_exist(path) else None
    if not asset:
        asset = ue.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, ue.Material, ue.MaterialFactoryNew())
    if not asset:
        raise RuntimeError(f"Cannot create {path}")
    EDIT.delete_all_material_expressions(asset)
    return asset

def load_texture(asset_id, channel):
    filename = f"{asset_id}_1K-JPG_{channel}.jpg"
    name = f"T_{asset_id}_{channel}"
    path = f"{DEST}/Textures/{name}"
    if ue.EditorAssetLibrary.does_asset_exist(path):
        texture = ue.EditorAssetLibrary.load_asset(path)
    else:
        with ZipFile(SOURCE / f"{asset_id}.zip") as archive:
            file_path = archive.extract(filename, SOURCE / "unpacked")
        task = ue.AssetImportTask()
        task.set_editor_property("filename", file_path)
        task.set_editor_property("destination_path", f"{DEST}/Textures")
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        paths = task.get_editor_property("imported_object_paths")
        if len(paths) != 1:
            raise RuntimeError(f"Import failed for {filename}: {paths}")
        texture = ue.EditorAssetLibrary.load_asset(paths[0])
    if channel != "Color":
        texture.set_editor_property("srgb", False)
        compression = (ue.TextureCompressionSettings.TC_NORMALMAP if channel == "NormalDX"
                       else ue.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("compression_settings", compression)
    ue.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=True)
    return texture

def finish(material):
    material.set_editor_property("used_with_instanced_static_meshes", True)
    EDIT.layout_material_expressions(material)
    EDIT.recompile_material(material)
    if not ue.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Cannot save {material.get_name()}")
    ue.log(f"SABARMATI LOOK SAVED: {material.get_path_name()}")

def surface(name, asset_id, centimeters, tint, vertical=False, road=False, paint=False):
    material = make_material(name)
    # World-space XY for ground; XZ for the aligned retaining walls/facades.
    position = node(material, "WorldPosition", -1000, -100)
    if vertical:
        axes = custom(material, "CMOT_FLOAT2", ("P",), """
const float3 N = abs(cross(ddx(P), ddy(P)));
if (N.z >= max(N.x, N.y)) return P.xy;
return N.x > N.y ? P.yz : P.xz;
""", -800, -100)
        connect(position, axes, "P")
    else:
        axes = node(material, "ComponentMask", -800, -100)
        axes.set_editor_property("r", True)
        axes.set_editor_property("g", True)
        connect(position, axes, "")
    scale = node(material, "Multiply", -610, -100)
    connect(axes, scale, "A")
    connect(scalar(material, 1.0 / centimeters, -800, 90), scale, "B")
    modes = (("Color", ue.MaterialSamplerType.SAMPLERTYPE_COLOR),
             ("NormalDX", ue.MaterialSamplerType.SAMPLERTYPE_NORMAL),
             ("Roughness", ue.MaterialSamplerType.SAMPLERTYPE_MASKS))
    for index, (channel, sampler) in enumerate(modes):
        sample = node(material, "TextureSample", -340, index * 200)
        sample.set_editor_property("texture", load_texture(asset_id, channel))
        sample.set_editor_property("sampler_type", sampler)
        connect(scale, sample, "UVs")
        if channel == "Color":
            tint_node = node(material, "Multiply", -130, 0)
            connect(sample, tint_node, "A", "RGB")
            connect(color(material, tint, -300, -140), tint_node, "B")
            if road or paint:
                # A second, broad scan sample changes the aggregate tone over several metres.
                macro_uv = node(material, "Multiply", -560, -380)
                connect(axes, macro_uv, "A")
                connect(scalar(material, 1.0 / 2380, -810, -400), macro_uv, "B")
                broad = node(material, "TextureSample", -320, -400)
                broad.set_editor_property("texture", sample.get_editor_property("texture"))
                connect(macro_uv, broad, "UVs")
                if paint:
                    code = """
float grain = dot(Fine, float3(.3,.59,.11));
float coverage = smoothstep(.035, .14, grain + Broad * .20);
return lerp(Fine * .74, float3(.65,.62,.53), .86 * coverage);
"""
                else:
                    code = """
// Recover the strip width and transverse coordinate even on a rotated bridge deck.
float2 dx = ddx(UV), dy = ddy(UV);
float det = max(abs(dx.x*dy.y - dx.y*dy.x), 1e-10);
float2 widths = float2(length(ddx(P)*dy.y - ddy(P)*dx.y),
                       length(ddy(P)*dx.x - ddx(P)*dy.x)) / det;
float across = widths.x < widths.y ? UV.x : UV.y;
float width = min(widths.x, widths.y);
float lanes = clamp(round(width / 360.), 1., 4.);
float lane = frac(across*lanes);
float wheel = 1. - smoothstep(.045,.12, abs(abs(lane-.5)-.22));
float dust = 1. - smoothstep(.018,.075,min(across,1.-across));
float2 metres = P.xy * .01;
float2 grid = metres / float2(21., 8.);
float2 id = floor(grid);
float hash = frac(sin(dot(id,float2(39.7,17.1))) * 13758.5);
float2 dist = (abs(frac(grid)-.5) - float2(.15 + .10*hash,.15)) * float2(21.,8.);
float edge = max(dist.x,dist.y) + (Broad-.15)*.55;
float alias = max(fwidth(edge),.025);
float repair = (1.-smoothstep(-alias,alias,edge)) * step(.57,hash);
float tar = (1.-smoothstep(.02,.055+alias,abs(edge))) * step(.57,hash);
float3 asphalt = lerp(Fine, Old * float3(.55,.59,.63), .31 + .25*smoothstep(.1,.26,Broad));
asphalt *= (.86 - wheel*.14 + dust*.17) * lerp(1., .69, repair);
return lerp(asphalt, asphalt*.42, tar*.79);
"""
                inputs = ("Fine", "Broad", "UV", "P", "Old") if road else ("Fine", "Broad")
                finish_color = custom(material, "CMOT_FLOAT3", inputs, code, 40, -180)
                connect(tint_node, finish_color, "Fine")
                connect(broad, finish_color, "Broad", "R")
                if road:
                    old_uv = node(material, "Multiply", -560, 760)
                    connect(axes, old_uv, "A")
                    connect(scalar(material, 1.0/516, -810, 790), old_uv, "B")
                    old_scan = node(material, "TextureSample", -300, 780)
                    old_scan.set_editor_property("texture", load_texture("Asphalt019", "Color"))
                    connect(old_uv, old_scan, "UVs")
                    connect(old_scan, finish_color, "Old", "RGB")
                    connect(node(material, "TextureCoordinate", -320, 650), finish_color, "UV")
                    connect(position, finish_color, "P")
                prop(finish_color, "BASE_COLOR")
            else:
                prop(tint_node, "BASE_COLOR")
        elif channel == "NormalDX":
            prop(sample, "NORMAL", "RGB")
        else:
            prop(sample, "ROUGHNESS", "R")
    finish(material)

def custom(material, output, names, code, x, y):
    expr = node(material, "Custom", x, y)
    expr.set_editor_property("output_type", getattr(ue.CustomMaterialOutputType, output))
    inputs = []
    for name in names:
        port = ue.CustomInput()
        port.set_editor_property("input_name", name)
        inputs.append(port)
    expr.set_editor_property("inputs", inputs)
    expr.set_editor_property("code", code)
    return expr

def metal_or_glass(name, base, roughness, specular, reflective=False):
    material = make_material(name)
    base_input = color(material, base, -300, -100)
    if reflective:
        edge = node(material, "Fresnel", -460, 150)
        blend = node(material, "LinearInterpolate", -100, -60)
        connect(base_input, blend, "A")
        connect(color(material, (.118, .158, .180), -320, 40), blend, "B")
        connect(edge, blend, "Alpha")
        base_input = blend
    prop(base_input, "BASE_COLOR")
    prop(scalar(material, roughness, -290, 290), "ROUGHNESS")
    prop(scalar(material, specular, -290, 390), "SPECULAR")
    finish(material)

def street_glazing():
    material = make_material("M_FacadeGlazing")
    # Opaque two-layer approximation for the SM5 path. The reveal and frames are geometry.
    glass = custom(material, "CMOT_FLOAT3", ("UV", "Reflection", "Seed", "Eye", "N"), """
float sky = smoothstep(-.09, .38, Reflection.z);
float3 reflection = lerp(float3(.087,.077,.059), float3(.27,.38,.46), sky);
float shift = clamp(dot(Eye.xy, float2(.77,.64)) * .16, -.15,.15);
float blind = smoothstep(.012,.036, frac((UV.y + shift * .24) * 18.));
float coverage = (1.-smoothstep(-.015,.015, UV.y - (.18 + Seed * .62) + shift));
float curtain = .5 + .5*cos((UV.x + shift) * 38. + Seed * 18.);
float3 room = lerp(float3(.025,.036,.035), float3(.32,.255,.165), curtain*.27 + coverage*blind*.71);
float grazing = pow(1.-saturate(abs(dot(Eye,N))), 3.);
return lerp(room, reflection, .26 + .52*grazing);
""", 40, -40)
    for port, kind in (("UV", "TextureCoordinate"), ("Reflection", "ReflectionVectorWS"),
                       ("Seed", "PerInstanceRandom"), ("Eye", "CameraVectorWS"), ("N", "PixelNormalWS")):
        connect(node(material, kind, -360, 0), glass, port)
    prop(glass, "BASE_COLOR")
    reflected_light = node(material, "Multiply", 260, 100)
    connect(glass, reflected_light, "A")
    connect(scalar(material, .31, 30, 180), reflected_light, "B")
    prop(reflected_light, "EMISSIVE_COLOR")
    prop(scalar(material, .24, -300, 250), "ROUGHNESS")
    prop(scalar(material, .51, -300, 320), "SPECULAR")
    finish(material)

def water():
    material = make_material("M_River")
    world = node(material, "WorldPosition", -700, 0)
    time = node(material, "Time", -700, 190)
    wave = node(material, "Custom", -420, 100)
    wave.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ("P", "T"):
        port = ue.CustomInput()
        port.set_editor_property("input_name", name)
        inputs.append(port)
    wave.set_editor_property("inputs", inputs)
    wave.set_editor_property("code", """
const float2 q = P.xy * 0.01;
const float w = max(length(ddx(q)), length(ddy(q)));
const float2 d = float2(.904,.428);
const float2 e = float2(-.321,.947);
const float2 f = float2(.582,.813);
// Modulate phase at metre scale and fade subpixel ripples toward the horizon.
const float warp = sin(dot(q,float2(.27,.13)) + T*.23) * .80;
const float a = dot(q,d)*3.5 + T*1.13 + warp;
const float b = dot(q,e)*6.1 - T*.92 + warp*.83;
const float c = dot(q,f)*11.3 + T*1.60 - warp*.59;
const float2 slope = cos(a)*.033*d*(1-smoothstep(.20,.60,w))
                   + cos(b)*.020*e*(1-smoothstep(.11,.36,w))
                   + cos(c)*.011*f*(1-smoothstep(.055,.17,w));
return normalize(float3(slope,1));
""")
    connect(world, wave, "P")
    connect(time, wave, "T")
    prop(wave, "NORMAL")
    # Dark green-grey body color rather than bright opaque cyan.
    prop(color(material, (0.022, 0.038, 0.032), -240, -150), "BASE_COLOR")
    prop(scalar(material, .19, -250, 310), "ROUGHNESS")
    prop(scalar(material, .47, -250, 430), "SPECULAR")
    finish(material)

scope = os.environ.get("SABARMATI_LOOK_SCOPE", "all")
if scope not in {"all", "facades", "water", "street"}:
    raise RuntimeError(f"Unknown surface subset: {scope}")
if scope == "all":
    surface("M_Paving", "PavingStones034", 285, (.94, .87, .76))
    surface("M_Concrete", "Concrete034", 460, (.87, .82, .76))
if scope in {"all", "water"}:
    water()
if scope in {"all", "facades"}:
    surface("M_Retaining", "Concrete034", 380, (.96, .83, .66), vertical=True)
    surface("M_FacadeSandstone", "Concrete034", 275, (.91, .64, .39), vertical=True)
    metal_or_glass("M_CoatedIron", (.052, .058, .061), .51, .41)
if scope in {"all", "street"}:
    surface("M_Asphalt", "Asphalt033", 250, (.82, .85, .87), road=True)
    surface("M_RoadStripe", "Asphalt033", 250, (1.0, 1.0, 1.0), paint=True)
if scope in {"all", "facades", "street"}:
    surface("M_FacadePlaster", "Plaster001", 145, (.89, .78, .62), vertical=True)
    surface("M_FacadeTerracotta", "Bricks051", 125, (.81, .72, .67), vertical=True)
    street_glazing()
ue.log("SABARMATI LOOK BUILD COMPLETE")
