"""Bake the fictional resident's hm08 skin atlas. Requires Pillow and NumPy."""
import argparse
from hashlib import sha256
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


LIGHT = 'skins/young_asian_male/young_lightskinned_male_diffuse3.png'
DARK = 'skins/young_african_male/young_darkskinned_male_diffuse.png'
FEMALE_LIGHT = 'skins/young_asian_female/young_lightskinned_female_diffuse3.png'
FEMALE_DARK = 'skins/young_african_female/young_darkskinned_female_diffuse.png'
BROW = 'eyebrows/eyebrow009/eyebrow009'
SOURCE_HASHES = {
    LIGHT: 'f50016a5507fc687dc8df06599c8ea48de950cd185de33a71cafc1319ddab4d5',
    DARK: 'e3f0dbb634e4c68561338f0087f7a122444e84ec56df102212f1222aa12e4d50',
    FEMALE_LIGHT: '1d82f782b7d2da732229633bd2d14754d873fff53cdb1d088977bcf67ae19dde',
    FEMALE_DARK: '96aa4a96b247fc90d371587bb88e6ae3bfc77e83f5126f17e6bbb713aa200470',
    BROW + '.png': '4d33c85718ca5accf44625652daf0ae6e8cc007ed8138280e4d95601717910c3',
    BROW + '.obj': '037a2edb3252e1fe4a57e63dcb1598916609fd0339f2b453b2f71e9c4cbd481c',
}
BASE_HASH = '8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c'
TORSO_MASTER_SIZE = 16384
GAME_NORMAL_SIZE = 4096


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


def smoothstep(start, end, value):
    fraction = np.clip((value-start)/(end-start), 0, 1)
    return fraction*fraction*(3-2*fraction)


def torso_weight(x, y):
    return (smoothstep(.65, 1.35, y) * (1-smoothstep(5.65, 6.15, y))
            * (1-smoothstep(1.52, 2.05, np.abs(x))))


def skin_relief_tile(seed):
    """Author shallow pits with different sizes, orientations, and depths."""
    rng = np.random.default_rng(seed)
    cells, cell_size = 48, 16
    size = cells*cell_size
    grain = Image.fromarray(rng.normal(0, .03, (96, 96)).astype(np.float32))
    relief = np.array(grain.resize((size, size), Image.Resampling.BICUBIC))
    y, x = np.mgrid[-8:9, -8:9]
    for row, column in np.ndindex(cells, cells):
        center_y, center_x = (np.array([row, column])*cell_size
                              + rng.integers(2, cell_size-2, size=2))
        angle = rng.uniform(0, np.pi)
        across = np.cos(angle)*x + np.sin(angle)*y
        along = -np.sin(angle)*x + np.cos(angle)*y
        radius = rng.uniform(1.3, 3.2)
        distance = (across/radius)**2 + (along/(radius*rng.uniform(.58, .9)))**2
        relief[(center_y+y) % size, (center_x+x) % size] += (
            rng.uniform(.15, .48)*(.18*np.exp(-.42*distance)-np.exp(-distance)))
    return relief


def bake_torso_normal(positions_uv, base_normal, seed, master_path):
    positions, uv, triangles = positions_uv
    corners = positions[triangles[:, :, 0]]
    heights, widths = corners[:, :, 1], corners[:, :, 0]
    body_faces = ((heights.max(axis=1) > .65) & (heights.min(axis=1) < 6.15)
                  & (widths.max(axis=1) > -2.05) & (widths.min(axis=1) < 2.05))
    tile = skin_relief_tile(seed)
    tile_size = tile.shape[0]
    tile = np.pad(tile[:, :, None], ((1, 1), (1, 1), (0, 0)), mode='wrap')
    # Position-based projection keeps the pattern aligned across the UV seams.
    # The hm08 reference coordinates use decimetres. Each tile spans about 9 cm.
    scale = tile_size/.9
    master = np.full((TORSO_MASTER_SIZE, TORSO_MASTER_SIZE), 32768, dtype=np.uint16)
    for triangle in triangles[body_faces]:
        covered = pixels(uv[triangle[:, 1]]*TORSO_MASTER_SIZE, TORSO_MASTER_SIZE, TORSO_MASTER_SIZE)
        if covered is None:
            continue
        rows, columns, weights = covered
        x, y, z = (weights@positions[triangle[:, 0]]).astype(np.float32).T
        strength = torso_weight(x, y)
        if not strength.any():
            continue
        # The side projection avoids stretching the pores around the rib cage.
        flank = smoothstep(.95, 1.72, np.abs(x))
        v = ((y+.09*z)*scale) % tile_size + 1
        front_u = ((x+.28*z)*scale) % tile_size + 1
        side_u = ((z+.28*x)*scale) % tile_size + 1
        frontal = bilinear(tile, front_u, v)[:, 0]
        lateral = bilinear(tile, side_u, v)[:, 0]
        relief = strength*((1-flank)*frontal+flank*lateral)
        master[rows, columns] = np.rint(32768 + 32767*np.clip(relief, -1, 1)).astype(np.uint16)

    if master_path is not None:
        Image.fromarray(master).save(master_path, compress_level=3)
    sample_width = TORSO_MASTER_SIZE//GAME_NORMAL_SIZE
    detail = master.reshape(GAME_NORMAL_SIZE, sample_width, GAME_NORMAL_SIZE, sample_width)
    detail = (detail.mean(axis=(1, 3), dtype=np.float32)-32768)/32767
    del master
    dy, dx = np.gradient(detail)
    existing = Image.fromarray(quantize(127.5*(base_normal+1)))
    existing = existing.resize((GAME_NORMAL_SIZE, GAME_NORMAL_SIZE), Image.Resampling.BILINEAR)
    normal = np.asarray(existing, dtype=np.float32)*(2/255)-1
    normal[:, :, 0] -= .72*dx
    normal[:, :, 1] -= .72*dy
    length = np.sqrt(np.sum(normal*normal, axis=2))
    normal /= length[:, :, None]
    return quantize(127.5*(normal+1))


def face_detail(system, base, colour, roughness, shaved_shadow):
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
        rgb = colour[rows, cols].astype(np.float32)
        if shaved_shadow:
            chin = oval(x/.36, (y-6.29)/.15)
            jaw = oval((abs(x)-.42)/.23, (y-6.45)/.24)
            upper_lip = oval(x/.25, (y-6.76)/.060)
            shaved = np.minimum(1, chin+.63*jaw+.82*upper_lip)*facing
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


def regional_skin(base, rgb, rough, positions_uv=None):
    """Paint vascular colour and dry contact skin in body space, across UV seams."""
    pos, uv, tris = positions_uv if positions_uv is not None else mesh(base, 'body')
    image = rgb if rgb is not None else rough
    height, width = image.shape[:2]
    for tri in tris:
        coverage = pixels(uv[tri[:, 1]]*(width, height), width, height)
        if coverage is None:
            continue
        rows, cols, weights = coverage
        if not len(rows):
            continue
        x, y, z = (weights@pos[tri[:, 0]]).T
        side = abs(x)
        front = np.clip((z-.62)/.45, 0., 1.)
        cheek = front*oval((side-.43)/.24, (y-7.03)/.21)
        under_eye = front*oval((side-.33)/.19, (y-7.23)/.075)
        lips = front*oval(x/.245, (y-6.615)/.073)
        ears = oval((side-.82)/.13, (y-7.12)/.30)
        elbows = oval((side-3.33)/.53, (y-2.40)/.53)
        knees = oval((side-.94)/.65, (y+4.54)/.57)*np.clip((z+.42)/.9, 0., 1.)
        fingers = np.clip((side-4.05)/.65, 0., 1.)*oval((side-4.86)/.8, (y-.44)/1.05)
        torso = torso_weight(x, y)
        torso_front = smoothstep(-.15, .40, z)
        chest = torso*torso_front*oval((side-.73)/.83, (y-4.64)/.86)
        flank = torso*oval((side-1.43)/.37, (y-3.36)/1.65)
        upper_back = torso*(1-torso_front)*oval(side/1.18, (y-4.68)/.98)
        if rgb is not None:
            original = rgb[rows, cols].astype(np.float32)
            warm = (cheek*.68 + ears*.95 + lips*.95 + elbows*.35 + knees*.29
                    + fingers*.40 + .45*chest + .28*upper_back)
            # Add local capillary warmth while retaining each resident's albedo and complexion.
            gain = 1 + warm[:, None]*[.047,-.026,-.036] - under_eye[:, None]*[.038,.048,.017]
            rgb[rows, cols] = np.clip(np.rint(original*gain), 0, 255).astype(np.uint8)
        else:
            dry = .055*cheek + .105*elbows + .096*knees + .065*fingers + .050*flank
            wet = .058*lips + .045*under_eye + .035*ears + .045*chest + .025*upper_back
            rough[rows, cols] = np.clip(np.rint(rough[rows, cols] + 255*(dry-wet)), 0, 255).astype(np.uint8)


def quantize(array):
    return np.clip(np.rint(array), 0, 255).astype(np.uint8)


def bake(system, base, output, preset='male', save_torso_master=False):
    light_asset, dark_asset = (FEMALE_LIGHT, FEMALE_DARK) if preset == 'female' else (LIGHT, DARK)
    prefix = 'T_FemaleSkin' if preset == 'female' else 'T_HeroSkin'
    verify(base, BASE_HASH)
    for asset in (light_asset, dark_asset, BROW + '.obj', BROW + '.png'):
        verify(system/asset, SOURCE_HASHES[asset])
    light = np.asarray(Image.open(system/light_asset).convert('RGB'), dtype=np.float32)
    dark = np.asarray(Image.open(system/dark_asset).convert('RGB'), dtype=np.float32)
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

    face_detail(system, base, colour, roughness, shaved_shadow=(preset == 'male'))
    output.mkdir(parents=True, exist_ok=True)
    positions_uv = mesh(base, 'body')
    regional_skin(base, colour, None, positions_uv)
    Image.fromarray(colour).save(output/f'{prefix}_Color.png')
    master_path = output/f'{prefix}_TorsoHeight16K.png' if save_torso_master else None
    body_normal = bake_torso_normal(positions_uv, normal, 9183 if preset == 'female' else 5347, master_path)
    Image.fromarray(body_normal).save(output/f'{prefix}_Normal.png')
    rough = Image.fromarray(quantize(roughness*255))
    spec = np.array(rough.resize((1024, 1024), Image.Resampling.BOX))
    regional_skin(base, None, spec, positions_uv)
    Image.fromarray(spec).save(output/f'{prefix}_Roughness.png')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('system_assets', type=Path, help='Unpacked MakeHuman CC0 system asset root')
    parser.add_argument('hm08_base', type=Path, help='The same MakeHuman base.obj used to build the resident')
    parser.add_argument('output', type=Path)
    parser.add_argument('--preset', choices=('male','female'), default='male')
    parser.add_argument('--save-torso-master', action='store_true',
                        help='Also save the 16K, 16-bit torso relief used to bake the game normal')
    args = parser.parse_args()
    bake(args.system_assets, args.hm08_base, args.output, args.preset, args.save_torso_master)
