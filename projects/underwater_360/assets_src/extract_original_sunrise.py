#!/usr/bin/env python3
"""Extract the approved sunrise texture/depth data from the source prototype."""
from __future__ import annotations

import base64
from collections import Counter
import io
import json
import math
from pathlib import Path
import re

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "mosaico_living_worlds_v11 (1).html"
ASSETS = ROOT / "assets_src"
GRID = 8
VOLUME_FACE_TARGETS = {"front": 350, "side": 100, "rear": 80}
SIDE_CONTOUR_VERTICES = 24
SIDE_RINGS = (0, 3, 6)


def embedded_assets() -> dict[str, bytes]:
    text = HTML.read_text(encoding="utf-8")
    pairs = re.findall(r'"([^"]+)":"data:[^;]+;base64,([^"]+)"', text)
    return {name: base64.b64decode(payload) for name, payload in pairs}


def simplify_volume_part(
    vertices: list[list[float]], faces: list[list[int]], target_faces: int,
    preserve_boundary: bool = True,
) -> tuple[list[list[float]], list[list[int]]]:
    """Collapse only real mesh edges, retaining topology and UV continuity."""
    current_vertices = [list(vertex) for vertex in vertices]
    current_faces = [list(face) for face in faces]
    best = (current_vertices, current_faces)

    while len(current_faces) > target_faces:
        edge_counts = Counter(
            tuple(sorted((face[index], face[(index + 1) % 3])))
            for face in current_faces
            for index in range(3)
        )
        boundary = {
            vertex
            for edge, count in edge_counts.items()
            if count == 1
            for vertex in edge
        }
        spans = [
            max(vertex[axis] for vertex in current_vertices)
            - min(vertex[axis] for vertex in current_vertices)
            for axis in range(5)
        ]
        spans = [max(value, 1e-6) for value in spans]
        candidates: list[tuple[float, int, int]] = []
        for (a, b), count in edge_counts.items():
            a_boundary = a in boundary
            b_boundary = b in boundary
            if preserve_boundary:
                if a_boundary or b_boundary:
                    continue
            else:
                if a_boundary != b_boundary:
                    continue
                if a_boundary and count != 1:
                    continue
            va, vb = current_vertices[a], current_vertices[b]
            xyz_cost = sum(((va[axis] - vb[axis]) / spans[axis]) ** 2 for axis in range(3))
            uv_cost = sum(((va[axis] - vb[axis]) / spans[axis]) ** 2 for axis in (3, 4))
            cost = xyz_cost + uv_cost * .2
            if a_boundary:
                cost *= 6.0
            candidates.append((cost, a, b))
        candidates.sort()
        if not candidates:
            break

        wanted = max(1, min(len(current_vertices) // 8,
                            (len(current_faces) - target_faces + 1) // 2))
        selected: list[tuple[int, int]] = []
        occupied: set[int] = set()
        for _, a, b in candidates:
            if a in occupied or b in occupied:
                continue
            selected.append((a, b))
            occupied.add(a)
            occupied.add(b)
            if len(selected) >= wanted:
                break
        if not selected:
            break

        remap = list(range(len(current_vertices)))
        for a, b in selected:
            current_vertices[a] = [
                (current_vertices[a][axis] + current_vertices[b][axis]) * .5
                for axis in range(5)
            ]
            remap[b] = a

        next_faces: list[list[int]] = []
        seen: set[tuple[int, int, int]] = set()
        for face in current_faces:
            mapped = [remap[index] for index in face]
            if len(set(mapped)) != 3:
                continue
            identity = tuple(sorted(mapped))
            if identity in seen:
                continue
            seen.add(identity)
            next_faces.append(mapped)
        used = sorted({index for face in next_faces for index in face})
        compact = {old: new for new, old in enumerate(used)}
        current_vertices = [current_vertices[index] for index in used]
        current_faces = [[compact[index] for index in face] for face in next_faces]
        if abs(len(current_faces) - target_faces) < abs(len(best[1]) - target_faces):
            best = ([list(vertex) for vertex in current_vertices],
                    [list(face) for face in current_faces])

    return best


def simplify_side_rings(
    vertices: list[list[float]], faces: list[list[int]], target_vertices: int,
) -> tuple[list[list[float]], list[list[int]]]:
    """Build a sparse closed shell from selected authored contour rings.

    Generic edge collapse cannot simplify this part: every source vertex lies
    on a boundary edge.  Recovering the authored rings lets us retain their
    silhouette and UV seam while removing redundant samples uniformly through
    the shell depth.
    """
    ring_count = 7
    if len(vertices) % ring_count:
        raise ValueError("sunrise side mesh no longer contains seven equal rings")
    ring_size = len(vertices) // ring_count
    adjacency: dict[int, set[int]] = {}
    for face in faces:
        for a, b in zip(face, face[1:] + face[:1]):
            if a < ring_size and b < ring_size:
                adjacency.setdefault(a, set()).add(b)
                adjacency.setdefault(b, set()).add(a)
    if not adjacency or any(len(neighbors) != 2 for neighbors in adjacency.values()):
        raise ValueError("sunrise side contour is not one closed loop")

    # Start at the UV seam and follow its increasing-index branch.  The three
    # unused source vertices are naturally skipped by the recovered adjacency.
    order = [0]
    previous = -1
    current = 0
    first_neighbors = sorted(adjacency[current])
    next_vertex = first_neighbors[0] if first_neighbors[0] != ring_size - 1 else first_neighbors[1]
    while next_vertex != order[0]:
        order.append(next_vertex)
        previous, current = current, next_vertex
        candidates = [item for item in adjacency[current] if item != previous]
        if len(candidates) != 1:
            raise ValueError("sunrise side contour traversal became ambiguous")
        next_vertex = candidates[0]
    if len(order) != len(adjacency):
        raise ValueError("sunrise side contour contains multiple loops")

    spans = [max(vertices[index][axis] for index in order)
             - min(vertices[index][axis] for index in order) for axis in range(3)]
    spans = [max(value, 1e-6) for value in spans]
    protected = {order[0], order[-1]}  # keep both sides of the authored UV seam
    selected = list(order)
    while len(selected) > target_vertices:
        candidate: tuple[float, int] | None = None
        for position, index in enumerate(selected):
            if index in protected:
                continue
            before = vertices[selected[position - 1]]
            point = vertices[index]
            after = vertices[selected[(position + 1) % len(selected)]]
            a = [(before[axis] / spans[axis]) for axis in range(3)]
            p = [(point[axis] / spans[axis]) for axis in range(3)]
            b = [(after[axis] / spans[axis]) for axis in range(3)]
            ab = [b[axis] - a[axis] for axis in range(3)]
            ap = [p[axis] - a[axis] for axis in range(3)]
            length = sum(value * value for value in ab)
            amount = max(0.0, min(1.0, sum(ap[axis] * ab[axis]
                                           for axis in range(3)) / max(length, 1e-12)))
            distance = sum((p[axis] - (a[axis] + ab[axis] * amount)) ** 2
                           for axis in range(3))
            if candidate is None or distance < candidate[0]:
                candidate = (distance, position)
        if candidate is None:
            break
        selected.pop(candidate[1])

    output_vertices = [vertices[ring * ring_size + index]
                       for ring in SIDE_RINGS for index in selected]
    output_faces: list[list[int]] = []
    width = len(selected)
    for ring in range(len(SIDE_RINGS) - 1):
        for position in range(width):
            following = (position + 1) % width
            front = ring * width + position
            front_next = ring * width + following
            back = (ring + 1) * width + position
            back_next = (ring + 1) * width + following
            output_faces.extend(([front, back, front_next],
                                 [front_next, back, back_next]))
    return output_vertices, output_faces


def bake_rock_material(size: tuple[int, int], exposure: float) -> Image.Image:
    """Create deterministic low-frequency rock that will not form UV stripes."""
    coarse = Image.new("L", (48, 24))
    coarse.putdata([
        66 + (((x * 73856093) ^ (y * 19349663) ^ ((x + y) * 83492791)) & 63)
        for y in range(24) for x in range(48)
    ])
    broad = Image.new("L", (12, 6))
    broad.putdata([
        58 + (((x * 2654435761) ^ (y * 2246822519) ^ 0x51F15E31) & 79)
        for y in range(6) for x in range(12)
    ])
    coarse = coarse.resize(size,Image.BICUBIC).filter(ImageFilter.GaussianBlur(1.3))
    broad = broad.resize(size,Image.BICUBIC).filter(ImageFilter.GaussianBlur(3.2))
    value = Image.blend(coarse,broad,.46)
    red = value.point(lambda p: min(255,round((p*.94+31)*exposure)))
    green = value.point(lambda p: min(255,round((p*.70+24)*exposure)))
    blue = value.point(lambda p: min(255,round((p*.43+17)*exposure)))
    return Image.merge("RGB",(red,green,blue))


def main() -> None:
    assets = embedded_assets()
    texture = Image.open(io.BytesIO(assets["sunrise_space.webp"])).convert("RGB")
    depth = Image.open(io.BytesIO(assets["sunrise_space_depth.png"])).convert("RGB")
    texture.save(ASSETS / "sunrise_scene.png", optimize=True)
    texture.save(ASSETS / "sunrise.jpg", quality=91, optimize=True)
    front_rgba = Image.open(io.BytesIO(assets["sunrise_foreground0.webp"])).convert("RGBA")
    front_image = front_rgba.convert("RGB")
    front_image.resize((768,768),Image.LANCZOS).save(
        ASSETS / "sunrise_cliff_front.png",optimize=True)
    # The prototype's flank/rear images are kaleidoscope patterns.  They become
    # obvious horizontal stripes when stretched over the volume.  Bake both
    # low-poly occlusion surfaces from the same photographed grass and rock as
    # the front so the palette and material remain continuous.
    side_image = bake_rock_material((512,128),.88)
    side_image.save(ASSETS / "sunrise_cliff_side.png",optimize=True)
    rear_image = bake_rock_material((256,256),.72)
    rear_image.save(ASSETS / "sunrise_cliff_rear.png",optimize=True)

    samples: list[int] = []
    for y in range(GRID + 1):
        sy = round(y * (depth.height - 1) / GRID)
        for x in range(GRID + 1):
            sx = round(x * (depth.width - 1) / GRID)
            red, green, _ = depth.getpixel((sx, sy))
            samples.append((red << 8) | green)

    rows = []
    width = GRID + 1
    for start in range(0, len(samples), width):
        rows.append("    " + ",".join(str(value) for value in samples[start:start + width]) + ",")
    header = """// Generated by assets_src/extract_original_sunrise.py
#pragma once
#include <stdint.h>
#define SUNRISE_DEPTH_GRID %d
static const uint16_t SUNRISE_DEPTH[(SUNRISE_DEPTH_GRID+1)*(SUNRISE_DEPTH_GRID+1)]={
""" % GRID + "\n".join(rows) + "\n};\n"
    (ROOT / "main" / "sunrise_depth.h").write_text(header, encoding="utf-8")

    text = HTML.read_text(encoding="utf-8")
    volumes = json.loads(re.search(r"const VOLUMES=(.*?);</script>",text,re.S).group(1))
    volume = volumes[2][0]
    chunks = ["// Generated by assets_src/extract_original_sunrise.py", "#pragma once",
              "#include <stdint.h>",
              "typedef struct { int16_t x,y,z,u,v; } sunrise_volume_vertex_t;",
              "typedef struct { uint16_t a,b,c; int8_t nx,ny,nz; uint16_t light; } sunrise_volume_face_t;"]
    center = [sum(vertex[axis] for vertex in volume["front"]["v"])
              / len(volume["front"]["v"]) for axis in range(3)]
    center[2] += volume["thickness"] * .46
    light = (-.48, .68, -.55)
    for part in ("front","side","rear"):
        if part == "side":
            vertices, faces = simplify_side_rings(
                volume[part]["v"], volume[part]["f"], SIDE_CONTOUR_VERTICES
            )
        else:
            vertices, faces = simplify_volume_part(
                volume[part]["v"], volume[part]["f"], VOLUME_FACE_TARGETS[part],
                preserve_boundary=part == "front",
            )
        print(f"{part}: {len(vertices)} vertices, {len(faces)} faces")
        chunks.append(f"#define SUNRISE_{part.upper()}_VERTEX_COUNT {len(vertices)}")
        chunks.append(f"#define SUNRISE_{part.upper()}_FACE_COUNT {len(faces)}")
        chunks.append(f"static const sunrise_volume_vertex_t SUNRISE_{part.upper()}_VERTICES[]={{")
        chunks.extend("    {%d,%d,%d,%d,%d}," % (
            round(v[0]*1000),round(v[1]*1000),round(v[2]*1000),round(v[3]),round(v[4]))
            for v in vertices)
        chunks.append("};")
        chunks.append(f"static const sunrise_volume_face_t SUNRISE_{part.upper()}_FACES[]={{")
        for face in faces:
            a,b,c=(vertices[index] for index in face)
            ab=[b[axis]-a[axis] for axis in range(3)]
            ac=[c[axis]-a[axis] for axis in range(3)]
            normal=[ab[1]*ac[2]-ab[2]*ac[1],
                    ab[2]*ac[0]-ab[0]*ac[2],
                    ab[0]*ac[1]-ab[1]*ac[0]]
            length=math.sqrt(sum(value*value for value in normal)) or 1.0
            normal=[value/length for value in normal]
            centroid=[(a[axis]+b[axis]+c[axis])/3 for axis in range(3)]
            if sum(normal[axis]*(centroid[axis]-center[axis]) for axis in range(3))<0:
                normal=[-value for value in normal]
            diffuse=max(0.0,sum(normal[axis]*light[axis] for axis in range(3)))
            shade=256 if part=="front" else round(256*(.78+.18*diffuse))
            chunks.append("    {%d,%d,%d,%d,%d,%d,%d}," % (
                face[0],face[1],face[2],*(round(value*127) for value in normal),shade))
        chunks.append("};")
    (ROOT / "main" / "sunrise_volume.h").write_text("\n".join(chunks)+"\n",encoding="utf-8")


if __name__ == "__main__":
    main()
