"""Import the first-morning wearable and bake standard UE skeletal PBR material graphs."""
import os
from pathlib import Path
import unreal as ue

source = Path(os.environ["SABARMATI_WARDROBE_SOURCE"])
folder = "/Game/Sabarmati/Riverfront/People/Wardrobe"
assets = ue.EditorAssetLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
edit = ue.MaterialEditingLibrary

def imported(name, suffix, kind):
    path = folder + "/" + name
    if kind == ue.StaticMesh:
        path += "/StaticMeshes/" + name
    fresh_textures = kind == ue.Texture2D and os.environ.get("SABARMATI_REIMPORT_WARDROBE_ART") == "1"
    if fresh_textures or not assets.does_asset_exist(path):
        task = ue.AssetImportTask()
        task.set_editor_property("filename", str(source/(name+suffix)))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("replace_existing",fresh_textures)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tools.import_asset_tasks([task])
    loaded = assets.load_asset(path)
    if not isinstance(loaded,kind):
        raise RuntimeError(path)
    return loaded

watch = imported("SM_RiverWristwatch",".glb",ue.StaticMesh)
if watch.get_num_sections(0)!=4:
    raise RuntimeError("Missing wristwatch material sections")
fabric = imported("T_CottonWeave",".png",ue.Texture2D)
normal = imported("T_CottonWeave_N",".png",ue.Texture2D)
normal.set_editor_property("compression_settings",ue.TextureCompressionSettings.TC_NORMALMAP)
normal.set_editor_property("srgb",False)
assets.save_loaded_asset(normal,False)
face = imported("T_ResidentFace",".png",ue.Texture2D)

def surface(name, colour_map, normal_map, repetitions, roughness, skin=False, uv_box=None, painted=False, tailoring=None):
    path = folder + "/" + name
    mat = (assets.load_asset(path) if assets.does_asset_exist(path) else
           tools.create_asset(name,folder,ue.Material,ue.MaterialFactoryNew()))
    edit.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_skeletal_mesh",True)
    mat.set_editor_property("two_sided",True)
    def expression(kind,**params):
        obj = edit.create_material_expression(mat,kind)
        for key,val in params.items():
            obj.set_editor_property(key,val)
        return obj
    def join(first, output, second, pin):
        if not edit.connect_material_expressions(first,output,second,pin):
            raise RuntimeError(f"{name} {pin}")
    def output(obj, pin, prop):
        if not edit.connect_material_property(obj,pin,prop):
            raise RuntimeError(f"{name} {prop}")
    uv = expression(ue.MaterialExpressionTextureCoordinate,u_tiling=repetitions,v_tiling=repetitions)
    if uv_box:
        u,v,w,h = uv_box
        uv.set_editor_property("u_tiling",1/w)
        uv.set_editor_property("v_tiling",1/h)
        offset = expression(ue.MaterialExpressionConstant2Vector,r=-u/w,g=-v/h)
        shifted = expression(ue.MaterialExpressionAdd)
        join(uv,"",shifted,"A")
        join(offset,"",shifted,"B")
        uv = shifted
    tex = expression(ue.MaterialExpressionTextureSample,texture=colour_map)
    join(uv,"",tex,"UVs")
    tint = expression(ue.MaterialExpressionVectorParameter,parameter_name="BaseColorFactor",
                      default_value=ue.LinearColor(1,1,1,1) if painted else (ue.LinearColor(.36,.22,.13,1) if skin else ue.LinearColor(.47,.40,.30,1)))
    dye = expression(ue.MaterialExpressionMultiply)
    join(tex,"RGB",dye,"A")
    join(tint,"",dye,"B")
    colour = dye
    if tailoring:
        chart = expression(ue.MaterialExpressionTextureCoordinate,u_tiling=1/.070,v_tiling=1/.088)
        offset = expression(ue.MaterialExpressionConstant2Vector,r=-.811/.070,g=-.006/.088)
        shift = expression(ue.MaterialExpressionAdd)
        join(chart,"",shift,"A"); join(offset,"",shift,"B")
        sewing=expression(ue.MaterialExpressionTextureSample,texture=tailoring,
                          sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        join(shift,"",sewing,"UVs")
        # A continuous hem crosses both imported material sections. This also
        # removes the coarse polygon boundaries on the original sleeve edges.
        trousers=expression(ue.MaterialExpressionVectorParameter,parameter_name="TrouserDye",
                            default_value=ue.LinearColor(.09,.09,.10,1))
        garment=expression(ue.MaterialExpressionLinearInterpolate)
        join(trousers,"",garment,"A");join(tint,"",garment,"B");join(sewing,"G",garment,"Alpha")
        dyed_fabric=expression(ue.MaterialExpressionMultiply)
        join(garment,"",dyed_fabric,"A");join(tex,"RGB",dyed_fabric,"B")
        shade=expression(ue.MaterialExpressionMultiply)
        join(dyed_fabric,"",shade,"A"); join(sewing,"R",shade,"B")
        button=expression(ue.MaterialExpressionConstant3Vector,constant=ue.LinearColor(.45,.39,.28,1))
        colour=expression(ue.MaterialExpressionLinearInterpolate)
        join(shade,"",colour,"A"); join(button,"",colour,"B"); join(sewing,"B",colour,"Alpha")
    output(colour,"",ue.MaterialProperty.MP_BASE_COLOR)
    if skin: output(expression(ue.MaterialExpressionConstant,r=.32),"",ue.MaterialProperty.MP_SPECULAR)
    rough = expression(ue.MaterialExpressionScalarParameter,parameter_name="RoughnessFactor",default_value=roughness)
    output(rough,"",ue.MaterialProperty.MP_ROUGHNESS)
    if normal_map:
        weave = expression(ue.MaterialExpressionTextureSample,texture=normal_map,sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        join(uv,"",weave,"UVs")
        output(weave,"RGB",ue.MaterialProperty.MP_NORMAL)
    edit.layout_material_expressions(mat)
    edit.recompile_material(mat)
    if not assets.save_loaded_asset(mat,False):
        raise RuntimeError(name)
    return mat

surface("M_ResidentCotton",fabric,normal,640.,.82)
surface("M_ResidentSkin",face,None,1.,.58,True)
if (source/"SM_RiverSweptHair.glb").is_file():
    tailoring = imported("T_RiverTailoring",".png",ue.Texture2D)
    tailoring.set_editor_property("srgb",False)
    tailoring.set_editor_property("address_x",ue.TextureAddress.TA_CLAMP)
    tailoring.set_editor_property("address_y",ue.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(tailoring,False)
    surface("M_RiverTailoredCloth",fabric,normal,640.,.82,tailoring=tailoring)
    hair = imported("SM_RiverSweptHair",".glb",ue.StaticMesh)
    fibres = imported("T_RiverSweptHair",".png",ue.Texture2D)
    irises = imported("T_RiverEyes",".png",ue.Texture2D)
    finish = surface("M_RiverSweptHair",fibres,None,1.,.48,painted=True)
    surface("M_RiverEyes",irises,None,1.,.22,uv_box=(.752,.038,.068,.038),painted=True)
    hair.set_material(0,finish)
    if not assets.save_loaded_asset(hair,False):
        raise RuntimeError("SM_RiverSweptHair")
    ue.log("SABARMATI PUBLIC HAIR AND EYES SAVED")
if (source/"SK_RiverTailoredKurta.glb").is_file():
    old = "/Game/Sabarmati/Riverfront/People/Kurta/SK_RiverResidentKurta"
    skeleton = assets.load_asset(old+"/SkeletalMeshes/SK_RiverResidentKurta_Skeleton")
    physics = assets.load_asset(old+"/SkeletalMeshes/SK_RiverResidentKurta_PhysicsAsset")
    if not skeleton or not physics: raise RuntimeError("Missing public kurta rig")
    path=folder+"/Tailored/SK_FittedKurta"
    if not assets.does_asset_exist(path):
        pipeline=ue.InterchangeGenericAssetsPipeline()
        pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties").set_editor_property("skeleton",skeleton)
        pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials",False)
        pipeline.get_editor_property("animation_pipeline").set_editor_property("import_animations",False)
        params=ue.ImportAssetParameters()
        params.set_editor_property("is_automated",True)
        params.set_editor_property("override_pipelines",[ue.SoftObjectPath(pipeline.get_path_name())])
        manager=ue.InterchangeManager.get_interchange_manager_scripted()
        data=manager.create_source_data(str(source/"SK_RiverTailoredKurta.glb"))
        if not manager.import_asset(folder+"/Tailored",data,params): raise RuntimeError("Kurta import failed")
        models=[a for a in assets.list_assets(folder+"/Tailored",recursive=True) if isinstance(assets.load_asset(a),ue.SkeletalMesh)]
        if len(models)!=1 or not assets.rename_asset(models[0],path): raise RuntimeError("Kurta mesh path: "+str(models))
    mesh=assets.load_asset(path)
    mesh.set_editor_property("physics_asset",physics)
    slots=list(mesh.get_editor_property("materials"))
    if len(slots)!=10: raise RuntimeError("Expected ten populated kurta slots")
    for index,slot in enumerate(slots):
        label=str(slot.get_editor_property("material_slot_name"))
        original=assets.load_asset(old+"/Materials/"+label)
        if not original: raise RuntimeError("Unknown kurta material "+label)
        slot.set_editor_property("material_interface",original)
        slots[index]=slot
    mesh.set_editor_property("materials",slots)
    for entry in mesh.get_editor_property("materials"):
        m=entry.get_editor_property("material_interface")
        if not m or not m.get_path_name().startswith(old+"/Materials/"): raise RuntimeError("Unexpected kurta surface "+str(m))
    assigned=mesh.get_editor_property("skeleton")
    if assigned!=skeleton or not assets.save_loaded_asset(mesh,False):
        raise RuntimeError("Failed to retain the public kurta skeleton: "+str(assigned))
    ue.log("SABARMATI TAILORED KURTA VERIFIED "+assigned.get_path_name())
ue.log("SABARMATI MORNING WARDROBE ASSETS SAVED")
