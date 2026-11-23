"""Reduce the CC0 tree by thinning whole connected leaf islands, not arbitrary faces."""
from array import array
from copy import deepcopy
import json
from pathlib import Path
import struct

folder = Path(__file__).resolve().parent.parent / "Saved/LookSources/tree-small-02"
asset = json.loads((folder / "tree_small_02_1k.gltf").read_text())
source = (folder / "tree_small_02.bin").read_bytes()
accessors = asset["accessors"]
views = asset["bufferViews"]

def payload(i):
    accessor = accessors[i]
    view = views[accessor["bufferView"]]
    assert not accessor.get("byteOffset") and not view.get("byteStride")
    offset = view.get("byteOffset", 0)
    return memoryview(source)[offset:offset + view["byteLength"]]

leaf = next(p for p in asset["meshes"][0]["primitives"]
            if "leaves" in asset["materials"][p["material"]]["name"])
triangles = payload(leaf["indices"]).cast("I")
positions = leaf["attributes"]["POSITION"]
parents = array("I", range(accessors[positions]["count"]))

def group(v):
    while parents[v] != v:
        parents[v] = parents[parents[v]]
        v = parents[v]
    return v

for i in range(0, len(triangles), 3):
    a, b, c = (group(triangles[i + delta]) for delta in (0, 1, 2))
    m = min(a, b, c)
    parents[a] = parents[b] = parents[c] = m

# Stable spread across the model, preserving geometry within each selected island.
def keep(v):
    n = (group(v) ^ 0x9E3779B9) & 0xFFFFFFFF
    n ^= n >> 16
    n = (n * 0x7FEB352D) & 0xFFFFFFFF
    n ^= n >> 15
    return n % 100 < 13

output_indices = array("I")
remap = {}
for i in range(0, len(triangles), 3):
    if keep(triangles[i]):
        for delta in (0, 1, 2):
            v = triangles[i + delta]
            output_indices.append(remap.setdefault(v, len(remap)))
assert 80000 < len(output_indices) // 3 < 350000
new_payloads = {leaf["indices"]: (output_indices.tobytes(), len(output_indices))}
for semantic, accessor_id in leaf["attributes"].items():
    stride = 8 if "TEXCOORD" in semantic else 12
    data = payload(accessor_id)
    new_payloads[accessor_id] = (
        b"".join(data[v * stride:(v + 1) * stride] for v in remap),
        len(remap),
    )
    if semantic == "POSITION":
        flat = memoryview(new_payloads[accessor_id][0]).cast("f")
        accessors[accessor_id]["min"] = [min(flat[a::3]) for a in range(3)]
        accessors[accessor_id]["max"] = [max(flat[a::3]) for a in range(3)]

output = bytearray()
old_views = deepcopy(views)
for i, accessor in enumerate(accessors):
    v = accessor["bufferView"]
    offset = old_views[v].get("byteOffset", 0)
    data = source[offset:offset + old_views[v]["byteLength"]]
    if i in new_payloads:
        data, accessor["count"] = new_payloads[i]
    output.extend(b"\0" * (-len(output) % 4))
    views[v]["byteOffset"] = len(output)
    views[v]["byteLength"] = len(data)
    output.extend(data)

assert len({a["bufferView"] for a in accessors}) == len(views)
asset["materials"][leaf["material"]].pop("alphaMode", None)  # geometry, source JPEG has no alpha
asset["buffers"][0] = {"byteLength": len(output), "uri": "SM_RiverfrontTree.bin"}
asset["meshes"][0]["name"] = asset["nodes"][0]["name"] = "SM_RiverfrontTree"
(folder / "SM_RiverfrontTree.bin").write_bytes(output)
(folder / "SM_RiverfrontTree.gltf").write_text(json.dumps(asset))
print("Leaf triangles: %d -> %d; binary %.1f MiB" %
      (len(triangles) // 3, len(output_indices) // 3, len(output) / (1024 * 1024)))
