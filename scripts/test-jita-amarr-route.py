#!/usr/bin/env python3
"""Independent Jita-Amarr shortest-hop check against the pinned binaries.

Does not use Trinity. Prints the reconstructed system names so a smoke log
can be compared with a second implementation of the same undirected BFS.
"""

from __future__ import annotations

import struct
import sys
from collections import deque
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
JITA = 30000142
AMARR = 30002187
EXPECTED_HOPS = 11


def load_systems(path: Path) -> dict[int, str]:
    data = path.read_bytes()
    magic, version, rec_size, count, sde, strings_size, known, other = struct.unpack_from(
        "<8sHHIIIII", data, 0
    )
    if magic != b"NEDEN1B\x00":
        raise SystemExit(f"bad systems magic {magic!r}")
    records_off = 64
    strings_off = records_off + count * rec_size
    strings = data[strings_off : strings_off + strings_size]
    names: dict[int, str] = {}
    for i in range(count):
        rec = data[records_off + i * rec_size : records_off + (i + 1) * rec_size]
        system_id, _x, _y, _z, name_off, name_len, space, _pad = struct.unpack("<IfffIHBB", rec)
        if space != 0:
            continue
        names[system_id] = strings[name_off : name_off + name_len].decode("utf-8")
    return names


def load_edges(path: Path) -> list[tuple[int, int]]:
    data = path.read_bytes()
    magic, version, rec_size, count, sde, flags = struct.unpack_from("<8sHHIII", data, 0)
    if magic != b"NEGATE1\x00":
        raise SystemExit(f"bad gates magic {magic!r}")
    edges = []
    off = 64
    for _ in range(count):
        src, dst = struct.unpack_from("<II", data, off)
        off += rec_size
        edges.append((src, dst))
    return edges


def shortest_route(edges: list[tuple[int, int]], src: int, dst: int) -> list[int]:
    adj: dict[int, list[int]] = {}
    for a, b in edges:
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    if src == dst:
        return [src]
    parent: dict[int, int] = {}
    seen = {src}
    queue = deque([src])
    found = False
    while queue and not found:
        current = queue.popleft()
        for nxt in adj.get(current, []):
            if nxt in seen:
                continue
            seen.add(nxt)
            parent[nxt] = current
            if nxt == dst:
                found = True
                break
            queue.append(nxt)
    if not found:
        raise SystemExit(f"no path {src} -> {dst}")
    path = [dst]
    at = dst
    while at != src:
        at = parent[at]
        path.append(at)
    path.reverse()
    return path


def main() -> int:
    systems_path = REPO / "data" / "new_eden_systems.bin"
    gates_path = REPO / "data" / "new_eden_stargates.bin"
    names = load_systems(systems_path)
    edges = load_edges(gates_path)
    edge_set = {tuple(sorted(edge)) for edge in edges}
    path = shortest_route(edges, JITA, AMARR)
    hops = len(path) - 1
    print("route:", " -> ".join(names.get(system_id, str(system_id)) for system_id in path))
    print(f"hops: {hops} systems: {len(path)}")
    if path[0] != JITA or path[-1] != AMARR:
        print("FAILED endpoints", file=sys.stderr)
        return 1
    if hops != EXPECTED_HOPS:
        print(f"FAILED hops {hops} != {EXPECTED_HOPS}", file=sys.stderr)
        return 1
    for a, b in zip(path, path[1:]):
        if tuple(sorted((a, b))) not in edge_set:
            print(f"FAILED missing edge {a}->{b}", file=sys.stderr)
            return 1
    print("ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
