"""Extract drivable traffic routes from the packaged Ahmedabad OSM linework."""
import argparse
import collections
import hashlib
import heapq
import json
import math
from pathlib import Path
import struct

HEADER,TILE,ROAD,LAND=(struct.Struct(x) for x in ("<8sI8ifIII","<iiII","<4iHBB","<4iB"))


def starting_street(network, spawn):
    """Follow the mapped starting road to two distant endpoints without inventing a connector."""
    def distance(a, b):
        dx, dy = b[0]-a[0], b[1]-a[1]
        t = max(0., min(1., ((spawn[0]-a[0])*dx+(spawn[1]-a[1])*dy)/(dx*dx+dy*dy)))
        return math.dist(spawn, (a[0]+t*dx, a[1]+t*dy))

    edges = ((distance(a,b),a,b) for a, links in network.items()
             for b,w in links.items() if a < b and w >= 620)
    gap, a, b = min(edges, default=(math.inf,None,None))
    if gap > 1000.:
        return None
    path = [a,b]
    for _ in range(2):
        path.reverse()
        while math.dist(spawn, path[-1]) < 72000.:
            a,b = path[-2:]
            dx,dy = b[0]-a[0],b[1]-a[1]
            length = math.hypot(dx,dy)
            choices = []
            for c,w in network[b].items():
                if w < 620 or c in path:
                    continue
                tx,ty = c[0]-b[0],c[1]-b[1]
                alignment = (dx*tx+dy*ty)/(length*math.hypot(tx,ty))
                if alignment > .82:
                    choices.append((alignment,w,c))
            if not choices:
                break
            path.append(max(choices)[-1])
    if min(math.dist(spawn, p) for p in (path[0],path[-1])) < 53000.:
        return None
    edges = list(zip(path,path[1:]))
    points = [[*a,network[a][b]] for a,b in edges]
    points.append([*path[-1],points[-1][2]])
    return {"area":"riverfront", "closed":False,
            "length_cm":round(sum(math.dist(a,b) for a,b in edges)), "points":points}


def extract(data, extra_stops=()):
    raw=data.read_bytes()
    h=HEADER.unpack_from(raw)
    if h[0]!=b"SABOSM2\0": raise ValueError("Unknown road file")
    spawn=h[6:8]; network=collections.defaultdict(dict); at=HEADER.size
    for _ in range(h[-1]):
        _,_,nr,ng=TILE.unpack_from(raw,at); at+=TILE.size
        for ax,ay,bx,by,w,bridge,_ in ROAD.iter_unpack(raw[at:at+nr*ROAD.size]):
            if w<500: continue
            a,b=(ax,ay),(bx,by)
            if a==b: continue
            network[a][b]=network[b][a]=w
        at+=nr*ROAD.size+ng*LAND.size
    if at!=len(raw): raise ValueError("Road data length mismatch")
    def alternate(start,end):
        best={start:0.}; prev={}; heap=[(0.,start)]
        while heap:
            cost,here=heapq.heappop(heap)
            if cost!=best[here]:continue
            if here==end:
                chain=[here]
                while chain[-1]!=start: chain.append(prev[chain[-1]])
                return chain[::-1]
            for nxt in sorted(network[here]):
                if here==start and nxt==end:continue
                proposed=cost+math.dist(here,nxt)
                if proposed<165000 and proposed<best.get(nxt,float("inf")):
                    best[nxt]=proposed;prev[nxt]=here;heapq.heappush(heap,(proposed,nxt))
        return []
    landing = starting_street(network, spawn)
    selected = [landing] if landing else []
    used=set()
    for area,centre,limit in [("riverfront",spawn,6),*extra_stops]:
        prior=len(selected)
        candidates=sorted((v for v,e in network.items() if len(e)>=3 and math.dist(v,centre)<145000),
                          key=lambda v:(math.dist(v,centre),v))
        for a in candidates:
            if len(selected)-prior==limit: break
            for b in sorted(network[a],key=lambda v:-network[a][v]):
                if (a,b) in used: continue
                path=alternate(a,b)
                if len(path)<6: continue
                edges=list(zip(path,path[1:]+[path[0]]))
                length=sum(math.dist(x,y) for x,y in edges)
                if length<22500 or sum((x,y) in used for x,y in edges)>len(edges)*.25: continue
                widths=[network[x][y] for x,y in edges]
                selected.append({"area": area, "length_cm":round(length),"points":[[x,y,w] for (x,y),w in zip(path,widths)]})
                for x,y in edges: used.add((x,y));used.add((y,x))
                break
        if len(selected)-prior<limit:
            raise ValueError(f"Insufficient connected routes around {area}")
    return {"source_sha256":hashlib.sha256(raw).hexdigest(),"licence":"ODbL 1.0","attribution":"© OpenStreetMap contributors","routes":selected}
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("roads",type=Path);p.add_argument("out",type=Path)
    p.add_argument("--sites",type=Path,help="Include closed street routes at named offline city destinations")
    a=p.parse_args()
    stops=[]
    if a.sites:
        stops=[(s["id"],s["location"][:2],2) for s in json.loads(a.sites.read_text())["city"]
               if "street_heading" in s and s["id"]!="city-landing"]
    doc=extract(a.roads,stops);a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(doc,separators=(",",":"))+"\n")
    print(len(doc["routes"]),"connected routes",sum(r["length_cm"] for r in doc["routes"])/100,"m")
