#!/usr/bin/env python3
"""Extract aurora and ocean textures, depth, and closed volumes from the prototype."""
from __future__ import annotations

import base64
from collections import Counter
import io
import json
import math
from pathlib import Path
import re

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "mosaico_living_worlds_v11 (1).html"
ASSETS = ROOT / "assets_src"
GRID = 8
VOLUME_FACE_TARGETS = {
    "ice-bank": {"front": 350, "side": 500, "rear": 300},
    "reef-left": {"front": 300, "side": 420, "rear": 260},
    "reef-right": {"front": 220, "side": 320, "rear": 180},
}
SIDE_CONTOUR_VERTICES = {
    "ice-bank": 32,
    "reef-left": 28,
    "reef-right": 24,
}


def embedded_assets() -> dict[str, bytes]:
    text = HTML.read_text(encoding="utf-8")
    pairs = re.findall(r'"([^"]+)":"data:[^;]+;base64,([^"]+)"', text)
    return {name: base64.b64decode(payload) for name, payload in pairs}


def simplify_volume_part(
    vertices: list[list[float]], faces: list[list[int]], target_faces: int,
    preserve_boundary: bool = True,
) -> tuple[list[list[float]], list[list[int]]]:
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
    label: str,
) -> tuple[list[list[float]], list[list[int]]]:
    ring_count = 7
    if len(vertices) % ring_count:
        raise ValueError(f"{label} side mesh is not seven equal rings")
    ring_size = len(vertices) // ring_count
    adjacency: dict[int, set[int]] = {}
    for face in faces:
        for a, b in zip(face, face[1:] + face[:1]):
            if a < ring_size and b < ring_size:
                adjacency.setdefault(a, set()).add(b)
                adjacency.setdefault(b, set()).add(a)
    if not adjacency or any(len(neighbors) != 2 for neighbors in adjacency.values()):
        raise ValueError(f"{label} side contour is not one closed loop")

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
            raise ValueError(f"{label} side contour traversal became ambiguous")
        next_vertex = candidates[0]
    if len(order) != len(adjacency):
        raise ValueError(f"{label} side contour contains multiple loops")

    spans = [max(vertices[index][axis] for index in order)
             - min(vertices[index][axis] for index in order) for axis in range(3)]
    spans = [max(value, 1e-6) for value in spans]
    protected = {order[0], order[-1]}
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
                       for ring in range(ring_count) for index in selected]
    output_faces: list[list[int]] = []
    width = len(selected)
    for ring in range(ring_count - 1):
        for position in range(width):
            following = (position + 1) % width
            front = ring * width + position
            front_next = ring * width + following
            back = (ring + 1) * width + position
            back_next = (ring + 1) * width + following
            output_faces.extend(([front, back, front_next],
                                 [front_next, back, back_next]))
    return output_vertices, output_faces


def write_depth_header(name: str, depth: Image.Image, mask: Image.Image) -> None:
    samples: list[int] = []
    mask_r: list[int] = []
    mask_g: list[int] = []
    for y in range(GRID + 1):
        sy = round(y * (depth.height - 1) / GRID)
        for x in range(GRID + 1):
            sx = round(x * (depth.width - 1) / GRID)
            red, green, _ = depth.getpixel((sx, sy))[:3]
            samples.append((red << 8) | green)
            mr, mg = mask.getpixel((sx, sy))[:2]
            mask_r.append(mr)
            mask_g.append(mg)
    width = GRID + 1
    rows = []
    red_rows = []
    green_rows = []
    for start in range(0, len(samples), width):
        rows.append("    " + ",".join(str(value) for value in samples[start:start + width]) + ",")
        red_rows.append("    " + ",".join(str(value) for value in mask_r[start:start + width]) + ",")
        green_rows.append("    " + ",".join(str(value) for value in mask_g[start:start + width]) + ",")
    prefix = name.upper()
    header = (
        f"// Generated by assets_src/extract_aurora_ocean.py\n"
        f"#pragma once\n#include <stdint.h>\n"
        f"#define {prefix}_DEPTH_GRID {GRID}\n"
        f"static const uint16_t {prefix}_DEPTH[({prefix}_DEPTH_GRID+1)*({prefix}_DEPTH_GRID+1)]={{\n"
        + "\n".join(rows) + "\n};\n"
        f"static const uint8_t {prefix}_MASK_R[({prefix}_DEPTH_GRID+1)*({prefix}_DEPTH_GRID+1)]={{\n"
        + "\n".join(red_rows) + "\n};\n"
        f"static const uint8_t {prefix}_MASK_G[({prefix}_DEPTH_GRID+1)*({prefix}_DEPTH_GRID+1)]={{\n"
        + "\n".join(green_rows) + "\n};\n"
    )
    (ROOT / "main" / f"{name}_depth.h").write_text(header, encoding="utf-8")


def write_volume_header(prefix: str, volume: dict, light: tuple[float, float, float]) -> None:
    chunks = [
        "// Generated by assets_src/extract_aurora_ocean.py",
        "#pragma once",
        "#include \"living_volume.h\"",
    ]
    center = [sum(vertex[axis] for vertex in volume["front"]["v"])
              / len(volume["front"]["v"]) for axis in range(3)]
    center[2] += volume["thickness"] * .46
    targets = VOLUME_FACE_TARGETS[volume["id"]]
    for part in ("front", "side", "rear"):
        if part == "side":
            vertices, faces = simplify_side_rings(
                volume[part]["v"], volume[part]["f"],
                SIDE_CONTOUR_VERTICES[volume["id"]], volume["id"],
            )
        else:
            vertices, faces = simplify_volume_part(
                volume[part]["v"], volume[part]["f"], targets[part],
            )
        print(f"{volume['id']} {part}: {len(vertices)} vertices, {len(faces)} faces")
        tag = f"{prefix}_{part.upper()}"
        chunks.append(f"#define {tag}_VERTEX_COUNT {len(vertices)}")
        chunks.append(f"#define {tag}_FACE_COUNT {len(faces)}")
        chunks.append(f"static const living_volume_vertex_t {tag}_VERTICES[]={{")
        chunks.extend("    {%d,%d,%d,%d,%d}," % (
            round(v[0] * 1000), round(v[1] * 1000), round(v[2] * 1000),
            round(v[3]), round(v[4]))
            for v in vertices)
        chunks.append("};")
        chunks.append(f"static const living_volume_face_t {tag}_FACES[]={{")
        for face in faces:
            a, b, c = (vertices[index] for index in face)
            ab = [b[axis] - a[axis] for axis in range(3)]
            ac = [c[axis] - a[axis] for axis in range(3)]
            normal = [ab[1] * ac[2] - ab[2] * ac[1],
                      ab[2] * ac[0] - ab[0] * ac[2],
                      ab[0] * ac[1] - ab[1] * ac[0]]
            length = math.sqrt(sum(value * value for value in normal)) or 1.0
            normal = [value / length for value in normal]
            centroid = [(a[axis] + b[axis] + c[axis]) / 3 for axis in range(3)]
            if sum(normal[axis] * (centroid[axis] - center[axis]) for axis in range(3)) < 0:
                normal = [-value for value in normal]
            shade = 256
            chunks.append("    {%d,%d,%d,%d,%d,%d,%d}," % (
                face[0], face[1], face[2], *(round(value * 127) for value in normal), shade))
        chunks.append("};")
    (ROOT / "main" / f"{prefix.lower()}_volume.h").write_text("\n".join(chunks) + "\n", encoding="utf-8")


def save_space(assets: dict[str, bytes], key: str, stem: str) -> tuple[Image.Image, Image.Image]:
    texture = Image.open(io.BytesIO(assets[f"{key}_space.webp"])).convert("RGB")
    depth = Image.open(io.BytesIO(assets[f"{key}_space_depth.png"])).convert("RGB")
    mask = Image.open(io.BytesIO(assets[f"{key}_mask.png"])).convert("RGB")
    texture.save(ASSETS / f"{stem}_scene.png", optimize=True)
    texture.save(ASSETS / f"{stem}.jpg", quality=91, optimize=True)
    return depth, mask


def fill_transparent_front(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    pixels = rgba.load()
    alpha = [pixels[x, y][3] for y in range(height) for x in range(width)]
    filled = Image.new("RGB", (width, height))
    dest = filled.load()
    for y in range(height):
        opaque = [x for x in range(width) if pixels[x, y][3] > 16]
        if len(opaque) < 4:
            continue
        start, stop = opaque[0], opaque[-1] + 1
        span = stop - start
        for x in range(width):
            source = opaque[0] + ((x - start) % span)
            dest[x, y] = pixels[source, y][:3]
    for y in range(height):
        if all(pixels[x, y][3] <= 16 for x in range(width)):
            neighbor = next((row for row in range(y + 1, height)
                             if any(pixels[x, row][3] > 16 for x in range(width))), None)
            if neighbor is None:
                neighbor = next((row for row in range(y - 1, -1, -1)
                                 if any(pixels[x, row][3] > 16 for x in range(width))), 0)
            for x in range(width):
                dest[x, y] = dest[x, neighbor]
    return filled


def save_volume_images(assets: dict[str, bytes], spec: dict) -> None:
    front = fill_transparent_front(Image.open(io.BytesIO(assets[spec["front"]])))
    front.resize((768, 768), Image.LANCZOS).save(ASSETS / spec["front_out"], optimize=True)
    side = Image.open(io.BytesIO(assets[spec["side"]])).convert("RGB")
    side.resize((512, 128), Image.LANCZOS).save(ASSETS / spec["side_out"], optimize=True)
    rear = Image.open(io.BytesIO(assets[spec["rear"]])).convert("RGB")
    rear.resize((256, 256), Image.LANCZOS).save(ASSETS / spec["rear_out"], optimize=True)
    for name, cell in (
        (spec["front_json"], 384),
        (spec["side_json"], 256),
        (spec["rear_json"], 192),
    ):
        image = spec["front_out"] if "front" in name else spec["side_out"] if "side" in name else spec["rear_out"]
        frame = Path(image).stem
        (ASSETS / name).write_text(
            json.dumps({
                "image": image,
                "columns": 1,
                "rows": 1,
                "output_cell": cell,
                "alpha_mode": "opaque",
                "frames": [{"name": frame, "pivot_x": 0, "pivot_y": 0}],
            }, indent=2) + "\n",
            encoding="utf-8",
        )


def main() -> None:
    assets = embedded_assets()
    aurora_depth, aurora_mask = save_space(assets, "aurora", "aurora")
    ocean_depth, ocean_mask = save_space(assets, "ocean", "ocean")
    write_depth_header("aurora", aurora_depth, aurora_mask)
    write_depth_header("ocean", ocean_depth, ocean_mask)

    save_volume_images(assets, {
        "front": "aurora_bank.webp",
        "side": "ice-bank_flank.webp",
        "rear": "ice-bank_rock.webp",
        "front_out": "aurora_ice_front.png",
        "side_out": "aurora_ice_side.png",
        "rear_out": "aurora_ice_rear.png",
        "front_json": "aurora_ice_front.json",
        "side_json": "aurora_ice_side.json",
        "rear_json": "aurora_ice_rear.json",
    })
    save_volume_images(assets, {
        "front": "ocean_foreground0.webp",
        "side": "reef-left_flank.webp",
        "rear": "reef-left_rock.webp",
        "front_out": "ocean_reef_left_front.png",
        "side_out": "ocean_reef_left_side.png",
        "rear_out": "ocean_reef_left_rear.png",
        "front_json": "ocean_reef_left_front.json",
        "side_json": "ocean_reef_left_side.json",
        "rear_json": "ocean_reef_left_rear.json",
    })
    save_volume_images(assets, {
        "front": "ocean_foreground1.webp",
        "side": "reef-right_flank.webp",
        "rear": "reef-right_rock.webp",
        "front_out": "ocean_reef_right_front.png",
        "side_out": "ocean_reef_right_side.png",
        "rear_out": "ocean_reef_right_rear.png",
        "front_json": "ocean_reef_right_front.json",
        "side_json": "ocean_reef_right_side.json",
        "rear_json": "ocean_reef_right_rear.json",
    })

    text = HTML.read_text(encoding="utf-8")
    volumes = json.loads(re.search(r"const VOLUMES=(.*?);</script>", text, re.S).group(1))
    write_volume_header("AURORA_ICE", volumes[0][0], (-.32, .81, -.37))
    write_volume_header("OCEAN_LEFT", volumes[1][0], (-.32, .81, -.37))
    write_volume_header("OCEAN_RIGHT", volumes[1][1], (-.32, .81, -.37))


if __name__ == "__main__":
    main()
