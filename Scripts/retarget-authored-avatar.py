"""Retarget Sabarmati's capsule-driven motions onto an Epic-style avatar rig in UE 5.8.

Set SABARMATI_AVATAR_BODY to the assembled body's Unreal asset path. Set
SABARMATI_AVATAR_MOTION to its dedicated output package folder. Run in
Unreal Editor with IKRig and the Python Script plugin enabled.
"""
import os

import unreal as ue


PEOPLE = "/Game/Sabarmati/Riverfront/People"
FOLDER = os.environ["SABARMATI_AVATAR_MOTION"].rstrip("/")
TARGET = os.environ["SABARMATI_AVATAR_BODY"]
MOTION = PEOPLE + "/Motion/Everyday"
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
ASSETS = ue.EditorAssetLibrary
CHAINS = (
    ("Spine", "Spine", "Chest", "spine_01", "spine_05"),
    ("Head", "Neck", "Head", "neck_01", "head"),
    ("LeftArm", "Shoulderl", "Handl", "upperarm_l", "hand_l"),
    ("RightArm", "Shoulderr", "Handr", "upperarm_r", "hand_r"),
    ("LeftLeg", "Hipl", "Footl", "thigh_l", "foot_l"),
    ("RightLeg", "Hipr", "Footr", "thigh_r", "foot_r"),
)
CLIPS = [f"{PEOPLE}/{name}" for name in ("A_Idle", "A_Walk", "A_Jog")]
CLIPS += [f"{MOTION}/{name}" for name in
          ("MM_Jump", "MM_Fall_Loop", "MM_Land", "A_FenceVault", "A_WallMantle")]


def asset(name, kind, factory):
    folder = FOLDER + "/Authoring"
    path = f"{folder}/{name}"
    if ASSETS.does_asset_exist(path):
        return ASSETS.load_asset(path)
    return TOOLS.create_asset(name, folder, kind, factory)


def rig_for(mesh, is_source):
    suffix = "Resident" if is_source else "Avatar"
    rig = asset("IK_" + suffix, ue.IKRigDefinition, ue.IKRigDefinitionFactory())
    controller = ue.IKRigController.get_controller(rig)
    controller.set_skeletal_mesh(mesh)
    controller.set_retarget_root("Pelvis" if is_source else "pelvis")
    for chain, source_start, source_end, target_start, target_end in CHAINS:
        start, end = (source_start, source_end) if is_source else (target_start, target_end)
        if (str(controller.get_retarget_chain_start_bone(chain)).lower() == start.lower() and
                str(controller.get_retarget_chain_end_bone(chain)).lower() == end.lower()):
            continue
        controller.remove_retarget_chain(chain)
        if str(controller.add_retarget_chain(chain, start, end, "None")) != chain:
            raise RuntimeError(f"Could not map {chain}: {start} to {end}")
    return rig


def run():
    source = ue.load_asset(f"{PEOPLE}/Wardrobe/Everyday/SK_RiverEveryday")
    target = ue.load_asset(TARGET)
    clips = [ASSETS.find_asset_data(path) for path in CLIPS]
    if not source or not target or not all(clip.is_valid() for clip in clips):
        raise RuntimeError("The assembled body and all Riverfront motion assets are required.")

    source_rig = rig_for(source, True)
    target_rig = rig_for(target, False)
    retargeter = asset("RTG_ResidentToAvatar", ue.IKRetargeter, ue.IKRetargetFactory())
    controller = ue.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(ue.RetargetSourceOrTarget.SOURCE, source_rig)
    controller.set_ik_rig(ue.RetargetSourceOrTarget.TARGET, target_rig)
    if controller.get_num_retarget_ops() == 0:
        controller.add_default_ops()
    controller.auto_align_all_bones(ue.RetargetSourceOrTarget.TARGET)

    settings = ue.IKRetargetBatchOperationInputs()
    for name, value in dict(assets_to_retarget=clips, source_mesh=source, target_mesh=target,
                            ik_retarget_asset=retargeter, target_path=FOLDER,
                            include_referenced_assets=False, overwrite_existing_files=True).items():
        settings.set_editor_property(name, value)
    results = ue.IKRetargetBatchOperation.run_batch_retarget(settings)
    if len(results) != len(clips):
        raise RuntimeError(f"Retargeted {len(results)} of {len(clips)} required clips.")
    for result in results:
        clip = result.get_asset()
        # The Character Movement capsule owns movement, including the obstacle path.
        clip.set_editor_property("enable_root_motion", False)
        clip.set_editor_property("force_root_lock", True)
        if not ASSETS.save_loaded_asset(clip, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {clip.get_path_name()}")
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    ue.SystemLibrary.execute_console_command(world, f"Sabarmati.BuildAuthoredAvatarMotion {FOLDER}")
    motion = ASSETS.load_asset(f"{FOLDER}/DA_AvatarMotion")
    if not motion:
        raise RuntimeError("The retargeted motion data asset was not saved.")
    ue.log(f"SABARMATI_AUTHORED_MOTION {motion.get_path_name()}")


run()
