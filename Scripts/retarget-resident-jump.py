"""Bake the installed Third Person template's jump phases onto both resident skeletons.

Use Unreal Editor with the template Mannequins available at /Game/Characters/Mannequins.
An authoring project can link its Content/Sabarmati to the Sabarmati Drive content folder.
"""
import unreal as ue


TEMPLATE = "/Game/Characters/Mannequins"
PEOPLE = "/Game/Sabarmati/Riverfront/People"
WORK = "/Game/Sabarmati/Avatar/RetargetWork"
CLIPS = ("MM_Jump", "MM_Fall_Loop", "MM_Land")
CHAINS = (
    ("Spine", "spine_01", "spine_05", "Spine", "Chest"),
    ("Head", "neck_01", "head", "Neck", "Head"),
    ("LeftArm", "upperarm_l", "hand_l", "Shoulderl", "Handl"),
    ("RightArm", "upperarm_r", "hand_r", "Shoulderr", "Handr"),
    ("LeftLeg", "thigh_l", "foot_l", "Hipl", "Footl"),
    ("RightLeg", "thigh_r", "foot_r", "Hipr", "Footr"),
)

tools = ue.AssetToolsHelpers.get_asset_tools()
library = ue.EditorAssetLibrary


def rig_for(label, mesh, source):
    rig = tools.create_asset("IK_" + label, WORK, ue.IKRigDefinition, ue.IKRigDefinitionFactory())
    controller = ue.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh) or not controller.set_retarget_root("pelvis" if source else "Pelvis"):
        raise RuntimeError(mesh.get_path_name())
    for chain, source_start, source_end, target_start, target_end in CHAINS:
        start, end = (source_start, source_end) if source else (target_start, target_end)
        if str(controller.add_retarget_chain(chain, start, end, "None")) != chain:
            raise RuntimeError(f"Could not define {chain} from {start} to {end}")
    return rig


source = library.load_asset(TEMPLATE + "/Meshes/SKM_Manny_Simple")
clips = [library.find_asset_data(f"{TEMPLATE}/Anims/Unarmed/Jump/{name}") for name in CLIPS]
if not source or not all(clip.is_valid() for clip in clips):
    raise RuntimeError("The Unreal Third Person template mesh and jump clips are required.")

source_rig = rig_for("Template", source, True)
for outfit, mesh_path in (("Everyday", "Wardrobe/Everyday/SK_RiverEveryday"),
                          ("Kurta", "Wardrobe/Tailored/SK_FittedKurta")):
    target = library.load_asset(f"{PEOPLE}/{mesh_path}")
    if not target:
        raise RuntimeError(mesh_path)
    target_rig = rig_for(outfit, target, False)
    retargeter = tools.create_asset("RTG_" + outfit, WORK, ue.IKRetargeter, ue.IKRetargetFactory())
    controller = ue.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(ue.RetargetSourceOrTarget.SOURCE, source_rig)
    controller.set_ik_rig(ue.RetargetSourceOrTarget.TARGET, target_rig)
    controller.add_default_ops()
    controller.auto_align_all_bones(ue.RetargetSourceOrTarget.TARGET)
    inputs = ue.IKRetargetBatchOperationInputs()
    for name, value in dict(assets_to_retarget=clips, source_mesh=source, target_mesh=target,
                            ik_retarget_asset=retargeter, target_path=f"{PEOPLE}/Motion/{outfit}",
                            include_referenced_assets=False, overwrite_existing_files=True).items():
        inputs.set_editor_property(name, value)
    results = ue.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if len(results) != len(clips):
        raise RuntimeError(f"Retargeted {len(results)} of {len(clips)} {outfit} clips")
    for result in results:
        sequence = result.get_asset()
        sequence.set_editor_property("enable_root_motion", False)
        sequence.set_editor_property("force_root_lock", True)
        if not library.save_loaded_asset(sequence, only_if_is_dirty=False):
            raise RuntimeError(sequence.get_path_name())
        ue.log(f"SABARMATI JUMP ASSET {sequence.get_path_name()}")
