"""Fit the licensed adult lower-body details without changing the waist and ankle seams."""
from collections import Counter

import numpy as np


def boundary_edges(faces):
    count = Counter(tuple(sorted((i, j))) for face in faces
                    for (i, _), (j, _) in zip(face, face[1:] + face[:1]))
    return {edge for edge, uses in count.items() if uses == 1}


def seam_positions(vertices, faces):
    return {tuple(sorted(tuple(np.round(vertices[i], 5)) for i in edge))
            for edge in boundary_edges(faces)}


def fit_bare_skin(preset, sources, body, uv, region, full_skin, read_obj, fit, surface_normals):
    if preset == "female":
        stem = sources / "3396"
        original, tex, polygons = read_obj(stem.with_suffix(".obj"))
        points, _ = fit(stem.with_suffix(".proxy"), body)
        faces = [face for _, face in polygons]
        lower = [face for face in faces if -6 <= points[[i for i, _ in face], 1].mean() < .42]
        if len(points) != len(original) or seam_positions(points, lower) != seam_positions(body, region):
            raise ValueError("Female lower body must share the original waist and ankle rings")
        return points, tex, lower, faces

    stem = sources / "3223"
    original, _, polygons = read_obj(stem.with_suffix(".obj"))
    fitted, _ = fit(stem.with_suffix(".mhclo"), body)
    anchors, _ = fit(stem.with_suffix(".mhclo"), body, offsets=False)
    faces = [face for _, face in polygons]
    if len(fitted) != len(original):
        raise ValueError("Male detail geometry and fitting map do not match")

    # Project the small attachment's UVs onto the original nearby skin. The hem
    # of the attachment sits just beneath that surface so the join stays hidden.
    skin_uvs = {i: t for face in full_skin for i, t in face if -1.7 < body[i, 1] < 1.6 and abs(body[i, 0]) < 1.2}
    indices = np.array(sorted(skin_uvs))
    distances = np.linalg.norm(anchors[:, None] - body[indices][None, :], axis=2)
    neighbours = distances.argsort(axis=1)[:, :3]
    closest = indices[neighbours[:, 0]]
    tex = uv[[skin_uvs[i] for i in indices[neighbours].flat]].reshape(len(anchors), 3, 2)
    distances = np.take_along_axis(distances, neighbours, axis=1)
    same_chart = np.linalg.norm(tex - tex[:, :1], axis=2) < .075
    weights = same_chart / np.maximum(distances, .01)**2
    skin_tex = (tex * (weights / weights.sum(axis=1, keepdims=True))[:, :, None]).sum(axis=1)

    rim = sorted({i for edge in boundary_edges(faces) for i in edge})
    normals = surface_normals(body, full_skin)
    delta = np.zeros_like(fitted)
    delta[rim] = anchors[rim] - .035 * normals[closest[rim]] - fitted[rim]
    adjacent = [set() for _ in fitted]
    for face in faces:
        for (i, _), (j, _) in zip(face, face[1:] + face[:1]):
            adjacent[i].add(j)
            adjacent[j].add(i)
    visited = set(rim)
    for _ in range(2):
        outside = {j for i in rim for j in adjacent[i] if j not in visited}
        for i in outside:
            delta[i] = .55 * delta[sorted(adjacent[i] & set(rim))].mean(axis=0)
        visited.update(outside)
        rim = sorted(outside)

    patch = [[(len(body)+i, len(uv)+i) for i, _ in face] for face in faces]
    return np.vstack((body, fitted+delta)), np.vstack((uv, skin_tex)), region+patch, full_skin+patch
