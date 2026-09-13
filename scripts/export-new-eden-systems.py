#!/usr/bin/env python3
"""Export New Eden known-space coordinates from EO-Map's Contract A artefact.

This does not reinterpret the SDE. It reads the sibling EO-Map pinned universe
database (map_data_eo_3464040.db, builder 1.5.0) and writes a slim static
handoff for the Carbon host:

  data/new_eden_systems.bin
  data/new_eden_systems.manifest.json
  src/new_eden_anchors.h

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

Coordinates
-----------
Contract A already converted raw SDE metres with
    P(raw) = (raw.x, raw.z, raw.y) / 9_460_000_000_000_000
and stored that as position_x/y/z (light years). The live EO-Map display
mapping in src/utils/universeCoordinates.ts is
    scene = (db.x, -db.z, -db.y)
The binary stores Contract A position_* unchanged. The Carbon host applies the
same display mapping before upload. No extra scale, centre, or axis swap.
"""

from __future__ import annotations

import argparse
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

ANCHOR_IDS = (
    30000142,  # Jita
    30002187,  # Amarr
    30002659,  # Dodixie
    30002510,  # Rens
    30002053,  # Hek
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


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", type=Path, default=default_db_path(repo_root))
    parser.add_argument("--out-bin", type=Path, default=repo_root / "data" / "new_eden_systems.bin")
    parser.add_argument("--out-manifest", type=Path, default=repo_root / "data" / "new_eden_systems.manifest.json")
    parser.add_argument("--out-header", type=Path, default=repo_root / "src" / "new_eden_anchors.h")
    parser.add_argument("--allow-db-mismatch", action="store_true")
    args = parser.parse_args()

    if HEADER_STRUCT.size != EXPECTED_HEADER_SIZE or RECORD_STRUCT.size != EXPECTED_RECORD_SIZE:
        raise RuntimeError(
            f"struct sizes {HEADER_STRUCT.size}/{RECORD_STRUCT.size} != {EXPECTED_HEADER_SIZE}/{EXPECTED_RECORD_SIZE}"
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

    centre = document["scene_aabb"]["centre"]
    print(f"source db       : {args.db}")
    print(f"source sha256   : {source_sha}")
    print(f"builder/sde     : {metadata.get('builder_version')} / {metadata.get('sde_build')}")
    print(f"known-space     : {len(rows)}")
    print(f"w-space omitted : {wormhole}")
    print(f"scene centre    : ({centre['x']:.6f}, {centre['y']:.6f}, {centre['z']:.6f})")
    print(f"binary          : {args.out_bin} ({len(blob)} bytes, {binary_sha})")
    print(f"manifest        : {args.out_manifest}")
    print(f"anchors header  : {args.out_header}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
