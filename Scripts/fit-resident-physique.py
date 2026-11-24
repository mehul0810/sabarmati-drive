"""Fit the fictional resident and dressed hm08 outfit together in glTF bind space."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct

import numpy as np


def smooth(low, high, x):
    t = np.clip((x-low)/(high-low), 0, 1)
    return t*t*(3-2*t)


def segment_volume(points, start, end, peak, spread, radius, gain):
    """Add volume around a bone segment, easing to zero near both joints."""
    a, axis = np.asarray(start), np.subtract(end, start)
    u = ((points-a)@axis)/(axis@axis)
    radial = points-a-u[:, None]*axis
    weight = np.exp(-((u-peak)/spread)**2)*smooth(-.14, .05, u)*(1-smooth(.87, 1.07, u))
    weight *= 1-smooth(radius, 1.55*radius, np.linalg.norm(radial, axis=1))
    return weight[:, None]*radial*gain


class AthleticBody:
    def __init__(self, base, muscle):
        if hashlib.sha256(base.read_bytes()).hexdigest() != '8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c':
            raise ValueError('Unexpected hm08 base OBJ')
        target_text = b'\n'.join(line.rstrip() for line in muscle.read_bytes().splitlines()) + b'\n'
        if hashlib.sha256(target_text).hexdigest() != '3904cefe0b14614eef78f835d081c5493753bfb3dcb9bd08afb3e26df4768fb6':
            raise ValueError('Unexpected MakeHuman muscle target')
        vertices, body_ids, group = [], set(), None
        for line in base.read_text().splitlines():
            f = line.split()
            if f[:1] == ['v']:
                vertices.append(list(map(float, f[1:4])))
            elif f[:1] == ['g']:
                group = f[1]
            elif f[:1] == ['f'] and group == 'body':
                body_ids.update(int(v.split('/')[0])-1 for v in f[1:])
        self.base = (np.asarray(vertices)+[0, 8.17, 0])*.102
        target = np.loadtxt(muscle)
        delta = np.zeros_like(self.base)
        delta[target[:, 0].astype(int)] = target[:, 1:]*.102

        # Smooth normalized convolution of the stock skin displacement gives
        # garments and exposed skin the same continuous field through air gaps.
        # Fourteen-millimetre cells retain the major muscle groups under clothing.
        ids = np.array(sorted(body_ids))
        self.step = .014
        self.origin = self.base[ids].min(axis=0)-.12
        cells = (self.base[ids]-self.origin)/self.step
        dims = (np.ceil(cells.max(axis=0)).astype(int)+12).tolist()
        grid = np.zeros((*dims, 4), np.float32)
        lower = np.floor(cells).astype(int)
        fraction = cells-lower
        samples = np.column_stack((delta[ids], np.ones(len(ids))))
        for corner in np.ndindex(2, 2, 2):
            weight = np.where(corner, fraction, 1-fraction).prod(axis=1)
            np.add.at(grid, tuple((lower+corner).T), weight[:, None]*samples)
        kernel = np.exp(-.5*(np.arange(-6, 7)/1.65)**2)
        for axis in range(3):
            padded = np.pad(grid, [(6, 6) if i == axis else (0, 0) for i in range(4)])
            filtered = np.zeros_like(grid)
            for k, weight in enumerate(kernel):
                view = [slice(None)]*4
                view[axis] = slice(k, k+grid.shape[axis])
                filtered += weight*padded[tuple(view)]
            grid = filtered
        self.grid = grid[:, :, :, :3]/np.maximum(grid[:, :, :, 3:], 1.e-4)

    def offsets(self, points):
        xyz = np.asarray(points)
        x, y, z = xyz.T
        ax = abs(x)
        cell = np.clip((xyz-self.origin)/self.step, 0, np.array(self.grid.shape[:3])-1.00001)
        lower = np.floor(cell).astype(int)
        fraction = cell-lower
        delta = np.zeros_like(xyz)
        for corner in np.ndindex(2, 2, 2):
            weight = np.where(corner, fraction, 1-fraction).prod(axis=1)
            delta += weight[:, None]*self.grid[tuple((lower+corner).T)]
        delta *= [.54, .18, .54]

        # Support ribcage breadth and anterior/posterior depth, keeping the
        # lower abdomen and fixed neck/shoulder joint centres in their rest pose.
        chest = smooth(1.065, 1.235, y)*(1-smooth(1.365, 1.46, y))
        trunk = chest*(1-smooth(.175, .29, ax))
        delta[:, 0] += .078*x*trunk
        delta[:, 2] += trunk*(.015*smooth(.052, .114, z)-.010*(1-smooth(-.072, .004, z)))
        cap = np.exp(-((ax-.203)/.073)**2-((y-1.355)/.072)**2)
        delta[:, 0] += .012*np.sign(x)*cap
        # Symmetric arm and leg regions follow the resident's existing A-pose.
        mirrored = np.column_stack((ax, y, z))
        volume = segment_volume(mirrored, (.171,1.368,.015), (.319,1.190,.013), .44,.43,.094,.16)
        volume += segment_volume(mirrored, (.319,1.190,.013), (.44,1.083,.179), .30,.39,.074,.095)
        volume += segment_volume(mirrored, (.113,.883,.013), (.161,.456,.033), .49,.43,.114,.145)
        volume += segment_volume(mirrored, (.161,.456,.033), (.224,.074,-.001), .42,.36,.088,.12)
        volume[:, 0] *= np.sign(x)
        delta += volume

        # Protect hand/watch and foot attachment volumes and the entire head.
        guard = smooth(.125,.19,y)*(1-smooth(.348,.403,ax))*(1-smooth(1.420,1.465,y))
        guard *= 1-(1-smooth(.095,.158,ax))*smooth(1.343,1.409,y)
        # Keep the everyday/kurta waist and watch position across outfit swaps.
        waist = (1-smooth(.155,.235,ax))*smooth(.87,.96,y)*(1-smooth(1.085,1.19,y))
        delta *= (guard*(1-waist))[:, None]
        return delta

    def transform(self, points, normals):
        delta = self.offsets(points)
        # Inverse-transpose of the displacement Jacobian also preserves the
        # source's smoothed normals at UV/material seams.
        jac = np.broadcast_to(np.eye(3), (len(points), 3, 3)).copy()
        for j in range(3):
            step = np.eye(3)[j]*.0007
            jac[:, :, j] += (self.offsets(points+step)-self.offsets(points-step))/.0014
        adjusted = np.linalg.solve(jac.transpose(0,2,1), normals[:, :, None])[:, :, 0]
        adjusted /= np.maximum(np.linalg.norm(adjusted, axis=1, keepdims=True), 1.e-12)
        adjusted[np.all(delta == 0, axis=1)] = normals[np.all(delta == 0, axis=1)]
        return points+delta, adjusted


def fit(source, destination, body):
    io = runpy.run_path(str(Path(__file__).with_name('prepare-riverfront-hatch.py')))
    scene, raw = io['read_glb'](source)
    data = bytearray(raw)
    for primitive in scene['meshes'][0]['primitives']:
        name = scene['materials'][primitive['material']]['name']
        if name in ('hair','iris','sclera','lips','shoe'):
            continue
        attr = primitive['attributes']
        points, normals = (np.asarray(list(io['vectors'](scene,raw,attr[k]))) for k in ('POSITION','NORMAL'))
        moved, adjusted = body.transform(points, normals)
        for channel, array in [('POSITION', moved), ('NORMAL', adjusted)]:
            acc = scene['accessors'][attr[channel]]
            view = scene['bufferViews'][acc['bufferView']]
            if acc['type'] != 'VEC3' or acc['componentType'] != 5126 or view.get('byteStride',12) != 12:
                raise ValueError('Expected a packed float32 VEC3 stream')
            offset = view.get('byteOffset',0)+acc.get('byteOffset',0)
            packed = array.astype('<f4').tobytes()
            data[offset:offset+len(packed)] = packed
            if channel == 'POSITION':
                acc.update(min=array.min(axis=0).tolist(), max=array.max(axis=0).tolist())
    header = json.dumps(scene,separators=(',',':')).encode()
    header += b' '*(-len(header)%4)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(struct.pack('<4sIII4s', b'glTF', 2, 28+len(header)+len(data), len(header), b'JSON')+
                            header+struct.pack('<I4s',len(data),b'BIN\0')+data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base_obj', type=Path)
    parser.add_argument('input_glb', type=Path)
    parser.add_argument('output_glb', type=Path)
    parser.add_argument('--muscle', type=Path, default=Path(__file__).resolve().parents[1]/
                        'SourceAssets/universal-male-young-maxmuscle-averageweight.target')
    args = parser.parse_args()
    fit(args.input_glb, args.output_glb, AthleticBody(args.base_obj, args.muscle))
