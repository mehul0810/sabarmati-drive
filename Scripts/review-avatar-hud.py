"""Exercise the installed Development game and retain real rendered evidence in an isolated directory."""
import argparse
import csv
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import uuid


PROJECT = Path(__file__).resolve().parents[1]
LEGACY = PROJECT / "Tests/Fixtures/riverfront-profile-before-avatar-selection.sav"
DEFAULT_GAME = Path.home() / "Applications/Grand City.app/Contents/MacOS/SabarmatiPrototype"


def png_size(picture):
    with picture.open("rb") as source:
        header = source.read(24)
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"No readable PNG at {picture}")
    return struct.unpack(">II", header[16:24])


def play(game, home, name, flags, success, size):
    home.mkdir(parents=True, exist_ok=True)
    log = home / (name + ".log")
    command = [str(game), "-windowed", "-metalsm5", "-RenderOffscreen", "-nosplash",
               "-nosound", "-unattended", "-SabarmatiTime=day", f"-UserDir={home}",
               f"-abslog={log}", f"-ResX={size[0]}", f"-ResY={size[1]}", *flags]
    print(f"Running {name}. Log {log}", flush=True)
    with (home / (name + "-console.log")).open("wb") as console:
        status = subprocess.run(command, stdout=console, stderr=subprocess.STDOUT, timeout=480)
    # Generic Unreal RequestExitWithStatus can exit with 0 even on a failed check.
    if status.returncode != 0 or not log.is_file() or success not in log.read_text(errors="replace"):
        raise RuntimeError(f"{name} failed or did not finish its assertions. See {log}")


def check_images(folder, names, size):
    for name in names:
        path = folder / (name + ".png")
        if png_size(path) != tuple(size):
            raise ValueError(f"Unexpected drawable dimensions at {path}")


def morning(game, home, size, legacy=False):
    slot = uuid.uuid4().hex.upper()
    flags = ["-SabarmatiMorningReview", "-SabarmatiMorningQAId=" + slot]
    folder = home / "Saved/MorningReviews" / slot
    if legacy:
        # Genuine pre-selection schema-2 save from a synthetic shop visit with a purchased shirt.
        slot_file = home / "Saved/SaveGames" / f"SabarmatiQA-{slot}.sav"
        slot_file.parent.mkdir(parents=True)
        shutil.copyfile(LEGACY, slot_file)
        flags.append("-SabarmatiLegacyAvatarReview")
    else:
        flags.append("-SabarmatiGenderReview")
        play(game, home, "first-morning", flags, "Sabarmati first morning complete:", size)
        check_images(folder, ["00-profile-selection", "01-apartment", "01-controls", "02-gender-male",
                              "02-gender-female", "03-apartment-street", "09-everyday-front",
                              "11-kurta-front", "07-family-walk", "07-pause"], size)
        results = (folder / "first-morning.txt").read_text(encoding="utf-8-sig")
        for key in ["created_female=1", "wardrobe_male_and_female=1", "map_and_controls_toggled=1"]:
            if key not in results.splitlines():
                raise ValueError(f"Expected {key} in {folder}/first-morning.txt")
    play(game, home, "continue", [*flags, "-SabarmatiMorningContinue"],
         "Sabarmati morning continue verified:", size)
    check_images(folder, ["08-continued-at-shop"], size)
    expected_mesh = "SK_RiverEveryday" if legacy else "SK_FemaleEveryday"
    report = (folder / "continued.txt").read_text(encoding="utf-8-sig").splitlines()
    for key in ["cold_process=1", "continued_at_shop=1", "cash=98750", "bank=500000",
                "equipped=shirt-indigo", "body_mesh=" + expected_mesh]:
        if key not in report:
            raise ValueError(f"Expected {key} in {folder}/continued.txt")


def gait(game, root, size):
    for outfit, expected in [("everyday", "SK_FemaleEveryday"), ("kurta", "SK_FemaleKurta")]:
        home = root / outfit
        flags = ["-SabarmatiGaitReview", "-SabarmatiGaitFemale"]
        if outfit == "kurta":
            flags.append("-SabarmatiGaitKurta")
        play(game, home, outfit, flags, "Sabarmati gait review finished: pass=1", size)
        log = (home / (outfit + ".log")).read_text(errors="replace")
        if not any(expected in line for line in log.splitlines() if "gait review started: mesh=" in line):
            raise ValueError("Gait test loaded the wrong outfit")
        csv_path, = (home / "Saved/GaitReviews").glob("*/summary.csv")
        with csv_path.open() as stream:
            rows = list(csv.DictReader(stream))
        if {(r["mode"], r["side"]) for r in rows if r["pass"] == "1"} != {
                ("walk", "left"), ("walk", "right"), ("run", "left"), ("run", "right")}:
            raise ValueError(f"Incomplete gait measurements at {csv_path}")
        check_images(csv_path.parent, ["walk-profile", "run-profile"], size)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--output", type=Path, default=Path(tempfile.gettempdir()),
                        help="Parent for a new unique review folder")
    parser.add_argument("--size", type=int, nargs=2, metavar=("WIDTH", "HEIGHT"), default=[1600, 900])
    parser.add_argument("--cases", nargs="+", choices=["morning", "legacy", "gait", "driving"],
                        default=["morning", "legacy", "gait", "driving"])
    args = parser.parse_args()
    if not args.game.is_file() or min(args.size) <= 0:
        parser.error("An installed game executable and a positive viewport size are required.")
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="sabarmati-avatar-hud-", dir=args.output.resolve()))
    print(f"Evidence directory {root}", flush=True)
    for case in args.cases:
        if case in ("morning", "legacy"):
            morning(args.game, root / case, args.size, legacy=case == "legacy")
        elif case == "gait":
            gait(args.game, root / case, args.size)
        else:
            home = root / case
            play(args.game, home, case, ["-SabarmatiSystemsReview"], "Sabarmati systems review complete:", args.size)
            report, = (home / "Saved/SystemsReviews").glob("*/systems.txt")
            check_images(report.parent, ["door", "cockpit", "driving-instruments", "tea-visit"], args.size)
    print(f"Passed. Evidence directory {root}", flush=True)


if __name__ == "__main__":
    main()
