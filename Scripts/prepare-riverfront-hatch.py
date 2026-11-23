#!/usr/bin/env python3
"""Split the existing RHD browser GLB into baked chassis and pivot-local wheel GLBs."""
from __future__ import annotations

import copy
import json
import math
from pathlib import Path
import struct
import sys

JSON = 0x4E4F534A
BIN = 0x004E4942
TYPES = {5120: 'b', 5121: 'B', 5122: 'h', 5123: 'H', 5125: 'I', 5126: 'f'}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}

def read_glb(path):
    data = path.read_bytes()
    magic, version, size = struct.unpack_from('<4sII', data)
    n, typ = struct.unpack_from('<II', data, 12)
    assert (magic, version, size, typ) == (b'glTF', 2, len(data), JSON)
    nb, typ = struct.unpack_from('<II', data, 20+n)
    assert typ == BIN and 28+n+nb == len(data)
    return json.loads(data[20:20+n]), data[28+n:]

def rotate(q, v):
    x, y, z, w = q
    a, b, c = v
    t = (2*(y*c-z*b),2*(z*a-x*c),2*(x*b-y*a))
    return (a+w*t[0]+y*t[2]-z*t[1],
            b+w*t[1]+z*t[0]-x*t[2],
            c+w*t[2]+x*t[1]-y*t[0])

def vectors(src, data, idx):
    a = src['accessors'][idx]
    bv = src['bufferViews'][a['bufferView']]
    c = TYPES[a['componentType']]
    n = NCOMP[a['type']]
    unpack = struct.Struct('<'+c*n)
    step = bv.get('byteStride', unpack.size)
    pos = bv.get('byteOffset',0) + a.get('byteOffset',0)
    assert not a.get('sparse')
    denom = {'b':127, 'B':255, 'h':32767, 'H':65535}.get(c)
    for i in range(a['count']):
        vec = unpack.unpack_from(data, pos + i*step)
        if a.get('normalized'):
            assert denom
            vec = tuple(max(-1., v/denom) for v in vec)
        yield vec

def write_parts(src, data, nodes, name, path, deform=None):
    out = {k:copy.deepcopy(src[k]) for k in ('asset','materials')}
    out.update(accessors=[], bufferViews=[], buffers=[{}], meshes=[],
               nodes=[{'name':name, 'mesh':0}], scenes=[{'nodes':[0]}], scene=0)
    if src.get('extensionsUsed'):
        out['extensionsUsed']=[x for x in src['extensionsUsed'] if x != 'KHR_mesh_quantization']
    b = bytearray()
    primitives = []
    lower = [math.inf]*3
    upper = [-math.inf]*3
    tri = 0

    def pack(values, typ, component, target):
        size = NCOMP[typ]
        assert values and all(len(v) == size for v in values)
        b.extend(b'\0'*(-len(b)%4))
        offset = len(b)
        packer = struct.Struct('<'+TYPES[component]*size)
        for v in values:
            b.extend(packer.pack(*v))
        bv = len(out['bufferViews'])
        out['bufferViews'].append({'buffer':0, 'byteOffset':offset,'byteLength':len(b)-offset,'target':target})
        a = {'bufferView':bv,'componentType':component,'count':len(values),'type':typ}
        if target == 34962 and typ == 'VEC3':
            a['min']=[min(v[i] for v in values) for i in range(size)]
            a['max']=[max(v[i] for v in values) for i in range(size)]
        out['accessors'].append(a)
        return len(out['accessors'])-1

    for idx, parents in nodes:
        n=src['nodes'][idx]
        transforms=[n]+[src['nodes'][i] for i in parents]
        for prim in src['meshes'][n['mesh']]['primitives']:
            q={k:v for k,v in prim.items() if k not in ('attributes','indices')}
            q['attributes']={}
            # POSITION first so NORMAL can be transformed by the local surface Jacobian.
            original_positions = None
            for sem, a in sorted(prim['attributes'].items(), key=lambda item: item[0] != 'POSITION'):
                vs=list(vectors(src,data,a))
                if sem in ('POSITION','NORMAL'):
                    for node in transforms:
                        rot=node.get('rotation',(0,0,0,1))
                        scale=node.get('scale',(1,1,1))
                        pos=node.get('translation',(0,0,0))
                        if sem == 'POSITION':
                            vs=[tuple(a+b for a,b in zip(rotate(rot,tuple(x*y for x,y in zip(v,scale))),pos)) for v in vs]
                        else:
                            assert scale[0] == scale[1] == scale[2] and scale[0]>0
                            vs=[rotate(rot,v) for v in vs]
                    if sem == 'POSITION':
                        if deform:
                            original_positions = vs
                            vs = [deform.point(v) for v in vs]
                        for i in range(3):
                            lower[i]=min(lower[i],min(v[i] for v in vs))
                            upper[i]=max(upper[i],max(v[i] for v in vs))
                    else:
                        if deform:
                            assert original_positions is not None
                            assert len(vs) == len(original_positions)
                            vs = [deform.normal(p, v) for p, v in zip(original_positions, vs)]
                        vs=[tuple(x/math.sqrt(sum(y*y for y in v)) for x in v) for v in vs]
                q['attributes'][sem]=pack(vs,src['accessors'][a]['type'],5126,34962)
            vecs=list(vectors(src,data,prim['indices']))
            assert len(vecs)%3 == 0
            tri+=len(vecs)//3
            q['indices']=pack(vecs,'SCALAR',5123 if max(i[0] for i in vecs)<65536 else 5125,34963)
            primitives.append(q)
    out['meshes']=[{'name':name,'primitives':primitives}]
    b.extend(b'\0'*(-len(b)%4))
    out['buffers'][0]['byteLength']=len(b)
    doc=json.dumps(out,separators=(',',':')).encode()
    doc+=b' '*(-len(doc)%4)
    output=struct.pack('<4sII',b'glTF',2,28+len(doc)+len(b))
    output+=struct.pack('<II',len(doc),JSON)+doc+struct.pack('<II',len(b),BIN)+b
    path.write_bytes(output)
    print(f"{path.name}: {tri} triangles, glTF bounds {[round(x,3) for x in lower]} to {[round(x,3) for x in upper]}")
    return tri

def main(src_path, dest):
    src,data=read_glb(src_path)
    nodes={n['name']:i for i,n in enumerate(src['nodes'])}
    root=src['nodes'][nodes['hatchback']]
    steering=src['nodes'][nodes['steering_wheel']]
    assert steering['translation'][0] < 0, "Expected source's RHD adaptation"
    car=[(i,()) for i in root['children'] if 'mesh' in src['nodes'][i]]
    car.append((steering['children'][0],(nodes['steering_wheel'],)))
    dest.mkdir(parents=True,exist_ok=True)
    layout={}
    count=write_parts(src,data,car,'SM_SabarmatiHatch',dest/'SM_SabarmatiHatch.glb')
    for side in ('FL','FR','RL','RR'):
        wheel=src['nodes'][nodes['wheel_'+side]]
        idx=wheel['children'][0]
        selected={k:v for k,v in wheel.items() if k in ('rotation','translation')}
        layout[side]=selected
        if side[0] == 'F':
            name='SM_HatchWheel'+side[1]
            count+=2*write_parts(src,data,[(idx,())],name,dest/(name+'.glb'))
    assert count == sum(len(list(vectors(src,data,j['indices'])))//3
                         for n in src['nodes'] if 'mesh' in n
                         for j in src['meshes'][n['mesh']]['primitives'])
    (dest/'hatch-wheel-layout.json').write_text(json.dumps(layout,indent=2)+'\n')

if __name__=='__main__':
    main(Path(sys.argv[1]),Path(sys.argv[2]))
