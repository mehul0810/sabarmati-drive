"""Attach an assembled character Blueprint to the native Riverfront player.

Run in the Unreal Editor after retarget-authored-avatar.py. Set the asset paths
SABARMATI_AVATAR_ASSEMBLY, SABARMATI_AVATAR_BODY, SABARMATI_AVATAR_MOTION
(the motion folder) and SABARMATI_PLAYER_AVATAR (the new Blueprint package).
SABARMATI_AVATAR_YAW is the rotation from the assembly to the capsule forward.
The assembly must have its origin at the feet.
"""
import os

import unreal as ue


ASSETS = ue.EditorAssetLibrary
DATA = ue.SubobjectDataBlueprintFunctionLibrary
BODY_TAG = "RiverfrontAvatarBody"


def subobjects(subsystem, blueprint):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        info = DATA.get_data(handle)
        yield handle, info, DATA.get_object_for_blueprint(info, blueprint)


def run():
    assembly_path = os.environ["SABARMATI_AVATAR_ASSEMBLY"]
    player_path = os.environ["SABARMATI_PLAYER_AVATAR"]
    assembly = ASSETS.load_asset(assembly_path)
    body = ASSETS.load_asset(os.environ["SABARMATI_AVATAR_BODY"])
    motion_folder = os.environ["SABARMATI_AVATAR_MOTION"].rstrip("/")
    motion = ASSETS.load_asset(f"{motion_folder}/DA_AvatarMotion")
    if not isinstance(assembly, ue.Blueprint) or not isinstance(body, ue.SkeletalMesh) or not motion:
        raise RuntimeError("An assembled actor Blueprint, its skeletal body and retargeted motion are required.")
    if ASSETS.does_asset_exist(player_path):
        raise RuntimeError(f"Choose a new player Blueprint path. {player_path} already exists.")

    subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
    bodies = {obj.get_path_name(): obj for _, _, obj in subobjects(subsystem, assembly)
              if isinstance(obj, ue.SkeletalMeshComponent) and obj.get_skeletal_mesh_asset() == body}
    if len(bodies) != 1:
        raise RuntimeError("The assembly must contain exactly one component using the retargeted body mesh.")

    body_template = next(iter(bodies.values()))
    tags = body_template.get_editor_property("component_tags")
    if not any(str(tag) == BODY_TAG for tag in tags):
        body_template.set_editor_property("component_tags", [*tags, BODY_TAG])
        if not ue.BlueprintEditorLibrary.compile_blueprint(assembly):
            raise RuntimeError(f"Could not compile {assembly_path}.")
    assembly_class = ASSETS.load_blueprint_class(assembly_path)

    factory = ue.BlueprintFactory()
    factory.set_editor_property("parent_class", ue.RiverfrontCharacter)
    folder, name = player_path.rsplit("/", 1)
    avatar = ue.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, ue.Blueprint, factory)
    if not avatar:
        raise RuntimeError(f"Could not create {player_path}.")
    root, _, capsule = next((entry for entry in subobjects(subsystem, avatar)
                             if DATA.is_root_component(entry[1])), (None, None, None))
    if not isinstance(capsule, ue.CapsuleComponent):
        raise RuntimeError("The player Blueprint needs a capsule root.")

    params = ue.AddNewSubobjectParams(parent_handle=root, new_class=ue.ChildActorComponent,
                                      blueprint_context=avatar)
    handle, error = subsystem.add_new_subobject(params)
    if not DATA.is_handle_valid(handle):
        raise RuntimeError(f"Could not attach the assembled avatar: {error}")
    subsystem.rename_subobject(handle, "AssembledAvatar")
    visual = DATA.get_object_for_blueprint(DATA.get_data(handle), avatar)
    visual.set_child_actor_class(assembly_class)
    visual.set_editor_property("relative_location", ue.Vector(0, 0, -capsule.get_unscaled_capsule_half_height()))
    visual.set_editor_property("relative_rotation", ue.Rotator(yaw=float(os.environ.get("SABARMATI_AVATAR_YAW", "0"))))
    if not ue.BlueprintEditorLibrary.compile_blueprint(avatar):
        raise RuntimeError(f"Could not compile {player_path}.")
    avatar_class = ASSETS.load_blueprint_class(player_path)
    defaults = ue.get_default_object(avatar_class)
    defaults.set_editor_property("use_blueprint_avatar", True)
    defaults.set_editor_property("authored_avatar_motion", motion)
    for blueprint in (assembly, avatar):
        if not ASSETS.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {blueprint.get_path_name()}.")
    ue.log(f"SABARMATI_PLAYER_AVATAR {avatar_class.get_path_name()}")


run()
