#!/usr/bin/env python3
"""Fetch reproducible raw OpenStreetMap geometry through Esri India's public feeds.

The runtime never contacts the network. This developer-only tool records the
paginated public API extract locally. Keep it out of the packaged game.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

BBOX = (72.4472, 22.9121, 72.7037, 23.1387)  # W, S, E, N; not an AMC boundary polygon
BUILDING_CORE = (72.559, 23.002, 72.591, 23.032)  # ~3.3 km central pilot
BASE = "https://livingatlas.esri.in/server/rest/services/OSM"
CLASSES = (
    "Motorway", "Motorway Link", "Trunk", "Trunk Link", "Primary", "Primary Link",
    "Secondary", "Secondary Link", "Tertiary", "Tertiary Link", "Residential",
    "Unclassified", "Living Street", "Service",
)
ROADS = "fclass IN (" + ",".join(repr(cls) for cls in CLASSES) + ")"
WATER = "fclass IN ('River','Riverbank','Water','Reservoir','Dock')"
PAGE_SIZE = 2000


def fetch(kind, **params):
    args = dict(geometry=",".join(map(str, BUILDING_CORE if kind == "Buildings" else BBOX)), geometryType="esriGeometryEnvelope",
                spatialRel="esriSpatialRelIntersects", inSR="4326", outSR="4326", **params)
    url = f"{BASE}/{kind}/MapServer/0/query?{urlencode(args)}"
    req = Request(url, headers={"User-Agent": "SabarmatiDrive open-data import/1.0"})
    for attempt in range(3):
        try:
            with urlopen(req, timeout=65) as response:
                result = json.load(response)
            if "error" in result:
                raise ValueError(result["error"])
            return result
        except (TimeoutError, URLError) as exc:
            if isinstance(exc, HTTPError) and exc.code not in (429, 500, 502, 503, 504):
                raise  # Do not retry a permission failure.
            if attempt >= 2:
                raise
            time.sleep(2 ** attempt)


def collect(kind, where, out_fields, directory):
    count = fetch(kind, f="json", where=where, returnCountOnly="true")["count"]
    if not 0 < count < 120000:
        raise ValueError(f"Unexpected {kind} count {count}; refusing an unbounded pull")
    def page(offset):
        target = directory / f"{kind.lower()}-{offset:06d}.geojson"
        if target.exists():
            value = json.loads(target.read_text())
        else:
            value = fetch(kind, f="geojson", where=where, orderByFields="objectid ASC",
                          outFields=out_fields, resultOffset=offset, resultRecordCount=PAGE_SIZE)
            if value.get("type") != "FeatureCollection":
                raise ValueError(f"Unexpected page type for {kind}+{offset}")
            target.write_text(json.dumps(value, ensure_ascii=False, separators=(",", ":")))
        rows = value["features"]
        if offset + PAGE_SIZE < count and len(rows) != PAGE_SIZE:
            raise ValueError(f"{kind}+{offset} returned {len(rows)} instead of requested page")
        return rows
    print(f"{kind}: downloading {count} features", flush=True)
    with ThreadPoolExecutor(max_workers=3) as worker:
        rows = []
        for n, result in enumerate(worker.map(page, range(0, count, PAGE_SIZE)), 1):
            rows.extend(result)
            print(f"{kind} page {n}: {len(rows)}/{count}", flush=True)
    ids = [row["properties"]["objectid"] for row in rows]
    stable = {}
    if ids != sorted(ids):
        raise ValueError(f"{kind} paging reordered IDs; use a fresh output directory")
    for row, object_id in zip(rows, ids):
        previous = stable.setdefault(object_id, row)
        if previous != row:
            raise ValueError(f"{kind} feature {object_id} changed during paging")
    if len(stable) != count:
        verified_count = fetch(kind, f="json", where=where, returnCountOnly="true")["count"]
        if len(stable) != verified_count:
            raise ValueError(f"{kind} paging does not match either reference count")
    if len(stable) != len(rows):
        print(f"{kind}: collapsed {len(rows) - len(stable)} identical rows; count verified")
    (directory / f"{kind.lower()}.geojson").write_text(json.dumps(
        {"type": "FeatureCollection", "features": list(stable.values())},
        ensure_ascii=False, separators=(",", ":")))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--only", choices=("Roads", "Water", "Buildings"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for kind, where, fields in (
        ("Roads", ROADS, "objectid,id,fclass,fname,brg_flg,tnl_flg,lyr_cnt"),
        ("Water", WATER, "objectid,id,fclass,fname"),
        ("Buildings", "1=1", "objectid,id,fclass,ftype"),
    ):
        if args.only in (None, kind):
            collect(kind, where, fields, args.output)


if __name__ == "__main__":
    main()
