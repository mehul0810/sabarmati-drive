#!/usr/bin/env python3
"""Turn the local OSM route snapshot into a bounded, streamed Unreal data file."""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import struct

# Source query bounds are a rectangle, not Ahmedabad's legal boundary.
BBOX = (72.4472, 22.9121, 72.7037, 23.1387)
ORIGIN = (72.5754464, 23.0164704)  # Atal Bridge vicinity, local projected map centre
CM_PER_DEG_LAT = 11_119_508.0
CM_PER_DEG_LON = CM_PER_DEG_LAT * math.cos(math.radians(ORIGIN[1]))
TILE = 60_000
WET_ROW = 1150
MAGIC = b"SABOSM2\0"
ROAD_WIDTHS = {
    "Motorway": 1280, "Motorway Link": 840, "Trunk": 1180, "Trunk Link": 740,
    "Primary": 1040, "Primary Link": 690, "Secondary": 875, "Secondary Link": 660,
    "Tertiary": 720, "Tertiary Link": 600, "Residential": 500, "Unclassified": 550,
    "Living Street": 410, "Service": 360,
}


def project(point):
    lon, lat = point[:2]
    return round((lat - ORIGIN[1]) * CM_PER_DEG_LAT), round((ORIGIN[0] - lon) * CM_PER_DEG_LON)


def distance_to_segment_sq(point, start, end):
    vx, vy = end[0] - start[0], end[1] - start[1]
    mag2 = vx * vx + vy * vy
    f = max(0, min(1, ((point[0] - start[0])*vx + (point[1] - start[1])*vy) / mag2)) if mag2 else 0
    return (point[0] - start[0] - f*vx)**2 + (point[1] - start[1] - f*vy)**2


def simplify(line, tolerance):
    if len(line) < 3:
        return line
    keep = {0, len(line) - 1}
    pending = [(0, len(line) - 1)]
    threshold = tolerance ** 2
    while pending:
        first, last = pending.pop()
        split, best = None, threshold
        for i in range(first + 1, last):
            d = distance_to_segment_sq(line[i], line[first], line[last])
            if d > best:
                split, best = i, d
        if split is not None:
            keep.add(split)
            pending.extend(((first, split), (split, last)))
    return [line[i] for i in sorted(keep)]


def ordered_projection_box():
    a, b = project(BBOX[:2]), project(BBOX[2:])
    return min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])


def clip(a, b, bounds):
    x0, y0 = a
    dx, dy = b[0] - x0, b[1] - y0
    minx, miny, maxx, maxy = bounds
    low, high = 0.0, 1.0
    for p, q in [(-dx, x0 - minx), (dx, maxx - x0), (-dy, y0 - miny), (dy, maxy - y0)]:
        if p == 0:
            if q < 0:
                return None
            continue
        r = q / p
        if p < 0:
            low = max(low, r)
        else:
            high = min(high, r)
    if low >= high:
        return None
    return (round(x0 + low*dx), round(y0 + low*dy)), (round(x0 + high*dx), round(y0 + high*dy))


def tiled_segments(a, b, width):
    # Split at exact 600 m boundaries; one adjacent tile cannot own a whole long road.
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy)
    if length < 32:
        return
    split = {0.0, 1.0}
    for axis, delta in ((0, dx), (1, dy)):
        if delta:
            start, end = sorted((a[axis], b[axis]))
            for edge in range((start // TILE + 1) * TILE, end, TILE):
                split.add((edge - a[axis]) / delta)
    samples = [(round(a[0]+t*dx), round(a[1]+t*dy)) for t in sorted(split)]
    for p, q in zip(samples, samples[1:]):
        if p != q:
            tile = ((p[0] + q[0]) // (2*TILE), (p[1] + q[1]) // (2*TILE))
            yield tile, (*p, *q, width)


def water_polygons(doc, extent):
    all_polys = []
    for f in doc["features"]:
        g = f["geometry"]
        if not g or g["type"] not in ("Polygon", "MultiPolygon"):
            continue
        polygons = [g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]
        for rings in polygons:
            rings = [simplify([project(p) for p in ring], 75) for ring in rings if len(ring) >= 4]
            if not rings:
                continue
            lo_x, lo_y = min(p[0] for p in rings[0]), min(p[1] for p in rings[0])
            hi_x, hi_y = max(p[0] for p in rings[0]), max(p[1] for p in rings[0])
            if lo_x < extent[2] and hi_x > extent[0] and lo_y < extent[3] and hi_y > extent[1]:
                all_polys.append(((lo_x, lo_y, hi_x, hi_y), rings))
    return all_polys


def wet_intervals(x, y0, y1, polys):
    spans = []
    for (lo_x, lo_y, hi_x, hi_y), rings in polys:
        if not (lo_x <= x < hi_x and lo_y < y1 and hi_y > y0):
            continue
        joins = []
        for ring in rings:
            for a, b in zip(ring, ring[1:] + ring[:1]):
                if (a[0] <= x < b[0]) or (b[0] <= x < a[0]):
                    joins.append(a[1] + (b[1] - a[1]) * (x - a[0]) / (b[0] - a[0]))
        joins.sort()
        for i in range(0, len(joins) - 1, 2):
            low, high = max(y0, round(joins[i])), min(y1, round(joins[i+1]))
            if high > low:
                spans.append((low, high))
    union = []
    for low, high in sorted(spans):
        if union and low <= union[-1][1]:
            union[-1] = union[-1][0], max(high, union[-1][1])
        else:
            union.append((low, high))
    return union


def tile_rectangles(box, polys):
    x0, y0, x1, y1 = box
    relevant = [row for row in polys if row[0][0] < x1 and row[0][2] > x0 and row[0][1] < y1 and row[0][3] > y0]
    if not relevant:
        return [(x0, y0, x1, y1, 0)]
    rects = []
    active = {}
    for start in range(x0, x1, WET_ROW):
        end = min(start + WET_ROW, x1)
        from_y = y0
        row = []
        for low, high in wet_intervals((start + end) / 2, y0, y1, relevant):
            if low > from_y:
                row.append((from_y, low, 0))
            row.append((low, high, 1))
            from_y = high
        if from_y < y1:
            row.append((from_y, y1, 0))
        next_active = {key: active.pop(key, start) for key in row}
        rects.extend((started, low, start, high, wet) for (low, high, wet), started in active.items())
        active = next_active
    rects.extend((started, low, x1, high, wet) for (low, high, wet), started in active.items())
    return rects


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("raw_directory", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    roads = json.loads((args.raw_directory / "roads.geojson").read_text())
    water = json.loads((args.raw_directory / "water.geojson").read_text())
    bounds = ordered_projection_box()
    x0, y0, x1, y1 = bounds
    polygons = water_polygons(water, bounds)
    if not polygons:
        raise ValueError("No water geometry; cannot silently use an invented river.")
    tiles = defaultdict(list)
    included_ways = set()
    spawn_options = []
    all_length = 0.0
    road_count = 0
    for f in roads["features"]:
        props, geom = f["properties"], f["geometry"]
        if not geom or geom["type"] not in ("LineString", "MultiLineString") or props.get("tnl_flg") == "T":
            continue
        kind = props["fclass"]
        width = ROAD_WIDTHS[kind]
        name = props.get("fname") or ""
        bridge = props.get("brg_flg") == "T"
        tier = 1 if width >= 640 else 0
        lines = [geom["coordinates"]] if geom["type"] == "LineString" else geom["coordinates"]
        for line in lines:
            line = simplify([project(p) for p in line], 46 if tier else 62)
            for a, b in zip(line, line[1:]):
                cut = clip(a, b, bounds)
                if not cut:
                    continue
                a, b = cut
                length = math.dist(a, b)
                if length > 1200 and width >= 640 and not bridge and abs(a[0]) < 80000 and 10000 < a[1] < 50000:
                    nx, ny = (b[0] - a[0]) / length, (b[1] - a[1]) / length
                    if nx < 0:
                        a, b, nx, ny = b, a, -nx, -ny
                    # Car on the surveyed centreline; walking spawn is at the usable driver-side edge.
                    cx, cy = round((a[0]+b[0])*.5), round((a[1]+b[1])*.5)
                    wx, wy = round(cx - ny * 216 + nx * 10), round(cy + nx * 216 + ny * 10)
                    wet = any(l <= cy <= h for l, h in wet_intervals(cx, cy - 300, cy + 300, polygons))
                    if not wet:
                        score = math.dist((cx, cy), (-1400, 20500))
                        if "riverfront" in name.lower().replace(" ", ""):
                            score *= .62
                        if abs(nx) < .5:
                            score *= 1.4
                        spawn_options.append((score, cx, cy, wx, wy, math.degrees(math.atan2(ny, nx)), name))
                for tile, row in tiled_segments(a, b, width):
                    tiles[tile].append((*row, int(bridge), tier))
                    included_ways.add(props.get("id") or str(f["id"]))
                    road_count += 1
                    all_length += math.dist(row[:2], row[2:4])
    if len(included_ways) < 30000 or not spawn_options or road_count > 400_000:
        raise ValueError(f"Unexpected road extract: {len(included_ways)} ways, {road_count} segments, {len(spawn_options)} spawns")
    _, cx, cy, wx, wy, yaw, spawn_name = min(spawn_options)
    keys = [(x,y) for x in range(x0 // TILE, (x1 - 1) // TILE + 1) for y in range(y0 // TILE, (y1 - 1) // TILE + 1)]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    partial = args.output.with_suffix(".partial")
    wet_rects = 0
    with partial.open("wb") as out:
        out.write(struct.pack("<8sI8ifIII", MAGIC, TILE, *bounds, cx, cy, wx, wy, yaw,
                              len(included_ways), road_count, len(keys)))
        for tx, ty in keys:
            rows = tiles[(tx, ty)]
            box = max(x0,tx*TILE),max(y0,ty*TILE),min(x1,(tx+1)*TILE),min(y1,(ty+1)*TILE)
            rects = tile_rectangles(box, polygons)
            out.write(struct.pack("<iiII", tx,ty,len(rows),len(rects)))
            for row in rows:
                out.write(struct.pack("<4iHBB", *row))
            for rect in rects:
                out.write(struct.pack("<4iB", *rect))
                wet_rects += rect[-1]
    if wet_rects < 100:
        raise ValueError("Water tessellation did not cover the river.")
    partial.replace(args.output)
    summary = {
        "licence": "ODbL 1.0", "attribution": "© OpenStreetMap contributors; sourced via Esri India Living Atlas",
        "bbox_lonlat": BBOX, "origin_lonlat": ORIGIN, "bounds_cm": bounds,
        "roads_way_count": len(included_ways), "road_strip_count": road_count,
        "road_length_km": round(all_length/100000, 1), "source_water_polygons": len(polygons),
        "water_strips": wet_rects, "tile_count": len(keys),
        "walk_spawn_cm": [wx,wy,110], "car_spawn_cm": [cx,cy,68], "yaw": round(yaw,4),
        "spawn_road": spawn_name, "sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
    }
    args.output.with_suffix(".json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
