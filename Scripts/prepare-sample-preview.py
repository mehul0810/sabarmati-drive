#!/usr/bin/env python3
"""Create an isolated Unreal project for the Game Animation Sample preview."""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SAMPLE_SECTIONS = {
    "ConsoleVariables",
    "/Script/Engine.DataDrivenConsoleVariableSettings",
    "/Script/Engine.PhysicsSettings",
    "/Script/Engine.CollisionProfile",
    "/Script/NavigationSystem.NavigationSystemV1",
    "/Script/PoseSearch.PoseSearchSettings",
}


def copy_sample_content(source, destination):
    # APFS clones keep this local preview quick to create and independently editable.
    try:
        subprocess.run(["/bin/cp", "-cR", str(source), str(destination)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError:
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(source, destination)


def sample_engine_settings(path):
    selected = []
    found = set()
    include = False
    for line in path.read_text().splitlines(keepends=True):
        if line.startswith("["):
            section = line.strip()[1:-1]
            include = section in SAMPLE_SECTIONS
            if include:
                found.add(section)
        if include:
            selected.append(line)
    missing = SAMPLE_SECTIONS - found
    if missing:
        raise ValueError("The sample is missing engine settings: " + ", ".join(sorted(missing)))
    return "".join(selected)


def validate_sample(sample):
    sample_project = sample / "GameAnimationSample.uproject"
    required = (
        sample_project,
        sample / "Content/Blueprints/GM_Sandbox.uasset",
        sample / "Content/MetaHumans/Kellan/BP_Kellan.uasset",
        sample / "Config/DefaultEngine.ini",
        sample / "Config/DefaultInput.ini",
        sample / "Config/DefaultGameplayTags.ini",
        sample / "Config/DefaultNetworkPrediction.ini",
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise ValueError("The sample project is incomplete: " + ", ".join(missing))

    game_file = PROJECT_ROOT / "SabarmatiPrototype.uproject"
    game = json.loads(game_file.read_text())
    animation_sample = json.loads(sample_project.read_text())
    if game.get("EngineAssociation") != animation_sample.get("EngineAssociation"):
        raise ValueError("The game and sample use different Unreal Engine versions.")
    collisions = [source.name for source in (sample / "Content").iterdir()
                  if (PROJECT_ROOT / "Content" / source.name).exists()]
    if collisions:
        raise ValueError("The game and sample have overlapping content folders: " + ", ".join(collisions))
    settings = sample_engine_settings(sample / "Config/DefaultEngine.ini")
    return game, animation_sample, settings


def prepare(sample, destination, game, animation_sample, settings):
    game_file = PROJECT_ROOT / "SabarmatiPrototype.uproject"

    for folder in ("Build", "Config", "Source"):
        source = PROJECT_ROOT / folder
        if source.exists():
            shutil.copytree(source, destination / folder, symlinks=False)
    shutil.copy2(game_file, destination / game_file.name)

    content = destination / "Content"
    content.mkdir()
    subprocess.run(["/usr/bin/rsync", "-a", "--copy-links", "--exclude=/Sabarmati/Avatar/",
                    str(PROJECT_ROOT / "Content") + "/", str(content) + "/"], check=True)
    for source in (sample / "Content").iterdir():
        target = content / source.name
        if target.exists():
            raise ValueError(f"The game and sample both contain Content/{source.name}.")
        if source.is_dir():
            copy_sample_content(source, target)
        else:
            shutil.copy2(source, target)

    plugins = {plugin["Name"]: plugin for plugin in game.get("Plugins", [])}
    for plugin in animation_sample.get("Plugins", []):
        if plugin["Name"] in plugins:
            if plugin.get("Enabled"):
                plugins[plugin["Name"]]["Enabled"] = True
        else:
            game.setdefault("Plugins", []).append(plugin)
    (destination / game_file.name).write_text(json.dumps(game, indent=2) + "\n")

    config = destination / "Config"
    for name in ("DefaultInput.ini", "DefaultGameplayTags.ini", "DefaultNetworkPrediction.ini"):
        shutil.copy2(sample / "Config" / name, config / name)
    with (config / "DefaultEngine.ini").open("a") as output:
        output.write("\n" + settings)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sample", type=Path, help="Path to the installed GameAnimationSample project")
    parser.add_argument("--destination", type=Path, help="A new directory for the preview project")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This preview setup requires macOS.")

    sample = args.sample.expanduser().resolve()
    try:
        game, animation_sample, settings = validate_sample(sample)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.exit(1, f"Could not read the sample: {error}\n")

    if args.destination:
        destination = args.destination.expanduser().resolve()
        if destination.exists() or destination == PROJECT_ROOT or destination == sample:
            parser.error("Choose a new preview directory.")
        if PROJECT_ROOT in destination.parents or sample in destination.parents:
            parser.error("Choose a directory outside both source projects.")
        destination.mkdir(parents=True)
    else:
        destination = Path(tempfile.mkdtemp(prefix="sabarmati-sample-preview-"))

    try:
        prepare(sample, destination, game, animation_sample, settings)
    except (OSError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Could not prepare preview in {destination}: {error}\n")
    print(destination / "SabarmatiPrototype.uproject")


if __name__ == "__main__":
    main()
