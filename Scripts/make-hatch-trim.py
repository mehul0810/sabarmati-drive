"""Machine the grille, lamp projectors and circular leather cockpit rim for the unbadged RHD hatch."""
from pathlib import Path
import math
import runpy
import sys
lib=runpy.run_path(str(Path(__file__).with_name("make-district-fleet.py"))); B=lib["Body"]
write=runpy.run_path(str(Path(__file__).with_name("prepare-riverfront-doors.py")))["store_mesh"]
def circle(b,material,centre,radius,tube,tilt=0,count=42):
    x,y,z=centre
    def ring(i,j):
        a=2*math.pi*i/count;c=2*math.pi*j/8
        r=radius+tube*math.cos(c)
        return (x+r*math.cos(a),y+r*math.sin(a),z+r*math.sin(a)*tilt+tube*math.sin(c))
    for i in range(count):
        for j in range(8):b.poly(material,[ring(i,j),ring(i+1,j),ring(i+1,j+1),ring(i,j+1)])
def make(dest):
    dest.mkdir(parents=True,exist_ok=True)
    body=B();paint=lib["MATS"]
    # Slim, slightly curved grille slats follow the central opening and reveal the dark radiator behind.
    for upper,columns,span,z,y in ((True,17,.38,2.048,.695),(False,23,.64,2.048,.392)):
        for row in range(3 if upper else 5):
            for i in range(columns):
                x=(i/(columns-1)-.5)*2*span
                centre=z-.060*(abs(x)/max(.031,span))**2
                next_x=x+min(.037,span*2/(columns-1)*.85)
                body.rod("panel" if row==0 and upper else "rubber",(x,y-row*.019,centre),(next_x,y-row*.019,centre-.001),.0025)
        for x in (-span,-span*.5,0,span*.5,span):
            body.rod("iron",(x,y-.095,z-.016),(x,y+.004,z-.016),.0036)
    for s in (-1,1):
        for j in range(2):
            x=s*(.615+j*.086);z=2.011-j*.038
            circle(body,"panel",(x,.715,z),.029,.004,0,22)
            for k in range(8):
                a=math.pi*k/4;body.rod("lamp",(x,.715,z+.002),(x+.020*math.cos(a),.715+.020*math.sin(a),z+.004),.0014)
        body.rod("lamp",(s*.49,.768,2.014),(s*.69,.764,1.987),.0045)
        body.rod("lamp",(s*.69,.764,1.987),(s*.806,.755,1.881),.0043)
        body.rod("panel",(s*.49,.777,2.015),(s*.807,.765,1.881),.003)
    # Original leather rim and contrasting stitch. Existing column, horn pad and stalks stay visible.
    circle(body,"rubber",(-.371,.93,.124),.184,.022,.34,72)
    circle(body,"upholstery",(-.371,.93,.102),.173,.0024,.33,72)
    for i in range(40):
        a=2*math.pi*i/40;r=.170
        def at(v):
            y=r*math.sin(v)
            return (-.371+r*math.cos(v),.93+y,.099+y*.34)
        body.rod("panel",at(a),at(a+.047),.001)
    for s in (-1,1):
        body.rod("iron",(-.371+s*.067,.931,.10),(-.371+s*.161,.951,.12),.012)
        body.rod("rubber",(-.371+s*.063,.923,.108),(-.371+s*.145,.944,.112),.015)
    # Delicate stationary vent louvres, dial surrounds and a small centre-stack trim.
    for x in (-.57,-.05,.33,.57):
        for k in range(4):body.rod("iron",(x-.045,.911+k*.008,.163),(x+.045,.911+k*.008,.163),.002)
    write("SM_HatchTrim",body.parts,paint,(0,0,0),dest)
if __name__=="__main__":make(Path(sys.argv[1]))
