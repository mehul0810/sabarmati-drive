"""Sculpt the exposed resident body and cut independent layers on its fitted rig."""
import numpy as np


def shape_chest(body, preset):
    # The neutral hm08 bust includes raised circular forms hidden by the everyday
    # shirt. Ease those into a continuous pectoral, or the supported knit surface.
    x, y, z = ((body+[0.,8.17,0.])*.102).T
    side = np.abs(x)
    def ramp(lo,hi,v):
        t = np.clip((v-lo)/(hi-lo),0,1)
        return t*t*(3-2*t)
    chest = ramp(1.145,1.191,y)*(1-ramp(1.26,1.31,y))*ramp(.045,.13,z)
    chest *= ramp(.014,.049,side)*(1-ramp(.132,.170,side))
    dome = .147-.020*((side-.052)/.114)**2-.26*(y-1.211)
    weight = (.86 if preset=="male" else .65)*chest
    body[:,2] -= weight*(z-dome)/.102
    body[:,1] += weight*.057


def refine_upper_anatomy(body, preset):
    x, y, z = body.T
    side = np.abs(x)

    def contour(cx, cy, sx, sy):
        return np.exp(-((side-cx)/sx)**2-((y-cy)/sy)**2)

    front = np.clip((z-.35)/.45, 0, 1)
    collarbone = np.exp(-((y-(5.27+.21*side))/.13)**2) * np.exp(-((side-.67)/.53)**2)
    navel = contour(0, 2.37, .12, .17)
    rectus = contour(.38, 2.91, .30, .62)
    lower_ribs = contour(.91, 3.23, .30, .31)
    if preset == "male":
        chest = .090*contour(.69, 4.42, .47, .40)-.042*contour(.78, 3.94, .51, .21)
        abdomen = .057*rectus+.043*lower_ribs
    else:
        chest = .073*contour(.65, 3.93, .40, .52)-.033*contour(.73, 3.44, .50, .17)
        abdomen = .025*rectus+.030*lower_ribs
    body[:, 2] += front*(.053*collarbone-.069*navel+chest+abdomen)


def refine_leg_anatomy(body, preset):
    x, y, z = body.T
    side = np.abs(x)
    patella = np.exp(-((side-1.53)/.29)**2-((y+3.69)/.34)**2)
    calf = np.exp(-((side-1.92)/.48)**2-((y+4.98)/.62)**2)
    front = np.clip((z-.31)/.33, 0, 1)
    rear = np.clip((.06-z)/.52, 0, 1)
    rise, calf_fullness = (.079, .054) if preset == "male" else (.054, .075)
    ankle = np.clip((y+5.75)/.32, 0, 1)
    body[:, 2] += rise*patella*front-calf_fullness*calf*rear*ankle


def cut_bralette(body, uv, normals, faces, tank=False):
    # One partition supplies both sides of every seam. Shared cut vertices use
    # identical positions and bone weights, including when the arms are raised.
    x,y,z = body.T
    side = np.abs(x)
    strap = np.clip(1-np.abs(side-.92)/.39,0,1)
    front = np.clip((z+.05)/.80,0,1)
    armhole = np.clip((side-.94)/.68,0,1)
    if tank:
        neckline = 4.30+.37*(1-front)+1.93*strap-.91*armhole
        hem = .87+.08*np.minimum(side,1.6)
    else:
        neckline = 4.64+.22*front+.17*np.minimum(side,1.)+1.53*strap-.72*armhole
        hem = 3.00+.10*np.minimum(side,1.6)
    upper = np.minimum(neckline-y,(1.72-side)*2.8+np.maximum(0.,4.64-y))
    coords = list(body)
    tex = list(uv)
    shading = list(normals)
    vertices = {}
    texcoords = {}
    panels = {name:[] for name in ("skin","foundation_knit","foundation_elastic")}

    def clip(poly, channel, sign):
        result = []
        for a,b in zip(poly,poly[1:]+poly[:1]):
            if sign*a[channel]>=0:
                result.append(a)
            if a[channel]*b[channel]<0:
                result.append(a+(b-a)*(a[channel]/(a[channel]-b[channel])))
        return result

    def add(name,poly):
        if len(poly)<3:
            return
        corners = []
        for row in poly:
            key = tuple(np.round(row[:3],8))
            if key not in vertices:
                vertices[key]=len(coords)
                coords.append(row[:3])
                shading.append(row[8:]/max(np.linalg.norm(row[8:]),1.e-12))
            # Use the body UVs for all surfaces so the seam duplicates are continuous.
            key_uv=tuple(np.round(row[3:5],8))
            if key_uv not in texcoords:
                texcoords[key_uv]=len(tex)
                tex.append(row[3:5])
            corners.append((vertices[key],texcoords[key_uv]))
        if len(set(i for i,t in corners))>=3:
            panels[name].append(corners)

    for face in faces:
        ids = [i for i,_ in face]
        if min(side[ids])>2.1 or max(y[ids]-hem[ids])<0 or max(upper[ids])<0:
            panels["skin"].append(face)
            continue
        polygon = [np.r_[body[i],uv[t],y[i]-hem[i],upper[i],y[i]-hem[i]-.15,normals[i]] for i,t in face]
        for n in range(1,len(face)-1):
            tri = [polygon[0],polygon[n],polygon[n+1]]
            above_hem = clip(tri,5,1)
            add("skin",clip(tri,5,-1))
            add("skin",clip(above_hem,6,-1))
            fabric = clip(above_hem,6,1)
            add("foundation_elastic",clip(fabric,7,-1))
            add("foundation_knit",clip(fabric,7,1))
    return np.asarray(coords),np.asarray(tex),np.asarray(shading),panels


def vertical_panel(vertices, uv, faces, low, high):
    """Cut a continuous sewn edge through the polygons and interpolate the original UVs."""
    polygons, positions, texcoords, keys = [], [], [], {}
    for face in faces:
        heights = [vertices[i,1] for i,_ in face]
        if max(heights)<=low or min(heights)>=high:
            continue
        poly = [np.r_[vertices[i],uv[t]] for i,t in face]
        for level,sign in ((low,1),(high,-1)):
            clipped = []
            for a,b in zip(poly,poly[1:]+poly[:1]):
                if (a[1]-level)*sign>=0:
                    clipped.append(a)
                if (a[1]-level)*(b[1]-level)<0:
                    clipped.append(a+(b-a)*((level-a[1])/(b[1]-a[1])))
            poly = clipped
        corners = []
        for row in poly:
            key = tuple(np.round(row,7))
            if key not in keys:
                keys[key] = len(positions)
                positions.append(row[:3])
                texcoords.append(row[3:])
            corners.append((keys[key],keys[key]))
        if len(set(corners))>=3:
            polygons.append(corners)
    return np.asarray(positions), np.asarray(texcoords), polygons


def lower_foundation(body, uv, faces, western=False, preset="male"):
    """Cut boxer briefs on the fitted garment shell and tuck the leg skin into its hems."""
    legs = [face for group,face in faces if group=="body"
            and -6.00 <= np.mean([body[i,1] for i,_ in face]) < -1.18]
    # Leave a short overlap inside the shorts and the ankle opening of each shoe.
    skin = body.copy()
    tuck = np.clip((skin[:,1]+1.68)/.52,0,1)
    side = np.where(skin[:,0]>=0,1.,-1.)
    skin[:,0] -= .055*(skin[:,0]-side*.98)*tuck
    skin[:,2] -= .085*(skin[:,2]-.22)*tuck
    if western:
        refine_leg_anatomy(skin, preset)
    yield "leg_skin",skin,uv,legs
    if western:
        shorts = skin.copy()
        taper = np.clip((shorts[:,1]+1.90)/1.04,0.,1.)
        taper = taper*taper*(3.-2.*taper)
        shorts[:,0] = side*1.04+(shorts[:,0]-side*1.04)*(1.-.37*taper)
        shorts[:,2] = .24+(shorts[:,2]-.24)*(1.-.32*taper)
        for name, top, mesh in (("shorts_skin",-.85,shorts),("trunk_skin",-.98,skin)):
            region = [face for group,face in faces if group=="body"
                      and -6.00 <= np.mean([body[i,1] for i,_ in face]) < top]
            yield name, mesh, uv, region
    # Continue the torso's real skin and UVs across the waist when the briefs are removed.
    # The upper foundation ends on the original .42 face ring; use its precise waist fit there.
    bare = body.copy()
    def ease(low, high, y):
        t = np.clip((y-low)/(high-low),0.,1.)
        return t*t*(3.-2.*t)
    y = bare[:,1]
    waist = (1.-ease(.45,1.72,y))*ease(-1.5,-.18,y)
    bare[:,0] *= 1.-.105*waist
    bare[:,2] = .13+(bare[:,2]-.13)*(1.-.19*waist)
    if western:
        refine_leg_anatomy(bare, preset)
        refine_upper_anatomy(bare, preset)
    for name,low,high in (("bare_lower_skin",-6.00,.42),("midriff_skin",.42,3.25)):
        region = []
        for group,face in faces:
            if group!="body": continue
            centre = body[[i for i,_ in face]].mean(axis=0)
            if low<=centre[1]<high and (centre[1]<-.5 or abs(centre[0])<2.4):
                region.append(face)
        yield name,bare,uv,region
    # Feet complete the same rings as the calves. An extra ankle section fits
    # inside each shoe opening, and the separate sock toe is shown without shoes.
    for side,sign in (("l",1),("r",-1)):
        def select(group, top):
            return [face for label,face in faces if label==group
                    and (centre:=body[[i for i,_ in face]].mean(axis=0))[1]<top and centre[0]*sign>0]

        foot = select("body",-6.00)
        yield "foot_skin_"+side,body,uv,foot
        if western:
            sock = select("helper-tights",-4.85)
            split = -7.51
            for name,low,high in (("cotton_sock",split,-5.80),("crew_sock",split,-5.08),
                                  ("sock_toe",-9.,split)):
                yield name+"_"+side,*vertical_panel(body,uv,sock,low,high)
            yield "foot_ankle_"+side,*vertical_panel(body,uv,foot,split,0.)
        else:
            yield "cotton_sock_"+side,body,uv,select("helper-tights",-5.80)
    shell = body.copy()
    shell[:,0] *= 1.030
    shell[:,2] = .18+(shell[:,2]-.18)*1.065
    tights = [face for group,face in faces if group=="helper-tights"]
    # Straight elastic and leg bands have cut edges independent of the source quad rows.
    underwear = [("underwear_knit",-1.53,.88),("underwear_band",.88,1.13)]
    if western:
        underwear.extend((("trunk_knit",-1.09,.71),("trunk_band",.71,.98)))
    for name,low,high in underwear:
        yield name,*vertical_panel(shell,uv,tights,low,high)
