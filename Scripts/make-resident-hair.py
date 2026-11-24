"""Fit two MakeHuman CC0 system hairstyles to the playable hm08 head.

Usage: python make-resident-hair.py BASE_OBJ SYSTEM_PACK_DIRECTORY OUTPUT_DIRECTORY
Keep the official short01/short04 OBJ UVs and use their mhclo barycentric
bindings: the source OBJs were sculpted on differently proportioned heads.
"""
import argparse
from collections import defaultdict
from pathlib import Path
import runpy

import numpy as np
from PIL import Image

SCRIPTS = Path(__file__).resolve().parent
STYLES = {"Sweep": "short01", "Crop": "short04"}


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


def forehead_wisps(head, skin_faces, cards, card_faces, atlas, pivot):
    """Fit tapered transitional hairs between the denser cut and the bare forehead."""
    rng = np.random.default_rng(2271)
    triangles3 = np.array([[cards[i[0]] for i in (f[0], f[j], f[j+1])]
                           for f in card_faces for j in range(1,len(f)-1)])
    starts = triangles3[:,[0,1,2],:]
    ends = triangles3[:,[1,2,0],:]
    skin = np.array([[head[i[0]] for i in (f[0],f[j],f[j+1])]
                     for f in skin_faces for j in range(1,len(f)-1)])
    skin = skin[(skin[:,:,1].max(1) > 7.1) & (skin[:,:,2].max(1) > .9)]
    a, b, c = skin[:,0], skin[:,1]-skin[:,0], skin[:,2]-skin[:,0]
    determinant = b[:,0]*c[:,1]-b[:,1]*c[:,0]
    valid = abs(determinant) > 1e-7
    divisor = np.where(valid,determinant,1.)
    ys, xs = np.nonzero((atlas[::8,::8,3]>235) & (atlas[::8,::8,0]>18) & (atlas[::8,::8,0]<71))
    def forehead(x,y):
        dx,dy=x-a[:,0],y-a[:,1]
        s,t=(dx*c[:,1]-dy*c[:,0])/divisor,(b[:,0]*dy-b[:,1]*dx)/divisor
        hits=valid & (s>=-.001) & (t>=-.001) & (s+t<=1.001)
        if not np.any(hits):
            return None
        return np.max((a[:,2]+s*b[:,2]+t*c[:,2])[hits])

    strands=[]
    for x in np.linspace(-.61,.61,330) + rng.uniform(-.0016,.0016,330):
        # Locate the lower edge of the forward-facing style at this temple angle.
        delta = ends[:,:,0]-starts[:,:,0]
        frac = (x-starts[:,:,0])/np.where(abs(delta)>1.e-7,delta,1.)
        cross = starts + frac[:,:,None]*(ends-starts)
        hit = (abs(delta)>1.e-7)&(frac>=0)&(frac<=1)&(cross[:,:,2]>1.27-.60*x*x)
        if not np.any(hit):
            continue
        edge = np.min(cross[:,:,1][hit])
        # As the scalp curves towards each temple, the hairs sweep outward.
        drift = .015 + .040*x + rng.uniform(-.010,.010)
        length = rng.uniform(.022,.061)
        width = rng.uniform(.0018,.0048)
        height = edge + .010*np.sin(38*x+.8) + .008*np.sin(91*x)
        sample = rng.integers(len(xs))
        uv = ((8*xs[sample]+.5)/atlas.shape[1],(8*ys[sample]+.5)/atlas.shape[0])
        layers=[]
        for t, taper in [(0.,.06),(.3,.57),(.64,1.),(1.,.6)]:
            cx=x+drift*t
            y=height-length*(1-t)+.036*t
            z=forehead(cx,y)
            if z is None: break
            z+=.0024+.007*t
            normal=np.array([cx*.85,.22,.98])
            normal/=np.linalg.norm(normal)
            layers.append([(*((np.array([cx+side*width*taper*.5,y,z])-pivot)*.102),*normal,*uv)
                           for side in [-1.,1.]])
        for lower,upper in zip(layers,layers[1:]):
            strands.append([lower[0],lower[1],upper[1],upper[0]])
    return strands


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
    skin_faces = groups["body"]
    scalp = triangles(head, normals(head, skin_faces), uvs, crown, pivot)
    out.mkdir(parents=True, exist_ok=True)
    write_glb = runpy.run_path(str(SCRIPTS / "prepare-riverfront-doors.py"))["store_mesh"]
    materials = [{"name": "scalp"}, {"name": "strands", "doubleSided": True}]
    for style, name in STYLES.items():
        folder = pack / "hair" / name
        rest, coordinates, groups = read_obj(folder / (name + ".obj"))
        faces = [face for group in groups.values() for face in group]
        wrapped = wrap_hair(folder / (name + ".mhclo"), head, len(rest))
        cards = triangles(wrapped, normals(wrapped, faces), coordinates, faces, pivot)
        # Preserve the authored wisps and alpha. Grade the two cuts into
        # the same near-black brown, retaining the variation along fibres.
        rgba = np.array(Image.open(folder / (name + "_diffuse.png")).convert("RGBA"))
        if style == "Sweep":
            cards.extend(forehead_wisps(head, skin_faces, wrapped, faces, rgba, pivot))
        write_glb("SM_Resident" + style, {0: scalp, 1: cards}, materials, (0, 0, 0), out)
        light = rgba[:, :, :3] @ np.array([.2126, .7152, .0722])
        if style == "Sweep":
            light *= 1.37
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
