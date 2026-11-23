#!/usr/bin/env python3
"""Standalone, no-engine check of the packaged offline Ahmedabad map data."""
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import struct
import unittest

DATA = Path(__file__).resolve().parents[1] / "Content" / "CityData" / "AhmedabadRoads.bin"
HEADER, TILE, ROAD, GROUND = (struct.Struct(fmt) for fmt in ("<8sI8ifIII", "<iiII", "<4iHBB", "<4iB"))


def distance_to_strip_squared(x, y, road):
    ax, ay, bx, by = road[:4]
    dx, dy = bx - ax, by - ay
    frac = ((x - ax)*dx + (y - ay)*dy) / (dx*dx + dy*dy)
    frac = min(1.0, max(0.0, frac))
    return (x - ax - dx * frac)**2 + (y - ay - dy * frac)**2


class AhmedabadCityDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = DATA.read_bytes()
        cls.summary = json.loads(DATA.with_suffix(".json").read_text())
        assert hashlib.sha256(data).hexdigest() == cls.summary["sha256"]
        (signature, size, x0, y0, x1, y1, cx, cy, wx, wy, yaw, ways, strips, count) = HEADER.unpack_from(data)
        assert signature == b"SABOSM2\0" and size == 60000
        cls.bounds = (x0, y0, x1, y1)
        cls.spawns = ((cx,cy), (wx,wy), yaw)
        cls.road_count = ways
        cls.paths = strips
        cls.items = {}
        offset = HEADER.size
        for _ in range(count):
            tx, ty, n_road, n_ground = TILE.unpack_from(data, offset)
            offset += TILE.size
            assert (tx, ty) not in cls.items
            roads = list(ROAD.iter_unpack(memoryview(data)[offset:offset + ROAD.size*n_road]))
            offset += ROAD.size*n_road
            ground = list(GROUND.iter_unpack(memoryview(data)[offset:offset + GROUND.size*n_ground]))
            offset += GROUND.size*n_ground
            assert len(roads) == n_road and len(ground) == n_ground
            assert all((a,b) != (c,d) and 360 <= w <= 1280 and bridge in (0,1) and major in (0,1)
                       and x0 <= a <= x1 and x0 <= c <= x1 and y0 <= b <= y1 and y0 <= d <= y1
                       and (a+c)//120000 == tx and (b+d)//120000 == ty for a,b,c,d,w,bridge,major in roads)
            assert ground and all(x0 <= a < c <= x1 and y0 <= b < d <= y1 and wet in (0,1)
                                  for a,b,c,d,wet in ground)
            cls.items[tx,ty] = roads, ground
        assert len(cls.items) == count
        assert offset == len(data)

    def test_extent_and_tile_coverage(self):
        x0,y0,x1,y1 = self.bounds
        self.assertGreater((x1-x0)/100000, 25)
        self.assertGreater((y1-y0)/100000, 26)
        self.assertGreater(self.road_count, 49000)
        self.assertEqual(sum(len(paths) for paths, _ in self.items.values()), self.paths)
        self.assertGreater(self.paths, 150000)
        self.assertGreater(sum(wet for _, ground in self.items.values() for *_, wet in ground), 6000)
        self.assertEqual(len(self.items), self.summary["tile_count"])

    def test_spawns_use_data_road_and_bank(self):
        car, walk, yaw = self.spawns
        self.assertAlmostEqual(math.dist(car, walk), 216.0, delta=2.0)
        u = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        nearby = self.items[car[0]//60000, car[1]//60000]
        for x,y in (car, walk, (car[0]+180*u[0], car[1]+180*u[1]), (car[0]-180*u[0], car[1]-180*u[1])):
            self.assertTrue(any(not r[5] and distance_to_strip_squared(x,y,r) <= (r[4]/2)**2 for r in nearby[0]))
            self.assertTrue(any(a <= x <= b and c <= y <= d and not wet for a,c,b,d,wet in nearby[1]))

    def test_optional_exterior_shells_are_well_formed(self):
        path = DATA.with_name("AhmedabadBuildings.bin")
        if not path.exists():
            self.skipTest("The core massing sidecar is optional.")
        payload = path.read_bytes()
        (count,) = struct.unpack_from("<I", payload, 8)
        self.assertEqual(payload[:8], b"SABBLD1\0")
        self.assertEqual(len(payload), 12 + 17 * count)
        self.assertGreater(count, 1350)
        meta = json.loads(path.with_suffix(".json").read_text())
        self.assertEqual(hashlib.sha256(payload).hexdigest(), meta["sha256"])
        self.assertEqual(count, meta["exterior_shells"])
        for x,y,w,d,h,r,palette in struct.iter_unpack("<iiHHHhB", memoryview(payload)[12:]):
            self.assertIn((x//60000, y//60000), self.items)
            self.assertTrue(125 <= w and 125 <= d and 150 <= h <= 3200)
            self.assertTrue(abs(r) <= 18000 and 0 <= palette <= 2)

    def test_named_street_routes_follow_connected_road_geometry(self):
        paths = json.loads(DATA.with_name("AhmedabadRoutes.json").read_text())
        self.assertEqual(paths["source_sha256"], self.summary["sha256"])
        self.assertEqual(paths["licence"], "ODbL 1.0")
        roads = defaultdict(set)
        for entries, _ in self.items.values():
            for ax, ay, bx, by, width, _, _ in entries:
                if width >= 500:
                    roads[tuple(sorted(((ax, ay), (bx, by))))].add(width)
        counts = Counter(route["area"] for route in paths["routes"])
        self.assertEqual(counts, {"riverfront": 6, "ashram": 2, "cg": 2, "sg-service": 2})
        for route in paths["routes"]:
            pts = route["points"]
            self.assertGreaterEqual(len(pts), 6)
            distance = 0.
            for start, end in zip(pts, pts[1:] + pts[:1]):
                a, b = tuple(start[:2]), tuple(end[:2])
                self.assertIn(start[2], roads[tuple(sorted((a, b)))])
                distance += math.dist(a, b)
            self.assertAlmostEqual(route["length_cm"], distance, delta=0.51)

    def test_preview_buildings_and_travel_stops(self):
        preview = DATA.with_name("AhmedabadDistrictBuildings.bin")
        manifest = json.loads(preview.with_suffix(".json").read_text())
        raw = preview.read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), manifest["sha256"])
        self.assertEqual(raw[:8], b"SABBLD1\0")
        rows = list(struct.iter_unpack("<iiHHHhB", raw[12:]))
        self.assertEqual(len(rows), struct.unpack_from("<I", raw, 8)[0])
        self.assertEqual(len(rows), manifest["exterior_shells"])
        self.assertEqual(sum(a["exterior_shells"] for a in manifest["areas"]), len(rows))
        self.assertEqual(manifest["licence"], "ODbL 1.0")
        self.assertIn("OpenStreetMap", manifest["attribution"])
        central = DATA.with_name("AhmedabadBuildings.bin").read_bytes()
        centres = {(x, y) for x,y,*_ in struct.iter_unpack("<iiHHHhB", central[12:])}
        for x, y, width, depth, height, yaw, palette in rows:
            self.assertNotIn((x,y), centres)
            centres.add((x,y))
            self.assertIn((x//60000,y//60000), self.items)
            self.assertTrue(min(width,depth) >= 125 and 150 <= height <= 3200)
            self.assertTrue(abs(yaw) <= 18000 and 0 <= palette <= 2)
        stops = {s["id"]: s for s in json.loads(DATA.with_name("ExplorationSites.json").read_text())["city"]}
        offset = 0
        for area in manifest["areas"]:
            n = area["exterior_shells"]
            self.assertLessEqual(n, area["input"])
            stop = stops[area["id"]]
            x, y = stop["location"][:2]
            self.assertTrue(math.isfinite(stop["street_heading"]))
            self.assertGreaterEqual(sum(math.hypot(rx-x,ry-y)<15000 for rx,ry,*_ in rows[offset:offset+n]), 4)
            nearby = [r for i in (-1,0,1) for j in (-1,0,1)
                      for r in self.items.get((x//60000+i, y//60000+j), ([],[]))[0]]
            self.assertTrue(any(not r[5] and distance_to_strip_squared(x,y,r) <= (r[4]/2)**2 for r in nearby))
            offset += n

    def test_bridge_data_spans_mapped_water(self):
        crossing = []
        for roads, ground in self.items.values():
            wet = [r for r in ground if r[-1]]
            if not wet:
                continue
            for ax,ay,bx,by,_,bridge,_ in roads:
                if bridge and any(a <= (ax+bx)/2 <= b and c <= (ay+by)/2 <= d for a,c,b,d,_ in wet):
                    crossing.append(bridge)
        self.assertGreater(len(crossing), 15)


if __name__ == "__main__":
    unittest.main(verbosity=2)
