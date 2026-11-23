#!/usr/bin/env python3
"""Prepare optional city-core exterior massing from local open footprints."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import runpy
import struct

CODE = runpy.run_path(str(Path(__file__).with_name("prepare-ahmedabad-city.py")))
PROJECT = CODE["project"]
CLIP = CODE["clip"]
TILE = CODE["TILE"]
HEADER = struct.Struct("<8sI8ifIII")
RECORD = struct.Struct("<iiHHHhB")
TILE_HEAD = struct.Struct("<iiII")
ROAD = struct.Struct("<4iHBB")
LAND = struct.Struct("<4iB")
OMIT_TYPES = {"military", "government", "police", "prison", "bunker", "roof", "carport"}


def terrain_index(path):
    data = memoryview(path.read_bytes())
    start = HEADER.size
    count = HEADER.unpack_from(data)[-1]
    tiles = {}
    for _ in range(count):
        tx, ty, roads, ground = TILE_HEAD.unpack_from(data, start)
        start += TILE_HEAD.size
        street_lines = list(ROAD.iter_unpack(data[start:start+ROAD.size*roads]))
        start += ROAD.size*roads
        land = list(LAND.iter_unpack(data[start:start+LAND.size*ground]))
        start += LAND.size*ground
        tiles[tx,ty] = street_lines, land
    return tiles


def mass(points):
    polygon = [PROJECT(p) for p in points]
    if len(polygon) < 4:
        return None
    extent = math.hypot(max(p[0] for p in polygon) - min(p[0] for p in polygon),
                        max(p[1] for p in polygon) - min(p[1] for p in polygon))
    if not 270 < extent < 14500:
        return None
    edges = list(zip(polygon, polygon[1:] + polygon[:1]))
    anchors = sorted(edges, key=lambda e: math.dist(*e), reverse=True)[:6]
    best = None
    for (a, b) in anchors:
        angle = math.atan2(b[1]-a[1], b[0]-a[0]) % math.pi
        c, s = math.cos(angle), math.sin(angle)
        us = [c*x + s*y for x,y in polygon]
        vs = [-s*x + c*y for x,y in polygon]
        u0,v0,u1,v1 = min(us),min(vs),max(us),max(vs)
        w,d = u1-u0,v1-v0
        uc,vc = (u0+u1)*.5,(v0+v1)*.5
        candidate = (w*d,c*uc-s*vc,s*uc+c*vc,w,d,angle,c,s)
        if best is None or candidate[0]<best[0]:
            best = candidate
    area,x,y,w,d,angle,c,s = best
    actual = abs(sum(a[0]*b[1]-a[1]*b[0] for a,b in edges))*.5
    if min(w,d)<190 or actual < 85000 or actual < area * .48:
        return None
    return x,y,w*.94,d*.94,angle,c,s,actual


def road_clear(item, network):
    x,y,w,d,angle,c,s,_=item
    tx,ty = int(x//TILE),int(y//TILE)
    item_r = math.hypot(w,d)*.5+740
    center = network.get((tx,ty))
    if not center or not any(a<=x<=b and c0<=y<=d0 and not wet for a,c0,b,d0,wet in center[1]):
        return False
    for i in range(tx-1,tx+2):
        for j in range(ty-1,ty+2):
            for ax,ay,bx,by,width,_,_ in network.get((i,j),((),()))[0]:
                if (min(ax,bx)>x+item_r or max(ax,bx)<x-item_r or
                    min(ay,by)>y+item_r or max(ay,by)<y-item_r):
                    continue
                half=width*.5+55
                a=(c*(ax-x)+s*(ay-y),-s*(ax-x)+c*(ay-y))
                b=(c*(bx-x)+s*(by-y),-s*(bx-x)+c*(by-y))
                if CLIP(a,b,(-w*.5-half,-d*.5-half,w*.5+half,d*.5+half)):
                    return False
    return True


def fit_buildings(features, network):
    result=[]
    for f in features:
        g,p=f.get("geometry"),f["properties"]
        if not g or (p.get("ftype") or "").casefold() in OMIT_TYPES or g["type"] not in ("Polygon","MultiPolygon"):
            continue
        parts=[g["coordinates"]] if g["type"]=="Polygon" else g["coordinates"]
        for outer,*holes in parts:
            item=mass(outer)
            if item is None or not road_clear(item,network):
                continue
            x,y,w,d,angle,c,s,area=item
            stable=int(hashlib.sha256(str(p.get("id") or f["id"]).encode()).hexdigest()[:8],16)
            # The feed has no survey heights. Plausible low-/mid-rise bands only.
            typical=1 if area<420_000 else 2 if area<1_300_000 else 3
            floors=typical+(stable % 3 if typical>1 else stable % 2)
            height=320*floors+45
            yaw=round(math.degrees(angle)*100)
            result.append((round(x),round(y),round(w),round(d),height,yaw,stable%3))
    return result


def build_districts(directory, road_data, core_path):
    network = terrain_index(road_data)
    core = core_path.read_bytes()
    if len(core)<12 or core[:8]!=b"SABBLD1\0" or len(core)!=12+17*struct.unpack_from("<I",core,8)[0]:
        raise ValueError("Expected a valid core footprint sidecar")
    seen = {(r[0], r[1]) for r in RECORD.iter_unpack(core[12:])}
    rows, areas = [], []
    bounds = runpy.run_path(str(Path(__file__).with_name("fetch-ahmedabad-osm.py")))["DISTRICT_BOUNDS"]
    for key, bbox in bounds.items():
        doc = json.loads((directory / key / "buildings.geojson").read_text())
        accepted = []
        for row in sorted(fit_buildings(doc["features"], network)):
            centre = row[:2]
            if centre in seen:
                continue
            accepted.append(row)
            seen.add(centre)
        if not 10 <= len(accepted) <= 1500:
            raise ValueError(f"{key}: unexpected number of clear exterior plots: {len(accepted)}")
        rows.extend(accepted)
        areas.append({"id":key,"bbox_lonlat":bbox,"input":len(doc["features"]),"exterior_shells":len(accepted)})
    return rows, {"areas":areas, "licence":"ODbL 1.0", "attribution":"© OpenStreetMap contributors"}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("buildings_geojson",type=Path,help="Core GeoJSON, or the parent folder of the three district snapshots")
    parser.add_argument("road_data",type=Path)
    parser.add_argument("output",type=Path)
    parser.add_argument("--districts",type=Path,metavar="CORE_BUILDINGS",help="Build the additive sidecar while excluding existing core plots")
    a=parser.parse_args()
    if a.districts:
        result, manifest = build_districts(a.buildings_geojson, a.road_data, a.districts)
    else:
        doc=json.loads(a.buildings_geojson.read_text())
        result=fit_buildings(doc["features"], terrain_index(a.road_data))
        manifest={"source_building_rows":len(doc["features"])}
    if not (30 if a.districts else 600) <= len(result) <= 5000:
        raise ValueError(f"Unexpected modeled building count {len(result)}")
    payload=b"SABBLD1\0"+struct.pack("<I",len(result))+b"".join(RECORD.pack(*r) for r in result)
    a.output.write_bytes(payload)
    manifest.update(exterior_shells=len(result),sha256=hashlib.sha256(payload).hexdigest())
    a.output.with_suffix(".json").write_text(json.dumps(manifest,indent=2)+"\n")
    print(json.dumps(manifest,indent=2))


if __name__ == "__main__":
    main()
