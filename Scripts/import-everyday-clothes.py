"""Bake the CC0 everyday clothing into Unreal using the existing resident skeleton."""
import os
from pathlib import Path
import unreal as ue

source = Path(os.environ["SABARMATI_EVERYDAY_SOURCE"])
people = "/Game/Sabarmati/Riverfront/People"
folder = people+"/Wardrobe/Everyday"
base = people+"/SK_RiverResident"
library = ue.EditorAssetLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
edit = ue.MaterialEditingLibrary


def texture(name, kind="color"):
    path = folder+"/"+name
    task = ue.AssetImportTask()
    task.filename = str(source/(name+".png"))
    task.destination_path = folder
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    asset = library.load_asset(path)
    if not isinstance(asset,ue.Texture2D):
        raise RuntimeError(path)
    if kind == "normal":
        asset.set_editor_property("compression_settings",ue.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "occlusion":
        asset.set_editor_property("compression_settings",ue.TextureCompressionSettings.TC_GRAYSCALE)
    asset.set_editor_property("srgb",kind=="color")
    library.save_loaded_asset(asset,False)
    return asset


def material(label, roughness, ambient=False):
    name = "M_Everyday"+label
    colour = texture("T_Everyday"+label)
    normal = texture("T_Everyday"+label+"_N","normal")
    ao = texture("T_EverydayOcclusion","occlusion") if ambient else None
    path = folder+"/"+name
    mat = library.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(name,folder,ue.Material,ue.MaterialFactoryNew())
    edit.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_skeletal_mesh",True)
    mat.set_editor_property("two_sided",True)
    def node(cls, **props):
        item = edit.create_material_expression(mat,cls)
        for key,value in props.items():
            item.set_editor_property(key,value)
        return item
    def connect(a, op, b, pin):
        if not edit.connect_material_expressions(a,op,b,pin):
            raise RuntimeError(name+" "+pin)
    def output(a, op, prop):
        if not edit.connect_material_property(a,op,prop):
            raise RuntimeError(name+" "+str(prop))
    color_node = node(ue.MaterialExpressionTextureSample,texture=colour)
    dye = node(ue.MaterialExpressionVectorParameter,parameter_name="BaseColorFactor",default_value=ue.LinearColor(1,1,1,1))
    multiply = node(ue.MaterialExpressionMultiply)
    connect(color_node,"RGB",multiply,"A")
    connect(dye,"",multiply,"B")
    output(multiply,"",ue.MaterialProperty.MP_BASE_COLOR)
    bump = node(ue.MaterialExpressionTextureSample,texture=normal,sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    output(bump,"RGB",ue.MaterialProperty.MP_NORMAL)
    output(node(ue.MaterialExpressionConstant,r=roughness),"",ue.MaterialProperty.MP_ROUGHNESS)
    output(node(ue.MaterialExpressionConstant,r=.31),"",ue.MaterialProperty.MP_SPECULAR)
    if ao:
        cavity = node(ue.MaterialExpressionTextureSample,texture=ao,sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        output(cavity,"R",ue.MaterialProperty.MP_AMBIENT_OCCLUSION)
    edit.layout_material_expressions(mat)
    edit.recompile_material(mat)
    if not library.save_loaded_asset(mat,False):
        raise RuntimeError(path)
    return mat


fabric = material("Fabric",.79,True)
shoes = material("Leather",.54)
skeleton = library.load_asset(base+"/SkeletalMeshes/SK_RiverResident_Skeleton")
physics = library.load_asset(base+"/SkeletalMeshes/SK_RiverResident_PhysicsAsset")
if not skeleton or not physics:
    raise RuntimeError("Load the existing resident skeleton and physics asset before importing the clothing.")
path = folder+"/SK_RiverEveryday"
pipeline = ue.InterchangeGenericAssetsPipeline()
pipeline.common_skeletal_meshes_and_animations_properties.skeleton = skeleton
pipeline.material_pipeline.import_materials = False
pipeline.animation_pipeline.import_animations = False
pipeline.mesh_pipeline.create_physics_asset = False
params = ue.ImportAssetParameters()
params.is_automated = True
params.override_pipelines = [ue.SoftObjectPath(pipeline.get_path_name())]
manager = ue.InterchangeManager.get_interchange_manager_scripted()
data = manager.create_source_data(str(source/"SK_RiverEveryday.glb"))
# Use a transient import folder so each invocation can refresh the intended mesh.
if not manager.import_asset(folder+"/Import",data,params):
    raise RuntimeError("Skeletal import failed")
models = [package for package in library.list_assets(folder+"/Import",recursive=True)
          if isinstance(library.load_asset(package),ue.SkeletalMesh)]
if len(models)!=1:
    raise RuntimeError(f"Unexpected imported meshes {models}")
if library.does_asset_exist(path):
    raise RuntimeError("Import into a clean scratch project before replacing the shipped mesh.")
if not library.rename_asset(models[0],path):
    raise RuntimeError("Could not place imported mesh")
mesh = library.load_asset(path)
mesh.set_editor_property("physics_asset",physics)
slots = list(mesh.get_editor_property("materials"))
labels = set()
for slot in slots:
    label = str(slot.get_editor_property("material_slot_name"))
    labels.add(label)
    if label in ("bottom_denim","top_teal_woven"):
        assign = fabric
    elif label == "shoe":
        assign = shoes
    else:
        assign = library.load_asset(base+"/Materials/"+label)
    if not assign:
        raise RuntimeError("Missing material "+label)
    slot.set_editor_property("material_interface",assign)
mesh.set_editor_property("materials",slots)
expected = {"skin","lips","hair","iris","sclera","bottom_denim","top_teal_woven","shoe"}
if labels != expected or mesh.get_editor_property("skeleton") != skeleton:
    raise RuntimeError(f"Invalid resident mesh: {labels}, {mesh.get_editor_property('skeleton')}")
if not library.save_loaded_asset(mesh,False):
    raise RuntimeError("Could not save the completed everyday mesh")
ue.log(f"SABARMATI EVERYDAY VERIFIED: {path}, {skeleton.get_path_name()}, {sorted(labels)}")
