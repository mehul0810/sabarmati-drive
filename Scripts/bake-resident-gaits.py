"""Bake planted locomotion plus the installed Third Person template's unarmed arm motion.

Run in Unreal Editor with the template Mannequins under /Game/Characters/Mannequins.
The baked sequences keep only resident skeleton tracks and run entirely offline.
"""
import sys
from pathlib import Path

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
from resident_gait import GAITS, SAMPLE_RATE, TRAVERSALS, add, gait_pose, idle_pose, scale, sub, traversal_pose, unit

PEOPLE = "/Game/Sabarmati/Riverfront/People"
LIB = ue.AnimPoseExtensions
OPTIONS = ue.AnimPoseEvaluationOptions()
TEMPLATE = "/Game/Characters/Mannequins/Anims/Unarmed"


def sample_arms(mode, first, last, count):
    path = f"{TEMPLATE}/{mode}/MF_Unarmed_{mode}_Fwd"
    clip = ue.load_asset(path)
    if not clip:
        raise RuntimeError(f"Missing the installed Third Person template clip {path}")
    keys = []
    for i in range(count+1):
        time = clip.get_play_length() * (first+(last-first)*i/count)
        sample = LIB.get_anim_pose_at_time(clip, time, OPTIONS)
        arms = {}
        for side in ("l", "r"):
            points = []
            for bone in ("upperarm", "lowerarm", "hand", "middle_01"):
                p = LIB.get_bone_pose(sample, f"{bone}_{side}", ue.AnimPoseSpaces.WORLD).translation
                points.append((p.x, p.z, p.y))
            arms[side] = [unit(sub(b,a)) for a,b in zip(points,points[1:])]
        keys.append(arms)
    result = []
    for i, key in enumerate(keys):
        arms = {}
        for side, directions in key.items():
            # Remove the small endpoint mismatch between measured left-foot strikes.
            a, b = keys[0][side], keys[-1][side]
            upper, fore, palm = [unit(add(d, scale(sub(x,y), i/count-.5)))
                                for d,x,y in zip(directions,a,b)]
            upper = (.66*upper[0], upper[1], upper[2]*(.95 if mode == "Jog" else .72))
            lower = (.70*fore[0], fore[1]-(.04 if mode == "Jog" else .34),
                     fore[2]*(.92 if mode == "Jog" else .62))
            arms[side] = (upper, lower, add(unit(lower), scale(sub(palm,fore), .54)))
        result.append(arms)
    return result


def reference_rig(sequence):
    source = LIB.get_anim_pose_at_time(sequence, 0., OPTIONS)
    rest, local = {}, {}
    for bone_name in LIB.get_bone_names(source):
        name_str = str(bone_name)
        transform = LIB.get_ref_bone_pose(source, bone_name, ue.AnimPoseSpaces.WORLD)
        p = transform.translation
        if abs(transform.rotation.w) < .99999:
            raise ValueError("Expected axis-aligned resident bind pose on " + name_str)
        rest[name_str] = (p.x*.01, p.z*.01, p.y*.01)
        local[name_str] = LIB.get_ref_bone_pose(source, bone_name, ue.AnimPoseSpaces.LOCAL)
    return rest, local


def bake(sequence, local, samples, fps):
    count = len(samples)-1
    controller = sequence.controller
    controller.open_bracket("Bake resident motion", False)
    try:
        controller.set_frame_rate(ue.FrameRate(numerator=fps, denominator=1), False)
        controller.set_number_of_frames(ue.FrameNumber(value=count), False)
        controller.remove_all_bone_tracks(False)
        for bone_name, transform in local.items():
            if not controller.add_bone_curve(bone_name, False):
                raise RuntimeError("Unable to add track " + bone_name)
            positions, rotations = [], []
            for pose, pelvis in samples:
                x, y, z, w = pose[bone_name]
                # glTF to Unreal swaps Y and Z, reversing the rotation axes.
                rotations.append(ue.Quat(-x, -z, -y, w))
                position = ue.Vector(100*pelvis[0], 100*pelvis[2], 100*pelvis[1]) if bone_name == "Pelvis" else transform.translation
                positions.append(position)
            if not controller.set_bone_track_keys(bone_name, positions, rotations, [transform.scale3d]*len(samples), False):
                raise RuntimeError("Unable to write track " + bone_name)
    finally:
        controller.close_bracket(False)
    if not ue.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False):
        raise RuntimeError("Unable to save " + sequence.get_path_name())
    print("RESIDENT_MOTION_SAVED", sequence.get_path_name(), count, fps)


for outfit, folder in (("Everyday", PEOPLE), ("Kurta", PEOPLE + "/Kurta")):
    reference = ue.load_asset(folder + "/A_Idle")
    rest, local = reference_rig(reference)
    for clip in ("A_Idle", *GAITS):
        sequence = ue.load_asset(folder + "/" + clip)
        period, fps = (3.8, 30) if clip == "A_Idle" else (GAITS[clip][0], SAMPLE_RATE)
        count = round(period*fps)
        if clip == "A_Idle":
            samples = [idle_pose(i/count, rest) for i in range(count+1)]
        else:
            # Use one left-to-left stride from each multistride source clip.
            arms = sample_arms("Jog", .286, .613, count) if clip == "A_Jog" else sample_arms("Walk", .400, .900, count)
            samples = [gait_pose(clip, i/count, rest, arm_sample) for i,arm_sample in enumerate(arms)]
        bake(sequence, local, samples, fps)
    motion_folder = f"{PEOPLE}/Motion/{outfit}"
    for clip, (period, _) in TRAVERSALS.items():
        sequence = ue.EditorAssetLibrary.load_asset(motion_folder + "/" + clip)
        if not sequence:
            factory = ue.AnimSequenceFactory()
            factory.set_editor_property("target_skeleton", reference.get_editor_property("skeleton"))
            sequence = ue.AssetToolsHelpers.get_asset_tools().create_asset(clip, motion_folder, ue.AnimSequence, factory)
        fps = 30
        count = round(period*fps)
        bake(sequence, local, [traversal_pose(clip, i/count, rest) for i in range(count+1)], fps)
