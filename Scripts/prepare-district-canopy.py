"""Reduce Poly Haven's CC0 Jacaranda for native SM5. Input is the official 1K glTF and sidecars."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def prepare(source,output,optimizer):
    original=source/"jacaranda_tree_1k.gltf"
    for path,md5 in ((original,"8fed93ff7cbdaac0f5c199ef1fd0f694"),(source/"jacaranda_tree.bin","ae25eacd5e67ea2e9bbbaa2854b6ed99")):
        with path.open("rb") as f:
            h=hashlib.md5()
            for block in iter(lambda:f.read(1024*1024),b""):h.update(block)
        if h.hexdigest()!=md5:raise ValueError(f"Unrecognised source {path.name}")
    output.mkdir(parents=True,exist_ok=True)
    file=output/"SM_Jacaranda.gltf"
    subprocess.run([str(optimizer),"-i",str(original),"-o",str(file),"-si","0.021","-sa","-noq","-km"],check=True)
    asset=json.loads(file.read_text())
    for m in asset["meshes"]:m["name"]="SM_Jacaranda"
    for n in asset["nodes"]:
        if "mesh" in n:n["name"]="SM_Jacaranda"
    for m in asset["materials"]:
        # Use the supplied leaf opacity separately in a shared Unreal masked material.
        m["alphaMode"]="OPAQUE";m.pop("extensions",None)
    file.write_text(json.dumps(asset,separators=(",",":")))
    print(sum(asset["accessors"][p["indices"]]["count"]//3 for m in asset["meshes"] for p in m["primitives"]),"triangles")
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("source",type=Path);p.add_argument("output",type=Path);p.add_argument("gltfpack",type=Path)
    a=p.parse_args();prepare(a.source,a.output,a.gltfpack.resolve())
