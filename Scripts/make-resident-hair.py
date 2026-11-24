"""Fit two MakeHuman CC0 system hairstyles to the playable hm08 head.

Usage: python make-resident-hair.py BASE_OBJ SYSTEM_PACK_DIRECTORY OUTPUT_DIRECTORY
Keep the official short02/short04 OBJ UVs and use their mhclo barycentric
bindings: the source OBJs were sculpted on differently proportioned heads.
"""
import argparse
from collections import defaultdict
from pathlib import Path
import runpy

import numpy as np
from PIL import Image

SCRIPTS = Path(__file__).resolve().parent
STYLES = {"Sweep": "short02", "Crop": "short04"}


def read_obj(path):
    positions, uvs, groups = [], [], defaultdict(list)
    group = "hair"
    for line in path.read_text().splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "v":
            positions.append(list(map(float, fields[1:4])))
        elif fields[0] == "vt":
            uvs.append([float(fields[1]), 1 - float(fields[2])])
        elif fields[0] == "g":
            group = fields[1]
        elif fields[0] == "f":
            groups[group].append([tuple(int(i) - 1 for i in ref.split("/")[:2]) for ref in fields[1:]])
    return np.asarray(positions), np.asarray(uvs), groups


def wrap_hair(path, head, expected_vertices):
    rows = [line.split() for line in path.read_text().splitlines()]
    scales = {}
    for fields in rows:
        if fields and fields[0] in ("x_scale", "y_scale", "z_scale"):
            axis = "xyz".index(fields[0][0])
            a, b = map(int, fields[1:3])
            scales[axis] = abs(head[a, axis] - head[b, axis]) / float(fields[3])
    scale = np.asarray([scales[i] for i in range(3)])
    vertices = []
    for fields in rows[rows.index(["verts", "0"]) + 1:]:
        if not fields or fields[0].startswith("#") or fields[0] == "material":
            continue
        if not fields[0].isdigit():
            break
        if len(fields) == 1:
            vertices.append(head[int(fields[0])])
        elif len(fields) == 9:
            references = head[list(map(int, fields[:3]))]
            weights = np.asarray(fields[3:6], dtype=float)
            offset = np.asarray(fields[6:], dtype=float)
            vertices.append(weights @ references + offset * scale)
        else:
            raise ValueError(f"Invalid hm08 vertex binding in {path}: {fields}")
    if len(vertices) != expected_vertices:
        raise ValueError(f"{path} mapped {len(vertices)}/{expected_vertices} hair vertices")
    return np.asarray(vertices)


def normals(vertices, faces):
    result = np.zeros_like(vertices)
    for face in faces:
        indices = [i for i, _ in face]
        polygon = vertices[indices]
        for j in range(1, len(face) - 1):
            n = np.cross(polygon[j] - polygon[0], polygon[j + 1] - polygon[0])
            result[[indices[0], indices[j], indices[j + 1]]] += n
    return result / np.maximum(np.linalg.norm(result, axis=1)[:, None], 1e-9)


def triangles(vertices, normal, uvs, faces, pivot):
    # The same hm08 -> glTF conversion used for the body, translated to Head.
    positions = (vertices - pivot) * .102
    return [[(*positions[i], *normal[i], *uvs[t]) for i, t in face] for face in faces]


def prepare(base, pack, out):
    head, uvs, groups = read_obj(base)
    # The old public body has a single 'hair' section containing both a
    # hair helper and pieces of actual skin. Restore only the missing skin.
    joint = sorted({i for face in groups["joint-head"] for i, _ in face})
    pivot = head[joint].mean(axis=0)
    crown = []
    for face in groups["body"]:
        x, y, z = head[[i for i, _ in face]].mean(axis=0)
        if y > 7.92 + .55 * np.clip(z - .5, -1., 1.):
            crown.append(face)
    scalp = triangles(head, normals(head, groups["body"]), uvs, crown, pivot)
    out.mkdir(parents=True, exist_ok=True)
    write_glb = runpy.run_path(str(SCRIPTS / "prepare-riverfront-doors.py"))["store_mesh"]
    materials = [{"name": "scalp"}, {"name": "strands", "doubleSided": True}]
    for style, name in STYLES.items():
        folder = pack / "hair" / name
        rest, coordinates, groups = read_obj(folder / (name + ".obj"))
        faces = [face for group in groups.values() for face in group]
        wrapped = wrap_hair(folder / (name + ".mhclo"), head, len(rest))
        cards = triangles(wrapped, normals(wrapped, faces), coordinates, faces, pivot)
        write_glb("SM_Resident" + style, {0: scalp, 1: cards}, materials, (0, 0, 0), out)
        # Preserve the authored wisps and alpha. Grade the two cuts into
        # the same near-black brown, retaining the variation along fibres.
        rgba = np.array(Image.open(folder / (name + "_diffuse.png")).convert("RGBA"))
        light = rgba[:, :, :3] @ np.array([.2126, .7152, .0722])
        if style == "Sweep":
            light *= .62
        rgba[:, :, :3] = np.clip(light[:, :, None] * [1., .86, .76], 0, 255).astype("uint8")
        Image.fromarray(rgba).save(out / ("T_Resident" + style + ".png"))
        print(style, len(rest), "card vertices,", sum(len(f)-2 for f in cards), "card triangles,",
              sum(len(f)-2 for f in scalp), "scalp triangles")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("pack", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    prepare(args.base, args.pack, args.out)
