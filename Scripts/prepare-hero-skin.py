"""Bake the fictional resident's hm08 skin atlas. Requires Pillow and NumPy."""
import argparse
from hashlib import sha256
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


LIGHT = 'skins/young_asian_male/young_lightskinned_male_diffuse3.png'
DARK = 'skins/young_african_male/young_darkskinned_male_diffuse.png'
BROW = 'eyebrows/eyebrow009/eyebrow009'
SOURCE_HASHES = {
    LIGHT: 'f50016a5507fc687dc8df06599c8ea48de950cd185de33a71cafc1319ddab4d5',
    DARK: 'e3f0dbb634e4c68561338f0087f7a122444e84ec56df102212f1222aa12e4d50',
    BROW + '.png': '4d33c85718ca5accf44625652daf0ae6e8cc007ed8138280e4d95601717910c3',
    BROW + '.obj': '037a2edb3252e1fe4a57e63dcb1598916609fd0339f2b453b2f71e9c4cbd481c',
}
BASE_HASH = '8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c'


def verify(path, expected):
    if sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError(f'Unexpected upstream asset revision: {path}')


def mesh(path, group=None):
    positions, uvs, triangles = [], [], []
    name = None
    for line in path.read_text().splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == 'v':
            positions.append(tuple(map(float, fields[1:4])))
        elif fields[0] == 'vt':
            u, v = map(float, fields[1:3])
            uvs.append((u, 1-v))
        elif fields[0] == 'g':
            name = fields[1]
        elif fields[0] == 'f' and (group is None or name == group):
            face = [tuple(int(i)-1 for i in point.split('/')[:2]) for point in fields[1:]]
            triangles.extend((face[0], face[i], face[i+1]) for i in range(1, len(face)-1))
    return np.asarray(positions), np.asarray(uvs), np.asarray(triangles)


def pixels(triangle, width, height):
    """Rasterize pixel centers, returning rows, columns and barycentric weights."""
    left, top = np.maximum(np.floor(triangle.min(axis=0)).astype(int), 0)
    right, bottom = np.minimum(np.ceil(triangle.max(axis=0)).astype(int), (width-1, height-1))
    if right < left or bottom < top:
        return None
    a, b = triangle[1:]-triangle[0]
    area = a[0]*b[1]-a[1]*b[0]
    if abs(area) < 1e-8:
        return None
    y, x = np.mgrid[top:bottom+1, left:right+1]
    dx, dy = x+.5-triangle[0, 0], y+.5-triangle[0, 1]
    wb, wc = (dx*b[1]-dy*b[0])/area, (dy*a[0]-dx*a[1])/area
    wa = 1-wb-wc
    mask = (wa >= 0) & (wb >= 0) & (wc >= 0)
    return y[mask], x[mask], np.stack((wa[mask], wb[mask], wc[mask]), axis=1)


def bilinear(image, x, y):
    height, width = image.shape[:2]
    x = np.clip(x-.5, 0, width-1)
    y = np.clip(y-.5, 0, height-1)
    x0, y0 = x.astype(int), y.astype(int)
    x1, y1 = np.minimum(x0+1, width-1), np.minimum(y0+1, height-1)
    a, b = (x-x0)[:, None], (y-y0)[:, None]
    return (1-b)*((1-a)*image[y0, x0] + a*image[y0, x1]) + b*((1-a)*image[y1, x0] + a*image[y1, x1])


def brow_card(system):
    # Orthographic projector in native hm08 space. The artwork follows the
    # accessory artist's hand-fit left and right cards, with their separate UVs.
    positions, uvs, triangles = mesh(system/(BROW+'.obj'))
    # Filter premultiplied hair coverage before projection into the lower-
    # resolution head chart. Keep deep brown strands and softer edges in motion.
    brow = Image.open(system/(BROW+'.png')).convert('RGBa').filter(ImageFilter.GaussianBlur(1.65))
    rgba = np.asarray(brow, dtype=np.float32)/255
    rgba[:, :, :3] = .88*rgba[:, :, :3] + .12*rgba[:, :, 3:4]*[.27, .18, .13]
    rgba *= .86
    # Set the fictional resident's resting brow roughly 11 mm lower than the
    # stock card on this particular neutral face.
    positions[:, 1] -= .11
    scale = 1024
    origin = positions.min(axis=0)[:2] - .01
    projected = (positions[:, :2]-origin)*scale
    width, height = np.ceil(projected.max(axis=0)+2).astype(int)
    card = np.zeros((height, width, 4), np.float32)
    for tri in triangles:
        covered = pixels(projected[tri[:, 0]], width, height)
        if covered is None:
            continue
        rows, cols, weights = covered
        st = weights@uvs[tri[:, 1]]
        samples = bilinear(rgba, st[:, 0]*rgba.shape[1], st[:, 1]*rgba.shape[0])
        card[rows, cols] = samples
    return card, origin, scale


def oval(x, y):
    return np.exp(-np.minimum(x*x+y*y, 64))


def face_detail(system, base, colour, roughness):
    card, brow_origin, scale = brow_card(system)
    height, width = roughness.shape
    positions, uvs, triangles = mesh(base, 'body')
    front = positions[triangles[:, :, 0]]
    on_face = (front[:, :, 1].max(axis=1) > 5.95) & (front[:, :, 2].max(axis=1) > .65)
    for tri in triangles[on_face]:
        covered = pixels(uvs[tri[:, 1]]*(width, height), width, height)
        if covered is None:
            continue
        rows, cols, weights = covered
        x, y, z = (weights@positions[tri[:, 0]]).T
        facing = np.clip((z-.65)/.35, 0, 1)
        if not len(x) or not facing.any():
            continue
        # Shaved shadow across the chin and short upper lip, excluding vermilion.
        chin = oval(x/.36, (y-6.29)/.15)
        jaw = oval((abs(x)-.42)/.23, (y-6.45)/.24)
        upper_lip = oval(x/.25, (y-6.76)/.060)
        shaved = np.minimum(1, chin+.63*jaw+.82*upper_lip)*facing
        rgb = colour[rows, cols].astype(np.float32)
        rgb -= shaved[:, None]*[9., 7., 5.]
        # Select the front facial surface only where the brow cards overlap it.
        sx, sy = (x-brow_origin[0])*scale, (y-brow_origin[1])*scale
        within = (sx >= 0) & (sy >= 0) & (sx < card.shape[1]) & (sy < card.shape[0]) & (z > .88)
        if within.any():
            stamp = bilinear(card, sx[within], sy[within])
            rgb[within] = rgb[within]*(1-stamp[:, 3:4]) + 255*stamp[:, :3]
        colour[rows, cols] = np.clip(np.rint(rgb), 0, 255).astype(np.uint8)
        nose = oval(x/.21, (y-7.05)/.30)
        forehead = oval(x/.38, (y-7.85)/.25)
        lips = oval(x/.28, (y-6.61)/.088)
        roughness[rows, cols] -= (.12*nose+.047*forehead+.14*lips)*facing


def quantize(array):
    return np.clip(np.rint(array), 0, 255).astype(np.uint8)


def bake(system, base, output):
    verify(base, BASE_HASH)
    for asset, digest in SOURCE_HASHES.items():
        verify(system/asset, digest)
    light = np.asarray(Image.open(system/LIGHT).convert('RGB'), dtype=np.float32)
    dark = np.asarray(Image.open(system/DARK).convert('RGB'), dtype=np.float32)
    colour = quantize(.57*light+.43*dark)
    del light, dark

    # Derive relief only from fine source variation, before painted hair and
    # beard shadow are applied. Add very shallow pore microstructure at full LOD.
    luma = Image.fromarray(colour).convert('L')
    fine = np.asarray(luma, np.float32)-np.asarray(luma.filter(ImageFilter.GaussianBlur(1.4)), np.float32)
    rng = np.random.default_rng(13209)
    pore = rng.normal(0, 1, fine.shape).astype(np.float32)
    relief = np.clip(fine/24, -.65, .65) + .062*pore
    dy, dx = np.gradient(relief)
    inv_length = 1/np.sqrt(dx*dx+dy*dy+1)
    normal = np.stack((-dx*inv_length, -dy*inv_length, inv_length), axis=-1)
    roughness = .565 + np.clip(fine*.004, -.044, .044) + .012*pore

    face_detail(system, base, colour, roughness)
    output.mkdir(parents=True, exist_ok=True)
    Image.fromarray(colour).save(output/'T_HeroSkin_Color.png')
    Image.fromarray(quantize(127.5*(normal+1))).save(output/'T_HeroSkin_Normal.png')
    rough = Image.fromarray(quantize(roughness*255))
    rough.resize((1024, 1024), Image.Resampling.BOX).save(output/'T_HeroSkin_Roughness.png')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('system_assets', type=Path, help='Unpacked MakeHuman CC0 system asset root')
    parser.add_argument('hm08_base', type=Path, help='The same MakeHuman base.obj used to build the resident')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    bake(args.system_assets, args.hm08_base, args.output)
