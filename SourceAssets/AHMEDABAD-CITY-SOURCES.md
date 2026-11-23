# Ahmedabad City offline map

The new City mode is separate from the detailed riverfront prototype. It uses
real road centerlines and water polygons from OpenStreetMap, not commercial
map tiles, stock road grids, or a render of a basemap.

**© OpenStreetMap contributors.** Data is licensed under the
[Open Data Commons Open Database License 1.0](https://opendatacommons.org/licenses/odbl/1-0/).
See [OpenStreetMap copyright and attribution](https://www.openstreetmap.org/copyright).
The local OpenStreetMap-derived runtime database under `Content/CityData` is
also offered under ODbL 1.0 and is independent of game source and art rights.
A public distribution should preserve the notice, offer the same derived data
and transformation scripts, and retain conspicuous in-game attribution. This
note does not publish the game or data.

Source: Esri India Living Atlas's public OpenStreetMap
[Road Network](https://livingatlas.esri.in/server/rest/services/OSM/Roads/MapServer/0)
[Waterways Polygon](https://livingatlas.esri.in/server/rest/services/OSM/Water/MapServer/0),
and [Building Footprints](https://livingatlas.esri.in/server/rest/services/OSM/Buildings/MapServer/0)
services, retrieved 2026-11-23. OpenStreetMap data is the source data; no
proprietary basemap style, satellite imagery or Esri rendering is embedded.

- WGS84 source rectangle: 72.4472 to 72.7037 longitude, 22.9121 to 23.1387
  latitude, or about **25.2 × 26.25 km**. It covers Ahmedabad and adjacent
  land. It is not the official municipal boundary.
- 50,030 distinct non-tunnel OSM road ways, 170,328 simplified/tile-cut road
  geometry portions, 142 intersecting water polygons. Parallel carriageways
  count independently, and the total line length is not a network audit.
- 1,892 offline 600-metre tiles; the runtime holds at most 25 near the
  player after a tile update. The map is self-contained once built. No token,
  live map service, location request, or Internet lookup is made in game.
- Road widths and markings depend on road class instead of local surveys.
  Terrain is level, water edges sampled in 11.5 m strips, and flyover grade
  and intersections approximated. This is a drivable geographic base,
  not a survey, photoreal city, or turn-by-turn routing system.
- The optional **~3.3 × 3.3 km central footprint pilot** adds 1,422 exterior
  building massing shells from 1,683 mapped footprints. The shell geometry
  uses fitted simplified rectangles, estimated heights and existing art
  materials, with procedural glazing, roof trim and street-facing openings.
  They are not reconstructed individual facades. No interiors are present.
  Shapes that overlap mapped roads or water are excluded. The rest of the
  city does not yet have exterior massing.
- The verified start is on Sabarmati Riverfront Road west of Atal's
  geographic center, on mapped pavement; adjacent driver-side walk start.

## Rebuild locally

```sh
python3 Scripts/fetch-ahmedabad-osm.py Saved/AhmedabadSource
python3 Scripts/prepare-ahmedabad-city.py Saved/AhmedabadSource Content/CityData/AhmedabadRoads.bin
python3 Scripts/prepare-ahmedabad-buildings.py Saved/AhmedabadSource/buildings.geojson Content/CityData/AhmedabadRoads.bin Content/CityData/AhmedabadBuildings.bin
python3 Tests/check_city_data.py
```

Each source request queries only the rectangle and specific transport
classes. A live OSM row may change while pages are downloading; identical
duplicates can be collapsed when unique row count still matches, but
inconsistent counts or conflicting duplicate data stop extraction. Re-run
into a fresh directory for a new snapshot. The game needs the road/water binary, plus the optional building binary.
The companion JSON manifests provide counts and SHA-256 digests; the raw
GeoJSON is not packaged.
