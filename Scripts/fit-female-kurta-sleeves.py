"""Relax the fitted female kurta's upper sleeve over the existing arm joints."""
import json
from pathlib import Path
import runpy
import struct

import numpy as np

ROOT = Path(__file__).resolve().parent
fit = runpy.run_path(str(ROOT/'fit-resident-physique.py'))
smooth = fit['smooth']


def joint_positions(scene):
    positions = {}
    parents = {child:i for i,node in enumerate(scene['nodes']) for child in node.get('children',[])}
    for i in scene['skins'][0]['joints']:
        node = scene['nodes'][i]
        positions[i] = np.asarray(node['translation']) + positions.get(parents.get(i), 0.)
    return {scene['nodes'][i]['name']: p for i,p in positions.items()}


class Sleeve(fit['SurfaceFit']):
    def __init__(self, bones):
        self.shoulder = bones['Shoulderl']
        upper = bones['Elbowl'] - self.shoulder
        self.upper_length = np.linalg.norm(upper)
        self.upper = upper / self.upper_length
        self.outward = np.cross(self.upper,[0.,0.,-1.])
        self.outward /= np.linalg.norm(self.outward)
        self.forward = np.cross(self.upper,self.outward)
        self.elbow = bones['Elbowl']
        fore = bones['Handl'] - self.elbow
        self.fore = fore / np.linalg.norm(fore)

    def coordinates(self, points):
        xyz = points.copy()
        xyz[:,0] = np.abs(xyz[:,0])
        q = xyz - self.shoulder
        along = q @ self.upper
        radial = q - along[:,None]*self.upper
        return xyz, along, radial

    def yoke(self, xyz):
        """Protect the fabric folded back over the rear collar."""
        return 1 - smooth(1.371,1.402,xyz[:,1])*(1-smooth(-.018,.007,xyz[:,2]))

    def offsets(self, points):
        xyz, along, radial = self.coordinates(points)
        across = radial @ self.outward
        depth = radial @ self.forward
        upper = smooth(-.05,.038,along) * (1-smooth(.193,.24,along))
        # Taper the deltoid cap into the upper sleeve. The arm-space falloff
        # preserves the underarm seam and the centre of the yoke.
        cap = np.exp(-((along-.044)/.070)**2)
        mid = np.exp(-((along-.162)/.045)**2)
        width = (-.0160*cap + .0085*mid)*0.8 * smooth(-.018,.040,across)
        ease = (1-smooth(.085,.133,abs(depth))) * smooth(.126,.185,xyz[:,0])
        delta = (width*ease)[:,None]*self.outward
        delta += (-.16*0.8*upper*depth*ease)[:,None]*self.forward
        fore = (xyz-self.elbow) @ self.fore
        delta *= (1-smooth(.00,.030,fore))[:,None]
        # The short sleeve terminates over visible forearm skin. Allow cloth
        # ease on the run-in to the cuff, following the forearm rather than
        # changing its opening depth or its join to the hand.
        offset = xyz-self.elbow-fore[:,None]*self.fore
        radius = np.linalg.norm(offset,axis=1)
        ease = smooth(.016,.049,fore)*(1-smooth(.090,.120,fore))*(1-smooth(.070,.11,radius))
        fore_out = np.cross(self.fore,[0.,0.,-1.])
        fore_out /= np.linalg.norm(fore_out)
        wrist_clearance = np.exp(-((xyz[:,0]-.348)/.031)**4-((xyz[:,1]-1.127)/.040)**4)
        ease *= np.maximum(wrist_clearance,smooth(-.042,-.020,offset @ fore_out))
        delta += (0.006*ease/np.maximum(radius,.012))[:,None]*offset
        delta *= self.yoke(xyz)[:,None]
        delta[:,0] *= np.sign(points[:,0])
        return delta

    def skin_weights(self, points, joints, weights, names):
        xyz, along, radial = self.coordinates(points)
        lateral = xyz[:,0]
        fore = (xyz-self.elbow) @ self.fore
        elbow = .50*(along-self.upper_length)+.50*fore
        # Constant blending around each sleeve ring distributes rotation along
        # the arm, avoiding the shear across the inner and outer elbow that a
        # nearest-bone distance assigns to this thin, loose garment.
        arm = smooth(.118,.205,lateral) * (1-smooth(.096,.134,np.linalg.norm(radial,axis=1)))
        arm *= smooth(1.09,1.19,xyz[:,1])
        # Incoming cuff and wrist bind positions stay intact.
        influence = 0.7*arm*(1-smooth(.00,.030,fore))*(1-smooth(0.103,0.163,along))
        influence *= self.yoke(xyz)
        target = np.zeros((len(xyz),len(names)))
        left = points[:,0] >= 0
        upper = smooth(-.073,.059,along)
        lower = smooth(-.077,.070,elbow)
        for side,mask in [('l',left),('r',~left)]:
            target[mask,names.index('Chest')] = 1-upper[mask]
            target[mask,names.index('Shoulder'+side)] = upper[mask]*(1-lower[mask])
            target[mask,names.index('Elbow'+side)] = upper[mask]*lower[mask]
        source = np.zeros_like(target)
        np.add.at(source,(np.arange(len(xyz))[:,None],joints),weights)
        result = source*(1-influence[:,None])+target*influence[:,None]
        ids = np.argsort(-result,axis=1,kind='stable')[:,:4]
        vals = np.take_along_axis(result,ids,axis=1)
        vals /= vals.sum(axis=1,keepdims=True)
        active = influence>1e-6
        joints[active],weights[active] = ids[active],vals[active]
        return joints,weights


def relax_outline(sleeve, sections):
    # Weld UV and material duplicates for adjacency only. The exported index
    # buffers keep the garment yoke, collar, seam and cuff boundaries intact.
    points = np.concatenate([section[0] for section in sections])
    vertex, reverse = np.unique(np.round(points,7),axis=0,return_inverse=True)
    reverse = reverse.reshape(-1)
    triangles = []
    start = 0
    for positions, indices in sections:
        triangles.extend(reverse[start+indices])
        start += len(positions)
    faces = np.asarray(triangles)
    edges = np.vstack([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]])
    edges = np.unique(np.sort(edges,axis=1),axis=0)
    edge_a,edge_b = np.vstack([edges,edges[:,::-1]]).T
    count = np.bincount(edge_a,minlength=len(vertex))
    xyz,along,radial = sleeve.coordinates(vertex)
    fore = (xyz-sleeve.elbow) @ sleeve.fore
    weight = smooth(.128,.19,xyz[:,0])*(1-smooth(.090,.122,np.linalg.norm(radial,axis=1)))
    weight *= smooth(-.041,.012,along)*(1-smooth(.00,.030,fore))
    weight *= (1-smooth(0.103,0.163,along))
    weights = (weight*sleeve.yoke(xyz))[:,None]
    smoothed = vertex.copy()
    # Relax the coarse cloth edges over the biceps and upper elbow.
    for _ in range(6):
        mean = np.zeros_like(smoothed)
        np.add.at(mean,edge_a,smoothed[edge_b])
        mean /= np.maximum(1,count[:,None])
        smoothed += 0.35 * weights * (mean-smoothed)
    def normals(values):
        triangles = values[faces]
        face_normal = np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
        result = np.zeros_like(values)
        for corner in faces.T:
            np.add.at(result,corner,face_normal)
        return result / np.maximum(np.linalg.norm(result,axis=1,keepdims=True),1.e-12)
    return ((smoothed-vertex)[reverse],(normals(smoothed)-normals(vertex))[reverse])


def refine(path):
    io = runpy.run_path(str(ROOT/'prepare-riverfront-hatch.py'))
    scene,raw = io['read_glb'](path)
    if not any(m['name']=='scarf' for m in scene['materials']):
        return False
    sleeve = Sleeve(joint_positions(scene))
    names = [scene['nodes'][i]['name'] for i in scene['skins'][0]['joints']]
    output = bytearray(raw)
    garments = [p for p in scene['meshes'][0]['primitives']
                if scene['materials'][p['material']]['name'] in ('top_teal_woven','top_seams','scarf')]
    sections = []
    for p in garments:
        points = np.asarray(list(io['vectors'](scene,raw,p['attributes']['POSITION'])))
        indices = np.asarray(list(io['vectors'](scene,raw,p['indices']))).reshape(-1,3)
        sections.append((points+sleeve.offsets(points),indices))
    relaxed, corrected = relax_outline(sleeve,sections)
    start = 0
    for p in garments:
        attrs=p['attributes']
        def read(key):
            return np.asarray(list(io["vectors"](scene,raw,attrs[key])))
        pos, normal = sleeve.transform(read('POSITION'),read('NORMAL'))
        region = slice(start,start+len(pos))
        start += len(pos)
        pos += relaxed[region]
        normal += corrected[region]
        normal /= np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1e-12)
        ids,weights = sleeve.skin_weights(read('POSITION'),read('JOINTS_0'),read('WEIGHTS_0'),names)
        for key,values in [('POSITION',pos),('NORMAL',normal),('JOINTS_0',ids),('WEIGHTS_0',weights)]:
            a=scene['accessors'][attrs[key]]
            view=scene['bufferViews'][a['bufferView']]
            data=values.astype('<u2' if key=='JOINTS_0' else '<f4').tobytes()
            offset=view.get('byteOffset',0)+a.get('byteOffset',0)
            if len(data) != view["byteLength"]:
                raise ValueError("Expected a packed vertex stream for "+key)
            output[offset:offset+len(data)]=data
            if key == "POSITION":
                a.update(min=pos.min(0).tolist(),max=pos.max(0).tolist())
    header=json.dumps(scene,separators=(',',':')).encode()
    header+=b' '*(-len(header)%4)
    path.write_bytes(struct.pack('<4sIII4s',b'glTF',2,28+len(header)+len(output),len(header),b'JSON')
                    +header+struct.pack('<I4s',len(output),b'BIN\0')+output)
    return True
