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
  building plots from 1,683 mapped footprints. The shell geometry
  uses fitted simplified rectangles, estimated heights and existing art
  materials, with procedural glazing, roof trim and street-facing openings.
  The closest large plot to the city start has two structural wings, a sheltered colonnade and an open cross-passage. Facades, heights and openings are artistic estimates. Private interiors are outside the playable area.
  Shapes that overlap mapped roads or water are excluded. A Development preview adds three smaller mapped pockets within the existing city extent.
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


The Atal west-bank reference area shows elongated flat-roofed buildings, secondary roof volumes, and narrow access lanes beside the larger carriageways. The original arcade interprets those features using the existing OSM plot. Esri World Imagery was viewed during development. All shipped geometry and textures come from the local map data, project models and the credited surface materials. [Satellite view centred on the west approach](https://www.arcgis.com/apps/mapviewer/index.html?basemap=satellite&center=72.572,23.017&level=18)

Major road sections within about 530 m of the city start receive precast kerbs, footpaths and drain grilles. The mesh builder checks adjacent mapped road widths and leaves openings at junctions. Width, drainage positions and façade details are inferred.

## Named street preview

Three 0.008° square windows were sampled around Ashram Road, CG Road and an SG Highway service road. The 1,264 incoming building records yielded 243, 559 and 362 exterior shells respectively. The preparation step filters roads, water and overlap with the central 1,422 footprints. The binary companion manifest records all three query boxes and the SHA-256 digest. The heights, glazing and frontage materials are estimates. The service road is a local segment beside the highway.

```sh
python3 Scripts/fetch-ahmedabad-osm.py Saved/AhmedabadDistrictSource --districts
python3 Scripts/prepare-ahmedabad-buildings.py Saved/AhmedabadDistrictSource Content/CityData/AhmedabadRoads.bin Content/CityData/AhmedabadDistrictBuildings.bin --districts Content/CityData/AhmedabadBuildings.bin
python3 Scripts/prepare-ahmedabad-routes.py Content/CityData/AhmedabadRoads.bin Content/CityData/AhmedabadRoutes.json --sites Content/CityData/ExplorationSites.json
```

Six closed traffic circuits around the river and two at each of the three streets follow connected, mapped road edges. Each circuit supports two directions. These tests and transforms run locally against the packaged data. [OSM source and attribution](https://www.openstreetmap.org/copyright)
