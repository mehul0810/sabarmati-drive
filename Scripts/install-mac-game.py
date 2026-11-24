"""Install one verified Sabarmati Drive app in the current user's Applications folder."""
import ctypes
import fcntl
import os
import plistlib
import subprocess
import sys
import tempfile
from pathlib import Path


def check_identity(bundle):
    if bundle.is_symlink() or not bundle.is_dir():
        raise ValueError(f"Expected a game bundle at {bundle}")
    with (bundle / "Contents/Info.plist").open("rb") as resource:
        info = plistlib.load(resource)
    if (info.get("CFBundleIdentifier") != "com.YourCompany.SabarmatiPrototype"
            or info.get("CFBundleExecutable") != "SabarmatiPrototype"):
        raise ValueError(f"Unrecognized game bundle at {bundle}")


def publish(staged, destination):
    # A directory lock also serializes installs from different checkouts.
    parent = os.open(destination.parent, os.O_RDONLY)
    try:
        fcntl.flock(parent, fcntl.LOCK_EX)
        if not os.path.lexists(destination):
            os.rename(staged, destination)
            return
        check_identity(destination)
        # macOS swaps two directories on the same volume in one operation. The
        # previous app is then in our private staging directory for cleanup.
        swap = ctypes.CDLL("/usr/lib/libSystem.B.dylib", use_errno=True).renamex_np
        swap.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint)
        swap.restype = ctypes.c_int
        rename_swap = 0x00000002  # RENAME_SWAP, <stdio.h>
        if swap(os.fsencode(staged), os.fsencode(destination), rename_swap):
            number = ctypes.get_errno()
            raise OSError(number, os.strerror(number), str(destination))
    finally:
        os.close(parent)


def install(source, destination):
    check_identity(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix=".sabarmati-update-", suffix=".noindex") as folder:
        staged = Path(folder) / "Sabarmati Drive.app"
        subprocess.run(["/usr/bin/ditto", "--norsrc", "--noextattr", str(source), str(staged)], check=True)
        subprocess.run(["/usr/bin/codesign", "--verify", "--strict", str(staged)], check=True)
        paks = staged / "Contents/UE/SabarmatiPrototype/Content/Paks"
        if not all(any(paks.glob(f"*.{extension}")) for extension in ("utoc", "ucas", "pak")):
            raise ValueError(f"Missing cooked game content at {paks}")
        publish(staged, destination)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 Scripts/install-mac-game.py COOKED_GAME.app")
    game = Path.home() / "Applications/Sabarmati Drive.app"
    install(Path(sys.argv[1]), game)
    print(f"Local Development app: {game}")
