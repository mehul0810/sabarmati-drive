"""Fit original short, swept hair and eye artwork to the public fictional resident.

The head is sampled from MakeHuman's CC0 hm08 body, also used by
make-district-residents.py. This script needs the same OBJ, the public
SK_RiverResident.glb export, and a directory for its GLB and two PNGs.
"""
import argparse
import collections
import math
from pathlib import Path
import runpy

SCRIPTS = Path(__file__).resolve().parent
TAU = math.tau
RINGS = 22
SIDES = 112


def scalp_sections(obj):
    vertices, faces = [], []
    group = ""
    for line in obj.read_text().splitlines():
        parts = line.split()
        if not parts: continue
        if parts[0] == "v":
            x,y,z = map(float, parts[1:4])
            vertices.append((x*.102,(y+8.17)*.102,z*.102))
        elif parts[0] == "g":
            group = parts[1]
        elif parts[0] == "f" and group == "body":
            points = [vertices[int(v.split("/")[0])-1] for v in parts[1:]]
            if min(p[1] for p in points)>1.465 and max(abs(p[0]) for p in points)<.14:
                faces.append(points)
    highest = max(p[1] for face in faces for p in face)
    bottom,top = 1.475,highest-.0003
    rows = []
    for row in range(97):
        y = bottom+(top-bottom)*row/96
        lines = []
        for face in faces:
            hits = []
            for a,b in zip(face,face[1:]+face[:1]):
                if (a[1]<=y<b[1]) or (b[1]<=y<a[1]):
                    t=(y-a[1])/(b[1]-a[1])
                    hits.append((a[0]+t*(b[0]-a[0]),a[2]+t*(b[2]-a[2])))
            if len(hits)==2: lines.append(hits)
        zs = [p[1] for line in lines for p in line]
        if not zs:
            raise ValueError(f"No scalp outline at {y}")
        centre=(min(zs)+max(zs))*.5
        radii=[]
        for col in range(SIDES):
            theta = TAU*col/SIDES
            dx,dz=math.sin(theta),math.cos(theta)
            nearest=0
            for (ax,az),(bx,bz) in lines:
                ex,ez=bx-ax,bz-az
                div=dx*ez-dz*ex
                if abs(div)<1.e-9: continue
                r=(ax*ez-(az-centre)*ex)/div
                along=(ax*dz-(az-centre)*dx)/div
                if -.0001<=along<=1.0001 and r>nearest: nearest=r
            radii.append(nearest)
        # The eyes and mouth are openings in the source body. Fill unobserved
        # directions from the two nearest closed angles on this same slice.
        closed=[i for i,r in enumerate(radii) if r>0]
        if not closed: raise ValueError(f"No scalp section at {y}")
        for i,r in enumerate(radii):
            if r>0: continue
            left=min(closed,key=lambda j:(i-j)%SIDES)
            right=min(closed,key=lambda j:(j-i)%SIDES)
            distance=(right-left)%SIDES
            weight=((i-left)%SIDES)/distance if distance else 0
            radii[i]=radii[left]*(1-weight)+radii[right]*weight
        rows.append((centre,radii))
    return bottom,top,rows


def hair_mesh(obj, doc, out):
    bottom,top,sections = scalp_sections(obj)
    nodes = {n["name"]:n for n in doc["nodes"]}
    pivot = tuple(sum(nodes[n]["translation"][i] for n in ("Root","Pelvis","Spine","Chest","Neck","Head")) for i in range(3))
    def surface(a,t):
        side=abs(math.sin(a)); front=math.cos(a)
        # Irregular front corners and a narrow pre-auricular sideburn. The nape
        # stays lower than the temples. The part is offset to the wearer’s left.
        edge=1.544 + .075*front + .018*side + .009*math.exp(-((side-.55)/.16)**2)*max(front,0)
        edge-= .019*math.exp(-((side-.88)/.075)**2)*max(front,0)
        edge+= .0016*math.sin(13*a)+.0009*math.sin(29*a+.18)+.0007*math.sin(53*a)
        phi=min(.999,max(-.09,t))*math.pi*.5
        y=min(top,max(bottom+.00001,edge+(top-edge)*math.sin(phi)))
        yi=(y-bottom)/(top-bottom)*96
        lo=min(95,int(yi)); fy=yi-lo
        si=(a/TAU%1)*SIDES
        col=int(si)%SIDES; fc=si-int(si)
        def interpolate(row):
            r=sections[row][1]
            return r[col]*(1-fc)+r[(col+1)%SIDES]*fc
        radius=interpolate(lo)*(1-fy)+interpolate(lo+1)*fy
        mid=sections[lo][0]*(1-fy)+sections[lo+1][0]*fy
        v=max(0,min(1,t)); crest=math.sin(v*math.pi*.76)
        # Heavier waved volume across the forehead, cropped at the ears.
        wave=.5+.5*math.cos(19*a-12*v+1.9*math.sin(a+.4))
        fullness=(.009+.009*max(front,0)*(.8-.35*math.sin(a)))*crest
        groove=.0045*math.exp(-((math.sin(a)-.36-.16*v)/.075)**2)*max(front,0)*crest
        lift=.0008+fullness-groove+.0036*wave*crest
        dx=math.sin(a); dz=math.cos(a)
        radius+=lift*max(0,math.cos(phi))
        y+=lift*max(0,math.sin(phi))
        # A slight forward sweep at the hairline breaks the default round cap.
        return (dx*radius-.004*v, y, mid+dz*radius+.005*max(front,0)*crest)
    def vertex(a,t,u,extra=0):
        p=surface(a,t)
        da,dt=surface(a+.002,t),surface(a,t+.001)
        A=[da[i]-p[i] for i in range(3)];B=[dt[i]-p[i] for i in range(3)]
        n=(A[1]*B[2]-A[2]*B[1],A[2]*B[0]-A[0]*B[2],A[0]*B[1]-A[1]*B[0])
        size=math.sqrt(sum(c*c for c in n))
        n=tuple(v/size for v in n) if size>1.e-9 else (0.,1.,0.)
        return (*(p[i]-pivot[i]+extra*n[i] for i in range(3)),*n,u,1.-t)
    parts=collections.defaultdict(list)
    for row in range(RINGS):
        t0,t1=(row/RINGS)*.985,((row+1)/RINGS)*.985
        for col in range(SIDES):
            u0,u1=col/SIDES,(col+1)/SIDES
            parts[0].append([vertex(u0*TAU,t0,u0),vertex(u1*TAU,t0,u1),
                             vertex(u1*TAU,t1,u1),vertex(u0*TAU,t1,u0)])
    crown=[surface(TAU*i/SIDES,.985) for i in range(SIDES)]
    cap=tuple(sum(p[i] for p in crown)/SIDES-pivot[i] for i in range(3))
    for i in range(SIDES):
        u0,u1=i/SIDES,(i+1)/SIDES
        parts[0].append([vertex(u0*TAU,.985,u0),vertex(u1*TAU,.985,u1),(*cap,0.,1.,0.,(u0+u1)*.5,0.)])
    # Tapered individual locks grow out of the hairline. Their widths and
    # lengths vary deterministically; the visible edge follows the skull.
    for n in range(137):
        a=TAU*(n+.29*math.sin(n*16.3))/137
        width=.011+.007*(.5+.5*math.sin(n*7.3))
        t0=-.016-.022*(.5+.5*math.sin(n*8.1))
        direction=.036*math.cos(a)
        tip=vertex(a+direction,t0,(a+direction)/TAU,.00055)
        mid0=vertex(a-width*.58,.018,(a-width*.58)/TAU,.001)
        mid1=vertex(a+width*.58,.018,(a+width*.58)/TAU,.001)
        base0=vertex(a-width,.066,(a-width)/TAU,.00028)
        base1=vertex(a+width,.066,(a+width)/TAU,.00028)
        parts[0].extend(([tip,mid1,mid0],[mid0,mid1,base1,base0]))
    mat=dict(name="cut_hair",doubleSided=True,pbrMetallicRoughness=dict(
        baseColorFactor=[.033,.020,.014,1],metallicFactor=0,roughnessFactor=.51))
    runpy.run_path(str(SCRIPTS/"prepare-riverfront-doors.py"))["store_mesh"]("SM_RiverSweptHair",parts,[mat],(0,0,0),out)
    print("Swept hair pivot",pivot,"source scalp height",top,"triangles",sum(len(v)-2 for v in parts[0]))


def hair_colour(out,png):
    size=512; tex=bytearray()
    for y in range(size):
        t=1-y/(size-1)
        for x in range(size):
            a=TAU*x/size
            flow=a-.78*t+.21*math.sin(a-.75*t)+.035*math.sin(3*a+2*t)+.014*math.sin(9*a-4*t)
            lock=.5+.5*math.cos(23*flow)
            strand=.5+.5*math.cos(215*flow+1.2*math.sin(11*flow))
            fine=.5+.5*math.sin(387*flow+math.sin(41*flow))
            root=.75+.25*min(1,t*7.)
            r=(28+14*lock+11*strand+5*fine)*root
            tex.extend((round(r),round(r*.76),round(r*.61)))
    png(out/"T_RiverSweptHair.png",size,tex)


def eye_colour(doc, data, read, out, png):
    # The source UVs dedicate only 0.8% of the atlas width to each iris.
    # Enlarge that original eye chart in a dedicated texture and material.
    size=512
    pixels=bytearray([173,161,144])*(size*size)
    for prim in doc["meshes"][0]["primitives"]:
        if doc["materials"][prim["material"]]["name"] not in ("iris","sclera"):continue
        attrs=prim["attributes"]; xyz=list(read(doc,data,attrs["POSITION"]))
        uv=[((u-.752)/.068*(size-1),(v-.038)/.038*(size-1)) for u,v in read(doc,data,attrs["TEXCOORD_0"])]
        indices=[row[0] for row in read(doc,data,prim["indices"])]
        for i in range(0,len(indices),3):
            a,b,c=indices[i:i+3];(s,t),(h,k),(u,v)=uv[a],uv[b],uv[c]
            den=(h-s)*(v-t)-(u-s)*(k-t)
            if abs(den)<1.e-7:continue
            for py in range(max(0,int(min(t,k,v))-1),min(size,math.ceil(max(t,k,v))+2)):
                for px in range(max(0,int(min(s,h,u))-1),min(size,math.ceil(max(s,h,u))+2)):
                    bb=((px+.5-s)*(v-t)-(py+.5-t)*(u-s))/den
                    cc=((h-s)*(py+.5-t)-(k-t)*(px+.5-s))/den
                    aa=1-bb-cc
                    if min(aa,bb,cc)<-.035:continue
                    x,y,z=(sum(w*xyz[index][j] for w,index in ((aa,a),(bb,b),(cc,c))) for j in range(3))
                    horizontal=(abs(x)-.03145)/.0059
                    vertical=(y-1.5765)/.0059
                    dist=math.hypot(horizontal,vertical); ang=math.atan2(vertical,horizontal)
                    lid=min(1,max(0,vertical*.25+.15))
                    veins=math.exp(-((abs(horizontal)-1.92)/.42)**2)
                    rgb=(181-12*lid+8*veins,175-14*lid-7*veins,161-12*lid-6*veins)
                    if dist<=1.03 and z>.131:
                        fibres=.5+.5*math.sin(53*ang+4*math.sin(8*ang)+3*dist)
                        melanin=math.sin(math.pi*min(1,dist))*(.6+.4*fibres)
                        rgb=(40+49*melanin,29+32*melanin,22+16*melanin)
                        if dist>.84:
                            outer=min(1,(dist-.84)/.15)
                            rgb=tuple(value*(1-.33*outer) for value in rgb)
                        if dist<.46:
                            f=max(0,min(1,(dist-.39)/.07))
                            rgb=tuple(8+(value-8)*f for value in rgb)
                    start=(py*size+px)*3
                    pixels[start:start+3]=bytes(int(max(0,min(255,v))) for v in rgb)
    png(out/"T_RiverEyes.png",size,pixels)


def tailoring(doc,data,read,out,png):
    """Bake the shirt construction and continuous garment hem on hm08's packed UVs."""
    size=1024
    pixels=bytearray([249,255,0])*(size*size)
    def line(x,y,ax,ay,bx,by):
        dx,dy=bx-ax,by-ay
        t=max(0,min(1,((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy)))
        return math.hypot(x-ax-t*dx,y-ay-t*dy)
    def pressed(d,width):
        return max(0,1-d/width)
    for pr in doc["meshes"][0]["primitives"]:
        name=doc["materials"][pr["material"]]["name"]
        if name not in ("top_teal_woven","top_seams","bottom_denim"): continue
        attrs=pr["attributes"]
        xyz=list(read(doc,data,attrs["POSITION"]))
        uv=[((u-.811)/.070*(size-1),(v-.006)/.088*(size-1)) for u,v in read(doc,data,attrs["TEXCOORD_0"])]
        ids=[row[0] for row in read(doc,data,pr["indices"])]
        for start in range(0,len(ids),3):
            a,b,c=ids[start:start+3]
            (s,t),(h,k),(u,v)=uv[a],uv[b],uv[c]
            den=(h-s)*(v-t)-(u-s)*(k-t)
            if abs(den)<1.e-7:continue
            for py in range(max(0,int(min(t,k,v))-1),min(size,math.ceil(max(t,k,v))+2)):
                for px in range(max(0,int(min(s,h,u))-1),min(size,math.ceil(max(s,h,u))+2)):
                    bb=((px+.5-s)*(v-t)-(py+.5-t)*(u-s))/den
                    cc=((h-s)*(py+.5-t)-(k-t)*(px+.5-s))/den; aa=1-bb-cc
                    if min(aa,bb,cc)<-.025:continue
                    x,y,z=(aa*xyz[a][j]+bb*xyz[b][j]+cc*xyz[c][j] for j in range(3))
                    ax=abs(x); shade=.97; pearl=0.
                    hem=.889+.028*min(1,(ax/.171)**2)+.006*max(0,-z/.14)
                    shirt=min(1,max(0,(y-hem+.0018)/.0036))
                    if name in ("top_teal_woven","top_seams"):
                        if z>.075 and ax<.205:
                            if ax<.019 and .93<y<1.394:
                                shade-=.18*pressed(abs(ax-.012),.0024)+.035*(ax<.012)
                                dy=(y-1.346+.0375)%.075-.0375
                                d=math.hypot(x,dy)
                                pearl=pressed(max(0,d-.0035),.0014)
                                if min(math.hypot(x-.001,dy),math.hypot(x+.001,dy))<.0007:pearl*=.3
                            collar=min(line(ax,y,.012,1.385,.055,1.339),line(ax,y,.055,1.339,.097,1.392))
                            shade-=.29*pressed(collar,.0031)
                            if x>.035 and x<.149 and 1.150<y<1.253:
                                d=min(line(x,y,.045,1.243,.136,1.243),
                                      line(x,y,.045,1.243,.049,1.173),
                                      line(x,y,.049,1.173,.090,1.157),
                                      line(x,y,.090,1.157,.131,1.173),
                                      line(x,y,.131,1.173,.136,1.243))
                                shade-=.23*pressed(d,.0025)
                            # Cloth compresses in short diagonals at the sides
                            # rather than repeating a fold across the panel.
                            fold=line(ax,y,.105,.981,.167,1.016)
                            shade-=.093*pressed(fold,.006)
                            fold=line(ax,y,.095,1.218,.158,1.262)
                            shade-=.075*pressed(fold,.0055)
                        if ax>.18:
                            distance=abs(y-(1.262-.49*(ax-.20)))
                            shade-=.087*pressed(distance,.0055)
                        if z<.01 and ax<.165:
                            shade-=.22*pressed(abs(y-(1.287+.055*ax)),.003)
                    else:
                        if z>.043 and y>.72:
                            shade-=.24*pressed(line(ax,y,.109,.924,.193,.793),.0034)
                            shade-=.18*pressed(line(x,y,.012,.93,.012,.79),.0026)
                        if .24<y<.51:
                            knee=math.exp(-((y-.37)/.092)**2)
                            shade-=.021*knee*(1+math.sin(99*y+17*x+23*z))
                    if 0<y-hem<.014:
                        shade-=.15*pressed(abs((y-hem)-.008),.0022)
                    pos=(py*size+px)*3
                    pixels[pos:pos+3]=bytes(round(255*min(1,max(0,c))) for c in (shade,shirt,pearl))
    png(out/"T_RiverTailoring.png",size,pixels)


if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("obj",type=Path); ap.add_argument("resident",type=Path); ap.add_argument("out",type=Path)
    args=ap.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    io=runpy.run_path(str(SCRIPTS/"prepare-riverfront-hatch.py"))
    png=runpy.run_path(str(SCRIPTS/"make-morning-wardrobe.py"))["png"]
    doc,data=io["read_glb"](args.resident)
    hair_mesh(args.obj,doc,args.out); hair_colour(args.out,png); eye_colour(doc,data,io["vectors"],args.out,png)
    tailoring(doc,data,io["vectors"],args.out,png)
