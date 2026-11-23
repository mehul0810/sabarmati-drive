"""Split the licensed RHD hatch into a parked body and independently hinged front doors.

Coordinates are metres in the existing prepared glTF. Polygon clipping retains
each interpolated UV and normal across the original door seam.
"""
import argparse
import collections
import json
import math
import runpy
import struct
from pathlib import Path

def divide(poly, plane):
    front,back=[],[]
    for a,b in zip(poly, poly[1:]+poly[:1]):
        da,db=plane(a),plane(b)
        (front if da>=0 else back).append(a)
        if (da<0)!=(db<0):
            t=da/(da-db)
            v=tuple(x+(y-x)*t for x,y in zip(a,b))
            front.append(v); back.append(v)
    return front,back

def cut(poly,side):
    # Y is vertical and +Z is forward. Right in the prepared glTF is -X.
    jamb=[(-.47,.32),(.45,.32),(.67,.51),(.67,.985),(.307,1.356),(-.47,1.356)]
    planes=[lambda p:side*p[0]-.646]
    for (z0,y0),(z1,y1) in zip(jamb,jamb[1:]+jamb[:1]):
        planes.append(lambda p,z0=z0,y0=y0,dz=z1-z0,dy=y1-y0: dz*(p[1]-y0)-dy*(p[2]-z0))
    out=[]
    for plane in planes:
        if not poly: break
        ds=[plane(v) for v in poly]
        if min(ds)>=0: continue
        if max(ds)<0: out.append(poly); return [],out
        poly,other=divide(poly,plane)
        if len(other)>=3: out.append(other)
    return (poly if len(poly)>=3 else []),out

def store_mesh(name, primitives, materials, pivot, dest):
    data=bytearray()
    doc={"asset":{"version":"2.0","generator":"Sabarmati Drive door preparation"},
        "materials":materials,"extensionsUsed":["KHR_materials_clearcoat"],"bufferViews":[],"accessors":[],
        "nodes":[{"name":name,"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0}
    def accessor(items,kind,fmt,target):
        while len(data)%4: data.append(0)
        offset=len(data); pack=struct.Struct("<"+fmt)
        for it in items: data.extend(pack.pack(*it))
        doc["bufferViews"].append({"buffer":0,"byteOffset":offset,"byteLength":len(data)-offset,"target":target})
        entry={"bufferView":len(doc["bufferViews"])-1,"componentType":5125 if fmt=="I" else 5126,"count":len(items),"type":kind}
        if kind=="VEC3":
            entry.update(min=[min(x[i] for x in items) for i in range(3)],max=[max(x[i] for x in items) for i in range(3)])
        doc["accessors"].append(entry)
        return len(doc["accessors"])-1
    meshes=[]
    for mat,polys in sorted(primitives.items()):
        verts=[]; idx=[]; lookup={}
        for poly in polys:
            keys=[]
            for v in poly:
                key=tuple(round(x,6) for x in v)
                if key not in lookup:
                    lookup[key]=len(verts); verts.append(v)
                keys.append(lookup[key])
            for i in range(1,len(keys)-1):
                if len({keys[0],keys[i],keys[i+1]})==3:
                    idx.extend((keys[0],keys[i],keys[i+1]))
        if not idx: continue
        pos=[tuple(v[i]-pivot[i] for i in range(3)) for v in verts]
        nor=[]
        for v in verts:
            length=math.sqrt(sum(n*n for n in v[3:6])) or 1
            nor.append(tuple(n/length for n in v[3:6]))
        uv=[v[6:8] for v in verts]
        meshes.append({"attributes":{"POSITION":accessor(pos,"VEC3","fff",34962),
            "NORMAL":accessor(nor,"VEC3","fff",34962),"TEXCOORD_0":accessor(uv,"VEC2","ff",34962)},
            "indices":accessor([(i,) for i in idx],"SCALAR","I",34963),"material":mat})
    if not meshes: raise ValueError(name)
    doc["meshes"]=[{"name":name,"primitives":meshes}]
    while len(data)%4: data.append(0)
    doc["buffers"]=[{"byteLength":len(data)}]
    j=json.dumps(doc,separators=(",",":")).encode()
    j+=b" "*(-len(j)%4)
    (dest/(name+".glb")).write_bytes(struct.pack("<4sIIII",b"glTF",2,28+len(j)+len(data),len(j),0x4e4f534a)
        +j+struct.pack("<II",len(data),0x004e4942)+data)

def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("hatch",type=Path); parser.add_argument("out",type=Path)
    args=parser.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    helpers=runpy.run_path(str(Path(__file__).with_name("prepare-riverfront-hatch.py")))
    j,data=helpers["read_glb"](args.hatch); read=lambda i:list(helpers["vectors"](j,data,i))
    panels=[collections.defaultdict(list) for _ in range(3)]
    for prim in j["meshes"][0]["primitives"]:
        mat=prim["material"]; attrib=prim["attributes"]
        positions=read(attrib["POSITION"])
        uvs=read(attrib["TEXCOORD_0"]) if "TEXCOORD_0" in attrib else [(p[2],p[1]) for p in positions]
        vertices=[(*p,*n,*uv) for p,n,uv in zip(positions,read(attrib["NORMAL"]),uvs)]
        indices=[row[0] for row in read(prim["indices"])]
        for i in range(0,len(indices),3):
            tri=[vertices[k] for k in indices[i:i+3]]; side=1 if sum(v[0] for v in tri)>=0 else -1
            if (max(side*v[0] for v in tri)<.646 or max(v[1] for v in tri)<.32 or min(v[1] for v in tri)>1.356
                    or min(v[2] for v in tri)>.67 or max(v[2] for v in tri)<-.47):
                panels[0][mat].append(tri); continue
            door,body=cut(tri,side)
            panels[0][mat].extend(body)
            if door: panels[1 if side==1 else 2][mat].append(door)
    result=[("SM_HatchCabin",(0,0,0)),("SM_HatchDoorL",(.79,.79,.62)),("SM_HatchDoorR",(-.79,.79,.62))]
    for parts,(name,pivot) in zip(panels,result):
        store_mesh(name,parts,j["materials"],pivot,args.out)
        print(name,sum(len(x) for x in parts.values()),"panels")

if __name__=="__main__": main()
