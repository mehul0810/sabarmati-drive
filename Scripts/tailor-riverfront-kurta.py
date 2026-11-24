"""Tailor the public MakeHuman-derived kurta to the existing 18-bone animation rig."""
import argparse
import collections
import json
import math
from pathlib import Path
import runpy
import struct


def make(source, dest):
    gltf=runpy.run_path(str(Path(__file__).with_name("prepare-riverfront-hatch.py")))
    doc,raw=gltf["read_glb"](source)
    blob=bytearray(raw)
    def read(i): return list(gltf["vectors"](doc,raw,i))
    def add(values,fmt,kind,bounds=False):
        while len(blob)%4:blob.append(0)
        target=len(blob); pack=struct.Struct("<"+fmt*len(values[0]))
        for v in values:blob.extend(pack.pack(*v))
        doc["bufferViews"].append(dict(buffer=0,byteOffset=target,byteLength=len(blob)-target))
        acc=dict(bufferView=len(doc["bufferViews"])-1,componentType={"f":5126,"H":5123,"I":5125}[fmt],
                 count=len(values),type=kind)
        if bounds:
            acc.update(min=[min(q) for q in zip(*values)],max=[max(q) for q in zip(*values)])
        doc["accessors"].append(acc)
        return len(doc["accessors"])-1

    mesh=doc['meshes'][0]
    mats={m['name']:i for i,m in enumerate(doc['materials'])}
    # Use the same original cropped haircut underneath the optional swept style
    # on both outfits. Both public exports use the identical rest skeleton.
    short,short_blob=gltf["read_glb"](source.with_name("SK_RiverResident.glb"))
    cropped=next(pr for pr in short["meshes"][0]["primitives"] if short["materials"][pr["material"]]["name"]=="hair")
    def smooth(a,b,x):
        t=max(0.,min(1.,(x-a)/(b-a)))
        return t*t*(3.-2.*t)

    bones={doc["nodes"][index]["name"]:i for i,index in enumerate(doc["skins"][0]["joints"])}
    def thigh_fit(at,points,panels):
        # Keep overlapping trouser/hem rings on the same thigh joint. The
        # skirt is cut above the kneecaps, with a gradual blend across its middle.
        source_joints,source_weights=read(at["JOINTS_0"]),read(at["WEIGHTS_0"])
        joints=[]; weights=[]
        for (x,y,z),old_ids,old_values in zip(points,source_joints,source_weights):
            lower=1-smooth(.70,.86,y) if panels else smooth(.34,.45,y)
            if lower<=0:
                joints.append(old_ids);weights.append(old_values);continue
            influence=collections.defaultdict(float)
            for i,w in zip(old_ids,old_values):influence[i]+=w*(1-lower)
            left=smooth(-.055,.055,x) if panels else float(x>=0.)
            calf=1-smooth(.391,.523,y)
            for side,mix in (("l",left),("r",1-left)):
                influence[bones["Hip"+side]]+=lower*mix*(1-calf)
                influence[bones["Knee"+side]]+=lower*mix*calf
            pairs=sorted(influence.items(),key=lambda p:-p[1])[:4]
            pairs+=[(0,0.)]*(4-len(pairs));total=sum(w for _,w in pairs)
            joints.append(tuple(i for i,_ in pairs));weights.append(tuple(w/total for _,w in pairs))
        at["JOINTS_0"]=add(joints,"H","VEC4")
        at["WEIGHTS_0"]=add(weights,"f","VEC4")

    # Identify the actual stitched boundary across split Unreal material/UV sections.
    # The source helper was clipped on coarse quads and has a 5 cm step mid-front.
    seams=collections.Counter()
    edge_binds={}
    edge_normals={}
    elbows={side:(sgn*.319,1.190,.013) for side,sgn in (("l",1),("r",-1))}
    fore={side:(sgn*.121,-.107,.166) for side,sgn in (("l",1),("r",-1))}
    for side,f in fore.items():
        fore[side]=tuple(v/math.hypot(*f) for v in f)
    def forearm(p,side):
        delta=tuple(v-c for v,c in zip(p,elbows[side]))
        distance=sum(a*b for a,b in zip(delta,fore[side]))
        radial=tuple(v-distance*a for v,a in zip(delta,fore[side]))
        return distance,radial
    for section in mesh["primitives"]:
        if doc["materials"][section["material"]]["name"] not in ("top_teal_woven","top_seams","scarf"):
            continue
        positions=read(section["attributes"]["POSITION"])
        at=section["attributes"]
        edge_normals.update(zip(positions,read(at["NORMAL"])))
        edge_binds.update(zip(positions,zip(read(at["JOINTS_0"]),read(at["WEIGHTS_0"]))))
        indices=[v[0] for v in read(section["indices"])]
        for tri in zip(indices[::3],indices[1::3],indices[2::3]):
            points=[positions[i] for i in tri]
            for i in range(3):
                seams[tuple(sorted((points[i],points[(i+1)%3])))]+=1
    hem={p for edge,count in seams.items() if count==1 for p in edge if p[1]<.65}
    cloth_faces=[]
    for prim in mesh['primitives']:
        name=doc['materials'][prim['material']]['name']
        if name=="skin":
            at=prim["attributes"]; coordinates=read(at["POSITION"])
            ids=[v[0] for v in read(prim["indices"])]
            visible=[]
            for tri in zip(ids[::3],ids[1::3],ids[2::3]):
                points=[coordinates[i] for i in tri]
                x,y,z=(sum(p[k] for p in points)/3 for k in range(3))
                neck=((x/.060)**2+((z-.022)/.055)**2)<1.0
                side="l" if x>=0 else "r"
                wrist=abs(x)>.275 and max(forearm(p,side)[0] for p in points)>.097
                if y>1.434 or neck or wrist: visible.extend((i,) for i in tri)
            prim["indices"]=add(visible,"I","SCALAR")
            continue
        if name=="hair":
            for channel,new_index in cropped["attributes"].items():
                acc=short["accessors"][new_index]
                values=list(gltf["vectors"](short,short_blob,new_index))
                prim["attributes"][channel]=add(values,"H" if channel=="JOINTS_0" else "f",acc["type"],channel=="POSITION")
            prim["indices"]=add(list(gltf["vectors"](short,short_blob,cropped["indices"])),"I","SCALAR")
            continue
        if name not in ('top_teal_woven','scarf','top_seams','bottom_denim'):continue
        at=prim['attributes']; points=read(at['POSITION']); ids=[v[0] for v in read(prim['indices'])]
        if name=='bottom_denim':
            # The knee-to-waist portion is covered by the opaque garment.
            # End the trouser inside its hem and reduce the concealed fabric.
            kept=[]
            for a,b,c in zip(ids[::3],ids[1::3],ids[2::3]):
                if min(points[i][1] for i in (a,b,c))<.70: kept.extend((a,b,c))
            prim['indices']=add([(i,) for i in kept],'I','SCALAR')
            narrow=[]
            for x,y,z in points:
                blend=smooth(.455,.558,y)*.46
                leg=math.copysign(.15,x)
                narrow.append((x*(1-blend)+leg*blend,y,z*(1-blend)+.026*blend))
            thigh_fit(at,points,False)
            at['POSITION']=add(narrow,'f','VEC3',True)
            continue
        joints=read(at['JOINTS_0']); weights=read(at['WEIGHTS_0'])
        for a,b,c in zip(ids[::3],ids[1::3],ids[2::3]):
            if min(points[i][1] for i in (a,b,c))>.944:
                cloth_faces.append([(points[i],joints[i],weights[i]) for i in (a,b,c)])
        # Clear the original whole-face colour assignments. The new scarf and
        # placket are actual fitted panels with consistent, continuous edges.
        prim['material']=mats['top_teal_woven']
        shaped=[]
        for x,y,z in points:
            ease=max(0,min(1,(.922-y)/.11))
            # Curve the two sides above the knee to clear the rising thigh and the trailing calf.
            vent=.135*smooth(.088,.200,abs(x))*(1-smooth(.525,.72,y))*(1-smooth(.025,.125,z))
            height=.517 if (x,y,z) in hem else y
            shaped.append((x*(1+.065*ease),height+vent,(z-.032)*(1+.14*ease)+.032))
        thigh_fit(at,points,True)
        at['POSITION']=add(shaped,'f','VEC3',True)

    def attachment(x,y,back=False):
        matches=[]
        near=[]
        for tri in cloth_faces:
            (a,_,_),(b,_,_),(c,_,_)=tri
            if max(abs(x-q[0]) for q in (a,b,c))>.175:continue
            den=(b[0]-a[0])*(c[1]-a[1])-(c[0]-a[0])*(b[1]-a[1])
            if abs(den)>1.e-9:
                wb=((x-a[0])*(c[1]-a[1])-(y-a[1])*(c[0]-a[0]))/den
                wc=((b[0]-a[0])*(y-a[1])-(b[1]-a[1])*(x-a[0]))/den
                bary=(1-wb-wc,wb,wc)
                if min(bary)>=-.015:
                    z=sum(v*p[0][2] for v,p in zip(bary,tri))
                    matches.append((z,bary,tri))
            for vertex in tri:
                p=vertex[0]
                if (p[2]>.045) != back: near.append(((x-p[0])**2+(y-p[1])**2,vertex))
        if matches:
            z,bary,tri=(min if back else max)(matches,key=lambda m:m[0])
            influence=collections.defaultdict(float)
            for coeff,(_,j,w) in zip(bary,tri):
                for index,value in zip(j,w):influence[index]+=max(0,coeff)*value
            pairs=sorted(influence.items(),key=lambda a:-a[1])[:4]
            pairs+= [(0,0.)]*(4-len(pairs)); total=sum(v for _,v in pairs)
            return z,[j for j,_ in pairs],[v/total for _,v in pairs]
        _,(p,j,w)=min(near,key=lambda a:a[0])
        return p[2],j,w

    def garment_panel(material, rows, authored_normals=None):
        xyz=[]; normals=[]; tex=[]; joints=[]; weights=[]; indices=[]
        columns=len(rows[0]); count=len(rows)
        for r,row in enumerate(rows):
            for c,(x,y,z,j,w) in enumerate(row):
                xyz.append((x,y,z));tex.append((c/(columns-1)*.0038,r/(count-1)*.021))
                j=tuple(j);w=tuple(w);joints.append(j);weights.append(w)
                left=row[max(0,c-1)];right=row[min(columns-1,c+1)]
                up=rows[max(0,r-1)][c];down=rows[min(count-1,r+1)][c]
                u=[right[i]-left[i] for i in range(3)];v=[up[i]-down[i] for i in range(3)]
                n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                length=math.hypot(*n) or 1
                normals.append(tuple(a/length for a in n))
                if r+1<count and c+1<columns:
                    i=r*columns+c;indices.extend((i,i+columns,i+1,i+1,i+columns,i+columns+1))
        if authored_normals is not None:
            normals=authored_normals
            # Mirror seams and duplicate UV borders must keep an outward winding.
            for offset in range(0,len(indices),3):
                a,b,c=indices[offset:offset+3]
                ab=[xyz[b][k]-xyz[a][k] for k in range(3)]
                ac=[xyz[c][k]-xyz[a][k] for k in range(3)]
                face=(ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
                if sum(face[k]*(normals[a][k]+normals[b][k]+normals[c][k]) for k in range(3))<0:
                    indices[offset+1],indices[offset+2]=c,b
        attr=dict(POSITION=add(xyz,'f','VEC3',True),NORMAL=add(normals,'f','VEC3'),TEXCOORD_0=add(tex,'f','VEC2'),
                  JOINTS_0=add(joints,'H','VEC4'),WEIGHTS_0=add(weights,'f','VEC4'))
        mesh['primitives'].append(dict(attributes=attr,indices=add([(v,) for v in indices],'I','SCALAR'),material=mats[material]))

    # Close the three upper openings of the original helper. Their boundaries
    # are shared by UV splits, so match by bind position and follow actual edges.
    adjacent=collections.defaultdict(list)
    for (a,b),count in seams.items():
        if count==1 and min(a[1],b[1])>1.1:
            adjacent[a].append(b);adjacent[b].append(a)
    loops=[]
    while adjacent:
        first=next(iter(adjacent));loop=[first];previous=None;current=first
        while True:
            neighbors=adjacent.pop(current)
            if len(neighbors)!=2: raise ValueError("Kurta boundary is not a closed loop")
            nxt=neighbors[0] if neighbors[0]!=previous else neighbors[1]
            if nxt==first: break
            loop.append(nxt);previous,current=current,nxt
        loops.append(loop)
    if sorted(map(len,loops))!=[22,22,54]:raise ValueError("Unexpected kurta collar or cuff topology")
    for loop in loops:
        neck=min(p[0] for p in loop)<0<max(p[0] for p in loop)
        side="l" if loop[0][0]>0 else "r"
        rows=[]; panel_normals=[]
        for n in range(7):
            t=n/6.;row=[]
            for p in loop+[loop[0]]:
                x,y,z=p
                if neck:
                    angle=math.atan2((z-.022)/.051,x/.057)
                    # Close the shoulder yoke around a modest standing collar.
                    rim=(.057*math.cos(angle),1.447-.004*math.sin(angle),.022+.051*math.sin(angle))
                    j2,w2=(bones["Chest"],bones["Neck"]),(0.74,.26)
                else:
                    depth,radial=forearm(p,side)
                    length=math.hypot(*radial)
                    rim=tuple(c+.116*axis+r*.041/max(length,.0001)
                               for c,axis,r in zip(elbows[side],fore[side],radial))
                    j2,w2=(bones["Elbow"+side],),(1.,)
                moved=[a+t*(b-a) for a,b in zip(p,rim)]
                if neck:moved[1]+=.003*math.sin(math.pi*t)
                j1,w1=edge_binds[p];influence=collections.defaultdict(float)
                for i,w in zip(j1,w1):influence[i]+=(1-t)*w
                for i,w in zip(j2,w2):influence[i]+=t*w
                values=sorted(influence.items(),key=lambda pair:-pair[1])[:4]
                values.extend([(0,0.)]*(4-len(values)));total=sum(w for _,w in values)
                desired=(.48*math.cos(angle),.90,.48*math.sin(angle)) if neck else radial
                radius=math.hypot(*desired) or 1.
                normal=[a*(1-t)+(b/radius)*t for a,b in zip(edge_normals[p],desired)]
                length=math.hypot(*normal) or 1.
                panel_normals.append(tuple(v/length for v in normal))
                row.append((*moved,[i for i,_ in values],[w/total for _,w in values]))
            rows.append(row)
        garment_panel("top_teal_woven",rows,panel_normals)

    # Trace the stole over the shoulder, round its crest, then down the front.
    # Each vertex borrows weights from the actual triangle directly underneath.
    scarf=[]
    for r in range(43):
        row=[]
        for c in range(9):
            lateral=c/8.-.5
            if r<=9:
                t=r/9.;y=1.240+.141*t;x=.097+.018*t+lateral*.064
                z,j,w=attachment(x,y,True);z-=.005+.002*math.cos(lateral*math.tau*2.4)
            elif r<=15:
                t=(r-9)/6.;x=.115+lateral*.064;y=1.381+.022*math.sin(math.pi*t)
                back,jb,wb=attachment(x,1.381,True);front,j,w=attachment(x,1.381)
                z=back*(1-t)+front*t + .004*(2*t-1)
            else:
                t=(r-15)/27.;y=1.381-.396*t+.002*math.sin(lateral*8)*t
                x=.115-.136*t+.014*math.sin(t*math.pi*1.3)+lateral*(.064+.014*t)
                z,j,w=attachment(x,y)
                z+=.004+.003*(.5+.5*math.cos(12*lateral+3*t))+.008*t**3
            row.append((x,y,z,j,w))
        scarf.append(row)
    garment_panel('scarf',scarf)
    placket=[]
    for r in range(15):
        y=1.390-r*.010
        row=[]
        for c in range(3):
            x=(c-1)*.009; z,j,w=attachment(x,y)
            row.append((x,y,z+.0018,j,w))
        placket.append(row)
    garment_panel('top_seams',placket)
    name=dest.stem
    mesh['name']=name
    for n in doc['nodes']:
        if 'mesh' in n:n['name']=name
    doc.pop('animations',None)
    while len(blob)%4:blob.append(0)
    doc['buffers']=[{'byteLength':len(blob)}]
    head=json.dumps(doc,separators=(',',':')).encode()
    head+=b' '*(-len(head)%4)
    dest.write_bytes(struct.pack('<4sIII4s',b'glTF',2,28+len(head)+len(blob),len(head),b'JSON')+head+
                     struct.pack('<I4s',len(blob),b'BIN\0')+blob)
    print(name,"uses",len(doc['skins'][0]['joints']),"existing bones and",len(scarf),"continuous scarf rows")


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path); parser.add_argument('destination',type=Path)
    args=parser.parse_args();make(args.source,args.destination)
