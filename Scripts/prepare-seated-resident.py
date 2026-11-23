"""Bake a seated public resident for two-wheelers and traffic cabs. No private avatar is used."""
import argparse
import collections
import math
from pathlib import Path
import runpy

def build(input_file,out):
    helpers=runpy.run_path(str(Path(__file__).with_name("prepare-riverfront-hatch.py")))
    write=runpy.run_path(str(Path(__file__).with_name("prepare-riverfront-doors.py")))["store_mesh"]
    g,raw=helpers["read_glb"](input_file)
    def read(i):return list(helpers["vectors"](g,raw,i))
    def rotate(v,deg,axis):
        c,s=math.cos(math.radians(deg)),math.sin(math.radians(deg));x,y,z=v
        return (x,y*c-z*s,y*s+z*c) if axis=="x" else (x*c-y*s,x*s+y*c,z)
    def add(a,b):return tuple(x+y for x,y in zip(a,b))
    def sub(a,b):return tuple(x-y for x,y in zip(a,b))
    def scale(a,k):return tuple(x*k for x in a)
    N=g["nodes"];J=g["skins"][0]["joints"];parent={c:i for i,n in enumerate(N) for c in n.get("children",[])}
    rest={};placed={};angles={}
    for i in J:
        n=N[i];name=n["name"];side=1 if name.endswith("l") else -1
        a,z=0.,0.
        if name.startswith("Shoulder"):a,z=-40,-side*29
        elif name.startswith("Elbow"):a=-23
        elif name.startswith("Hip"):a,z=-74,-side*2
        elif name.startswith("Knee"):a=78
        elif name.startswith("Foot"):a=-6
        elif name=="Spine":a=6.
        elif name=="Head":a=-4.
        local=tuple(n.get("translation",[0,0,0]));p=parent.get(i)
        rest[i]=add(rest[p],local) if p is not None else local
        def orient(v,bone):
            while bone is not None:
                az,roll=angles[bone];v=rotate(rotate(v,az,"x"),roll,"z");bone=parent.get(bone)
            return v
        placed[i]=add(placed[p],orient(local,p)) if p is not None else local
        angles[i]=(a,z)
    baked=collections.defaultdict(list)
    mats=list(g["materials"])
    for prim in g["meshes"][0]["primitives"]:
        attr=prim["attributes"]
        points=read(attr["POSITION"]);normals=read(attr["NORMAL"]);uv=read(attr["TEXCOORD_0"])
        joints=read(attr["JOINTS_0"]);weight=read(attr["WEIGHTS_0"])
        vertices=[]
        for v,n,t,bones,wts in zip(points,normals,uv,joints,weight):
            p=(0.,0.,0.);total=(0.,0.,0.)
            for b,w in zip(bones,wts):
                if not w:continue
                q=J[b];p=add(p,scale(add(placed[q],orient(sub(v,rest[q]),q)),w));total=add(total,scale(orient(n,q),w))
            length=max(.001,sum(x*x for x in total)**.5)
            vertices.append((*add(p,(0.,-.23,-.06)),*scale(total,1./length),*t))
        ix=[j[0] for j in read(prim["indices"])]
        baked[prim["material"]].extend([vertices[j] for j in ix[k:k+3]] for k in range(0,len(ix),3))
    out.mkdir(parents=True,exist_ok=True)
    write("SM_SeatedResident",baked,mats,(0,0,0),out)
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("clothed",type=Path);p.add_argument("output",type=Path)
    a=p.parse_args();build(a.clothed,a.output)
