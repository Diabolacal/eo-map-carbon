#!/usr/bin/env python3
"""Export New Eden known-space coordinates and stargates from EO-Map Contract A.

This does not reinterpret the SDE. It reads the sibling EO-Map pinned universe
database (map_data_eo_3464040.db, builder 1.5.0) and writes slim static
handoffs for the Carbon host:

  data/new_eden_systems.bin
  data/new_eden_systems.manifest.json
  src/new_eden_anchors.h
  data/new_eden_stargates.bin
  data/new_eden_stargates.manifest.json
  src/new_eden_gates_expect.h
  data/new_eden_star_visuals.bin
  data/new_eden_star_visuals.manifest.json

Runtime Carbon never opens SQLite, never talks to ESI, and never loads the
EO-Map web app.

Inclusion
---------
EO-Map's Contract A table has 8,089 visible (hidden=0) systems: 5,485 New Eden
known-space (30xxxxxx) plus 2,604 Anoikis / W-space (31xxxxxx). The live map
draws both, but boots and frames New Eden only; W-space is a separate cluster
about 1,300 LY away (see eve-frontier-map/src/config/eveOnlineMap.ts and
wormholeHome.ts). This milestone exports known-space only so the first native
view is the recognisable New Eden geometry.

Gates come from the same artefact's `stargates` table (13,978 directed rows,
already known-space only). Carbon stores unique undirected pairs
`source_id < dest_id` (6,989). That matches EO-Map's topology artefact
`gateEdges: 6989`. Bidirectional SDE rows would otherwise stack two identical
3D segments. W-space, wormholes, jump bridges, and Ansiblex are not in this
table and are not exported.

Coordinates
-----------
Contract A already converted raw SDE metres with
    P(raw) = (raw.x, raw.z, raw.y) / 9_460_000_000_000_000
and stored that as position_x/y/z (light years). The live EO-Map display
mapping in src/utils/universeCoordinates.ts is
    scene = (db.x, -db.z, -db.y)
The systems binary stores Contract A position_* unchanged. Gate records store
system ids only; the host looks up the already-mapped scene positions. No
extra scale, centre, axis swap, or 2D layout.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import math
import sqlite3
import struct
import sys
from pathlib import Path

MAGIC = b"NEDEN1B\0"
VERSION = 1
RECORD_SIZE = 24
HEADER_STRUCT = struct.Struct("<8sHHI4I32s")
RECORD_STRUCT = struct.Struct("<IfffIHBB")
EXPECTED_HEADER_SIZE = 64
EXPECTED_RECORD_SIZE = 24

PINNED_DB_NAME = "map_data_eo_3464040.db"
PINNED_DB_SHA256 = "874262496556d933bbbc184cdec8f469660f6c1108b21e25f0481475b68eba81"
PINNED_SDE_BUILD = 3464040
PINNED_BUILDER_VERSION = "1.5.0"
EVE_LY_METERS = 9_460_000_000_000_000
NEW_EDEN_SYSTEM_MIN = 30_000_000
NEW_EDEN_SYSTEM_MAX = 30_999_999
WORMHOLE_SYSTEM_MIN = 31_000_000
WORMHOLE_SYSTEM_MAX = 31_999_999
EXPECTED_KNOWN_SPACE = 5485
EXPECTED_W_SPACE = 2604
EXPECTED_DIRECTED_STARGATES = 13978
EXPECTED_UNDIRECTED_EDGES = 6989
EXPECTED_JITA_AMARR_HOPS = 11
EXPECTED_JITA_REACHABLE = 5228

GATE_MAGIC = b"NEGATE1\0"
GATE_VERSION = 1
GATE_RECORD_SIZE = 8
GATE_HEADER_STRUCT = struct.Struct("<8sHHIII32s8s")
GATE_RECORD_STRUCT = struct.Struct("<II")
EXPECTED_GATE_HEADER_SIZE = 64
EXPECTED_GATE_RECORD_SIZE = 8

STAR_MAGIC = b"NESTAR1\0"
STAR_VERSION = 1
STAR_RECORD_SIZE = 12
STAR_HEADER_STRUCT = struct.Struct("<8sHHIII32s8s")
STAR_RECORD_STRUCT = struct.Struct("<IfB3x")
EXPECTED_STAR_HEADER_SIZE = 64
EXPECTED_STAR_RECORD_SIZE = 12
EXPECTED_TEMP_MIN_K = 2010.0
EXPECTED_TEMP_MAX_K = 7496.0
EXPECTED_JITA_TEMP_K = 7305.0
EXPECTED_JITA_SPECTRAL = "F"

JITA_ID = 30000142
AMARR_ID = 30002187
NIARJA_ID = 30003504  # Pochven; EO-Map marks it unreachable from the main graph
THERA_ID = 31000005

# EO-Map tools/eve-online-sde/test_contract_a.py test_named_gate_neighbours
JITA_NEIGHBOR_NAMES = frozenset(
    {"Ikuchi", "Maurasi", "Muvolailen", "New Caldari", "Niyabainen", "Perimeter", "Sobaseki"}
)

ANCHOR_IDS = (
    30000142,  # Jita
    30002187,  # Amarr
    30002659,  # Dodixie
    30002510,  # Rens
    30002053,  # Hek
)

HUB_NEIGHBOR_IDS = (
    JITA_ID,
    AMARR_ID,
    30002659,  # Dodixie
    30002510,  # Rens
    30002053,  # Hek
    30100000,  # Zarzakh — four real static gates; still drawn
)

# Live EO-Map app transform. Do not use the stale metadata string in the
# already-built DB (it still documents the pre-2026-08-27 (x, z, -y) mapping).
# Source of truth: eve-frontier-map/src/utils/universeCoordinates.ts.


def db_to_scene(x: float, y: float, z: float) -> tuple[float, float, float]:
    return (x, -z, -y)


def as_f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def default_db_path(repo_root: Path) -> Path:
    return repo_root.parent / "eo-map" / "eve-frontier-map" / "public" / PINNED_DB_NAME


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_star_visuals(db_path: Path) -> list[tuple[int, str, float]]:
    connection = sqlite3.connect(f"file:{db_path.as_posix()}?mode=ro", uri=True)
    try:
        rows = list(
            connection.execute(
                "SELECT id, star_class, star_temperature FROM systems "
                "WHERE hidden = 0 AND id BETWEEN ? AND ? ORDER BY id ASC",
                (NEW_EDEN_SYSTEM_MIN, NEW_EDEN_SYSTEM_MAX),
            )
        )
    finally:
        connection.close()
    if len(rows) != EXPECTED_KNOWN_SPACE:
        raise RuntimeError(f"star visual row count {len(rows)} != {EXPECTED_KNOWN_SPACE}")
    temps = [float(temp) for _sid, _cls, temp in rows]
    if min(temps) != EXPECTED_TEMP_MIN_K or max(temps) != EXPECTED_TEMP_MAX_K:
        raise RuntimeError(f"temperature window {min(temps)}-{max(temps)} != {EXPECTED_TEMP_MIN_K}-{EXPECTED_TEMP_MAX_K}")
    by_id = {int(sid): (cls, float(temp)) for sid, cls, temp in rows}
    jita_cls, jita_temp = by_id[JITA_ID]
    if float(jita_temp) != EXPECTED_JITA_TEMP_K:
        raise RuntimeError(f"Jita temperature {jita_temp} != {EXPECTED_JITA_TEMP_K}")
    if not str(jita_cls).startswith(EXPECTED_JITA_SPECTRAL):
        raise RuntimeError(f"Jita star_class {jita_cls!r} does not start with {EXPECTED_JITA_SPECTRAL}")
    for sid, cls, temp in rows:
        if not math.isfinite(float(temp)) or float(temp) <= 0:
            raise RuntimeError(f"system {sid} has a non-finite temperature")
        if not cls:
            raise RuntimeError(f"system {sid} has an empty star_class")
    return [(int(sid), str(cls), float(temp)) for sid, cls, temp in rows]


def write_star_binary(rows: list[tuple[int, str, float]], source_sha: bytes, dest: Path) -> bytes:
    records = bytearray()
    for system_id, star_class, temperature in rows:
        letter = star_class.strip()[:1].upper().encode("ascii")
        if not letter:
            raise RuntimeError(f"system {system_id} has no spectral letter")
        records.extend(STAR_RECORD_STRUCT.pack(system_id, float(temperature), letter[0]))
    header = STAR_HEADER_STRUCT.pack(
        STAR_MAGIC,
        STAR_VERSION,
        STAR_RECORD_SIZE,
        len(rows),
        PINNED_SDE_BUILD,
        0,
        source_sha,
        b"\0" * 8,
    )
    if STAR_HEADER_STRUCT.size != EXPECTED_STAR_HEADER_SIZE or STAR_RECORD_STRUCT.size != EXPECTED_STAR_RECORD_SIZE:
        raise RuntimeError("star visual binary layout drift")
    blob = header + bytes(records)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(blob)
    return blob


def write_star_manifest(
    dest: Path,
    *,
    source_sha: str,
    metadata: dict[str, str],
    rows: list[tuple[int, str, float]],
    binary_sha: str,
    binary_bytes: int,
) -> dict:
    letters: dict[str, int] = collections.Counter()
    for _sid, star_class, _temp in rows:
        letters[star_class.strip()[:1].upper()] += 1
    temps = [temp for _sid, _cls, temp in rows]
    by_id = {sid: (cls, temp) for sid, cls, temp in rows}
    anchors = []
    for system_id in ANCHOR_IDS:
        star_class, temperature = by_id[system_id]
        anchors.append(
            {
                "id": system_id,
                "star_class": star_class,
                "star_temperature": temperature,
            }
        )
    document = {
        "artifact": {
            "filename": "new_eden_star_visuals.bin",
            "bytes": binary_bytes,
            "sha256": binary_sha,
            "format": "NESTAR1",
            "version": STAR_VERSION,
            "record": "system_id u32, temperature_k f32, spectral_letter u8, pad 3",
        },
        "source": {
            "kind": "EO-Map Contract A systems.star_temperature / star_class (raw metadata)",
            "sibling_path": "../eo-map/eve-frontier-map/public/map_data_eo_3464040.db",
            "filename": PINNED_DB_NAME,
            "sha256": source_sha,
            "builder_version": metadata.get("builder_version", PINNED_BUILDER_VERSION),
            "sde_build": int(metadata.get("sde_build", PINNED_SDE_BUILD)),
            "sde_fields": ["mapStars.statistics.temperature", "mapStars.statistics.spectralClass"],
        },
        "selection": {
            "space": "new-eden-known-space",
            "system_count": len(rows),
            "temperature_min_k": min(temps),
            "temperature_max_k": max(temps),
            "spectral_letter_counts": dict(sorted(letters.items())),
        },
        "presentation": {
            "colour": "runtime blackbody from temperature (EO-Map getStellarTemperatureColor)",
            "emissive": "runtime 1+5*log10-lerp(T,2300,7496)^3",
            "size": "runtime heuristic aSize from system id; not SDE radius/luminosity",
            "not_used_for_universe_map": ["radius", "luminosity_sde", "security_status"],
        },
        "anchors": anchors,
        "generator": "scripts/export-new-eden-systems.py",
    }
    dest.write_text(json.dumps(document, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    return document


def load_rows(db_path: Path) -> tuple[list[tuple[int, str, float, float, float]], dict[str, str], int, int]:
    connection = sqlite3.connect(f"file:{db_path.as_posix()}?mode=ro", uri=True)
    try:
        metadata = {key: value for key, value in connection.execute("SELECT key, value FROM metadata")}
        total = connection.execute("SELECT count(*) FROM systems").fetchone()[0]
        known = connection.execute(
            "SELECT count(*) FROM systems WHERE id BETWEEN ? AND ?",
            (NEW_EDEN_SYSTEM_MIN, NEW_EDEN_SYSTEM_MAX),
        ).fetchone()[0]
        wormhole = connection.execute(
            "SELECT count(*) FROM systems WHERE id BETWEEN ? AND ?",
            (WORMHOLE_SYSTEM_MIN, WORMHOLE_SYSTEM_MAX),
        ).fetchone()[0]
        if total != known + wormhole:
            raise RuntimeError(f"unexpected id bands: total={total} known={known} w={wormhole}")
        if known != EXPECTED_KNOWN_SPACE or wormhole != EXPECTED_W_SPACE:
            raise RuntimeError(
                f"unexpected system counts for this pin: known={known} w={wormhole} "
                f"(expected {EXPECTED_KNOWN_SPACE}/{EXPECTED_W_SPACE})"
            )
        rows = list(
            connection.execute(
                "SELECT id, name, position_x, position_y, position_z FROM systems "
                "WHERE hidden = 0 AND id BETWEEN ? AND ? ORDER BY id ASC",
                (NEW_EDEN_SYSTEM_MIN, NEW_EDEN_SYSTEM_MAX),
            )
        )
    finally:
        connection.close()
    return rows, metadata, known, wormhole


def write_binary(rows: list[tuple[int, str, float, float, float]], source_sha: bytes, dest: Path) -> bytes:
    records = bytearray()
    strings = bytearray()
    seen_ids: set[int] = set()
    for system_id, name, x, y, z in rows:
        if system_id in seen_ids:
            raise RuntimeError(f"duplicate system id {system_id}")
        seen_ids.add(system_id)
        if not name:
            raise RuntimeError(f"system {system_id} has an empty name")
        if not all(math.isfinite(value) for value in (x, y, z)):
            raise RuntimeError(f"system {system_id} has a non-finite coordinate")
        encoded = name.encode("utf-8")
        name_offset = len(strings)
        strings.extend(encoded)
        records.extend(
            RECORD_STRUCT.pack(
                int(system_id),
                float(x),
                float(y),
                float(z),
                name_offset,
                len(encoded),
                0,  # known-space
                0,
            )
        )
    header = HEADER_STRUCT.pack(
        MAGIC,
        VERSION,
        RECORD_SIZE,
        len(rows),
        PINNED_SDE_BUILD,
        len(strings),
        len(rows),
        0,
        source_sha,
    )
    if HEADER_STRUCT.size != EXPECTED_HEADER_SIZE or RECORD_STRUCT.size != EXPECTED_RECORD_SIZE:
        raise RuntimeError("binary layout drift")
    blob = header + bytes(records) + bytes(strings)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(blob)
    return blob


def write_manifest(
    dest: Path,
    *,
    db_path: Path,
    source_sha: str,
    metadata: dict[str, str],
    rows: list[tuple[int, str, float, float, float]],
    binary_sha: str,
    binary_bytes: int,
    known: int,
    wormhole: int,
) -> dict:
    scenes = [db_to_scene(as_f32(x), as_f32(y), as_f32(z)) for _sid, _name, x, y, z in rows]
    xs = [p[0] for p in scenes]
    ys = [p[1] for p in scenes]
    zs = [p[2] for p in scenes]
    centre = (
        (min(xs) + max(xs)) / 2,
        (min(ys) + max(ys)) / 2,
        (min(zs) + max(zs)) / 2,
    )
    anchors = []
    by_id = {int(row[0]): row for row in rows}
    for system_id in ANCHOR_IDS:
        row = by_id.get(system_id)
        if row is None:
            raise RuntimeError(f"missing anchor system {system_id}")
        _sid, name, x, y, z = row
        scene = db_to_scene(as_f32(x), as_f32(y), as_f32(z))
        anchors.append(
            {
                "id": system_id,
                "name": name,
                "db": {"x": as_f32(x), "y": as_f32(y), "z": as_f32(z)},
                "scene": {"x": scene[0], "y": scene[1], "z": scene[2]},
            }
        )
    document = {
        "artifact": {
            "filename": "new_eden_systems.bin",
            "bytes": binary_bytes,
            "sha256": binary_sha,
            "format": "NEDEN1B",
            "version": VERSION,
        },
        "source": {
            "kind": "EO-Map Contract A universe artefact (export, not a second SDE interpretation)",
            "sibling_path": "../eo-map/eve-frontier-map/public/map_data_eo_3464040.db",
            "filename": PINNED_DB_NAME,
            "sha256": source_sha,
            "builder_version": metadata.get("builder_version", PINNED_BUILDER_VERSION),
            "sde_build": int(metadata.get("sde_build", PINNED_SDE_BUILD)),
            "sde_url": metadata.get("sde_url"),
            "sde_sha256": metadata.get("sde_sha256"),
            "sde_release_date": metadata.get("sde_release_date"),
            "eve_ly_meters": int(metadata.get("eve_ly_meters", EVE_LY_METERS)),
            "db_transform": metadata.get("db_transform"),
        },
        "selection": {
            "space": "new-eden-known-space",
            "id_range": [NEW_EDEN_SYSTEM_MIN, NEW_EDEN_SYSTEM_MAX],
            "hidden": 0,
            "system_count": len(rows),
            "known_space_count": known,
            "wormhole_space_count_in_source": wormhole,
            "wormhole_space_exported": 0,
            "reason": (
                "EO-Map default home is New Eden. W-space is a separate ~1,300 LY "
                "cluster and is omitted from this first visual reproduction."
            ),
        },
        "coordinate_contract": {
            "binary_stores": "Contract A position_x/y/z (light years, uncentred)",
            "host_display_transform": "(x, -z, -y)",
            "host_display_transform_source": "eve-frontier-map/src/utils/universeCoordinates.ts dbToScenePosition",
            "composed_raw_to_scene": "(raw.x, -raw.y, -raw.z) / 9460000000000000",
            "no_extra_scale": True,
            "no_centring_of_vertices": True,
            "units": "light-year",
        },
        "scene_aabb": {
            "min": {"x": min(xs), "y": min(ys), "z": min(zs)},
            "max": {"x": max(xs), "y": max(ys), "z": max(zs)},
            "extent": {"x": max(xs) - min(xs), "y": max(ys) - min(ys), "z": max(zs) - min(zs)},
            "centre": {"x": centre[0], "y": centre[1], "z": centre[2]},
        },
        "anchors": anchors,
        "generator": "scripts/export-new-eden-systems.py",
    }
    dest.write_text(json.dumps(document, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    return document


def write_anchors_header(dest: Path, document: dict) -> None:
    centre = document["scene_aabb"]["centre"]
    lines = [
        "// Generated by scripts/export-new-eden-systems.py. Do not edit by hand.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace neweden",
        "{",
        f"constexpr uint32_t kExpectedSystemCount = {document['selection']['system_count']};",
        f"constexpr uint32_t kExpectedKnownSpaceCount = {document['selection']['known_space_count']};",
        f"constexpr uint32_t kExpectedOtherSpaceCount = {document['selection']['wormhole_space_exported']};",
        f"constexpr uint32_t kExpectedSdeBuild = {document['source']['sde_build']};",
        f"constexpr float kCentreX = {centre['x']:.9g}f;",
        f"constexpr float kCentreY = {centre['y']:.9g}f;",
        f"constexpr float kCentreZ = {centre['z']:.9g}f;",
        "",
        "struct AnchorExpect",
        "{",
        "	uint32_t id;",
        "	const char* name;",
        "	float x;",
        "	float y;",
        "	float z;",
        "};",
        "",
        "constexpr AnchorExpect kAnchors[] = {",
    ]
    for anchor in document["anchors"]:
        scene = anchor["scene"]
        lines.append(
            f"	{{ {anchor['id']}, \"{anchor['name']}\", {scene['x']:.9g}f, {scene['y']:.9g}f, {scene['z']:.9g}f }},"
        )
    lines.extend(["};", "}", ""])
    dest.write_text("\n".join(lines), encoding="utf-8")


def hop_distance(adjacency: dict[int, set[int]], origin: int, destination: int) -> int | None:
    if origin == destination:
        return 0
    dist = {origin: 0}
    queue = collections.deque([origin])
    while queue:
        current = queue.popleft()
        here = dist[current]
        for nxt in adjacency.get(current, ()):
            if nxt in dist:
                continue
            dist[nxt] = here + 1
            if nxt == destination:
                return dist[nxt]
            queue.append(nxt)
    return None


def reachable_count(adjacency: dict[int, set[int]], origin: int) -> int:
    seen = {origin}
    queue = collections.deque([origin])
    while queue:
        current = queue.popleft()
        for nxt in adjacency.get(current, ()):
            if nxt not in seen:
                seen.add(nxt)
                queue.append(nxt)
    return len(seen)


def load_gates(
    db_path: Path, known_ids: set[int]
) -> tuple[list[tuple[int, int]], dict[int, list[tuple[int, str]]], dict[str, int]]:
    connection = sqlite3.connect(f"file:{db_path.as_posix()}?mode=ro", uri=True)
    try:
        directed = connection.execute("SELECT count(*) FROM stargates").fetchone()[0]
        self_edges = connection.execute(
            "SELECT count(*) FROM stargates WHERE source_system_id = destination_system_id"
        ).fetchone()[0]
        wspace = connection.execute(
            "SELECT count(*) FROM stargates WHERE source_system_id BETWEEN ? AND ? "
            "OR destination_system_id BETWEEN ? AND ?",
            (WORMHOLE_SYSTEM_MIN, WORMHOLE_SYSTEM_MAX, WORMHOLE_SYSTEM_MIN, WORMHOLE_SYSTEM_MAX),
        ).fetchone()[0]
        nonreciprocal = connection.execute(
            "SELECT count(*) FROM stargates a LEFT JOIN stargates b "
            "ON a.source_system_id = b.destination_system_id "
            "AND a.destination_system_id = b.source_system_id WHERE b.id IS NULL"
        ).fetchone()[0]
        if directed != EXPECTED_DIRECTED_STARGATES:
            raise RuntimeError(f"unexpected directed stargate count {directed}")
        if self_edges:
            raise RuntimeError(f"self-edges in Contract A stargates: {self_edges}")
        if wspace:
            raise RuntimeError(f"W-space stargate rows in Contract A: {wspace}")
        if nonreciprocal:
            raise RuntimeError(f"non-reciprocal stargate rows: {nonreciprocal}")

        directed_rows = list(
            connection.execute("SELECT source_system_id, destination_system_id FROM stargates")
        )
        hub_neighbors: dict[int, list[tuple[int, str]]] = {}
        for hub_id in HUB_NEIGHBOR_IDS:
            rows = list(
                connection.execute(
                    "SELECT s.id, s.name FROM stargates g JOIN systems s "
                    "ON s.id = g.destination_system_id WHERE g.source_system_id = ? "
                    "ORDER BY s.id ASC",
                    (hub_id,),
                )
            )
            hub_neighbors[hub_id] = [(int(sid), str(name)) for sid, name in rows]
    finally:
        connection.close()

    adjacency: dict[int, set[int]] = collections.defaultdict(set)
    undirected: dict[tuple[int, int], None] = {}
    for source, dest in directed_rows:
        source = int(source)
        dest = int(dest)
        if source == dest:
            raise RuntimeError(f"self-edge {source}")
        if source not in known_ids or dest not in known_ids:
            raise RuntimeError(f"stargate {source}->{dest} is not in the known-space export")
        if not (NEW_EDEN_SYSTEM_MIN <= source <= NEW_EDEN_SYSTEM_MAX):
            raise RuntimeError(f"source {source} is outside known-space")
        if not (NEW_EDEN_SYSTEM_MIN <= dest <= NEW_EDEN_SYSTEM_MAX):
            raise RuntimeError(f"destination {dest} is outside known-space")
        adjacency[source].add(dest)
        adjacency[dest].add(source)
        pair = (source, dest) if source < dest else (dest, source)
        undirected[pair] = None

    edges = list(undirected.keys())
    edges.sort()
    if len(edges) != EXPECTED_UNDIRECTED_EDGES:
        raise RuntimeError(f"undirected edge count {len(edges)} != {EXPECTED_UNDIRECTED_EDGES}")

    jita_names = {name for _sid, name in hub_neighbors[JITA_ID]}
    if jita_names != JITA_NEIGHBOR_NAMES:
        raise RuntimeError(f"Jita neighbours {sorted(jita_names)} != EO-Map pin {sorted(JITA_NEIGHBOR_NAMES)}")

    hops = hop_distance(adjacency, JITA_ID, AMARR_ID)
    if hops != EXPECTED_JITA_AMARR_HOPS:
        raise RuntimeError(f"Jita-Amarr hops {hops} != EO-Map pin {EXPECTED_JITA_AMARR_HOPS}")
    if hop_distance(adjacency, JITA_ID, THERA_ID) is not None:
        raise RuntimeError("Thera must not be on the static known-space gate graph")
    if hop_distance(adjacency, JITA_ID, NIARJA_ID) is not None:
        raise RuntimeError("Niarja (Pochven) must stay disconnected from Jita on static gates")
    jita_reachable = reachable_count(adjacency, JITA_ID)
    if jita_reachable != EXPECTED_JITA_REACHABLE:
        raise RuntimeError(f"Jita reachable {jita_reachable} != EO-Map main component {EXPECTED_JITA_REACHABLE}")

    stats = {
        "directed": directed,
        "undirected": len(edges),
        "jita_amarr_hops": hops,
        "jita_reachable": jita_reachable,
        "self_edges": self_edges,
        "wspace_edges": wspace,
        "nonreciprocal": nonreciprocal,
    }
    return edges, hub_neighbors, stats


def write_gate_binary(edges: list[tuple[int, int]], source_sha: bytes, dest: Path) -> bytes:
    records = bytearray()
    seen: set[tuple[int, int]] = set()
    for source, dest_id in edges:
        if source >= dest_id:
            raise RuntimeError(f"edge not canonical: {source},{dest_id}")
        if (source, dest_id) in seen:
            raise RuntimeError(f"duplicate undirected edge {source},{dest_id}")
        seen.add((source, dest_id))
        records.extend(GATE_RECORD_STRUCT.pack(source, dest_id))
    header = GATE_HEADER_STRUCT.pack(
        GATE_MAGIC,
        GATE_VERSION,
        GATE_RECORD_SIZE,
        len(edges),
        PINNED_SDE_BUILD,
        0,
        source_sha,
        b"\0" * 8,
    )
    if GATE_HEADER_STRUCT.size != EXPECTED_GATE_HEADER_SIZE or GATE_RECORD_STRUCT.size != EXPECTED_GATE_RECORD_SIZE:
        raise RuntimeError("gate binary layout drift")
    blob = header + bytes(records)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(blob)
    return blob


def write_gate_manifest(
    dest: Path,
    *,
    source_sha: str,
    metadata: dict[str, str],
    edges: list[tuple[int, int]],
    hub_neighbors: dict[int, list[tuple[int, str]]],
    stats: dict[str, int],
    binary_sha: str,
    binary_bytes: int,
    names_by_id: dict[int, str],
) -> dict:
    hubs = []
    for hub_id in HUB_NEIGHBOR_IDS:
        hubs.append(
            {
                "id": hub_id,
                "name": names_by_id[hub_id],
                "neighbors": [{"id": sid, "name": name} for sid, name in hub_neighbors[hub_id]],
            }
        )
    document = {
        "artifact": {
            "filename": "new_eden_stargates.bin",
            "bytes": binary_bytes,
            "sha256": binary_sha,
            "format": "NEGATE1",
            "version": GATE_VERSION,
        },
        "source": {
            "kind": "EO-Map Contract A universe artefact (export, not a second SDE interpretation)",
            "sibling_path": "../eo-map/eve-frontier-map/public/map_data_eo_3464040.db",
            "filename": PINNED_DB_NAME,
            "sha256": source_sha,
            "table": "stargates",
            "builder_version": metadata.get("builder_version", PINNED_BUILDER_VERSION),
            "sde_build": int(metadata.get("sde_build", PINNED_SDE_BUILD)),
        },
        "selection": {
            "space": "new-eden-known-space",
            "id_range": [NEW_EDEN_SYSTEM_MIN, NEW_EDEN_SYSTEM_MAX],
            "hidden": 0,
            "directed_stargate_rows_in_source": stats["directed"],
            "undirected_edges_exported": stats["undirected"],
            "deduplication": "unique (min(src,dst), max(src,dst)); fully reciprocal so A-B and B-A collapse to one segment",
            "self_edges": stats["self_edges"],
            "wspace_edges": stats["wspace_edges"],
            "nonreciprocal_rows": stats["nonreciprocal"],
            "wormhole_space_exported": 0,
            "reason": (
                "Contract A stargates are already known-space only. Carbon draws one "
                "straight 3D segment per unique pair using the 1B system positions."
            ),
        },
        "validation": {
            "jita_id": JITA_ID,
            "amarr_id": AMARR_ID,
            "niarja_id": NIARJA_ID,
            "jita_amarr_hops": stats["jita_amarr_hops"],
            "jita_reachable_including_jita": stats["jita_reachable"],
            "jita_neighbors_source": "tools/eve-online-sde/test_contract_a.py test_named_gate_neighbours",
            "jita_amarr_hops_source": "eve-frontier-map/src/eo/routeDistance/__tests__/hopCount.test.ts",
            "main_component_source": "eve-frontier-map/scripts/generate-gate-topology.mjs EXPECTED.counts",
        },
        "hubs": hubs,
        "coordinate_contract": {
            "binary_stores": "undirected known-space system id pairs",
            "host_display_transform": "(x, -z, -y) applied to the matching 1B system positions",
            "geometry": "straight 3D segment between scene positions; no 2D layout, curves, or midpoints",
        },
        "generator": "scripts/export-new-eden-systems.py",
    }
    dest.write_text(json.dumps(document, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    return document


def write_gates_header(dest: Path, document: dict) -> None:
    validation = document["validation"]
    selection = document["selection"]
    lines = [
        "// Generated by scripts/export-new-eden-systems.py. Do not edit by hand.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace neweden",
        "{",
        f"constexpr uint32_t kExpectedEdgeCount = {selection['undirected_edges_exported']};",
        f"constexpr uint32_t kExpectedDirectedStargateCount = {selection['directed_stargate_rows_in_source']};",
        f"constexpr uint32_t kExpectedJitaAmarrHops = {validation['jita_amarr_hops']};",
        f"constexpr uint32_t kExpectedJitaReachable = {validation['jita_reachable_including_jita']};",
        f"constexpr uint32_t kJitaId = {validation['jita_id']};",
        f"constexpr uint32_t kAmarrId = {validation['amarr_id']};",
        f"constexpr uint32_t kNiarjaId = {validation['niarja_id']};",
        "",
        "struct NeighborExpect",
        "{",
        "	uint32_t id;",
        "	const char* name;",
        "};",
        "",
    ]
    for hub in document["hubs"]:
        ident = "".join(ch if ch.isalnum() else "" for ch in hub["name"])
        lines.append(f"constexpr NeighborExpect k{ident}Neighbors[] = {{")
        for neighbor in hub["neighbors"]:
            lines.append(f"	{{ {neighbor['id']}, \"{neighbor['name']}\" }},")
        lines.append("};")
        lines.append("")
    lines.extend(["}", ""])
    dest.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", type=Path, default=default_db_path(repo_root))
    parser.add_argument("--out-bin", type=Path, default=repo_root / "data" / "new_eden_systems.bin")
    parser.add_argument("--out-manifest", type=Path, default=repo_root / "data" / "new_eden_systems.manifest.json")
    parser.add_argument("--out-header", type=Path, default=repo_root / "src" / "new_eden_anchors.h")
    parser.add_argument("--out-gates-bin", type=Path, default=repo_root / "data" / "new_eden_stargates.bin")
    parser.add_argument("--out-gates-manifest", type=Path, default=repo_root / "data" / "new_eden_stargates.manifest.json")
    parser.add_argument("--out-gates-header", type=Path, default=repo_root / "src" / "new_eden_gates_expect.h")
    parser.add_argument("--out-star-bin", type=Path, default=repo_root / "data" / "new_eden_star_visuals.bin")
    parser.add_argument("--out-star-manifest", type=Path, default=repo_root / "data" / "new_eden_star_visuals.manifest.json")
    parser.add_argument("--allow-db-mismatch", action="store_true")
    args = parser.parse_args()

    if HEADER_STRUCT.size != EXPECTED_HEADER_SIZE or RECORD_STRUCT.size != EXPECTED_RECORD_SIZE:
        raise RuntimeError(
            f"struct sizes {HEADER_STRUCT.size}/{RECORD_STRUCT.size} != {EXPECTED_HEADER_SIZE}/{EXPECTED_RECORD_SIZE}"
        )
    if GATE_HEADER_STRUCT.size != EXPECTED_GATE_HEADER_SIZE or GATE_RECORD_STRUCT.size != EXPECTED_GATE_RECORD_SIZE:
        raise RuntimeError(
            f"gate struct sizes {GATE_HEADER_STRUCT.size}/{GATE_RECORD_STRUCT.size} != "
            f"{EXPECTED_GATE_HEADER_SIZE}/{EXPECTED_GATE_RECORD_SIZE}"
        )
    if STAR_HEADER_STRUCT.size != EXPECTED_STAR_HEADER_SIZE or STAR_RECORD_STRUCT.size != EXPECTED_STAR_RECORD_SIZE:
        raise RuntimeError(
            f"star struct sizes {STAR_HEADER_STRUCT.size}/{STAR_RECORD_STRUCT.size} != "
            f"{EXPECTED_STAR_HEADER_SIZE}/{EXPECTED_STAR_RECORD_SIZE}"
        )
    if not args.db.is_file():
        raise SystemExit(f"Contract A database not found: {args.db}")

    source_sha = sha256_file(args.db)
    if source_sha != PINNED_DB_SHA256 and not args.allow_db_mismatch:
        raise SystemExit(
            f"source DB sha256 {source_sha} does not match pinned {PINNED_DB_SHA256}. "
            "Pass --allow-db-mismatch only when intentionally refreshing the pin."
        )

    rows, metadata, known, wormhole = load_rows(args.db)
    if len(rows) != EXPECTED_KNOWN_SPACE:
        raise RuntimeError(f"export row count {len(rows)} != {EXPECTED_KNOWN_SPACE}")

    source_sha_bytes = bytes.fromhex(source_sha)
    blob = write_binary(rows, source_sha_bytes, args.out_bin)
    binary_sha = hashlib.sha256(blob).hexdigest()
    document = write_manifest(
        args.out_manifest,
        db_path=args.db,
        source_sha=source_sha,
        metadata=metadata,
        rows=rows,
        binary_sha=binary_sha,
        binary_bytes=len(blob),
        known=known,
        wormhole=wormhole,
    )
    write_anchors_header(args.out_header, document)

    known_ids = {int(row[0]) for row in rows}
    names_by_id = {int(row[0]): str(row[1]) for row in rows}
    edges, hub_neighbors, gate_stats = load_gates(args.db, known_ids)
    gate_blob = write_gate_binary(edges, source_sha_bytes, args.out_gates_bin)
    gate_sha = hashlib.sha256(gate_blob).hexdigest()
    gate_document = write_gate_manifest(
        args.out_gates_manifest,
        source_sha=source_sha,
        metadata=metadata,
        edges=edges,
        hub_neighbors=hub_neighbors,
        stats=gate_stats,
        binary_sha=gate_sha,
        binary_bytes=len(gate_blob),
        names_by_id=names_by_id,
    )
    write_gates_header(args.out_gates_header, gate_document)

    star_rows = load_star_visuals(args.db)
    system_ids = [int(row[0]) for row in rows]
    star_ids = [int(row[0]) for row in star_rows]
    if star_ids != system_ids:
        raise RuntimeError("star visual ids are not in the same order as the systems export")
    star_blob = write_star_binary(star_rows, source_sha_bytes, args.out_star_bin)
    star_sha = hashlib.sha256(star_blob).hexdigest()
    star_document = write_star_manifest(
        args.out_star_manifest,
        source_sha=source_sha,
        metadata=metadata,
        rows=star_rows,
        binary_sha=star_sha,
        binary_bytes=len(star_blob),
    )

    centre = document["scene_aabb"]["centre"]
    print(f"source db       : {args.db}")
    print(f"source sha256   : {source_sha}")
    print(f"builder/sde     : {metadata.get('builder_version')} / {metadata.get('sde_build')}")
    print(f"known-space     : {len(rows)}")
    print(f"w-space omitted : {wormhole}")
    print(f"directed gates  : {gate_stats['directed']}")
    print(f"undirected edges: {gate_stats['undirected']}")
    print(f"Jita-Amarr hops : {gate_stats['jita_amarr_hops']}")
    print(f"Jita reachable  : {gate_stats['jita_reachable']}")
    print(f"scene centre    : ({centre['x']:.6f}, {centre['y']:.6f}, {centre['z']:.6f})")
    print(f"binary          : {args.out_bin} ({len(blob)} bytes, {binary_sha})")
    print(f"manifest        : {args.out_manifest}")
    print(f"anchors header  : {args.out_header}")
    print(f"gates binary    : {args.out_gates_bin} ({len(gate_blob)} bytes, {gate_sha})")
    print(f"gates manifest  : {args.out_gates_manifest}")
    print(f"gates header    : {args.out_gates_header}")
    print(f"star visuals    : {args.out_star_bin} ({len(star_blob)} bytes, {star_sha})")
    print(f"star manifest   : {args.out_star_manifest}")
    print(
        f"star temps      : {star_document['selection']['temperature_min_k']}-"
        f"{star_document['selection']['temperature_max_k']} K"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
