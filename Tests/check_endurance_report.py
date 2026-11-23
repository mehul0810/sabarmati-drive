import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("endurance_report", ROOT / "Scripts/report-district-endurance.py")
reporter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(reporter)


class EnduranceReportTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.run_dir = Path(temp.name)
        (self.run_dir / "settings.txt").write_text("gpu=Test GPU\nviewport=1600x900\n")
        (self.run_dir / "endurance.txt").write_text(
            "duration_s=60.010\nframes=3600\nloops=1\nwalk_metres=10.00\ndrive_metres=40.00\n"
        )
        self.series = self.run_dir / "endurance.csv"
        self.series.write_text("elapsed_s,minute,wall_ms\n" + "".join(
            f"{i / 60:.3f},0,16.667\n" for i in range(1, 3601)
        ))

    def test_accepts_a_full_minute_with_a_rounded_boundary(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            reporter.report(self.run_dir, 1)
        self.assertIn("Last full minute: 3,600 frames", output.getvalue())

    def test_reports_focus_only_for_recordings_that_capture_it(self):
        lines = self.series.read_text().splitlines()
        lines = [lines[0] + ",focused"] + [row + (",1" if i % 2 else ",0") for i, row in enumerate(lines[1:])]
        self.series.write_text("\n".join(lines) + "\n")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            reporter.report(self.run_dir, 1)
        self.assertIn("Game focused: 1,800 frames", output.getvalue())
        self.assertIn("Game in background: 1,800 frames", output.getvalue())

    def test_rejects_a_short_run_for_a_longer_claim(self):
        with self.assertRaisesRegex(ValueError, "requested duration"):
            reporter.report(self.run_dir, 20)

    def test_rejects_a_missing_middle_frame(self):
        lines = self.series.read_text().splitlines()
        self.series.write_text("\n".join(lines[:100] + lines[101:]) + "\n")
        with self.assertRaisesRegex(ValueError, "gap"):
            reporter.report(self.run_dir, 1)
