"""Bake the two playable upper-body foundations using the shipped face and physique fits."""
import argparse
from pathlib import Path
from shutil import copyfile
import runpy
from tempfile import TemporaryDirectory

SCRIPTS = Path(__file__).resolve().parent
ASSETS = SCRIPTS.parent / "SourceAssets"


def tools(name):
    return runpy.run_path(str(SCRIPTS / name))


def bake(base, system, output, lower=False, tee=False, western=False, anatomy=None):
    output.mkdir(parents=True, exist_ok=True)
    physique = tools("fit-resident-physique.py")
    athlete = physique["AthleticBody"](base, ASSETS/"universal-male-young-maxmuscle-averageweight.target")
    female = tools("fit-resident-female.py")["ResidentFit"](base, ASSETS/"river-resident-female.target")
    with TemporaryDirectory() as temp:
        work = Path(temp)
        reference = work/"reference.glb"
        tools("make-district-residents.py")["make"](base,reference,False)
        styles = ("LowerBody", "Foundation", "TankFoundation", "FineKnit", "Quilted", "Blazer") if western else (
            "LowerBody" if lower else "CrewTee" if tee else "Foundation",
        )
        build = tools("make-everyday-clothes.py")["build"]
        face = tools("sculpt-resident-face.py")["apply"]
        for style in styles:
            for preset in ("male","female"):
                name = f"SK_{preset.title()}{style}.glb"
                build(base, reference, system, work, preset, lower_only=style=="LowerBody", tee=tee,
                      western=style if western else None, anatomy=anatomy)
                sculpt = work/name
                if style != "LowerBody":
                    sculpt = work/"sculpt.glb"
                    face(base, ASSETS/"river-resident-adult.target", work/name, sculpt)
                fixed = ("hair","iris","sclera","shoe","street_shoe_l","street_shoe_r",
                         "court_shoe_l","court_shoe_r","zip_boot_l","zip_boot_r")
                physique["fit"](sculpt, output/name, athlete, fixed_materials=fixed+("lips",))
                if tee and preset=="male":
                    for texture in ("T_JerseyKnit.png","T_JerseyKnit_N.png"):
                        copyfile(work/texture,output/texture)
                if preset == "female":
                    physique["fit"](output/name, output/name, female, fixed_materials=fixed)
        if western:
            from resident_western import write_fabrics
            write_fabrics(system, output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("base","system","output"):
        parser.add_argument(key,type=Path)
    kinds=parser.add_mutually_exclusive_group()
    kinds.add_argument("--tee",action="store_true",help="Bake a short-sleeved crew neck on each rig")
    kinds.add_argument("--lower",action="store_true",help="Bake independent legs and fitted boxer briefs")
    kinds.add_argument("--western",action="store_true",help="Bake Western patterns and revised body layers")
    parser.add_argument("--anatomy",type=Path,help="Directory with the licensed adult anatomy OBJ and fitting sources")
    args = parser.parse_args()
    bake(args.base,args.system,args.output,args.lower,args.tee,args.western,args.anatomy)
