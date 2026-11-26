"""Bake the two playable upper-body foundations using the shipped face and physique fits."""
import argparse
from pathlib import Path
import runpy
from tempfile import TemporaryDirectory

SCRIPTS = Path(__file__).resolve().parent
ASSETS = SCRIPTS.parent / "SourceAssets"


def tools(name):
    return runpy.run_path(str(SCRIPTS / name))


def bake(base, system, output):
    output.mkdir(parents=True, exist_ok=True)
    physique = tools("fit-resident-physique.py")
    athlete = physique["AthleticBody"](base, ASSETS/"universal-male-young-maxmuscle-averageweight.target")
    female = tools("fit-resident-female.py")["ResidentFit"](base, ASSETS/"river-resident-female.target")
    with TemporaryDirectory() as temp:
        work = Path(temp)
        reference = work/"reference.glb"
        tools("make-district-residents.py")["make"](base,reference,False)
        for preset in ("male","female"):
            name = "SK_"+preset.title()+"Foundation.glb"
            tools("make-everyday-clothes.py")["build"](base, reference, system, work, preset)
            sculpt = work/"sculpt.glb"
            tools("sculpt-resident-face.py")["apply"](base, ASSETS/"river-resident-adult.target", work/name, sculpt)
            physique["fit"](sculpt, output/name, athlete)
            if preset == "female":
                physique["fit"](output/name, output/name, female, fixed_materials=("hair","iris","sclera","shoe"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("base","system","output"):
        parser.add_argument(key,type=Path)
    args = parser.parse_args()
    bake(args.base,args.system,args.output)
