"""Fit the CC0 MakeHuman Western patterns to the two resident rigs."""
import numpy as np
from PIL import Image
from resident_anatomy import boundary_edges


UPPER_PATTERNS = {"FineKnit": "male_casualsuit02", "Quilted": "male_casualsuit05", "Blazer": "male_elegantsuit01"}
FABRICS = {
    "FineKnit": (UPPER_PATTERNS["FineKnit"], True),
    "Quilted": (UPPER_PATTERNS["Quilted"], False),
    "Blazer": (UPPER_PATTERNS["Blazer"], False),
    "ShortDenim": ("female_casualsuit02", False),
    "CourtRunner": ("shoes05", False),
    "ZipBoot": ("shoes03", False),
}


def western_panels(style, system, body, read_obj, fit, components):
    def pattern(source, select):
        stem = system / "clothes" / source / source
        original, texcoords, faces = read_obj(stem.with_suffix(".obj"))
        vertices, _ = fit(stem.with_suffix(".mhclo"), body)
        if len(original) != len(vertices):
            raise ValueError(f"{source} pattern and bindings have different vertex counts")
        parts = components(len(vertices), faces)
        selected = {part for part in set(parts) if select(vertices[parts == part, 1])}
        panels = [face for _, face in faces if parts[face[0][0]] in selected]
        if not panels:
            raise ValueError(f"No matching pattern panels for {source}")
        return vertices, texcoords, panels

    if style in UPPER_PATTERNS:
        vertices, texcoords, faces = pattern(UPPER_PATTERNS[style], lambda y: y.min() > -1)
        if style in ("Blazer", "Quilted"):
            # Keep the side vents clear of the hands in the resident's relaxed stance.
            hem = np.clip((2.3-vertices[:, 1])/2., 0, 1)
            hem = hem * hem * (3-2*hem)
            if style == "Blazer":
                torso = np.abs(vertices[:, 0]) < 2.32
                vertices[:, 1] += .78*hem*torso
                vertices[:, 0] *= 1+.045*hem*torso
                vertices[:, 2] = .20+(vertices[:, 2]-.20)*(1+.055*hem*torso)
            else:
                vertices[:, 0] *= 1-.055*hem
        yield "western_top", vertices, texcoords, faces
    elif style == "LowerBody":
        yield "tailored_trousers", *pattern(UPPER_PATTERNS["Blazer"], lambda y: y.min() < -5)
        vertices, texcoords, faces = pattern(FABRICS["ShortDenim"][0], lambda y: -2 < y.min() and y.max() < 2)
        # Level the two original turned-up cuffs at mid-thigh. The crotch and
        # pocket fabric taper into the change so they can still flex at the hips.
        rim = sorted({i for edge in boundary_edges(faces) for i in edge if vertices[i, 1]<-.26})
        if not rim:
            raise ValueError("Missing the open denim cuffs")
        across = (0, 2)
        nearest = np.linalg.norm(vertices[:, across][:, None, :] - vertices[rim][:, across], axis=2).argmin(axis=1)
        hem = np.clip((.57-vertices[:, 1])/.93, 0, 1)
        hem = hem * hem * (3-2*hem)
        vertices[:, 1] += (-1.59-vertices[rim, 1][nearest])*hem
        vertices[:, 0] += np.sign(vertices[:, 0])*.10*hem
        vertices[:, 2] = .28 + (vertices[:, 2]-.28)*(1+.035*hem)
        yield "denim_shorts", vertices, texcoords, faces
        for slot, material in (("court_shoe", "CourtRunner"), ("zip_boot", "ZipBoot")):
            vertices, texcoords, faces = pattern(FABRICS[material][0], lambda y: y.max() < -7)
            for side, sign in (("l", 1), ("r", -1)):
                yield f"{slot}_{side}", vertices, texcoords, [f for f in faces if vertices[f[0][0], 0] * sign > 0]


def write_fabrics(system, output):
    for style, (source, unprinted) in FABRICS.items():
        stem = system / "clothes" / source / source
        image = Image.open(stem.with_name(stem.name + ("_ao.png" if unprinted else "_diffuse.png")))
        if unprinted:
            image = image.convert("L").point(lambda value: round(126 + .45 * value))
        elif style in ("Blazer", "ShortDenim"):
            # Preserve the folds, seams and inset details while accepting fabric dyes.
            image = image.convert("L").point(lambda value: min(255, round(80 + .85 * value)))
        elif style == "CourtRunner":
            image = image.convert("L")
        image.thumbnail((1024, 1024), Image.Resampling.LANCZOS)
        image.convert("RGB").save(output / f"T_{style}.png")
        normal_path = stem.with_name(stem.name + "_normal.png")
        if normal_path.exists():
            normal = Image.open(normal_path).convert("RGB")
            normal.thumbnail((1024, 1024), Image.Resampling.LANCZOS)
            red, green, blue = normal.split()
            Image.merge("RGB", (red, green.point(lambda value: 255 - value), blue)).save(output / f"T_{style}_N.png")
