"""Author the hatch's invisible Chaos chassis and wheel skeleton as a rigged glTF."""
import json
from pathlib import Path
import struct
import sys


def build(destination):
    data = bytearray()
    views = []
    accessors = []

    def accessor(values, kind, fmt, component, target=None):
        data.extend(b"\0" * (-len(data) % 4))
        offset = len(data)
        pack = struct.Struct("<" + fmt)
        for row in values:
            data.extend(pack.pack(*row))
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data) - offset}
        if target:
            view["target"] = target
        views.append(view)
        result = dict(bufferView=len(views)-1, componentType=component, count=len(values), type=kind)
        if kind == "VEC3" and not accessors:
            result["min"] = [min(p[i] for p in values) for i in range(3)]
            result["max"] = [max(p[i] for p in values) for i in range(3)]
        accessors.append(result)
        return len(accessors)-1

    # Unreal takes glTF's Y as elevation and Z as its lateral axis. Chassis
    # is in metres here and its origin matches the existing gameplay pawn.
    positions = [(x, y, z) for z in (-.91, .91) for y in (-.38, .42) for x in (-2.04, 2.04)]
    quads = [(0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)]
    triangles = [(value,) for a, b, c, d in quads for value in (a, b, c, a, c, d)]
    vertices = {
        "POSITION": accessor(positions, "VEC3", "fff", 5126, 34962),
        "JOINTS_0": accessor([(0, 0, 0, 0)]*8, "VEC4", "HHHH", 5123, 34962),
        "WEIGHTS_0": accessor([(1, 0, 0, 0)]*8, "VEC4", "ffff", 5126, 34962),
    }
    indices = accessor(triangles, "SCALAR", "H", 5123, 34963)
    axles = [("FrontLeft", 1.22, -.77), ("RearLeft", -1.38, -.77),
             ("FrontRight", 1.22, .77), ("RearRight", -1.38, .77)]
    bindings = [(0., 0., 0.)] + [(x, -.364, y) for _, x, y in axles]
    inverse = accessor([(1,0,0,0, 0,1,0,0, 0,0,1,0, -x,-y,-z,1) for x,y,z in bindings],
                       "MAT4", "16f", 5126)
    nodes = [{"name": "Chassis", "children": [1, 2, 3, 4]}]
    nodes += [{"name": name, "translation": list(offset)} for (name, _, _), offset in zip(axles, bindings[1:])]
    nodes.append({"name": "SK_HatchPhysics", "skin": 0, "mesh": 0})
    encoded = json.dumps({
        "asset": {"version": "2.0", "generator": "Sabarmati Drive hatch physique"},
        "scene": 0, "scenes": [{"nodes": [0, 5]}], "nodes": nodes,
        "skins": [{"joints": [0, 1, 2, 3, 4], "skeleton": 0, "inverseBindMatrices": inverse}],
        "meshes": [{"name": "SK_HatchPhysics", "primitives": [{"attributes": vertices, "indices": indices}]}],
        "bufferViews": views, "accessors": accessors, "buffers": [{"byteLength": len(data)}],
    }, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    destination.write_bytes(struct.pack("<4sIIII", b"glTF", 2, 28+len(encoded)+len(data), len(encoded), 0x4e4f534a)
                            + encoded + struct.pack("<II", len(data), 0x004e4942) + data)


if __name__ == "__main__":
    build(Path(sys.argv[1]))
