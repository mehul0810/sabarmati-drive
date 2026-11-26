"""Fit the CC0 MakeHuman male_casualsuit03 and shoes01 to the original riverfront rig.

Inputs: original hm08 base.obj, SK_RiverResident.glb, and the official system pack.
Install numpy and Pillow for this offline authoring step. The game uses cooked assets.
"""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import runpy
import struct

import numpy as np
from PIL import Image, ImageDraw

SCRIPTS = Path(__file__).parent
SUIT = "male_casualsuit03"
SHOES = "shoes01"


def read_obj(path):
    vertices, uv, faces = [], [], []
    group = None
    for line in path.read_text().splitlines():
        row = line.split()
        if row[:1] == ["v"]:
            vertices.append([float(n) for n in row[1:4]])
        elif row[:1] == ["vt"]:
            uv.append([float(n) for n in row[1:3]])
        elif row[:1] == ["g"]:
            group = row[1]
        elif row[:1] == ["f"]:
            faces.append((group, [tuple(int(n)-1 for n in token.split("/")[:2]) for token in row[1:]]))
    return np.asarray(vertices), np.asarray(uv), faces


def fit(path, body):
    """Apply MakeClothes' base-vertex bindings and axis-scaled fitting offsets."""
    definition = path.read_text()
    if "basemesh hm08" not in definition or "released as CC0" not in definition:
        raise ValueError(f"Unexpected asset source: {path}")
    config, bindings = definition.split("verts 0\n", 1)
    bindings, mask = bindings.split("delete_verts")
    scale = np.ones(3)
    for axis, a, b, factor in re.findall(r"([xyz])_scale (\d+) (\d+) (\S+)", config):
        k = "xyz".index(axis)
        scale[k] = abs(body[int(a),k]-body[int(b),k])/float(factor)
    points = []
    for row in bindings.splitlines():
        numbers = row.split()
        if len(numbers) == 9:
            ids = np.array(numbers[:3], int)
            barycentric = np.array(numbers[3:6], float)
            points.append(barycentric @ body[ids] + np.array(numbers[6:9], float)*scale)
        elif len(numbers) == 1:
            points.append(body[int(numbers[0])])
        elif numbers:
            raise ValueError(row)
    hidden = set()
    for start,end in re.findall(r"(\d+)(?:\s*-\s*(\d+))?",mask):
        hidden.update(range(int(start),int(end or start)+1))
    return np.array(points), hidden


def components(count, faces):
    parent = list(range(count))
    def root(n):
        while parent[n] != n:
            parent[n] = parent[parent[n]]
            n = parent[n]
        return n
    for _, face in faces:
        for v,_ in face[1:]:
            parent[root(v)] = root(face[0][0])
    return np.array([root(n) for n in range(count)])


def write_textures(asset, name, uv, panels, output):
    rgb = Image.open(asset/f"{name}_diffuse.png").convert("RGB")
    normal = Image.open(asset/f"{name}_normal.png").convert("RGB")
    if name == SUIT:
        # Retain authored checks, buttons and denim whiskering while allowing both saved wardrobe dyes.
        gray = np.array(rgb.convert("L"), float)
        result = gray.copy()
        for faces in panels.values():
            mask = Image.new("L",rgb.size)
            draw = ImageDraw.Draw(mask)
            for f in faces:
                draw.polygon([(u*mask.width,(1-v)*mask.height) for _,t in f for u,v in [uv[t]]],fill=255)
            visible = np.asarray(mask) != 0
            median = np.median(gray[visible])
            # Extend the panel transformation over atlas gutters for distant mips.
            from PIL import ImageFilter
            visible = np.asarray(mask.filter(ImageFilter.MaxFilter(9))) != 0
            result[visible] = np.clip(209*(np.maximum(gray[visible],1)/median)**.66, 16, 255)
        Image.fromarray(result.astype("uint8")).convert("RGB").save(output/"T_EverydayFabric.png")
        ao = Image.open(asset/f"{name}_ao.png").convert("L")
        ao.resize((1024,1024),Image.Resampling.LANCZOS).save(output/"T_EverydayOcclusion.png")
        normal_size, label = 2048, "Fabric"
    else:
        rgb.thumbnail((1024,1024),Image.Resampling.LANCZOS)
        rgb.save(output/"T_EverydayLeather.png")
        normal_size, label = 1024, "Leather"
    normal.thumbnail((normal_size,normal_size),Image.Resampling.LANCZOS)
    # OpenGL source tangents -> Unreal / DirectX texture convention.
    r,g,b = normal.split()
    Image.merge("RGB",(r,g.point(lambda x:255-x),b)).save(output/f"T_Everyday{label}_N.png")


def smooth(edge0, edge1, value):
    t = np.clip((value-edge0)/(edge1-edge0),0,1)
    return t*t*(3-2*t)


def build(base, reference, system, output, foundation=None, lower_only=False):
    io = runpy.run_path(str(SCRIPTS/"prepare-riverfront-hatch.py"))
    scene, raw = io["read_glb"](reference)
    binary = bytearray(raw)
    body, body_uv, body_faces = read_obj(base)
    mats = {m["name"]:i for i,m in enumerate(scene["materials"])}
    mesh = scene["meshes"][0]
    mesh["name"] = "SK_RiverEveryday"
    scene["nodes"][-1]["name"] = mesh["name"]
    mesh["primitives"] = [p for p in mesh["primitives"] if scene["materials"][p["material"]]["name"]
                          in ("hair","lips","iris","sclera")]
    scene.pop("animations",None)  # The installed A_Idle/A_Walk/A_Jog share the existing skeleton.
    matrix_data = scene["accessors"][scene["skins"][0]["inverseBindMatrices"]]
    start = scene["bufferViews"][matrix_data["bufferView"]]["byteOffset"]
    matrices = np.frombuffer(raw, dtype="<f4", count=16*matrix_data["count"], offset=start).reshape(-1,16)
    bones = -np.array(matrices)[:,12:15]
    names = [scene["nodes"][i]["name"] for i in scene["skins"][0]["joints"]]
    origin = np.array((0,8.17,0))
    rank = {n:i for i,n in enumerate(names)}
    ends = dict(Pelvis="Spine",Spine="Chest",Chest="Neck",Neck="Head")
    for side in "lr":
        for a,b in [("Shoulder","Elbow"),("Elbow","Hand"),("Hip","Knee"),("Knee","Foot")]:
            ends[a+side]=b+side

    def skinning(positions, slot):
        influences = np.zeros((len(positions),len(bones)))
        y = positions[:,1]
        for side in "lr":
            selected = (positions[:,0]>=0) if side=="l" else (positions[:,0]<0)
            hip,knee,foot = [rank[n+side] for n in ("Hip","Knee","Foot")]
            if slot == "bottom_denim" or slot == "shoe":
                # At the knee, a 16 cm flex band distributes bending across several
                # edge rings. Upper thigh / calf vertices follow the correct segment.
                pelvis = smooth(bones[hip,1]-.085,bones[hip,1]+.10,y)
                thigh = smooth(bones[knee,1]-.08,bones[knee,1]+.08,y)
                shin = smooth(bones[foot,1]-.028,bones[foot,1]+.050,y)
                influences[selected,rank["Pelvis"]] = pelvis[selected]
                influences[selected,hip] = ((1-pelvis)*thigh)[selected]
                influences[selected,knee] = ((1-pelvis)*(1-thigh)*shin)[selected]
                influences[selected,foot] = ((1-pelvis)*(1-thigh)*(1-shin))[selected]
                if slot=="shoe":
                    sole = y < bones[foot,1]+.012
                    influences[sole & selected] = 0
                    influences[sole & selected,foot] = 1
            else:
                # Match the original arm, neck and wrist weights exactly.
                pool = ["Pelvis","Spine","Chest","Neck","Head"]+[n for n in names if n.endswith(side)]
                for n in pool:
                    i = rank[n]
                    end = bones[rank[ends[n]]] if n in ends else (
                        np.array((0,1.72,.055)) if n=="Head" else bones[i]+(0,-.09,.055))
                    segment = end-bones[i]
                    d = positions-bones[i]
                    t = np.clip(d@segment/max(segment@segment,.000001),0,1)
                    distance = np.linalg.norm(d-t[:,None]*segment,axis=1)
                    weights = 1/np.maximum(.015,distance)**5
                    if n not in ("Neck","Head"):
                        weights[(y>1.48)&(np.abs(positions[:,0])<.18)] = 0
                    influences[selected,i] = weights[selected]
        if slot == "top_teal_woven":
            # The rig has no clavicles. The shirt yoke follows the chest until it
            # crosses the shoulder; sleeves progressively pick up the arm joints.
            sleeve = smooth(.124,.240,np.abs(positions[:,0]))
            chest = smooth(1.12,1.29,y)
            for n in ("Shoulderl","Elbowl","Handl","Shoulderr","Elbowr","Handr"):
                i = rank[n]
                retained = influences[:,i] * sleeve
                transferred = influences[:,i] - retained
                influences[:,rank["Chest"]] += chest*transferred
                influences[:,rank["Spine"]] += (1-chest)*transferred
                influences[:,i] = retained
        if foundation and slot in ("skin","foundation_knit","foundation_elastic"):
            # A bare pectoral belongs to the rib cage. The nearest-arm weighting
            # of an A-pose pulls the lateral chest into the armpit at rest.
            core = (1-smooth(.145,.235,np.abs(positions[:,0])))*smooth(.87,.99,y)*(1-smooth(1.355,1.43,y))
            influences /= np.maximum(influences.sum(axis=1,keepdims=True),1.e-12)
            influences *= 1-core[:,None]
            chest = smooth(1.04,1.24,y)
            waist = 1-smooth(.93,1.08,y)
            influences[:,rank["Chest"]] += core*chest
            influences[:,rank["Pelvis"]] += core*(1-chest)*waist
            influences[:,rank["Spine"]] += core*(1-chest)*(1-waist)
        ids = np.argsort(-influences,axis=1,kind="stable")[:,:4]
        vals = np.take_along_axis(influences,ids,axis=1)
        if slot not in ("shoe","bottom_denim"):
            vals[:,3] = 0  # Reference rig exports three influences for the body.
        return ids,vals/vals.sum(axis=1)[:,None]

    def accessor(rows,kind,fmt,bounds=False):
        dtype,component = {"f":("<f4",5126),"H":("<u2",5123),"I":("<u4",5125)}[fmt]
        values = np.asarray(rows,dtype=dtype)
        binary.extend(bytes(-len(binary)%4))
        offset = len(binary)
        binary.extend(values.tobytes())
        view = len(scene["bufferViews"])
        scene["bufferViews"].append(dict(buffer=0,byteOffset=offset,byteLength=len(binary)-offset))
        field = dict(bufferView=view,componentType=component,count=len(values),type=kind)
        if bounds:
            field.update(min=values.min(axis=0).tolist(),max=values.max(axis=0).tolist())
        scene["accessors"].append(field)
        return len(scene["accessors"])-1

    def triangles(polygons):
        return [(f[0],f[i],f[i+1]) for f in polygons for i in range(1,len(f)-1)]

    def surface_normals(verts, faces):
        normals = np.zeros_like(verts)
        indices = np.array([[v for v,_ in tri] for tri in triangles(faces)])
        a,b,c = verts[indices].transpose((1,0,2))
        products = np.cross(b-a,c-a)
        for i in range(3):
            np.add.at(normals,indices[:,i],products)
        return normals/np.maximum(np.linalg.norm(normals,axis=1,keepdims=True),1e-12)

    def add_panel(slot, source_vertices, uv, faces, normal_faces=None, normals=None):
        verts = (source_vertices+origin)*.102
        if normals is None:
            normals = surface_normals(verts,normal_faces or faces)
        tris = triangles(faces)
        unique = dict.fromkeys(k for tri in tris for k in tri)
        keys = {k:i for i,k in enumerate(unique)}
        vis,uis = np.array(list(keys)).T
        tex = uv[uis].copy()
        tex[:,1] = 1-tex[:,1]
        ids,weights = skinning(verts[vis],"shoe" if slot.startswith("street_shoe") else "bottom_denim" if lower_only else slot)
        if slot in ("bare_lower_skin","midriff_skin"):
            # Use torso skin weights at its shared ring, transitioning into the denim leg rig.
            coords = verts[vis]
            torso_ids,torso_weights = skinning(coords,"skin")
            blend = smooth(-1.5,-.18,source_vertices[vis,1])[:,None]
            all_weights = np.zeros((len(vis),len(bones)))
            rows = np.arange(len(vis))[:,None]
            np.add.at(all_weights,(rows,ids),weights*(1-blend))
            np.add.at(all_weights,(rows,torso_ids),torso_weights*blend)
            ids = np.argsort(-all_weights,axis=1,kind="stable")[:,:4]
            weights = np.take_along_axis(all_weights,ids,axis=1)
            weights /= weights.sum(axis=1)[:,None]
        assert np.allclose(weights.sum(axis=1),1) and np.isfinite(weights).all()
        attrs = dict(POSITION=accessor(verts[vis],"VEC3","f",True),NORMAL=accessor(normals[vis],"VEC3","f"),
                     TEXCOORD_0=accessor(tex,"VEC2","f"),JOINTS_0=accessor(ids,"VEC4","H"),
                     WEIGHTS_0=accessor(weights,"VEC4","f"))
        ix = accessor([keys[k] for tri in tris for k in tri],"SCALAR","I")
        mesh["primitives"].append(dict(attributes=attrs,indices=ix,material=mats[slot]))
        print(slot,len(tris),"triangles")

    def write_mesh(filename):
        binary.extend(bytes(-len(binary)%4))
        scene["buffers"] = [dict(byteLength=len(binary))]
        header = json.dumps(scene,separators=(",",":")).encode()
        header += b" "*(-len(header)%4)
        mesh["name"] = scene["nodes"][-1]["name"] = filename
        output.joinpath(filename+".glb").write_bytes(
            struct.pack("<4sIII4s",b"glTF",2,28+len(header)+len(binary),len(header),b"JSON")
            +header+struct.pack("<I4s",len(binary),b"BIN\0")+binary)

    if lower_only:
        from resident_foundation import lower_foundation
        mesh["primitives"] = []
        for name, vertices, texcoords, polygons in lower_foundation(body, body_uv, body_faces):
            mats[name] = len(scene["materials"])
            scene["materials"].append({"name":name,"doubleSided":True})
            body_normals = [f for g,f in body_faces if g=="body"] if name in ("bare_lower_skin","midriff_skin") or name.startswith("foot_skin") else None
            add_panel(name,vertices,texcoords,polygons,normal_faces=body_normals)
        shoe = system/"clothes"/SHOES
        verts,tex,shoe_faces = read_obj(shoe/(SHOES+".obj"))
        fitted,_ = fit(shoe/(SHOES+".mhclo"),body)
        components_by_vertex = components(len(verts),shoe_faces)
        # shoes01 also contains two disconnected ankle socks. Keep only the leather uppers and soles.
        leather = {part for part in set(components_by_vertex) if fitted[components_by_vertex==part,1].min() < -8.}
        for side,sign in (("l",1),("r",-1)):
            name="street_shoe_"+side
            mats[name]=len(scene["materials"])
            scene["materials"].append({"name":name})
            faces=[f for _,f in shoe_faces if components_by_vertex[f[0][0]] in leather and fitted[f[0][0],0]*sign>0]
            add_panel(name,fitted,tex,faces)
        write_mesh("SK_"+foundation.title()+"LowerBody")
        return

    hidden = set()
    for name in (SUIT,SHOES):
        asset = system/"clothes"/name
        verts,uv,faces = read_obj(asset/f"{name}.obj")
        fitted, masked = fit(asset/f"{name}.mhclo",body)
        if len(verts)!=len(fitted):
            raise ValueError("Source OBJ and fitting definition differ")
        hidden.update(masked)
        comps = components(len(verts),faces)
        if name==SUIT:
            # Buttons/collar and the shirt form one connected garment. The jeans have their own waistband.
            parts = {i:("bottom_denim" if fitted[comps==i,1].min()<0 else "top_teal_woven") for i in set(comps)}
            shirt = np.array([parts[i]=="top_teal_woven" for i in comps])
            # The neutral hm08 body has a narrow waist. Ease the untucked shirt
            # from rib cage to hem, preserving its native collar, placket and cuffs.
            x, y = np.abs(fitted[:,0]), fitted[:,1].copy()
            torso = shirt * (1-smooth(1.75,2.15,x))
            drape = smooth(1.1,2.9,y)*(1-smooth(3.65,4.75,y))
            hem = 1-smooth(.65,1.8,y)
            fitted[:,0] += np.sign(fitted[:,0])*torso*(.32*drape-.046*hem)*smooth(.10,1.5,x)
            # Let the outer shoulder slope away from the collar when the arm rests.
            cap = shirt*smooth(1.15,1.90,x)*(1-smooth(2.20,2.72,x))*smooth(4.8,5.65,y)
            fitted[:,1] -= .15*cap
            # Tuck the waistband beneath the untucked shirt as the hip flexes.
            # Leave the visible pockets, thigh and knee silhouettes at their authored width.
            jeans = np.array([parts[i]=="bottom_denim" for i in comps])
            waist = smooth(.40,1.30,fitted[:,1])*jeans*(0. if foundation else 1.)
            fitted[:,0] *= 1-.07*waist
            fitted[:,2] = .13+(fitted[:,2]-.13)*(1-.16*waist)
        else:
            parts = dict.fromkeys(comps,"shoe")
        panels = defaultdict(list)
        for _,face in faces:
            panels[parts[comps[face[0][0]]]].append(face)
        for slot, polygons in panels.items():
            if foundation and slot == "top_teal_woven":
                continue
            add_panel(slot,fitted,uv,polygons)
        if not foundation:
            write_textures(asset,name,uv,panels,output)

    body_polygons = [f for group,f in body_faces if group=="body"]
    if foundation:
        # Overlap the inside of the original waistband. The cut stays inside the jeans
        # when the hips flex and does not change the visible abdomen above the belt.
        from resident_foundation import shape_chest
        shape_chest(body,foundation)
        waist = 1-smooth(.45,1.72,body[:,1])
        body[:,0] *= 1-.105*waist
        body[:,2] = .13+(body[:,2]-.13)*(1-.19*waist)
    exposed = []
    for face in body_polygons:
        indices = [v for v,_ in face]
        x,y,z = body[indices].mean(axis=0)
        # This long-sleeved shirt covers the torso and upper arms. The generic
        # MakeClothes hide mask leaves some shoulder faces for shorter garments;
        # cull them even when relaxed sleeves move independently from the body.
        # Keep a margin of skin inside the open collar and both sleeve cuffs.
        # Coarse shoulder quads can span from the neck into the visible yoke.
        # Keep the full neck above the collar and only its inner faces beneath it.
        collar = y>5.84 or (y>4.20 and max(abs(body[i,0]) for i in indices)<.65)
        cuff = abs(x)>3.05 and y<3.90
        if foundation and y < .42:
            continue
        if not foundation and ((y<6.30 and not collar and not cuff) or all(i in hidden for i in indices)):
            continue
        if abs(x)<.39 and 6.50<y<6.65 and z>1.10:
            continue  # The existing lips use their own material.
        if y>7.92+.55*max(-1,min(1,z-.5)):
            continue  # Scalp is already part of the original hair section.
        exposed.append(face)
    if foundation == "female":
        from resident_foundation import cut_bralette
        normals = surface_normals(body,body_polygons)
        body, body_uv, normals, panels = cut_bralette(body,body_uv,normals,exposed)
        for name, polygons in panels.items():
            if name!="skin":
                mats[name] = len(scene["materials"])
                scene["materials"].append({"name":name,"doubleSided":True})
            add_panel(name,body,body_uv,polygons,normals=normals)
    else:
        add_panel("skin",body,body_uv,exposed,body_polygons)
    write_mesh("SK_RiverEveryday" if not foundation else "SK_"+foundation.title()+"Foundation")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("base","reference","system","output"):
        p.add_argument(name,type=Path)
    p.add_argument("--foundation", choices=("male","female"), help="Build the uncovered upper body on the everyday rig")
    args = p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    build(args.base,args.reference,args.system,args.output,args.foundation)
