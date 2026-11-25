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
 ("panel",(.43,.46,.43),.55,.53),("teal",(.036,.21,.16),.34,.36),("textile",(.27,.24,.17),.83,0.),
 ("canopy",(.54,.32,.048),.72,0.)]
MATS=[{"name":n,"doubleSided":True,"pbrMetallicRoughness":{"baseColorFactor":[*color,.42 if n=="glass" else 1],
        "metallicFactor":metal,"roughnessFactor":rough},**({"alphaMode":"BLEND"} if n=="glass" else {})} for n,color,rough,metal in PALETTE]
IX={v[0]:i for i,v in enumerate(PALETTE)}

# Axle offsets and rolling radii in metres, matching the runtime wheel mounts.
WHEEL_LAYOUTS = {
    "Sedan": (1.43, .32, .78, .21),
    "PremiumHatch": (1.22, .31, .73, .18),
    "LuxurySUV": (1.57, .37, .84, .25),
    "Sport": (1.32, .32, .83, .24),
    "Bus": (2.73, .43, .97, .29),
    "Van": (1.49, .35, .80, .22),
    "Truck": (2.15, .44, .92, .29),
    "Ambulance": (1.56, .35, .84, .22),
    "Lorry": (2.58, .48, 1.00, .31),
}

def wheel_openings(stations, axle, radius):
    """Raise the sill into a circular opening where a tyre meets the body."""
    clearance = radius + .038
    points = {row[0] for row in stations}
    for centre in (-axle, axle):
        points.update(centre + clearance * t for t in (-1, -.94, -.79, -.55, -.29, 0, .29, .55, .79, .94, 1))
    result = []
    for z in sorted(points):
        if z <= stations[0][0] or z >= stations[-1][0]:
            continue
        a, b = next((a, b) for a, b in zip(stations, stations[1:]) if a[0] <= z <= b[0])
        alpha = (z - a[0]) / (b[0] - a[0])
        width, bottom, top = (left + (right - left) * alpha for left, right in zip(a[1:], b[1:]))
        offset = abs(abs(z) - axle)
        if offset < clearance:
            arch = radius + math.sqrt(clearance ** 2 - offset ** 2)
            bottom = min(top - .065, max(bottom, arch))
        result.append((z, width, bottom, top))
    return [stations[0], *result, stations[-1]]

class Body:
    def __init__(self): self.parts=collections.defaultdict(list)
    def poly(self,name,p,normals=None):
        a,b,c=p[:3]
        ab=[b[i]-a[i] for i in range(3)]; ac=[c[i]-a[i] for i in range(3)]
        n=(ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
        m=sum(x*x for x in n)**.5
        if m<.000001: return
        if normals is None:
            normals = [tuple(i/m for i in n)] * len(p)
        self.parts[IX[name]].append([(*pt,*normal,pt[2]+pt[0],pt[1]) for pt,normal in zip(p,normals)])

    def surface(self, name, columns, rows, point):
        """Sample a curved panel with continuous normals across tessellation edges."""
        vertices = []
        for row in range(rows + 1):
            v = row / rows
            for column in range(columns + 1):
                u = column / columns
                position = point(u, v)
                pu = point(min(1.,u+.001),v)
                pv = point(u,min(1.,v+.001))
                du = tuple(a-b for a,b in zip(pu,point(max(0.,u-.001),v)))
                dv = tuple(a-b for a,b in zip(pv,point(u,max(0.,v-.001))))
                nx = du[1]*dv[2]-du[2]*dv[1]
                ny = du[2]*dv[0]-du[0]*dv[2]
                nz = du[0]*dv[1]-du[1]*dv[0]
                length = math.sqrt(nx*nx+ny*ny+nz*nz) or 1.
                vertices.append((position,(nx/length,ny/length,nz/length)))
        for row in range(rows):
            for column in range(columns):
                i = row * (columns + 1) + column
                p, n = zip(*(vertices[j] for j in (i,i+1,i+columns+2,i+columns+1)))
                self.poly(name,p,n)
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
        # These cross sections are clockwise when viewed from the nose.
        # Keep outward normals on the paint, including the ends of each panel.
        self.poly(name,rings[0])
        for a,b in zip(rings,rings[1:]):
            for i in range(8): self.poly(name,[a[i],b[i],b[(i+1)%8],a[(i+1)%8]])
        self.poly(name,list(reversed(rings[-1])))
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
    pane=[(x,base,z0+.016),(x,base,z1-.016),(x*.91,base+height,z1-.016),(x*.91,base+height,z0+.016)]
    body.poly("glass",list(reversed(pane)) if side>0 else pane)

def passenger(name,L,W,H,hood,roof,finish):
    b=Body()
    axle, radius, track, tyre = WHEEL_LAYOUTS[name]
    skin = [(-L*.5,W*.84,.38,.87),(-L*.44,W*.98,.26,.98),(-L*.29,W,.24,1.),
        (L*.22,W,.24,1.01),(L*.40,W*.97,.28,.91),(L*.5,W*.86,.34,.77)]
    b.loft(finish, wheel_openings(skin, axle, radius))
    b.box("iron", 0, .29, 0, 2 * track - tyre - .05, .14, L * .81)
    b.loft("glass",[(-L*roof,W*.83,.98,1.01),(-L*.22,W*.86,.97,H-.04),(L*.06,W*.86,.99,H),(L*hood,W*.88,.96,1.01)])
    b.loft(finish,[(-L*.235,W*.81,H-.06,H-.015),(-L*.17,W*.84,H-.03,H+.015),(L*.055,W*.85,H-.025,H+.015),(L*.10,W*.81,H-.06,H-.02)])
    for s in (-1,1):
        x=s*W*.46
        for z in (-.19*L,.0,.17*L):
            b.rod("iron",(x,.96,z),(s*W*.413,H-.032,z-.09),.027)
        b.rod("iron",(s*W*.49,.49,-axle+radius+.06),(s*W*.49,.48,axle-radius-.06),.024)
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
    b = Body()
    # An open step-through cabin over a narrow underframe and pressed lower panels.
    body = [(-1.31,1.19,.32,.66),(-1.24,1.30,.23,.74),(-1.10,1.33,.21,.77),
            (-.88,1.32,.21,.75),(-.72,1.30,.22,.72),(-.52,1.26,.22,.56),
            (-.40,1.23,.22,.43),(.47,1.11,.18,.38),(.69,1.11,.19,.59),
            (.83,1.06,.25,.75),(.99,.92,.31,.82),(1.10,.81,.39,.79),(1.17,.70,.43,.71)]
    b.loft("teal",wheel_openings(body,1.0,.226))
    b.loft("iron",[(-1.24,.78,.172,.36),(-.56,.84,.172,.35),(.66,.72,.175,.31)])
    b.loft("rubber",[(-.55,1.06,.35,.375),(.48,1.02,.34,.368),(.69,.98,.35,.373)])
    b.loft("canopy",[(.78,1.06,.73,.86),(.99,.92,.77,.89),(1.10,.80,.72,.86),(1.175,.70,.68,.79)])
    b.loft("rubber",[(1.14,.64,.46,.53),(1.23,.60,.44,.51)])
    for x in (-.23,.23):
        b.rod("iron",(x,.48,1.17),(x,.47,1.23),.024)
    # Smooth front mudguard. Clearance follows the actual rolling tyre.
    b.surface("teal",18,8,lambda u,v:
        ((v-.5)*(.23-.04*abs(2*u-1)**2),.226+.269*math.sin(2.76-u*2.50),1.+.269*math.cos(2.76-u*2.50)))
    for s in (-1,1):
        b.rod("panel",(s*.095,.53,1.023),(s*.092,.242,1.0),.022)
        b.rod("rubber",(s*.097,.38,1.013),(s*.095,.265,1.002),.029)

    def roof(u, v):
        x = 2*u-1
        width = .632 - .083*(1-v)**3 - .035*v**7
        z = .65-1.94*v + .10*abs(x)**4*(2*v-1)
        y = 1.572 + .164*(1-x*x) + .012*math.sin(math.pi*v) - .032*(1-v)**4 - .018*v**6
        return x*width,y,z

    b.surface("canopy",18,22,roof)
    def lining(u,v):
        x,y,z = roof(1.-u,v)
        return x,y-.013,z
    b.surface("textile",14,16,lining)
    # The fabric turns down at its bound edges and closes behind the bench.
    for side in (0.,1.):
        def hem(u,v):
            x,y,z = roof(side,u)
            return x * (1.-.038*v),y-.067*v,z
        b.surface("canopy",22,3,hem if side==0. else lambda u,v:hem(1.-u,v))
        for i in range(12):
            b.rod("textile",roof(side,i/12),roof(side,(i+1)/12),.007)
    b.surface("canopy",16,5,lambda u,v:
        ((2*u-1)*(.597+.046*v), roof(u,1.)[1]-.42*v,-1.29+.10*abs(2*u-1)**4+.014*v))
    for v in (.29,.56,.82):
        # Support bows inside the roof show individual ribs through the side opening.
        for i in range(8):
            a = roof(i/8,v)
            z = roof((i+1)/8,v)
            b.rod("iron",(a[0],a[1]-.025,a[2]),(z[0],z[1]-.025,z[2]),.010)

    def windscreen(u,v):
        x = 2*u-1
        return x*(.495-.026*v),.86+.685*v+.021*(1-x*x),1.072-.499*v+.064*(1-x*x)

    b.surface("glass",16,10,windscreen)
    for s in (0.,1.):
        for i in range(8):
            b.rod("iron",windscreen(s,i/8),windscreen(s,(i+1)/8),.025)
    for v in (0.,1.):
        for i in range(12):
            b.rod("rubber",windscreen(i/12,v),windscreen((i+1)/12,v),.018)
    b.rod("iron",windscreen(.35,.025),windscreen(.69,.45),.009)
    b.rod("rubber",windscreen(.612,.36),windscreen(.76,.52),.009)

    b.loft("upholstery",[(-.98,1.12,.573,.705),(-.91,1.17,.57,.716),(-.55,1.15,.584,.711),(-.51,1.06,.602,.691)])
    b.loft("upholstery",[(-1.12,1.04,.68,1.184),(-1.06,1.16,.67,1.224),(-1.0,1.10,.69,1.19)])
    b.loft("upholstery",[(.16,.41,.45,.554),(.23,.48,.44,.572),(.50,.47,.445,.57),(.55,.36,.469,.546)])
    b.loft("iron",[(.69,.75,.73,.847),(.81,.79,.73,.87),(.95,.70,.74,.842)])
    b.rod("panel",(0,.40,.83),(0,.92,.765),.022)
    b.rod("panel",(-.27,.936,.69),(0,.92,.765),.017)
    b.rod("panel",(0,.92,.765),(.27,.936,.69),.017)
    b.box("rubber",0,.876,.814,.18,.063,.115)
    for side in (-1,1):
        b.rod("rubber",(side*.21,.931,.71),(side*.32,.947,.676),.028)
        b.rod("iron",(side*.599,.66,-1.105),(side*.572,1.59,-1.14),.027)
        b.rod("iron",(side*.548,.534,.62),(side*.529,1.56,.61),.021)
        b.rod("panel",(side*.585,.56,-.411),(side*.585,.94,-.37),.018)
        b.rod("panel",(side*.585,.94,-.37),(side*.596,.98,-.74),.018)
        b.rod("panel",(side*.596,.98,-.74),(side*.602,.70,-.91),.018)
        b.rod("rubber",(side*.581,.31,-.39),(side*.544,.31,.58),.013)
        b.rod("panel",(side*.47,1.025,.966),(side*.633,1.258,.882),.009)
        # These mirrors and lamp housings are separate shells so bright lamps leave their surroundings dark.
        b.surface("iron",12,8,lambda u,v:
            (side*.623+.061*math.sin(math.pi*v)*math.cos(math.tau*u),
             1.277+.046*math.cos(math.pi*v),.869+.022*math.sin(math.pi*v)*math.sin(math.tau*u)))
        b.box("panel",side*.623,1.277,.847,.087,.054,.01)
        b.box("rubber",side*.519,.656,-1.287,.158,.152,.062)
        b.box("red",side*.519,.687,-1.322,.122,.081,.029)
        b.box("amber",side*.519,.617,-1.32,.119,.038,.024)
        b.box("amber",side*.338,.683,1.157,.084,.048,.036)
        for i in range(4):
            b.box("iron",side*(.31+i*.029),.542,-1.312,.016,.095,.019)
    b.loft("iron",[(1.135,.31,.643,.819),(1.18,.295,.65,.812),(1.205,.25,.664,.782)])
    b.surface("lamp",12,6,lambda u,v:
        ((2*u-1)*.121*(1.-.17*abs(2*v-1)**3),.724+(2*v-1)*.057,
         1.207+.012*(1.-(2*u-1)**2)*(1.-(2*v-1)**2)))
    b.box("cream",0,.427,-1.326,.30,.081,.016)
    b.rod("rubber",(-.55,.375,-1.32),(.55,.375,-1.32),.032)
    return b


def rickshaw_wheel():
    b = Body()

    def lathe(name, profile):
        for (start_x,start_r),(end_x,end_r) in zip(profile,profile[1:]):
            def ring(u,v):
                a = math.tau*u
                radius = start_r+(end_r-start_r)*v
                return start_x+(end_x-start_x)*v, radius*math.cos(a), radius*math.sin(a)
            b.surface(name,32,1,ring)

    # Utility tyre over a pressed steel wheel with a deep hub and cooling recesses.
    lathe("rubber",[(-.045,.123),(-.054,.151),(-.055,.192),(-.049,.215),(-.034,.225),
        (.034,.225),(.049,.215),(.055,.192),(.054,.151),(.045,.123)])
    lathe("iron",[(-.043,.122),(-.050,.123),(-.050,.115),(-.029,.110),(-.025,.085),
        (-.041,.057),(-.045,.043),(-.043,.012)])
    lathe("iron",[(.043,.012),(.045,.043),(.041,.057),(.025,.085),
        (.029,.110),(.050,.115),(.050,.123),(.043,.122)])
    for side in (-1,1):
        for i in range(6):
            a = math.tau*i/6
            y,z = .085*math.cos(a),.085*math.sin(a)
            b.box("rubber",side*.031,y,z,.009,.018,.016)
        for i in range(4):
            a = math.tau*i/4
            y,z = .040*math.cos(a),.040*math.sin(a)
            b.box("panel",side*.046,y,z,.012,.012,.012)
        b.box("iron",side*.047,0,0,.023,.033,.033)
    return b

def commercial(kind,L,W,H):
    b=Body(); bus=kind=="Bus"; tall=kind in ("Truck","Lorry")
    axle, radius, track, tyre = WHEEL_LAYOUTS[kind]
    frame_width = 2 * track - tyre - .08
    b.loft("iron",[(-L*.49,frame_width*.85,.39,.54),(-L*.43,frame_width,.35,.60),
        (L*.42,frame_width,.35,.60),(L*.49,frame_width*.82,.43,.57)])
    cab=L if bus else (2.05 if tall else L)
    begin=L*.5-cab
    lower_cab = [(begin,W*.91,.54,1.29),(begin+.20,W*.98,.49,1.32),
        (L*.43,W,.52,1.27),(L*.5,W*.92,.62,1.18)]
    b.loft("teal" if bus else "cream", wheel_openings(lower_cab, axle, radius))
    roof_z=H if not tall else H-.26
    b.loft("cream",[(begin+.04,W*.92,roof_z-.17,roof_z-.07),(begin+.27,W*.98,roof_z-.17,roof_z),(L*.43,W*.96,roof_z-.17,roof_z),(L*.478,W*.88,roof_z-.19,roof_z-.06)])
    b.poly("glass",[(-W*.45,1.24,L*.494),(W*.45,1.24,L*.494),(W*.427,roof_z-.17,L*.464),(-W*.427,roof_z-.17,L*.464)])
    b.box("cream",0,roof_z-.202,L*.462,W*.86,.10,.108)
    b.box("iron",0,.59,L*.492,W*.87,.18,.13)
    rear=L*.49 if not tall else cab-L*.5
    b.box("cream",0,1.36,-rear,W*.90,.24,.065)
    b.poly("glass",[(W*.44,1.45,-rear),(-W*.44,1.45,-rear),(-W*.42,roof_z-.17,-rear+.08),(W*.42,roof_z-.17,-rear+.08)])
    if not tall:
        b.box("iron",0,.59,-L*.49,W*.90,.16,.14)
        b.box("panel",0,.88,-L*.499,.37,.115,.033)
    for s in (-1,1):
        count=9 if bus else 3 if not tall else 1
        b.rod("cream",(s*W*.475,1.30,begin+.06),(s*W*.455,1.30,L*.465),.037)
        b.rod("cream",(s*W*.436,roof_z-.21,begin+.095),(s*W*.43,roof_z-.21,L*.464),.032)
        b.rod("iron",(s*W*.448,1.37,-rear),(s*W*.423,roof_z-.19,-rear+.08),.031)
        for i in range(count):
            z0=begin+.15+(cab-.45)*i/count; z1=begin+.15+(cab-.45)*(i+1)/count
            glass_x=s*W*.482
            glazing(b,glass_x,z0,z1,1.31,roof_z-1.50,s)
            b.rod("iron",(glass_x*.91,roof_z-.18,z0),(glass_x,1.31,z0),.024)
        b.rod("iron",(glass_x*.91,roof_z-.18,z1),(glass_x,1.31,z1),.024)
        b.rod("iron",(s*W*.47,1.30,L*.49),(s*W*.43,roof_z-.14,L*.46),.046)
        b.box("lamp",s*W*.36,.93,L*.499,.30,.16,.04); b.box("red",s*W*.38,1.0,-L*.489,.20,.16,.04)
        b.rod("iron",(s*W*.47,1.89,L*.37),(s*W*.58,1.89,L*.40),.021)
        b.box("iron",s*W*.58,1.79,L*.40,.09,.25,.08)
        if bus:
            for row in range(8): b.box("upholstery",s*W*.31,1.42,begin+.72+row*.87,.37,.46,.12)
        if kind=="Ambulance": b.box("amber",s*W*.491,1.225,-.49,.025,.085,L*.55)
    if tall:
        cargoL=L-cab-.15
        floor = 2 * radius + .075
        b.box("panel",0,(H+floor)/2,-L*.5+cargoL/2,W*.98,H-floor,cargoL)
        for i in range(int(cargoL/.15)):
            for side in (-1,1): b.box("cream",side*W*.495,(H+floor)/2,-L*.49+i*.15,.027,H-floor-.08,.029)
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
    write("SM_TrafficRickshawWheel",rickshaw_wheel().parts,MATS,(0,0,0),args.out)
