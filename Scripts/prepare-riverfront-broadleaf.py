"""Rebuild a compact canopy from the CC0 source's actual leaf locations."""
from array import array
from copy import deepcopy
import json
from math import cos, sin, tau
from pathlib import Path
from random import Random

folder = Path(__file__).resolve().parent.parent / "Saved/LookSources/tree-small-02"
asset = json.loads((folder / "tree_small_02_1k.gltf").read_text())
source = (folder / "tree_small_02.bin").read_bytes()
primitives = asset["meshes"][0]["primitives"]
branches, leaves, trunk = primitives
assert "leaves" in asset["materials"][leaves["material"]]["name"]

def source_view(index):
    v = asset["bufferViews"][index]
    start = v.get("byteOffset", 0)
    return source[start:start + v["byteLength"]]

original_positions = memoryview(source_view(asset["accessors"][leaves["attributes"]["POSITION"]]["bufferView"])).cast("f")
rng = Random(291)
points, normals, colors, triangles = array("f"), array("f"), array("f"), array("I")
cells = {}
# Two sparsely distributed leaves per 10 cm cell, oversample then de-clump.
for j in rng.sample(range(len(original_positions) // 3), 52000):
    origin = original_positions[j * 3:j * 3 + 3]
    key = tuple(int(v * 10) for v in origin)
    if cells.get(key, 0) >= 2:
        continue
    cells[key] = 1 + cells.get(key, 0)
    yaw, pitch = rng.random() * tau, rng.uniform(-.88, .88)
    dx, dz = cos(yaw), sin(yaw)
    axis = (dx * cos(pitch), sin(pitch), dz * cos(pitch))
    side = (-dz, 0, dx)
    normal = (-dx * sin(pitch), cos(pitch), -dz * sin(pitch))
    length, width = rng.uniform(.036, .071), rng.uniform(.012, .024)
    tone = rng.uniform(.65, 1.12)
    k = len(points) // 3
    for along, across, bend in [(1,0,0),(.14,1,0),(-1,0,0),(.14,-1,0),(.18,0,.007)]:
        points.extend(origin[i] + axis[i] * length * along + side[i] * width * across + normal[i] * bend
                      for i in range(3))
        normals.extend(normal)
        colors.extend((.071 * tone, .139 * tone, .030 * tone, 1.))
    triangles.extend(k + i for i in (0,4,1,1,4,2,2,4,3,3,4,0))
    if k >= 39995:
        break

buffer = bytearray()
accessors, views = [], []
def append(data, accessor, target):
    buffer.extend(b"\0" * (-len(buffer) % 4))
    views.append({"buffer": 0, "byteOffset": len(buffer), "byteLength": len(data), "target": target})
    accessor["bufferView"] = len(views) - 1
    accessors.append(accessor)
    buffer.extend(data)
    return len(accessors) - 1

def preserve(p):
    p = deepcopy(p)
    refs = {}
    for old in [p["indices"], *p["attributes"].values()]:
        a = deepcopy(asset["accessors"][old])
        old_view = a["bufferView"]
        refs[old] = append(source_view(old_view), a, asset["bufferViews"][old_view].get("target", 34962))
    p["indices"] = refs[p["indices"]]
    p["attributes"] = {name: refs[old] for name, old in p["attributes"].items()}
    return p

def floats(data, dimensions, bounded=False):
    a = {"componentType": 5126, "count": len(data) // dimensions, "type": f"VEC{dimensions}"}
    if bounded:
        a["min"] = [min(data[i::dimensions]) for i in range(dimensions)]
        a["max"] = [max(data[i::dimensions]) for i in range(dimensions)]
    return append(data.tobytes(), a, 34962)

leaf_attrs = {"POSITION": floats(points, 3, True), "NORMAL": floats(normals, 3), "COLOR_0": floats(colors, 4)}
leaf_indices = append(triangles.tobytes(), {"componentType": 5125, "count": len(triangles), "type": "SCALAR"}, 34963)
asset["materials"][leaves["material"]] = {
    "name": "broadleaf_canopy", "doubleSided": True,
    "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": .78},
}
new_name = "SM_RiverfrontBroadleaf"
asset["meshes"][0] = {"name": new_name, "primitives": [
    preserve(branches), {"attributes": leaf_attrs, "indices": leaf_indices, "material": leaves["material"]}, preserve(trunk)]}
asset["nodes"][0]["name"] = new_name
asset["accessors"], asset["bufferViews"] = accessors, views
asset["buffers"] = [{"byteLength": len(buffer), "uri": new_name + ".bin"}]
(folder / (new_name + ".bin")).write_bytes(buffer)
(folder / (new_name + ".gltf")).write_text(json.dumps(asset))
print(f"Canopy: {len(points)//15} leaves, {len(triangles)//3} triangles; binary {len(buffer)/1048576:.1f} MiB.")
