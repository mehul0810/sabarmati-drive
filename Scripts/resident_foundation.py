"""Cut a fitted athletic bralette and its elastic edge from the resident's torso surface."""
import numpy as np


def make_bralette(body, faces):
    # Distances use the source hm08 units. Clip across quads for a continuous
    # neckline and armhole even where the underlying mesh has broad polygons.
    x, y, z = body.T
    side = np.abs(x)
    strap = np.clip(1-np.abs(side-.94)/.36, 0, 1)
    front = np.clip((z+.05)/.80, 0, 1)
    neckline = 4.70 + .22*front + .17*np.minimum(side,1.) + 1.32*strap
    hem = 3.02 + .12*np.minimum(side,1.6)
    upper = np.minimum(neckline-y, (1.69-side)*2.8 + np.maximum(0.,4.64-y))
    bands = [(y-hem, upper), (y-hem, hem+.15-y)]
    names = ("foundation_knit","foundation_elastic")
    panels = {}
    covered = set()

    def clip(poly, channel):
        result = []
        for a,b in zip(poly,poly[1:]+poly[:1]):
            if a[channel]>=0:
                result.append(a)
            if (a[channel]>0 and b[channel]<0) or (a[channel]<0 and b[channel]>0):
                result.append(a+(b-a)*(a[channel]/(a[channel]-b[channel])))
        return result

    for part, ((low, high), name) in enumerate(zip(bands,names)):
        verts, uvs, polygons, lookup = [], [], [], {}
        for index,face in enumerate(faces):
            ids = [i for i,_ in face]
            if max(side[ids])>2.24 or max(low[ids])<0 or max(high[ids])<0:
                continue
            if not part and min(low[ids])>.10 and min(high[ids])>.10:
                covered.add(index)
            poly = [np.r_[body[i],low[i],high[i]] for i in ids]
            poly = clip(clip(poly,3),4)
            if len(poly)<3:
                continue
            corners = []
            for row in poly:
                point = row[:3]
                key = tuple(np.round(point,6))
                if key not in lookup:
                    lookup[key] = len(verts)
                    outward = point*np.array([1.,0.,1.])-[0.,0.,.12]
                    outward /= max(np.linalg.norm(outward),1.e-8)
                    verts.append(point+outward*(.035 if part else .018))
                    uvs.append([point[0]*.5,point[1]*.5])
                v = lookup[key]
                corners.append((v,v))
            if len(set(corners))>=3:
                polygons.append(corners)
        panels[name] = np.asarray(verts),np.asarray(uvs),polygons
    return covered,panels
