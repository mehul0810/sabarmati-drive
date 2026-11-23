"""Validate and summarize the rows and PNGs in one native benchmark directory."""
import argparse
import csv
import math
import statistics
import struct
from pathlib import Path


def inspect_run(folder):
    log = folder / "engine.log"
    if log.is_file():
        failed = next((line for line in log.read_text(errors="replace").splitlines()
                       if "Failed to compile Material /Game/Sabarmati/" in line), None)
        if failed:
            raise ValueError("A project shader fell back to the default material: " + failed)
    settings = dict(row.split("=", 1) for row in (folder / "settings.txt").read_text().splitlines())
    width, height = map(int, settings["viewport"].split("x"))
    drive = dict(row.split("=", 1) for row in (folder / "drive.txt").read_text().splitlines())
    if settings["windowed"] != "1" or drive["road_contact"] != "1" or float(drive["distance_m"]) < 45:
        raise ValueError("Window or driving check failed")
    if settings["time"] != "legacy-day":
        inputs = (folder / "input.txt").read_text().splitlines()
        if len(inputs) != 3 or set(inputs) != {"day: 0 vehicle beams", "golden: 0 vehicle beams", "night: 2 vehicle beams"}:
            raise ValueError("Lighting input or headlight check failed")
    groups = {}
    with (folder / "frames.csv").open(newline="") as source:
        for row in csv.DictReader(source):
            samples = groups.setdefault(row["view"], [])
            timings = tuple(float(row[c]) for c in ("wall_ms", "game_ms", "render_ms", "gpu_ms"))
            if int(row["frame"]) != len(samples) or timings[0] <= 0 or not all(math.isfinite(x) and x >= 0 for x in timings):
                raise ValueError(f"Invalid frame in {row['view']}")
            samples.append(timings)
    expected = {"atal-landing", "shopfront", "parked-hatch", "driving"}
    if settings.get("format") == "2":
        expected.update(("tea-room", "garden-walk"))
    if set(groups) != expected:
        raise ValueError("Missing or unexpected camera view")
    for name, rows in groups.items():
        if len(rows) < (30 if name == "driving" else 180):
            raise ValueError(f"Incomplete samples for {name}")
        with (folder / (name + ".png")).open("rb") as source:
            header = source.read(24)
        if header[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", header[16:24]) != (width, height):
            raise ValueError(f"Capture size mismatch for {name}")
    wall_seconds = sum(row[0] for row in groups["driving"]) / 1000
    if wall_seconds < 2.5:
        raise ValueError("Insufficient chase duration")
    if settings.get("format") == "2" and (int(drive.get("contact_samples", 0)) != len(groups["driving"])
            or abs(float(drive.get("duration_s", 0)) - wall_seconds) > .05):
        raise ValueError("Chase contact count or duration mismatch")
    return settings, drive, groups


def percentile(values, q):
    return sorted(values)[math.ceil(q * len(values)) - 1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    settings, drive, groups = inspect_run(args.folder)
    print(f"{settings['time']} | {settings['gpu']} | {settings['viewport']} | {drive['distance_m']} m driven")
    print(f"Chase covered {len(groups['driving'])} frames in {sum(row[0] for row in groups['driving'])/1000:.3f} s")
    print("| View | Frames | Median ms | P95 ms | P99 ms | Game median ms | Render median ms | GPU median ms |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|")
    for name, rows in groups.items():
        wall, game, render, gpu = zip(*rows)
        gpu_ms = f"{statistics.median(gpu):.2f}" if any(gpu) else "unavailable"
        print(f"| {name} | {len(rows)} | {statistics.median(wall):.2f} | {percentile(wall,.95):.2f} | "
              f"{percentile(wall,.99):.2f} | {statistics.median(game):.2f} | {statistics.median(render):.2f} | {gpu_ms} |")


if __name__ == "__main__":
    main()
