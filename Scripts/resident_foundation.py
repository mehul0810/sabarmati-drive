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
