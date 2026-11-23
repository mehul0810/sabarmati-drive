"""Import the original generated waveforms and attach near/far spatial attenuation."""
import os
from pathlib import Path
import unreal as ue

dest="/Game/Sabarmati/Riverfront/Audio"
src=Path(os.environ.get("SABARMATI_AUDIO_DIR",str(Path(ue.Paths.project_saved_dir())/"DistrictAudio")))
tools=ue.AssetToolsHelpers.get_asset_tools()
assets=ue.EditorAssetLibrary
def attenuation(name,inner,outer):
    path=dest+"/"+name
    asset=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,dest,ue.SoundAttenuation,ue.SoundAttenuationFactory())
    shape=asset.get_editor_property("attenuation")
    shape.set_editor_property("attenuation_shape_extents",ue.Vector(inner,inner,inner))
    shape.set_editor_property("falloff_distance",outer-inner)
    asset.set_editor_property("attenuation",shape)
    if not assets.save_loaded_asset(asset,False): raise RuntimeError(path)
    return asset
near=attenuation("ATT_Near",125,2200)
far=attenuation("ATT_Street",420,8800)
loops={"Engine","Tyres","Rain","River","Wind"}
for name in ("Engine","Tyres","Horn","Shift","FootStone","FootGrit","Rain","River","Wind","TeaBell","Contact"):
    key="S_"+name
    file=src/(key+".wav")
    if not file.is_file(): raise RuntimeError(str(file))
    task=ue.AssetImportTask()
    for prop,val in (("filename",str(file)),("destination_name",key),("destination_path",dest),("automated",True),("replace_existing",True),("save",True)):
        task.set_editor_property(prop,val)
    tools.import_asset_tasks([task])
    wave=assets.load_asset(dest+"/"+key)
    if not isinstance(wave,ue.SoundWave): raise RuntimeError(key)
    wave.set_editor_property("looping",name in loops)
    wave.set_editor_property("attenuation_settings",near if name in ("FootStone","FootGrit","TeaBell","Shift","Contact") else far)
    if not assets.save_loaded_asset(wave,False): raise RuntimeError(key)
ue.log("SABARMATI SPATIAL AUDIO SAVED")
