"""Build the shared System One state from selected repository files."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
DEFAULT_MANIFEST = HERE / "state_files.json"


def load_json(path: Path) -> Any:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def repo_root() -> Path:
    return HERE.parents[1]


def build_state(
    root: Path | None = None,
    manifest_path: Path | None = None,
    char_budget: int | None = None,
) -> dict[str, Any]:
    root = root or repo_root()
    manifest = load_json(manifest_path or DEFAULT_MANIFEST)
    files = list(manifest["files"])
    drop_order = list(manifest.get("drop_last_if_over_budget") or [])
    budget = char_budget
    if budget is None:
        budget = int(manifest.get("english_char_budget_hint") or 150000)

    contents: dict[str, str] = {}
    missing: list[str] = []
    for rel in files:
        path = root / rel
        if not path.is_file():
            missing.append(rel)
            continue
        contents[rel] = path.read_text(encoding="utf-8")

    if missing:
        raise FileNotFoundError("state files missing: " + ", ".join(missing))

    dropped: list[str] = []
    while True:
        state = {
            "repository": "eo-map-carbon",
            "note": (
                "Answer only from these repository files. This is a TrinityAL "
                "consumption experiment snapshot, not generated commentary."
            ),
            "files": {rel: contents[rel] for rel in files if rel in contents},
        }
        encoded = json.dumps(state, ensure_ascii=False)
        if len(encoded) <= budget or not drop_order:
            break
        candidate = drop_order.pop(0)
        if candidate in files:
            files.remove(candidate)
            dropped.append(candidate)

    included = [rel for rel in manifest["files"] if rel in state["files"]]
    file_chars = {rel: len(text) for rel, text in state["files"].items()}
    return {
        "state": state,
        "included_files": included,
        "dropped_files": dropped,
        "file_chars": file_chars,
        "state_chars": len(json.dumps(state, ensure_ascii=False)),
        "request_token_budget": int(manifest.get("request_token_budget") or 32000),
        "english_char_budget_hint": budget,
    }


def estimate_tokens(char_count: int) -> dict[str, int]:
    """TypeSafe docs: ~32k tokens is roughly 150k English characters.

    Code is denser than English, so chars/3.5 and chars/4 are both reported.
    The API-reported input_tokens is authoritative after a live call.
    """
    return {
        "chars": char_count,
        "est_chars_div_4": char_count // 4,
        "est_chars_div_35": int(char_count / 3.5),
        "est_chars_div_3": char_count // 3,
    }
