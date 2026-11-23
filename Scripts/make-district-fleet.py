"""Build original, unbadged district traffic bodies in metres, with separate runtime wheel mounts."""
import argparse
import collections
import math
import runpy
from pathlib import Path

# glTF axes are right-handed, Y up and +Z forward. Unreal drives in +X.
PALETTE=[("body",(.16,.27,.31),.31,.46),("cream",(.73,.64,.43),.46,.22),
 ("iron",(.042,.052,.052),.43,.72),("glass",(.15,.26,.29),.14,.19),
 ("upholstery",(.12,.095,.077),.75,.02),("rubber",(.027,.034,.036),.78,0.),
 ("amber",(.87,.35,.046),.25,.20),("red",(.66,.036,.016),.25,.18),("lamp",(.82,.80,.63),.19,.17),
 ("panel",(.43,.46,.43),.55,.53),("teal",(.036,.21,.16),.34,.36),("textile",(.27,.24,.17),.83,0.)]
MATS=[{"name":n,"doubleSided":True,"pbrMetallicRoughness":{"baseColorFactor":[*color,.42 if n=="glass" else 1],
        "metallicFactor":metal,"roughnessFactor":rough},**({"alphaMode":"BLEND"} if n=="glass" else {})} for n,color,rough,metal in PALETTE]
IX={v[0]:i for i,v in enumerate(PALETTE)}
class Body:
    def __init__(self): self.parts=collections.defaultdict(list)
    def poly(self,name,p):
        a,b,c=p[:3]
        ab=[b[i]-a[i] for i in range(3)]; ac=[c[i]-a[i] for i in range(3)]
        n=(ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
        m=sum(x*x for x in n)**.5
        if m<.000001: return
        normal=tuple(i/m for i in n)
        self.parts[IX[name]].append([(*pt,*normal,pt[2]+pt[0],pt[1]) for pt in p])
    def box(self,name,x,y,z,w,h,l):
        pts=[(x+sx*w/2,y+sy*h/2,z+sz*l/2) for sx,sy,sz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        for face in ((0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4)):
            self.poly(name,[pts[i] for i in face])
    def loft(self,name,stations):
        rings=[]
        for z,width,bottom,top in stations:
            r=min(width*.14,(top-bottom)*.23,.19)
            rings.append([(x,y,z) for x,y in ((-width/2+r,bottom),(-width/2,bottom+r),(-width/2,top-r),(-width/2+r,top),
                (width/2-r,top),(width/2,top-r),(width/2,bottom+r),(width/2-r,bottom))])
        self.poly(name,list(reversed(rings[0])))
        for a,b in zip(rings,rings[1:]):
            for i in range(8): self.poly(name,[a[i],a[(i+1)%8],b[(i+1)%8],b[i]])
        self.poly(name,rings[-1])
    def rod(self,name,a,b,r):
        vec=[b[i]-a[i] for i in range(3)]
        size=sum(v*v for v in vec)**.5
        if size<.0001:return
        f=[v/size for v in vec]; u=[-f[2],0,f[0]] if abs(f[1])<.94 else [1,0,0]
        m=sum(v*v for v in u)**.5; u=[v/m for v in u]
        v=[f[1]*u[2]-f[2]*u[1],f[2]*u[0]-f[0]*u[2],f[0]*u[1]-f[1]*u[0]]
        for j in range(10):
            offsets=[[r*(math.cos(2*math.pi*k/10)*u[i]+math.sin(2*math.pi*k/10)*v[i]) for i in range(3)] for k in (j,j+1)]
            pts=[tuple(p[i]+o[i] for i in range(3)) for p,o in [(a,offsets[0]),(a,offsets[1]),(b,offsets[1]),(b,offsets[0])]]
            self.poly(name,pts)

def glazing(body,x,z0,z1,base,height,side):
    body.poly("glass",[(x,base,z0),(x,base,z1),(x*.91,base+height,z1-.15),(x*.91,base+height,z0+.12)])
    body.rod("iron",(x*.91,base+height,z0+.12),(x,base,z0),.026)

def passenger(name,L,W,H,hood,roof,finish):
    b=Body()
    b.loft(finish,[(-L*.5,W*.84,.38,.87),(-L*.44,W*.98,.26,.98),(-L*.29,W,.24,1.),
        (L*.22,W,.24,1.01),(L*.40,W*.97,.28,.91),(L*.5,W*.86,.34,.77)])
    b.loft("glass",[(-L*roof,W*.83,.98,1.01),(-L*.22,W*.86,.97,H-.04),(L*.06,W*.86,.99,H),(L*hood,W*.88,.96,1.01)])
    b.loft(finish,[(-L*.235,W*.81,H-.06,H-.015),(-L*.17,W*.84,H-.03,H+.015),(L*.055,W*.85,H-.025,H+.015),(L*.10,W*.81,H-.06,H-.02)])
    for s in (-1,1):
        x=s*W*.46
        for z in (-.19*L,.0,.17*L):
            b.rod("iron",(x,.96,z),(s*W*.413,H-.032,z-.09),.027)
        b.rod("iron",(s*W*.49,.49,-L*.31),(s*W*.49,.48,L*.29),.024)
        b.box(finish,s*W*.54,1.03,L*.12,.16,.10,.24)
        for z in (-L*.18,L*.07): b.box("panel",s*(W*.5+.007),.90,z,.025,.028,.115)
        b.box("lamp",s*W*.32,.70,L*.493,W*.19,.09,.046)
        b.box("red",s*W*.34,.76,-L*.493,W*.17,.092,.05)
    b.box("iron",0,.51,L*.495,W*.53,.13,.048)
    for j in range(3): b.box("panel",0,.47+j*.046,L*.508,W*.51,.012,.012)
    return b

def two_wheeler(scooter):
    b=Body()
    b.loft("teal" if scooter else "body",[(-.91,.19,.51,.60),(-.77,.44,.39,.75),(-.27,.46,.40,.79),
        (.05,.25,.28,.57),(.38,.27,.31,.82),(.55,.26,.53, .89)])
    b.loft("upholstery",[(-.74,.28,.73,.80),(-.55,.35,.75,.85),(-.19,.36,.75,.84),(.04,.24,.73,.79)])
    for s in (-1,1):
        b.rod("panel",(s*.075,.98,.46),(s*.075,.26,.66),.028)
        b.rod("iron",(s*.11,.54,-.24),(s*.10,.25,-.59),.034)
        b.rod("iron",(0,.96,.42),(s*.28,1.02,.35),.018)
        b.rod("panel",(s*.24,1.01,.35),(s*.33,1.22,.42),.008)
        b.box("glass",s*.34,1.22,.42,.10,.05,.02)
    b.box("lamp",0,1.,.48,.20,.12,.05); b.box("red",0,.64,-.86,.13,.06,.04)
    if not scooter:
        b.loft("cream",[(-.19,.22,.64,.77),(.03,.34,.66,.88),(.24,.24,.67,.83)])
        b.rod("panel",(.19,.36,-.73),(.19,.36,-.04),.055)
    return b

def rickshaw():
    b=Body()
    b.loft("teal",[(-1.27,1.28,.27,.76),(-1.18,1.4,.19,.84),(-.52,1.4,.22,.83),(.40,1.19,.19,.42),(1.18,.57,.27,.62)])
    b.loft("cream",[(-1.25,1.23,1.61,1.72),(-1.01,1.41,1.63,1.82),(.63,1.29,1.61,1.78),(.92,1.12,1.57,1.69)])
    for s in (-1,1):
        b.rod("iron",(s*.63,.60,-1.11),(s*.61,1.66,-1.11),.035)
        b.rod("cream",(s*.52,.68,1.06),(s*.54,1.63,.82),.041)
        b.rod("panel",(s*.66,.72,-.57),(s*.62,.74,.50),.021)
    b.poly("glass",[(-.49,.91,1.00),(.49,.91,1.00),(.53,1.57,.82),(-.53,1.57,.82)])
    b.box("upholstery",0,.57,-.72,1.16,.15,.45); b.box("upholstery",0,.87,-.94,1.17,.54,.12)
    b.box("upholstery",0,.59,.43,.48,.13,.37); b.box("textile",0,1.15,-1.18,1.18,.65,.05)
    b.box("lamp",0,.63,1.16,.25,.14,.10)
    for s in (-1,1): b.box("red",s*.51,.63,-1.26,.14,.13,.06)
    return b

def commercial(kind,L,W,H):
    b=Body(); bus=kind=="Bus"; tall=kind in ("Truck","Lorry")
    b.loft("iron",[(-L*.49,W*.85,.39,.54),(-L*.43,W*.96,.35,.60),(L*.42,W*.95,.35,.60),(L*.49,W*.82,.43,.57)])
    cab=L if bus else (2.05 if tall else L)
    begin=L*.5-cab
    b.loft("teal" if bus else "cream",[(begin,W*.91,.54,1.29),(begin+.20,W*.98,.49,1.32),(L*.43,W,.52,1.27),(L*.5,W*.92,.62,1.18)])
    roof_z=H if not tall else H-.26
    b.loft("cream",[(begin+.04,W*.92,roof_z-.17,roof_z-.07),(begin+.27,W*.98,roof_z-.17,roof_z),(L*.43,W*.96,roof_z-.17,roof_z),(L*.478,W*.88,roof_z-.19,roof_z-.06)])
    b.poly("glass",[(-W*.45,1.24,L*.494),(W*.45,1.24,L*.494),(W*.427,roof_z-.17,L*.464),(-W*.427,roof_z-.17,L*.464)])
    for s in (-1,1):
        count=9 if bus else 3 if not tall else 1
        for i in range(count):
            z0=begin+.15+(cab-.45)*i/count; z1=begin+.15+(cab-.45)*(i+1)/count
            glazing(b,s*W*.482,z0,z1,1.31,roof_z-1.50,s)
        b.rod("iron",(s*W*.47,1.30,L*.49),(s*W*.43,roof_z-.14,L*.46),.046)
        b.box("lamp",s*W*.36,.93,L*.499,.30,.16,.04); b.box("red",s*W*.38,1.0,-L*.489,.20,.16,.04)
        b.rod("iron",(s*W*.47,1.89,L*.37),(s*W*.58,1.89,L*.40),.021)
        b.box("iron",s*W*.58,1.79,L*.40,.09,.25,.08)
        if bus:
            for row in range(8): b.box("upholstery",s*W*.31,1.42,begin+.72+row*.87,.37,.46,.12)
        if kind=="Ambulance": b.box("amber",s*W*.491,1.27,-.49,.025,.14,L*.55)
    if tall:
        cargoL=L-cab-.15
        b.box("panel",0,(H+.63)/2,-L*.5+cargoL/2,W*.98,H-.63,cargoL)
        for i in range(int(cargoL/.15)):
            for side in (-1,1): b.box("cream",side*W*.495,(H+.68)/2,-L*.49+i*.15,.027,H-.76,.029)
    if kind=="Ambulance": b.box("amber",0,H+.06,.79,.86,.11,.24)
    if bus: b.box("iron",0,roof_z-.28,L*.473,W*.64,.23,.05)
    return b

if __name__=="__main__":
    a=argparse.ArgumentParser(description=__doc__); a.add_argument("out",type=Path); args=a.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    write=runpy.run_path(str(Path(__file__).with_name("prepare-riverfront-doors.py")))["store_mesh"]
    fleet={key:passenger(key,*data) for key,data in {"PremiumHatch":(4.06,1.75,1.54,.24,.41,"body"),"Sedan":(4.67,1.83,1.46,.27,.32,"body"),
        "LuxurySUV":(5.08,1.98,1.86,.27,.44,"iron"),"Sport":(4.36,1.94,1.19,.30,.32,"amber")}.items()}
    fleet.update({"Scooter":two_wheeler(True),"Motorcycle":two_wheeler(False),"Rickshaw":rickshaw()})
    for key,data in {"Bus":(8.6,2.40,2.95),"Van":(4.90,1.94,2.10),"Truck":(6.83,2.28,2.96),"Lorry":(8.1,2.39,3.32),"Ambulance":(5.13,2.01,2.32)}.items():
        fleet[key]=commercial(key,*data)
    for key,body in fleet.items():
        write("SM_Traffic"+key,body.parts,MATS,(0,0,0),args.out)
