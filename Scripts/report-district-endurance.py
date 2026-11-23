"""Check a completed native endurance pass and report cold and warm frame distributions."""
import argparse
import csv
import math
from pathlib import Path

def report(root):
    def metadata(file):
        return dict(line.split("=",1) for line in (root/file).read_text().splitlines() if "=" in line)
    totals=metadata("endurance.txt"); settings=metadata("settings.txt")
    groups={}; all_frames=[]; previous=-1.
    with (root/"endurance.csv").open(newline="") as source:
        for frame in csv.DictReader(source):
            t=float(frame["elapsed_s"]); ms=float(frame["wall_ms"]); minute=int(frame["minute"])
            if not math.isfinite(ms) or ms<0 or t<previous or minute!=int(t/60):raise ValueError("Invalid frame series")
            groups.setdefault(minute,[]).append(ms);all_frames.append(ms);previous=t
    expected=float(totals["duration_s"])
    if expected-previous>.75 or len(all_frames)!=int(totals["frames"]) or int(totals["loops"])<1 or min(float(totals[k]) for k in ("walk_metres","drive_metres"))<=0:
        raise ValueError("The traversal or recording did not complete")
    def stats(values):
        ordered=sorted(values)
        def p(f):return ordered[min(len(ordered)-1,math.ceil(f*len(ordered))-1)]
        return f"{len(ordered):,} frames, median {p(.5):.2f} ms, p95 {p(.95):.2f} ms, p99 {p(.99):.2f} ms, {sum(x>33.33 for x in ordered)} over 33.33 ms, {sum(x>100. for x in ordered)} over 100 ms"
    print(f"{settings['gpu']} | {settings['viewport']} | {expected:.2f} s | {totals['loops']} traversal loops")
    print(f"Walked {totals['walk_metres']} m and drove {totals['drive_metres']} m.")
    print("All frames:",stats(all_frames));print("First minute:",stats(groups[min(groups)]))
    usable=[m for m in groups if (m+1)*60<=expected+.01]
    if usable:print("Last full minute:",stats(groups[max(usable)]))
    print(f"Worst frame {max(all_frames):.2f} ms.")
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("run",type=Path);args=p.parse_args()
    report(args.run)
