"""Reproject MakeHuman's CC0 brown_eye.png onto the public resident's two eye charts.

Run with SK_RiverResident.glb, brown_eye.png and an output directory. The baked
map is shared by iris and sclera, including the narrow triangles across the lid.
"""
import argparse
from pathlib import Path
import runpy

import numpy as np
from PIL import Image


def bake(resident, artwork, out):
    gltf = runpy.run_path(str(Path(__file__).with_name("prepare-riverfront-hatch.py")))
    doc, raw = gltf["read_glb"](resident)
    atlas = np.asarray(Image.open(artwork).convert("RGB"), dtype=float)
    if atlas.shape != (1024, 1024, 3):
        raise ValueError("Expected MakeHuman's 1024 x 1024 system brown-eye atlas")

    # The two forward-facing eye charts in the source have 116-texel iris radii.
    # Treat each colour channel in display space, preserving the fine scleral vessels
    # and tempering the strongly red source iris to a deep warm brown.
    result = np.full((2048, 2048, 3), [169, 161, 153], dtype=np.uint8)
    for section in doc["meshes"][0]["primitives"]:
        if doc["materials"][section["material"]]["name"] not in {"iris", "sclera"}:
            continue
        attributes = section["attributes"]

        def read(index):
            return np.array(list(gltf["vectors"](doc, raw, index)))

        positions = read(attributes["POSITION"])
        chart = (read(attributes["TEXCOORD_0"]) - [.752, .038]) / [.068, .038] * result.shape[0]
        for tri in read(section["indices"]).reshape(-1, 3):
            xy = chart[tri]
            x0, y0 = np.maximum(0, np.floor(xy.min(axis=0) - 2).astype(int))
            x1, y1 = np.minimum(result.shape[0], np.ceil(xy.max(axis=0) + 2).astype(int))
            dx, dy = xy[1] - xy[0]
            ex, ey = xy[2] - xy[0]
            area = dx * ey - ex * dy
            if abs(area) < 1.e-5:
                continue
            x, y = np.meshgrid(np.arange(x0, x1) + .5 - xy[0, 0], np.arange(y0, y1) + .5 - xy[0, 1])
            b, c = (x * ey - y * ex) / area, (dx * y - dy * x) / area
            bary = np.stack([1 - b - c, b, c], axis=-1)
            point = bary @ positions[tri]
            left = positions[tri[0], 0] > 0
            center = [724., 306.] if left else [299., 726.]
            # Iris diameter 12 mm in object space. Retain the existing forward gaze.
            offset = point[:, :, :2] - ([.0314, 1.57655] if left else [-.0314, 1.57655])
            sample = np.clip(center + offset * [116/.006, -116/.006], 0, 1022.)
            sx, sy = sample[:, :, 0].astype(int), sample[:, :, 1].astype(int)
            fx, fy = (sample[:, :, 0] - sx)[:, :, None], (sample[:, :, 1] - sy)[:, :, None]
            rgb = ((atlas[sy,sx]*(1-fx) + atlas[sy,sx+1]*fx)*(1-fy) +
                   (atlas[sy+1,sx]*(1-fx) + atlas[sy+1,sx+1]*fx)*fy)
            iris = np.clip((.0063 - np.linalg.norm(offset, axis=2)) / .0007, 0, 1)
            red = rgb[:, :, 0].copy()
            rgb[:, :, 1] += .28 * iris * np.maximum(0, red - rgb[:, :, 1])
            rgb[:, :, 2] += .18 * iris * np.maximum(0, red - rgb[:, :, 2])
            # The back half never receives a second iris as the eye turns away.
            rgb[point[:, :, 2] < .130] = [165, 155, 147]
            inside = bary.min(axis=-1) >= -.01
            result[y0:y1,x0:x1][inside] = np.clip(rgb[inside],0,255).astype(np.uint8)
    out.mkdir(parents=True, exist_ok=True)
    Image.fromarray(result).save(out / "T_RiverEyes.png")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resident", type=Path)
    parser.add_argument("artwork", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    bake(args.resident, args.artwork, args.out)
