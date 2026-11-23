#!/usr/bin/env python3
"""Build an original badge-free crossover using the project's CC0 hatch mesh."""

from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parents[1]
glb = runpy.run_path(str(ROOT / "Scripts/prepare-riverfront-hatch.py"))

def smooth(v, lo, hi):
    t = max(0.0, min(1.0, (v - lo) / (hi - lo)))
    return t * t * (3.0 - 2.0 * t)

def cross(a, b):
    return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])

class Crossover:
    @staticmethod
    def point(v):
        x, y, z = v
        # Keep the proven 2.60 m axle spacing and wheel-arch positions.
        # Broad, tall centre, squarer bumper ends, and shorter roof taper.
        hood = smooth(z, .36, 1.43)
        tail = smooth(-z, .70, 1.62)
        height = smooth(y, .64, 1.30)
        y += height * (.18 + .058 * hood + .028 * tail)
        x *= 1.0 + .046 * height
        z += .16 * smooth(z, 1.55, 2.05) - .09 * smooth(-z, 1.71, 2.05)
        return x, y, z

    @classmethod
    def normal(cls, p, n):
        # Cofactors of the local derivative transform normals without breaking the
        # relationship between the windshield, lamps and the deformed sheet metal.
        e = 1e-4
        def shift(i, d):
            return cls.point(tuple(v + d if j == i else v for j, v in enumerate(p)))
        a, b, c = [tuple((u-v)/(2*e) for u,v in zip(shift(i,e), shift(i,-e))) for i in range(3)]
        bc, ca, ab = cross(b,c), cross(c,a), cross(a,b)
        return tuple(n[0]*bc[i] + n[1]*ca[i] + n[2]*ab[i] for i in range(3))

def main():
    src, data = glb["read_glb"](ROOT / "SourceAssets/Sabarmati-Right-Hand-Drive-Hatchback.glb")
    nodes = {node["name"]: i for i, node in enumerate(src["nodes"])}
    root = src["nodes"][nodes["hatchback"]]
    steering = src["nodes"][nodes["steering_wheel"]]
    assert steering["translation"][0] < 0
    body = [(i, ()) for i in root["children"] if "mesh" in src["nodes"][i]]
    body.append((steering["children"][0], (nodes["steering_wheel"],)))
    # Keep paint material names distinct so importing this GLB can't recolor the hatch.
    colors = {"paint_body", "paint"}
    for material in src["materials"]:
        if material["name"] in colors:
            material["pbrMetallicRoughness"].update(
                baseColorFactor=[0.23, 0.125, 0.075, 1.0], metallicFactor=.28, roughnessFactor=.32
            )
        material["name"] = "crossover_" + material["name"]
    dest = ROOT / "Saved/LookSources/hatch"
    dest.mkdir(parents=True, exist_ok=True)
    name = "SM_SabarmatiCrossover"
    glb["write_parts"](src, data, body, name, dest / f"{name}.glb", deform=Crossover)

if __name__ == "__main__":
    main()
