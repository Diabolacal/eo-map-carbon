"""Load gold-set questions and convert them to TypeSafe SDK objects."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
DEFAULT_GOLD = HERE / "gold_set.json"


def load_gold(path: Path | None = None) -> dict[str, Any]:
    with (path or DEFAULT_GOLD).open(encoding="utf-8") as handle:
        gold = json.load(handle)
    questions = gold.get("questions") or []
    if not questions:
        raise ValueError("gold set has no questions")
    ids = [item["id"] for item in questions]
    if len(ids) != len(set(ids)):
        raise ValueError("gold set question ids are not unique")
    for item in questions:
        if item["type"] not in {"noul", "choice"}:
            raise ValueError(f"unsupported type for {item['id']}: {item['type']}")
        if "expected" not in item:
            raise ValueError(f"missing expected answer for {item['id']}")
        if "source" not in item:
            raise ValueError(f"missing source rationale for {item['id']}")
        if item["type"] == "choice":
            criteria = item.get("criteria") or {}
            if item["expected"] not in criteria:
                raise ValueError(f"expected choice {item['expected']!r} not in criteria for {item['id']}")
    return gold


def api_questions(gold: dict[str, Any]) -> dict[str, Any]:
    """JSON-serialisable question map sent to System One (no gold labels)."""
    out: dict[str, Any] = {}
    for item in gold["questions"]:
        body: dict[str, Any] = {
            "type": item["type"],
            "instructions": item["instructions"],
        }
        if item.get("criteria") is not None:
            body["criteria"] = item["criteria"]
        out[item["id"]] = body
    return out


def sdk_questions(gold: dict[str, Any]) -> dict[str, Any]:
    from typesafe_sdk import Choice, Noul

    out: dict[str, Any] = {}
    for item in gold["questions"]:
        if item["type"] == "noul":
            kwargs: dict[str, Any] = {"instructions": item["instructions"]}
            if item.get("criteria") is not None:
                kwargs["criteria"] = item["criteria"]
            out[item["id"]] = Noul(**kwargs)
        else:
            out[item["id"]] = Choice(
                instructions=item["instructions"],
                criteria=item["criteria"],
            )
    return out
