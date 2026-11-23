import importlib.util
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "Scripts/prepare-mac-profile.py"
SPEC = importlib.util.spec_from_file_location("mac_profile", SCRIPT)
profiles = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(profiles)


class MacOfflineProfileTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        self.original = root / "checkout.sav"
        self.active = root / "Application Support" / "SaveGames" / "player.sav"

    def test_racing_first_launches_seed_one_complete_profile(self):
        data = bytes(range(256)) * 10
        self.original.write_bytes(data)
        with ThreadPoolExecutor(max_workers=4) as workers:
            list(workers.map(lambda _: profiles.seed(self.original, self.active), range(8)))
        self.assertEqual(self.active.read_bytes(), data)
        self.assertEqual(self.original.read_bytes(), data)
        self.assertEqual(list(self.active.parent.iterdir()), [self.active])

    def test_existing_progress_wins_on_later_launches(self):
        self.original.write_bytes(b"checkout progress")
        profiles.seed(self.original, self.active)
        self.active.write_bytes(b"later normal game progress")
        profiles.seed(self.original, self.active)
        self.assertEqual(self.active.read_bytes(), b"later normal game progress")
        self.assertEqual(self.original.read_bytes(), b"checkout progress")

    def test_new_player_can_create_a_profile_in_game(self):
        profiles.seed(self.original, self.active)
        self.assertTrue(self.active.parent.is_dir())
        self.assertFalse(self.active.exists())
