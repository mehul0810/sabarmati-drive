"""Combine parts on the same skin into one mesh, without changing the rig or source file."""
from pathlib import Path
import json
import struct

project = Path(__file__).resolve().parent.parent
data = (project / "SourceAssets/fictional-rigged.glb").read_bytes()
header, ver, size, json_length, kind = struct.unpack_from("<4sIII4s", data)
assert (header, ver, size, kind) == (b"glTF", 2, len(data), b"JSON")
end = 20 + json_length
scene = json.loads(data[20:end])
nodes = scene["nodes"]
parts = [i for i, n in enumerate(nodes) if "mesh" in n]
assert len(parts) == 12 and all(nodes[i].get("skin") == 0 for i in parts)
assert all(not set(nodes[i]) & {"matrix", "translation", "rotation", "scale", "children"} for i in parts)
assert all(ch["target"].get("node") not in parts for a in scene["animations"] for ch in a["channels"])
mesh = {"name": "SK_Sabarmati", "primitives": [p for m in scene["meshes"] for p in m["primitives"]]}
scene["meshes"] = [mesh]
keep = parts[0]
nodes[keep] = {"name": "SK_Sabarmati", "mesh": 0, "skin": 0}
unused = set(parts[1:])
for i in unused:
    nodes[i] = {"name": f"Unused_{i}"}
for n in nodes:
    if "children" in n:
        n["children"] = [i for i in n["children"] if i not in unused]
for entry in scene["scenes"]:
    entry["nodes"] = [i for i in entry["nodes"] if i not in unused]
for anim in scene["animations"]:
    anim["name"] = "A_" + anim["name"]
packed = json.dumps(scene, ensure_ascii=True, separators=(",", ":")).encode("ascii")
packed += b" " * (-len(packed) % 4)
out = struct.pack("<4sIII4s", b"glTF", 2, 20 + len(packed) + len(data[end:]), len(packed), b"JSON")
path = project / "Saved/LookSources/SK_Sabarmati.glb"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_bytes(out + packed + data[end:])
print(f"Prepared {len(mesh['primitives'])} material primitives on one skin and {len(scene['animations'])} animations.")
