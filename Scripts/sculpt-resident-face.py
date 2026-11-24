"""Transfer the public resident's hm08 adult sculpt to a finished outfit GLB."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import runpy
import struct

import numpy as np


def unit(vectors):
    return vectors / np.maximum(np.linalg.norm(vectors, axis=1, keepdims=True), 1e-12)


def point_key(p):
    # Resident generators store metres as float32, often in split UV vertices.
    return tuple(np.rint(p * 1e6).astype(int))


def sculpt_lookup(base, target):
    if sha256(base.read_bytes()).hexdigest() != '8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c':
        raise ValueError('Expected the original hm08 district resident base OBJ')
    vertices, triangles = [], []
    group = None
    for line in base.read_text().splitlines():
        fields = line.split()
        if fields[:1] == ['v']:
            vertices.append([float(k) for k in fields[1:4]])
        elif fields[:1] == ['g']:
            group = fields[1]
        elif fields[:1] == ['f'] and group == 'body':
            face = [int(val.split('/')[0])-1 for val in fields[1:]]
            triangles.extend((face[0], face[k], face[k+1]) for k in range(1, len(face)-1))
    points = (np.asarray(vertices) + [0, 8.17, 0]) * .102
    triangles = np.asarray(triangles)
    changes = np.loadtxt(target)
    delta = np.zeros_like(points)
    delta[changes[:, 0].astype(int)] = changes[:, 1:]*.102

    def vertex_normals(xyz):
        a, b, c = xyz[triangles].transpose(1, 0, 2)
        normals = np.zeros_like(xyz)
        triangle_normals = np.cross(b-a, c-a)
        for corner in triangles.T:
            np.add.at(normals, corner, triangle_normals)
        return unit(normals)

    normal_delta = vertex_normals(points+delta) - vertex_normals(points)
    affected = np.flatnonzero((delta != 0).any(axis=1) | (np.abs(normal_delta)>1e-9).any(axis=1))
    # Round to float32 first so keys match the already-exported GLBs exactly.
    return {point_key(points[i].astype(np.float32).astype(float)): (delta[i], normal_delta[i]) for i in affected}


def apply(base, target, source, destination):
    io = runpy.run_path(str(Path(__file__).with_name('prepare-riverfront-hatch.py')))
    scene, raw = io['read_glb'](source)
    blob = bytearray(raw)
    lookup = sculpt_lookup(base, target)

    def replace_vec3(index, values, bounds=False):
        acc = scene['accessors'][index]
        view = scene['bufferViews'][acc['bufferView']]
        if acc['componentType'] != 5126 or acc['type'] != 'VEC3' or view.get('byteStride', 12) != 12:
            raise ValueError('Expected packed float32 VEC3')
        start = view.get('byteOffset', 0) + acc.get('byteOffset', 0)
        encoded = values.astype('<f4').tobytes()
        blob[start:start+len(encoded)] = encoded
        if bounds:
            acc['min'] = values.min(axis=0).tolist()
            acc['max'] = values.max(axis=0).tolist()

    count = 0
    for primitive in scene['meshes'][0]['primitives']:
        if scene['materials'][primitive['material']]['name'] not in ('skin', 'lips'):
            continue
        attr = primitive['attributes']
        positions = np.asarray(list(io['vectors'](scene, raw, attr['POSITION'])))
        normals = np.asarray(list(io['vectors'](scene, raw, attr['NORMAL'])))
        for i, p in enumerate(positions):
            correction = lookup.get(point_key(p))
            if correction is None:
                continue
            movement, normal_delta = correction
            positions[i] += movement
            changed = normals[i]+normal_delta
            normals[i] = changed / max(np.linalg.norm(changed), 1e-12)
            count += np.any(movement != 0)
        replace_vec3(attr['POSITION'], positions, bounds=True)
        replace_vec3(attr['NORMAL'], normals)
    if count < 350:
        raise ValueError(f'Only {count} face vertices matched. Use a neutral hm08 outfit GLB.')
    header = json.dumps(scene, separators=(',', ':')).encode()
    header += b' '*(-len(header)%4)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(struct.pack('<4sIII4s', b'glTF', 2, 28+len(header)+len(blob), len(header), b'JSON') +
                            header + struct.pack('<I4s', len(blob), b'BIN\0') + blob)
    print(f'{destination.name}: sculpt applied to {count} skin vertices')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base_obj', type=Path)
    parser.add_argument('source_glb', type=Path)
    parser.add_argument('output_glb', type=Path)
    target = Path(__file__).resolve().parents[1]/'SourceAssets/river-resident-adult.target'
    parser.add_argument('--target', type=Path, default=target)
    args = parser.parse_args()
    apply(args.base_obj, args.target, args.source_glb, args.output_glb)
