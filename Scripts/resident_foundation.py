"""Shape the exposed chest and cut a sewn athletic bralette on the resident body."""
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


def cut_bralette(body, uv, normals, faces):
    # One partition supplies both sides of every seam. Shared cut vertices use
    # identical positions and bone weights, including when the arms are raised.
    x,y,z = body.T
    side = np.abs(x)
    strap = np.clip(1-np.abs(side-.92)/.39,0,1)
    front = np.clip((z+.05)/.80,0,1)
    armhole = np.clip((side-.94)/.68,0,1)
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


def lower_foundation(body, uv, faces):
    """Cut boxer briefs on the fitted garment shell and tuck the leg skin into its hems."""
    legs = [face for group,face in faces if group=="body"
            and -6.00 <= np.mean([body[i,1] for i,_ in face]) < -1.18]
    # Leave a short overlap inside the shorts and the ankle opening of each shoe.
    skin = body.copy()
    tuck = np.clip((skin[:,1]+1.68)/.52,0,1)
    side = np.where(skin[:,0]>=0,1.,-1.)
    skin[:,0] -= .055*(skin[:,0]-side*.98)*tuck
    skin[:,2] -= .085*(skin[:,2]-.22)*tuck
    yield "leg_skin",skin,uv,legs
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
    for name,low,high in (("bare_lower_skin",-6.00,.42),("midriff_skin",.42,3.25)):
        region = []
        for group,face in faces:
            if group!="body": continue
            centre = body[[i for i,_ in face]].mean(axis=0)
            if low<=centre[1]<high and (centre[1]<-.5 or abs(centre[0])<2.4):
                region.append(face)
        yield name,bare,uv,region
    # Feet complete the same rings as the calves. The tights helper forms a closed sock toe.
    for side,sign in (("l",1),("r",-1)):
        for group,name,top in (("body","foot_skin",-6.00),("helper-tights","cotton_sock",-5.80)):
            region=[]
            for label,face in faces:
                if label!=group: continue
                centre=body[[i for i,_ in face]].mean(axis=0)
                if centre[1]<top and centre[0]*sign>0: region.append(face)
            yield name+"_"+side,body,uv,region
    shell = body.copy()
    shell[:,0] *= 1.030
    shell[:,2] = .18+(shell[:,2]-.18)*1.065
    tights = [face for group,face in faces if group=="helper-tights"]
    # Straight elastic and leg bands have cut edges independent of the source quad rows.
    for name,low,high in (("underwear_knit",-1.53,.88),("underwear_band",.88,1.13)):
        polygons,positions,texcoords,keys = [],[],[],{}
        for face in tights:
            heights = [shell[i,1] for i,_ in face]
            if max(heights)<=low or min(heights)>=high: continue
            poly = [np.r_[shell[i],uv[t]] for i,t in face]
            for level,sign in ((low,1),(high,-1)):
                clipped = []
                for a,b in zip(poly,poly[1:]+poly[:1]):
                    if (a[1]-level)*sign>=0: clipped.append(a)
                    if (a[1]-level)*(b[1]-level)<0:
                        clipped.append(a+(b-a)*((level-a[1])/(b[1]-a[1])))
                poly=clipped
            corners=[]
            for row in poly:
                key=tuple(np.round(row,7))
                if key not in keys:
                    keys[key]=len(positions)
                    positions.append(row[:3])
                    texcoords.append(row[3:])
                corners.append((keys[key],keys[key]))
            if len(set(corners))>=3: polygons.append(corners)
        yield name,np.asarray(positions),np.asarray(texcoords),polygons

def jersey_panels(body, uv, faces):
    """Cut a short-sleeve crew neck from the continuous fitted clothing shell."""
    sections={name:([],[],[]) for name in ("jersey_knit","jersey_binding")}
    for g,face in faces:
        if g!="helper-tights": continue
        poly=[]
        for i,t in face:
            x,y,z=body[i]
            # Lower the neckline at the front, lift it behind the nape.
            neck=5.68-.39*np.clip((z+.10)/1.0,0,1)+.23*np.minimum(abs(x),1.)
            # Sleeves finish above the elbow. The hem hangs away from the waist.
            edge=min(y-.46,neck-y,2.62-abs(x))
            ease=np.clip((3.7-y)/2.9,0,1)
            garment=[x*(1.045+.07*ease),y,z+.065*np.sign(z-.12)+.07*ease*np.cos(4.*x)]
            poly.append(np.r_[garment,uv[t],edge])
        def clip(rows,level,above):
            out=[]
            for a,b in zip(rows,rows[1:]+rows[:1]):
                av,bv=a[-1]-level,b[-1]-level
                if av*(1 if above else -1)>=0: out.append(a)
                if av*bv<0: out.append(a+(b-a)*(av/(av-bv)))
            return out
        for j in range(1,len(poly)-1):
            tri=clip([poly[0],poly[j],poly[j+1]],0,True)
            for slot,rows in (("jersey_knit",clip(tri,.09,True)),("jersey_binding",clip(tri,.09,False))):
                if len(rows)<3: continue
                v,tex,f=sections[slot]; indices=[]
                for row in rows:
                    indices.append((len(v),len(tex)));v.append(row[:3]);tex.append(row[3:5])
                f.append(indices)
    for name,(v,tex,f) in sections.items():
        # Weld across the source triangles before generating smooth normals.
        points,inverse=np.unique(np.round(v,7),axis=0,return_inverse=True)
        yield name,points,np.asarray(tex),[[(inverse[i],t) for i,t in face] for face in f]
