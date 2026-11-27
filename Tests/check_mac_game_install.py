import importlib.util
import plistlib
import sys
import tempfile
import unittest
from pathlib import Path


INSTALLER = Path(__file__).resolve().parents[1] / "Scripts/install-mac-game.py"
SPEC = importlib.util.spec_from_file_location("mac_game_installer", INSTALLER)
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


class MacGameInstallTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.applications = self.root / "Applications"
        self.applications.mkdir()
        self.destination = self.applications / "Grand City.app"

    def staged(self, version, identifier="com.YourCompany.SabarmatiPrototype"):
        package = self.root / version
        contents = package / "Contents"
        contents.mkdir(parents=True)
        with (contents / "Info.plist").open("wb") as resource:
            plistlib.dump({"CFBundleIdentifier": identifier, "CFBundleExecutable": "SabarmatiPrototype",
                           "CFBundleVersion": version}, resource)
        return package

    def version(self, bundle):
        with (bundle / "Contents/Info.plist").open("rb") as resource:
            return plistlib.load(resource)["CFBundleVersion"]

    @unittest.skipUnless(sys.platform == "darwin", "Uses the macOS atomic directory exchange")
    def test_update_exchanges_the_game_at_the_same_path(self):
        installer.publish(self.staged("1"), self.destination)
        new = self.staged("2")
        installer.publish(new, self.destination)
        self.assertEqual(list(self.applications.iterdir()), [self.destination])
        self.assertEqual(self.version(self.destination), "2")
        self.assertEqual(self.version(new), "1")

    def test_unrelated_application_with_same_name_is_preserved(self):
        self.staged("other", "com.example.Other").rename(self.destination)
        update = self.staged("2")
        with self.assertRaises(ValueError):
            installer.publish(update, self.destination)
        self.assertEqual(self.version(self.destination), "other")
        self.assertEqual(self.version(update), "2")

    @unittest.skipUnless(sys.platform == "darwin", "Uses the macOS atomic directory exchange")
    def test_rename_replaces_the_legacy_install(self):
        legacy = self.applications / "Sabarmati Drive.app"
        self.staged("1").rename(legacy)
        installer.publish(self.staged("2"), self.destination, legacy.name)
        self.assertEqual(list(self.applications.iterdir()), [self.destination])
        self.assertEqual(self.version(self.destination), "2")
