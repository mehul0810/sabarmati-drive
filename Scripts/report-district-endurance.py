"""Check a completed native endurance pass and report cold and warm frame times."""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


def read_metadata(path):
    return dict(line.split("=", 1) for line in path.read_text().splitlines() if "=" in line)


def frame_groups(path):
    groups = defaultdict(list)
    focus = defaultdict(list)
    previous = 0.0
    with path.open(newline="") as source:
        for row in csv.DictReader(source):
            elapsed = float(row["elapsed_s"])
            wall_ms = float(row["wall_ms"])
            minute = int(row["minute"])
            if not math.isfinite(elapsed) or not math.isfinite(wall_ms) or elapsed < previous or wall_ms <= 0:
                raise ValueError("Invalid wall time in the frame series")
            # UE writes elapsed seconds to three decimals. At a minute boundary a frame
            # can round up while retaining the preceding minute from the engine clock.
            if minute < 0 or not minute * 60 <= elapsed <= (minute + 1) * 60:
                raise ValueError("Frame is in the wrong minute")
            if abs((elapsed - previous) * 1000 - wall_ms) > 1.01:
                raise ValueError("The elapsed time contains a gap in the recorded frames")
            if "focused" in row:
                if row["focused"] not in {"0", "1"}:
                    raise ValueError("Unknown frame focus state")
                focus[row["focused"]].append(wall_ms)
            groups[minute].append(wall_ms)
            previous = elapsed
    if not groups:
        raise ValueError("The recording contains no frames")
    return groups, previous, focus


def describe(values):
    ordered = sorted(values)

    def percentile(fraction):
        return ordered[math.ceil(fraction * len(ordered)) - 1]

    return (
        f"{len(ordered):,} frames, median {percentile(.5):.2f} ms, "
        f"p95 {percentile(.95):.2f} ms, p99 {percentile(.99):.2f} ms, "
        f"{sum(ms > 33.33 for ms in ordered)} over 33.33 ms, "
        f"{sum(ms > 100 for ms in ordered)} over 100 ms"
    )


def report(root, minimum_minutes=0.0):
    totals = read_metadata(root / "endurance.txt")
    settings = read_metadata(root / "settings.txt")
    duration = float(totals["duration_s"])
    if not math.isfinite(minimum_minutes) or minimum_minutes < 0:
        raise ValueError("Requested duration must be a finite nonnegative number of minutes")
    if not math.isfinite(duration) or duration <= 0 or duration < minimum_minutes * 60:
        raise ValueError("The run did not reach the requested duration")

    groups, last_elapsed, focus = frame_groups(root / "endurance.csv")
    frames = [frame for group in groups.values() for frame in group]
    walk, drive = (float(totals[k]) for k in ("walk_metres", "drive_metres"))
    if not 0 <= duration - last_elapsed <= .75 or len(frames) != int(totals["frames"]):
        raise ValueError("The recording does not match its completion summary")
    if int(totals["loops"]) < 1 or not all(math.isfinite(m) and m > 0 for m in (walk, drive)):
        raise ValueError("The run did not complete a walking and driving loop")

    print(f"{settings['gpu']} | {settings['viewport']} | {duration:.2f} s | {totals['loops']} traversal loops")
    print(f"Walked {walk:.2f} m and drove {drive:.2f} m.")
    print("All frames:", describe(frames))
    for key, label in (("1", "Game focused"), ("0", "Game in background")):
        if focus[key]:
            print(label + ":", describe(focus[key]))
    print("First minute:", describe(groups[0]))
    full_minutes = [minute for minute in groups if (minute + 1) * 60 <= duration + .001]
    if full_minutes:
        print("Last full minute:", describe(groups[max(full_minutes)]))
    print(f"Worst frame {max(frames):.2f} ms.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("--minutes", type=float, default=0, help="Required minimum wall-clock duration")
    args = parser.parse_args()
    try:
        report(args.run, args.minutes)
    except (KeyError, OSError, ValueError) as error:
        parser.exit(1, f"Invalid endurance recording: {error}\n")
