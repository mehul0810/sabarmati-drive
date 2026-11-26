"""Bake the two playable upper-body foundations using the shipped face and physique fits."""
import argparse
from pathlib import Path
import runpy
from tempfile import TemporaryDirectory

SCRIPTS = Path(__file__).resolve().parent
ASSETS = SCRIPTS.parent / "SourceAssets"


def tools(name):
    return runpy.run_path(str(SCRIPTS / name))


def bake(base, system, output, lower=False):
    output.mkdir(parents=True, exist_ok=True)
    physique = tools("fit-resident-physique.py")
    athlete = physique["AthleticBody"](base, ASSETS/"universal-male-young-maxmuscle-averageweight.target")
    female = tools("fit-resident-female.py")["ResidentFit"](base, ASSETS/"river-resident-female.target")
    with TemporaryDirectory() as temp:
        work = Path(temp)
        reference = work/"reference.glb"
        tools("make-district-residents.py")["make"](base,reference,False)
        for preset in ("male","female"):
            name = "SK_"+preset.title()+("LowerBody.glb" if lower else "Foundation.glb")
            tools("make-everyday-clothes.py")["build"](base, reference, system, work, preset, lower_only=lower)
            sculpt = work/"sculpt.glb"
            if lower:
                sculpt = work/name
            else:
                tools("sculpt-resident-face.py")["apply"](base, ASSETS/"river-resident-adult.target", work/name, sculpt)
            fixed=("hair","iris","sclera","shoe","street_shoe_l","street_shoe_r")
            physique["fit"](sculpt, output/name, athlete, fixed_materials=fixed+("lips",))
            if preset == "female":
                physique["fit"](output/name, output/name, female, fixed_materials=fixed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("base","system","output"):
        parser.add_argument(key,type=Path)
    parser.add_argument("--lower",action="store_true",help="Bake independent legs and fitted boxer briefs")
    args = parser.parse_args()
    bake(args.base,args.system,args.output,args.lower)
