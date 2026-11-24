"""Fit the second fictional adult resident to both finished, rigged outfit GLBs."""
import argparse
import hashlib
from pathlib import Path
import runpy

import numpy as np

SCRIPTS = Path(__file__).resolve().parent
physique = runpy.run_path(str(SCRIPTS / "fit-resident-physique.py"))
skin = runpy.run_path(str(SCRIPTS / "prepare-hero-skin.py"))
smooth = physique["smooth"]

# Original MakeHuman hm08 CC0 macrodetails, tracked by immutable Git blob.
SOURCES = {
    "african": ("dd5743e48700267d76596f575bf17b4b5cc3b3e0", "a7d2b130667cc81ecd1ca0598da9be27bec1864a"),
    "asian": ("8cd3ef3e9ddb06ee2d166f75f9ac10871251f938", "f862c96add11c31ab1ca75c69e1c67ae5e0e3d5f"),
    "caucasian": ("c3b82f92c5ced85599199cd184b0faf3b3fc6881", "9d1f0cbeedc9a6a51abe33f1ebb5fa7c5a7edbf1"),
}


def write_target(base, upstream, out):
    original, _, triangles = skin["mesh"](base, "body")
    body_ids = np.unique(triangles[:, :, 0])
    delta = np.zeros_like(original)
    for family, blobs in SOURCES.items():
        for gender, sign, digest in zip(("male", "female"), (-1, 1), blobs):
            path = upstream / f"{family}-{gender}-young.target"
            data = path.read_bytes()
            header = f"blob {len(data)}\0".encode()
            if hashlib.sha1(header + data).hexdigest() != digest:
                raise ValueError(f"Unknown upstream target revision {path}")
            target = np.loadtxt(path)
            delta[target[:, 0].astype(int)] += sign * target[:, 1:4] / len(SOURCES)
    # Keep the paired differences. The art fit removes stature and rest-pose
    # offsets before applying this target to an existing animation skeleton.
    with out.open("w") as handle:
        handle.write("# Paired adult hm08 resident target. CC0. See SourceAssets/RESIDENT-FEMALE.md.\n")
        for index in body_ids:
            if np.any(delta[index]):
                handle.write(f"{index} " + " ".join(f"{v:.5f}" for v in delta[index]) + "\n")


class ResidentFit(physique["SurfaceFit"]):
    """Transfer the authored shape while retaining eye seats, joints and gear sockets."""

    def __init__(self, base, target):
        skin["verify"](base, skin["BASE_HASH"])
        vertices, _, faces = skin["mesh"](base, "body")
        indices = np.unique(faces[:, :, 0])
        vertices = (vertices + [0, 8.17, 0]) * .102
        rows = np.loadtxt(target)
        delta = np.zeros_like(vertices)
        delta[rows[:, 0].astype(int)] = rows[:, 1:4] * .102
        # The original male and female macros include 13 cm of stature
        # difference. Remove its vertical gradient to retain the shared rig.
        delta[:, 1] += .085 * vertices[:, 1]
        self.body = physique["SkinField"](vertices[indices], delta[indices])
        head = indices[(vertices[indices, 1] > 1.40) & (np.abs(vertices[indices, 0]) < .15)]
        self.head = physique["SkinField"](vertices[head], delta[head], step=.004)

    def offsets(self, points):
        x, y, z = points.T
        side = np.abs(x)
        d = self.body.sample(points) * [.78, .20, .52]
        # Restore the centred pelvic and arm posture of the bind pose. Ease off
        # towards the wrists, knees and feet, where accessories and shoes meet.
        d[:, 2] -= .010 * (1 - smooth(.92, 1.14, y))
        d[:, 0] -= .006 * np.sign(x) * np.exp(-((side - .207) / .08)**2 - ((y - 1.34) / .10)**2)
        hip = np.exp(-((y - .866) / .103)**2)
        d[:, 0] += .012 * np.sign(x) * hip * smooth(.052, .14, side)
        waist = np.exp(-((y - 1.056) / .094)**2)
        d[:, 0] -= .007 * np.sign(x) * waist * smooth(.058, .13, side)
        # Shape cloth across the upper torso as one continuous surface. The
        # separation stays subtle at the centre of the buttoned woven shirt.
        front = smooth(.035, .110, z)
        chest = np.exp(-((side - .078) / .083)**2 - ((y - 1.249) / .075)**2)
        d[:, 2] += .043 * front * chest
        guard = smooth(.64, .835, y) * (1 - smooth(1.405, 1.455, y)) * (1 - smooth(.30, .406, side))
        d *= guard[:, None]

        # The separate head mesh, hair cap and eyeballs attach to the original
        # head joint. Preserve the scalp, nape and complete eyelid/iris seat.
        facial = (smooth(1.418, 1.487, y) * (1 - smooth(1.592, 1.625, y))
                  * smooth(.020, .085, z) * (1 - smooth(.083, .103, side)))
        orbit = np.exp(-((side - .036) / .025)**4 - ((y - 1.576) / .017)**4)
        facial *= 1 - orbit
        active = facial > 0
        if np.any(active):
            ax, ay = side[active], y[active]
            head = self.head.sample(points[active]) * [.68, .42, .42]
            # Preserve the original eyes' depth, then sculpt the jaw taper and
            # cheek transition of this individual, including the visible lips.
            head[:, 2] += .003
            lower = np.exp(-((ay - 1.490) / .037)**2)
            head[:, 0] -= .0055 * np.sign(x[active]) * smooth(.022, .060, ax) * lower
            cheek = np.exp(-((ay - 1.548) / .022)**2 - ((ax - .052) / .025)**2)
            head[:, 2] += .0038 * cheek
            d[active] += facial[active, None] * head
        return d


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_obj", type=Path)
    parser.add_argument("--build-target", type=Path, metavar="UPSTREAM_DIRECTORY")
    parser.add_argument("--target", type=Path, default=SCRIPTS.parent / "SourceAssets/river-resident-female.target")
    parser.add_argument("--outfit", nargs=2, type=Path, action="append", metavar=("INPUT_GLB", "OUTPUT_GLB"))
    args = parser.parse_args()
    if not args.outfit and not args.build_target:
        parser.error("Supply --outfit or --build-target.")
    if args.build_target:
        write_target(args.base_obj, args.build_target, args.target)
    if args.outfit:
        fit = ResidentFit(args.base_obj, args.target)
        refine_sleeves = runpy.run_path(str(SCRIPTS / "fit-female-kurta-sleeves.py"))["refine"]
        for source, destination in args.outfit:
            physique["fit"](source, destination, fit, fixed_materials=("hair","iris","sclera","shoe"))
            refine_sleeves(destination)


if __name__ == "__main__":
    main()
