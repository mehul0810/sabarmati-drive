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
    cloth_faces=[]
    for prim in mesh['primitives']:
        name=doc['materials'][prim['material']]['name']
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
                if min(points[i][1] for i in (a,b,c))<.575: kept.extend((a,b,c))
            prim['indices']=add([(i,) for i in kept],'I','SCALAR')
            narrow=[]
            for x,y,z in points:
                blend=max(0,min(1,(y-.48)/.074))*.25
                leg=math.copysign(.15,x)
                narrow.append((x*(1-blend)+leg*blend,y,z*(1-blend)+.026*blend))
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
            shaped.append((x*(1+.065*ease),y,(z-.032)*(1+.14*ease)+.032))
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

    def garment_panel(material, rows):
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
        attr=dict(POSITION=add(xyz,'f','VEC3',True),NORMAL=add(normals,'f','VEC3'),TEXCOORD_0=add(tex,'f','VEC2'),
                  JOINTS_0=add(joints,'H','VEC4'),WEIGHTS_0=add(weights,'f','VEC4'))
        mesh['primitives'].append(dict(attributes=attr,indices=add([(v,) for v in indices],'I','SCALAR'),material=mats[material]))

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
