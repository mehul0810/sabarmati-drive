"""Bake the original watch and surface maps for the public first-morning wardrobe.

Input is SK_RiverResident.glb from make-district-residents.py. Mesh units are
metres, with Y up in glTF. All watch geometry and surface artwork are original.
"""
import argparse
import collections
import math
from pathlib import Path
import runpy
import struct
import zlib

SCRIPTS = Path(__file__).resolve().parent


def png(path, size, pixels):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    rows = b"".join(b"\0" + pixels[y*size*3:(y+1)*size*3] for y in range(size))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def textile(out):
    size = 256
    colour = bytearray()
    normal = bytearray()
    def weave(x, y):
        dx, dy = x * math.tau/8, y * math.tau/8
        over = 1 if (x//8 + y//8) % 2 else -1
        return .33*(math.cos(dx) + math.cos(dy)) + over*.26*(math.cos(dx) - math.cos(dy))
    for y in range(size):
        for x in range(size):
            fiber = weave(x, y)
            dye = 3.5*math.sin(math.tau*x/128 + .6*math.sin(math.tau*y/64))
            value = round(244 + 4*fiber + .33*dye + math.sin(math.tau*(x+y)/16))
            colour.extend((value, value, value))
            nx = -.035*(weave(x+1,y)-weave(x-1,y))
            ny = -.035*(weave(x,y+1)-weave(x,y-1))
            length = math.sqrt(1 + nx*nx + ny*ny)
            normal.extend((round(127.5*(1+nx/length)), round(127.5*(1+ny/length)), round(127.5*(1+1/length))))
    png(out/"T_CottonWeave.png", size, colour)
    png(out/"T_CottonWeave_N.png", size, normal)


def skin_texture(source, out):
    io = runpy.run_path(str(SCRIPTS/"prepare-riverfront-hatch.py"))
    doc, raw = io["read_glb"](source)
    primitive = next(p for p in doc["meshes"][0]["primitives"] if doc["materials"][p["material"]]["name"] == "skin")
    def vectors(i):
        return list(io["vectors"](doc, raw, i))
    p = vectors(primitive["attributes"]["POSITION"])
    size = 1024
    uv = [(u*(size-1),v*(size-1)) for u,v in vectors(primitive["attributes"]["TEXCOORD_0"])]
    indices = [v[0] for v in vectors(primitive["indices"])]
    pixels = bytearray([239,232,225]) * (size*size)
    def oval(x, y):
        return math.exp(-min(35, x*x+y*y))
    for walk in range(0,len(indices),3):
        a,b,c = indices[walk:walk+3]
        if max(p[i][1] for i in (a,b,c)) < 1.39:
            continue
        (s,t),(h,k),(u,v) = uv[a],uv[b],uv[c]
        area = (h-s)*(v-t) - (u-s)*(k-t)
        if abs(area)<1.e-8:
            continue
        for py in range(max(0,math.floor(min(t,k,v))-1),min(size,math.ceil(max(t,k,v))+2)):
            for px in range(max(0,math.floor(min(s,h,u))-1),min(size,math.ceil(max(s,h,u))+2)):
                wb = ((px+.5-s)*(v-t) - (py+.5-t)*(u-s))/area
                wc = ((h-s)*(py+.5-t) - (k-t)*(px+.5-s))/area
                wa = 1-wb-wc
                if min(wa,wb,wc)<-.025:
                    continue
                x,y,z = (wa*p[a][i]+wb*p[b][i]+wc*p[c][i] for i in range(3))
                if z<.086:
                    continue
                ax = abs(x)
                eyebrow = oval((ax-.034)/.022,(y-(1.603-.32*(ax-.034)))/.0035)
                eyelid = oval((ax-.032)/.023, (y-1.585)/.005)
                cheek = oval((ax-.052)/.023,(y-1.543)/.019)
                nostril = oval((ax-.009)/.004,(y-1.529)/.0025)
                beard = oval(x/.047,(y-1.477)/.017) + .45*oval(x/.032,(y-1.515)/.0038)
                grain = ((px*73856093 ^ py*19349663) % 101)/100
                shading = 1 - eyebrow*(.60+.17*grain) - eyelid*.18 - nostril*.62 - beard*(.105+.115*grain)
                rgb = (239 + 11*cheek,232 - 10*cheek,225 - 11*cheek)
                start = 3*(py*size+px)
                pixels[start:start+3] = bytes(max(0,min(255,round(channel*shading + 2*(grain-.5)))) for channel in rgb)
    png(out/"T_ResidentFace.png",size,pixels)


def watch(out):
    # Author in millimetres. The wrist axis is Y; the dial looks towards +X.
    parts = collections.defaultdict(list)
    materials = []
    for name, colour, roughness, metal in (("strap",(.070,.034,.017),.74,0),
            ("brushed_case",(.55,.39,.19),.31,.85),("ink_dial",(.022,.044,.043),.34,.18),
            ("indices",(.81,.72,.50),.32,.62)):
        materials.append(dict(name=name,doubleSided=True,pbrMetallicRoughness=dict(
            baseColorFactor=[*colour,1],metallicFactor=metal,roughnessFactor=roughness)))
    def polygon(mat, points):
        a,b,c = points[:3]
        ab,ac = ([p[i]-a[i] for i in range(3)] for p in (b,c))
        n = (ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
        length = math.sqrt(sum(i*i for i in n))
        if length:
            parts[mat].append([(*(v*.001 for v in p),*(v/length for v in n),p[1]*.01,p[2]*.01) for p in points])
    def ring(mat, profiles, along_x, count=40):
        rings = []
        for axis, r0, r1 in profiles:
            ring_points = []
            for n in range(count):
                angle = math.tau*n/count
                c,s = math.cos(angle)*r0, math.sin(angle)*r1
                ring_points.append((axis,c,s) if along_x else (c,axis,s))
            rings.append(ring_points)
        for inner,outer in zip(rings,rings[1:]):
            for j in range(count):
                k=(j+1)%count
                polygon(mat,[inner[j],inner[k],outer[k],outer[j]])
    ring(0,[(-9,29,33),(-10,28,32),(-10,26,30),(10,26,30),(10,28,32),(9,29,33),(-9,29,33)],False)
    ring(1,[(25,17,17),(28,19,19),(32,19,19),(33,17,17),(32.4,16.2,16.2)],True)
    ring(2,[(32.4,.01,.01),(32.4,16.3,16.3)],True)
    def hand(mat,angle,inner,outer,width,height):
        along=(math.cos(angle),math.sin(angle))
        side=(-along[1]*width/2, along[0]*width/2)
        polygon(mat,[(height,along[0]*r+side[0]*offset,along[1]*r+side[1]*offset)
                     for r,offset in ((inner,-1),(outer,-1),(outer,1),(inner,1))])
    for hour in range(12):
        hand(3,math.tau*hour/12,11 if hour%3==0 else 12.5,14.7,1.55 if hour%3==0 else .9,32.65)
    hand(3,math.tau*10/12,0,8,1.9,32.85)
    hand(3,math.tau*2/12,-2,12.4,.95,33.)
    write=runpy.run_path(str(SCRIPTS/"prepare-riverfront-doors.py"))["store_mesh"]
    write("SM_RiverWristwatch",parts,materials,(0,0,0),out)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resident", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    textile(args.output)
    skin_texture(args.resident,args.output)
    watch(args.output)
