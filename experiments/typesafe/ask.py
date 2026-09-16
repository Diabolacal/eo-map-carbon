"""One-shot System One query over named repository files.

Used by the Jita-Amarr routing experiment for preflight and postflight
fan-out. Does not score against gold; it records answers, latency, and usage.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]

from build_state import estimate_tokens
from run_benchmark import (
    DEFAULT_MODEL,
    DEFAULT_TIMEOUT_S,
    answers_from_sdk,
    sanitize_response_json,
    usage_from_sdk,
    write_json,
)


def load_questions(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    questions = payload.get("questions") or payload
    if not isinstance(questions, dict) or not questions:
        raise ValueError(f"no questions in {path}")
    return questions


def build_named_state(files: list[str], extras: list[tuple[str, Path]], note: str) -> dict[str, Any]:
    named: dict[str, str] = {}
    for rel in files:
        path = REPO_ROOT / rel
        named[rel] = path.read_text(encoding="utf-8")
    extra_named: dict[str, str] = {}
    for label, path in extras:
        extra_named[label] = path.read_text(encoding="utf-8")
    return {
        "note": note,
        "files": named,
        "external_readonly": extra_named,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Single TypeSafe System One fan-out")
    parser.add_argument("--questions", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--file", action="append", dest="files", default=[])
    parser.add_argument(
        "--extra",
        action="append",
        default=[],
        metavar="LABEL=PATH",
        help="Read-only extra document, e.g. eomap_hopCount=C:/dev/eo-map/...",
    )
    parser.add_argument("--note", default="")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--measure-only", action="store_true")
    args = parser.parse_args(argv)

    extras: list[tuple[str, Path]] = []
    for item in args.extra:
        if "=" not in item:
            print(f"invalid --extra {item!r}, expected LABEL=PATH", file=sys.stderr)
            return 2
        label, raw = item.split("=", 1)
        extras.append((label, Path(raw)))

    questions = load_questions(args.questions)
    state = build_named_state(args.files, extras, args.note)
    body = {"state": state, "model": args.model, "questions": questions}
    encoded = json.dumps(body, ensure_ascii=False)
    measure = {
        "payload_chars": len(encoded),
        "payload_bytes_utf8": len(encoded.encode("utf-8")),
        "token_estimates": estimate_tokens(len(encoded)),
        "file_chars": {name: len(text) for name, text in state["files"].items()},
        "extra_chars": {name: len(text) for name, text in state["external_readonly"].items()},
        "question_count": len(questions),
    }
    print(f"questions: {len(questions)}")
    print(f"files: {args.files}")
    print(f"payload chars: {measure['payload_chars']}")
    print(f"token estimates: {measure['token_estimates']}")

    if args.measure_only:
        write_json(args.out, {"ok": True, "measure": measure, "questions": questions})
        return 0

    if not os.environ.get("TYPESAFE_API_KEY"):
        print("TYPESAFE_API_KEY is not set", file=sys.stderr)
        return 2

    from typesafe_sdk import RetryPolicy, TypeSafeClient

    retry = RetryPolicy(max_retries=0, timeout=DEFAULT_TIMEOUT_S)
    started = time.perf_counter()
    with TypeSafeClient(timeout=DEFAULT_TIMEOUT_S, retry=retry, model=args.model) as client:
        response = client.system_one(
            state=state,
            questions=questions,
            model=args.model,
            timeout=DEFAULT_TIMEOUT_S,
        )
    latency = time.perf_counter() - started
    answers = answers_from_sdk(response)
    input_tokens, output_tokens = usage_from_sdk(response)
    model = getattr(response, "model", None) or args.model
    result = {
        "ok": True,
        "model": model,
        "latency_s": latency,
        "input_tokens": input_tokens,
        "output_tokens": output_tokens,
        "measure": measure,
        "answers": answers,
        "response_json": sanitize_response_json(model, answers, input_tokens, output_tokens),
        "files": args.files,
        "note": args.note,
    }
    write_json(args.out, result)
    print(f"model={model} latency={latency:.3f}s in={input_tokens} out={output_tokens}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
