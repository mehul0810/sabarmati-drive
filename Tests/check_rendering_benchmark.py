import csv
import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("render_report", ROOT / "Scripts/report-rendering-benchmark.py")
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.run_dir = Path(self.temp.name)
        (self.run_dir / "settings.txt").write_text("time=night\nviewport=32x18\nwindowed=1\ngpu=test\n")
        (self.run_dir / "drive.txt").write_text("distance_m=45.2\nroad_contact=1\n")
        (self.run_dir / "input.txt").write_text("night: 2 vehicle beams\nday: 0 vehicle beams\ngolden: 0 vehicle beams\n")
        with (self.run_dir / "frames.csv").open("w", newline="") as output:
            writer = csv.writer(output)
            writer.writerow(("view","frame","wall_ms","game_ms","render_ms","gpu_ms"))
            for view in ("atal-landing","shopfront","parked-hatch","driving"):
                (self.run_dir / (view + ".png")).write_bytes(b"\x89PNG\r\n\x1a\n" + b"\x00\x00\x00\rIHDR" + struct.pack(">II",32,18))
                for i in range(180):
                    writer.writerow((view,i,16.7,1.5,2.2,10))

    def test_valid_run_and_nearest_rank_tail(self):
        _, _, groups = report.inspect_run(self.run_dir)
        self.assertEqual(len(groups["driving"]), 180)
        self.assertEqual(report.percentile(range(1,101), .95), 95)

    def test_rejects_vehicle_or_input_failure(self):
        (self.run_dir / "input.txt").write_text("night: 0 vehicle beams\n")
        with self.assertRaisesRegex(ValueError, "headlight"):
            report.inspect_run(self.run_dir)

    def test_rejects_stale_or_rescaled_capture(self):
        (self.run_dir / "shopfront.png").write_bytes(b"x"*24)
        with self.assertRaisesRegex(ValueError, "Capture size"):
            report.inspect_run(self.run_dir)

    def test_rejects_one_chase_frame(self):
        data = self.run_dir / "frames.csv"
        rows = data.read_text().splitlines()
        data.write_text("\n".join(r for r in rows if not r.startswith("driving,") or r.startswith("driving,0,")) + "\n")
        with self.assertRaisesRegex(ValueError, "Incomplete samples"):
            report.inspect_run(self.run_dir)

    def test_registry_paths_and_rights(self):
        entries = json.loads((ROOT / "SourceAssets/asset-registry.json").read_text())["assets"]
        self.assertEqual(len(entries), len({asset["id"] for asset in entries}))
        for asset in entries:
            self.assertTrue((ROOT / asset["location"]).is_file(), asset["location"])
            for field in ("category","source","license","geometry","textures","collision","lod","verification"):
                self.assertTrue(asset[field].strip(), (asset["id"],field))


if __name__ == "__main__":
    unittest.main(verbosity=2)
