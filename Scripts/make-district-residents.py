"""Build clothed, fictional pedestrians and an original gait rig from MakeHuman's CC0 hm08 topology."""
import argparse
import collections
import json
import math
import struct
from pathlib import Path

from resident_gait import GAITS, SAMPLE_RATE, gait_pose, idle_pose

# Metres, glTF Y-up. All joints, clothes, expressions and animation belong to this export.
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def add(a,b): return tuple(x+y for x,y in zip(a,b))
def mul(a,k): return tuple(x*k for x in a)
def norm(a): return math.sqrt(sum(x*x for x in a))
def unit(a): return mul(a,1/max(norm(a),1e-12))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def distance_line(p,a,b):
    ab=sub(b,a)
    t=min(1,max(0,dot(sub(p,a),ab)/max(.000001,dot(ab,ab))))
    return norm(sub(p,add(a,mul(ab,t))))

PALETTE={"skin":(.40,.21,.127),"lips":(.34,.13,.104),"hair":(.028,.020,.015),"sclera":(.60,.54,.43),
         "iris":(.05,.032,.015),"top_teal_woven":(.13,.27,.24),"top_seams":(.13,.16,.15),
         "bottom_denim":(.094,.11,.13),"shoe":(.08,.054,.038),"scarf":(.46,.181,.10)}
def make(source,out,long_garment):
    vertices=[]; uvs=[]; groups=collections.defaultdict(list); group=None
    for line in source.read_text().splitlines():
        f=line.split()
        if not f:continue
        if f[0]=="v":vertices.append(tuple(float(x) for x in f[1:4]))
        elif f[0]=="vt":uvs.append(tuple(float(x) for x in f[1:3]))
        elif f[0]=="g":group=f[1]
        elif f[0]=="f":
            groups[group].append([tuple(int(q)-1 for q in item.split("/")[:2]) for item in f[1:]])
    def convert(v): return (v[0]*.102,(v[1]+8.17)*.102,v[2]*.102)
    def centroid(group):
        ids={i for poly in groups[group] for i,_ in poly}
        return convert(tuple(sum(vertices[i][k] for i in ids)/len(ids) for k in range(3)))
    names=[]; coords=[]; parents=[]
    def bone(name,xyz,parent=None):
        names.append(name);coords.append(xyz);parents.append(names.index(parent) if parent else None)
    bone("Root",(0,0,0));bone("Pelvis",centroid("joint-pelvis"),"Root")
    bone("Spine",centroid("joint-spine-2"),"Pelvis");bone("Chest",centroid("joint-spine-1"),"Spine")
    bone("Neck",(0,1.415,.048),"Chest");bone("Head",centroid("joint-head"),"Neck")
    for s in ("l","r"):
        for name,j,parent in (("Shoulder","shoulder","Chest"),("Elbow","elbow","Shoulder"+s),
                              ("Hand","hand","Elbow"+s),("Hip","upper-leg","Pelvis"),
                              ("Knee","knee","Hip"+s),("Foot","ankle","Knee"+s)):
            bone(name+s,centroid("joint-"+s+"-"+j),parent)
    ends={0:coords[1],1:coords[2],2:coords[3],3:coords[4],4:coords[5],5:(0,1.72,.055)}
    for i,(n,c) in enumerate(zip(names,coords)):
        if i<6:continue
        ends[i]=coords[i+1] if n[:-1] in ("Shoulder","Elbow","Hip","Knee") else add(c,(0,-.09,.055))
    def weights(p):
        x,y,z=p; side="l" if x>0 else "r"
        pool=[1,2,3,4,5]+[i for i,n in enumerate(names) if n.endswith(side)]
        if y<.73: pool=[i for i in pool if names[i].startswith(("Hip","Knee","Foot"))]
        if y>1.48 and abs(x)<.18:pool=[4,5]
        ranked=sorted((distance_line(p,coords[i],ends[i]),i) for i in pool)[:3]
        values=[1/max(.015,dis)**5 for dis,_ in ranked]
        return tuple([i for _,i in ranked]+[0]*(4-len(ranked))),tuple([v/sum(values) for v in values]+[0.]*(4-len(values)))
    tris=collections.defaultdict(list); positions={}; normals=collections.defaultdict(lambda:(0,0,0)); tex={}
    for g in ("body","helper-tights","helper-skirt","helper-hair","helper-l-eye","helper-r-eye"):
        for face in groups[g]:
            raw=[vertices[i] for i,_ in face]; x,y,z=[sum(v[k] for v in raw)/len(raw) for k in range(3)]
            if g=="body":
                if not(y>5.42 or (abs(x)>3.15 and y<3.78)):continue
                material="skin"
                if abs(x)<.39 and 6.50<y<6.65 and z>1.10:material="lips"
                if y>7.92+.55*max(-1,min(1,z-.5)): material="hair"
            elif g.endswith("eye"):
                material="iris" if abs(abs(x)-.31)<.085 and abs(y-7.29)<.094 and z>1.27 else "sclera"
            elif g=="helper-hair":
                behind=max(0,1.05-z)
                if y<8.04-min(3.1 if long_garment else 1.42,behind*(2.8 if long_garment else 1.17)):continue
                material="hair"
            elif g=="helper-skirt":
                if not long_garment or y< -3.0 or y>.55:continue
                material="top_seams" if y< -2.75 else "top_teal_woven"
            else:
                if y>5.65 or (abs(x)>3.21 and y<3.6):continue
                material=("shoe" if y< -6.86 else "bottom_denim" if y<(.0 if long_garment else .78) else
                          "top_seams" if y<(.26 if long_garment else .96) or (abs(x)>2.95 and y<3.9) else "top_teal_woven")
                if long_garment and y>2.35 and abs(x-.33*(y-3.55))<.42 and z>.75:material="scarf"
            keys=[]
            for index,uv in face:
                key=(g,index,uv,material); X,Y,Z=vertices[index]
                if g=="helper-tights":
                    if Y>0 and abs(X)<2.8:
                        X*=1.055;Z=.4+(Z-.4)*1.13
                    elif Y<1:
                        mid=(1 if X>0 else -1)*(1.16+.115*max(0,-Y))
                        X=mid+(X-mid)*1.17;Z=.36+(Z-.36)*1.19
                if g=="helper-skirt": X*=.965;Z=.3+(Z-.3)*1.04
                p=convert((X,Y,Z));positions[key]=p;tex[key]=uvs[uv];keys.append(key)
            for j in range(1,len(keys)-1):
                tri=(keys[0],keys[j],keys[j+1]);a,b,c=[positions[k] for k in tri]
                normal=cross(sub(b,a),sub(c,a))
                if norm(normal)<1.e-12:continue
                tris[material].append(tri)
                for key in tri:
                    nk=key[:2];normals[nk]=add(normals[nk],normal)
    binary=bytearray();views=[];accessors=[]
    def data(values,fmt,kind,minimum=None,maximum=None):
        while len(binary)%4:binary.append(0)
        start=len(binary);pack=struct.Struct("<"+fmt*len(values[0]))
        for row in values:binary.extend(pack.pack(*row))
        views.append({"buffer":0,"byteOffset":start,"byteLength":len(binary)-start})
        accessors.append({"bufferView":len(views)-1,"componentType":{"f":5126,"H":5123,"I":5125}[fmt],"count":len(values),"type":kind})
        if minimum is not None:accessors[-1].update(min=minimum,max=maximum)
        return len(accessors)-1
    materials=[];primitives=[]
    for mat,faces in tris.items():
        indices={key:i for i,key in enumerate(dict.fromkeys(key for face in faces for key in face))}
        coords_list=[positions[k] for k in indices]; influences=[weights(p) for p in coords_list]
        p=data(coords_list,"f","VEC3",[min(a) for a in zip(*coords_list)],[max(a) for a in zip(*coords_list)])
        prim={"attributes":{"POSITION":p,"NORMAL":data([unit(normals[k[:2]]) for k in indices],"f","VEC3"),
              "TEXCOORD_0":data([(tex[k][0],1-tex[k][1]) for k in indices],"f","VEC2"),
              "JOINTS_0":data([j for j,w in influences],"H","VEC4"),"WEIGHTS_0":data([w for j,w in influences],"f","VEC4")},
              "indices":data([(indices[k],) for f in faces for k in f],"I","SCALAR"),"material":len(materials)}
        primitives.append(prim);materials.append({"name":mat,"doubleSided":True,"pbrMetallicRoughness":{
          "baseColorFactor":[*PALETTE[mat],1.],"metallicFactor":0.,"roughnessFactor":.45 if mat in ("iris","sclera") else .78}})
    nodes=[]
    for i,n in enumerate(names):
        parent=parents[i];children=[j for j,p in enumerate(parents) if p==i]
        nodes.append({"name":n,"translation":sub(coords[i],coords[parent]) if parent is not None else coords[i]})
        if children:nodes[-1]["children"]=children
    matrices=[(1,0,0,0,0,1,0,0,0,0,1,0,-x,-y,-z,1) for x,y,z in coords]
    skin={"joints":list(range(len(names))),"inverseBindMatrices":data(matrices,"f","MAT4"),"skeleton":0}
    nodes.append({"name":out.stem,"mesh":0,"skin":0});clips=[]
    rest = dict(zip(names, coords))
    for clip, period in [("A_Idle", 3.8)] + [(name, gait[0]) for name, gait in GAITS.items()]:
        samplers=[];channels=[]
        frames=round(period*(30 if clip=="A_Idle" else SAMPLE_RATE))
        times=data([(i*period/frames,) for i in range(frames+1)],"f","SCALAR",[0],[period])
        def channel(node, path, values, kind):
            channels.append({"sampler":len(samplers),"target":{"node":node,"path":path}})
            samplers.append({"input":times,"output":data(values,"f",kind)})
        poses=[idle_pose(i/frames, rest) if clip == "A_Idle" else gait_pose(clip, i/frames, rest)
               for i in range(frames+1)]
        channel(names.index("Pelvis"), "translation", [pelvis for _,pelvis in poses], "VEC3")
        for i,name in enumerate(names):
            if name != "Root":
                channel(i, "rotation", [pose[name] for pose,_ in poses], "VEC4")
        clips.append({"name":clip,"samplers":samplers,"channels":channels})
    scene={"asset":{"version":"2.0","generator":"Sabarmati Drive resident pipeline"},"scene":0,"scenes":[{"nodes":[0,len(nodes)-1]}],
      "nodes":nodes,"skins":[skin],"meshes":[{"name":out.stem,"primitives":primitives}],"animations":clips,"materials":materials,
      "buffers":[{"byteLength":len(binary)}],"bufferViews":views,"accessors":accessors}
    header=json.dumps(scene,separators=(",",":")).encode();header+=b" "*(-len(header)%4);binary.extend(b"\0"*(-len(binary)%4))
    out.write_bytes(struct.pack("<4sIII4s",b"glTF",2,28+len(header)+len(binary),len(header),b"JSON")+header+struct.pack("<I4s",len(binary),b"BIN\0")+binary)
    print(out.name,sum(map(len,tris.values())),"triangles",len(names),"joints",len(clips),"clips")
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("base",type=Path);parser.add_argument("output",type=Path)
    args=parser.parse_args();args.output.mkdir(exist_ok=True,parents=True)
    make(args.base,args.output/"SK_RiverResident.glb",False)
    make(args.base,args.output/"SK_RiverResidentKurta.glb",True)
