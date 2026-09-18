#!/usr/bin/env python3
"""Extract the approved sunrise texture/depth data from the source prototype."""
from __future__ import annotations

import base64
from collections import Counter
import io
import json
from pathlib import Path
import re

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "mosaico_living_worlds_v11 (1).html"
ASSETS = ROOT / "assets_src"
GRID = 16
VOLUME_FACE_TARGETS = {"front": 450, "side": 650, "rear": 400}


def embedded_assets() -> dict[str, bytes]:
    text = HTML.read_text(encoding="utf-8")
    pairs = re.findall(r'"([^"]+)":"data:[^;]+;base64,([^"]+)"', text)
    return {name: base64.b64decode(payload) for name, payload in pairs}


def simplify_volume_part(
    vertices: list[list[float]], faces: list[list[int]], target_faces: int
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


def main() -> None:
    assets = embedded_assets()
    texture = Image.open(io.BytesIO(assets["sunrise_space.webp"])).convert("RGB")
    depth = Image.open(io.BytesIO(assets["sunrise_space_depth.png"])).convert("RGB")
    texture.save(ASSETS / "sunrise_scene.png", optimize=True)
    texture.save(ASSETS / "sunrise.jpg", quality=91, optimize=True)
    for source,output,size in (
        ("sunrise_foreground0.webp","sunrise_cliff_front.png",(480,480)),
        ("sunrise-cliff_flank.webp","sunrise_cliff_side.png",(512,128)),
        ("sunrise-cliff_rock.webp","sunrise_cliff_rear.png",(256,256)),
    ):
        image = Image.open(io.BytesIO(assets[source])).convert("RGB")
        image.resize(size,Image.LANCZOS).save(ASSETS/output,optimize=True)

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
              "typedef struct { uint16_t a,b,c; } sunrise_volume_face_t;"]
    for part in ("front","side","rear"):
        vertices, faces = simplify_volume_part(
            volume[part]["v"], volume[part]["f"], VOLUME_FACE_TARGETS[part]
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
        chunks.extend("    {%d,%d,%d}," % tuple(face) for face in faces)
        chunks.append("};")
    (ROOT / "main" / "sunrise_volume.h").write_text("\n".join(chunks)+"\n",encoding="utf-8")


if __name__ == "__main__":
    main()
