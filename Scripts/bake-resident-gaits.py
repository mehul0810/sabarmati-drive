"""Rebuild native locomotion sequences on both resident skeletons using Unreal's animation controller."""
import sys
from pathlib import Path

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
from resident_gait import GAITS, SAMPLE_RATE, gait_pose, idle_pose

PEOPLE = "/Game/Sabarmati/Riverfront/People"
LIB = ue.AnimPoseExtensions


def bake(path, name):
    sequence = ue.load_asset(path)
    if not sequence:
        raise RuntimeError("Missing resident animation " + path)
    source = LIB.get_anim_pose_at_time(sequence, 0., ue.AnimPoseEvaluationOptions())
    rest, local = {}, {}
    for bone_name in LIB.get_bone_names(source):
        name_str = str(bone_name)
        transform = LIB.get_ref_bone_pose(source, bone_name, ue.AnimPoseSpaces.WORLD)
        p = transform.translation
        if abs(transform.rotation.w) < .99999:
            raise ValueError("Expected axis-aligned resident bind pose on " + name_str)
        rest[name_str] = (p.x*.01, p.z*.01, p.y*.01)
        local[name_str] = LIB.get_ref_bone_pose(source, bone_name, ue.AnimPoseSpaces.LOCAL)
    period, fps = (3.8, 30) if name == "A_Idle" else (GAITS[name][0], SAMPLE_RATE)
    count = round(period*fps)
    samples = [(idle_pose(i/count, rest) if name == "A_Idle" else gait_pose(name, i/count, rest))
               for i in range(count+1)]
    controller = sequence.controller
    controller.open_bracket("Bake planted resident gait", False)
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
        raise RuntimeError("Unable to save " + path)
    print("RESIDENT_GAIT_SAVED", path, count, fps)


for folder in (PEOPLE, PEOPLE + "/Kurta"):
    for clip in ("A_Idle", *GAITS):
        bake(folder + "/" + clip, clip)
