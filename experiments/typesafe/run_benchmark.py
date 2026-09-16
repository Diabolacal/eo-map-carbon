"""Run the isolated Jev repository-comprehension benchmark.

All gold-set questions share one System One state and are sent in a single
request per run. This is an experiment with TypeSafe Jev, not a Carbon feature.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent

from build_state import build_state, estimate_tokens
from questions import api_questions, load_gold
from score import aggregate_runs, revised_accuracy, score_run

DEFAULT_MODEL = "jev-latest"
DEFAULT_RUNS = 5
DEFAULT_TIMEOUT_S = 120.0


def utc_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def answers_from_sdk(response: Any) -> dict[str, Any]:
    raw_http = getattr(response, "raw_http_response", None)
    if raw_http is not None:
        try:
            payload = raw_http.json()
            if isinstance(payload, dict) and "answers" in payload:
                return payload["answers"]
        except Exception:
            pass

    out: dict[str, Any] = {}
    answers = getattr(response, "answers", {}) or {}
    for key, answer in answers.items():
        kind = getattr(answer, "type", None)
        if kind == "noul" or hasattr(answer, "noul"):
            out[key] = {"type": "noul", "noul": float(answer.noul)}
            continue
        item: dict[str, Any] = {"type": "choice"}
        if hasattr(answer, "choice"):
            item["choice"] = answer.choice
        if hasattr(answer, "probabilities"):
            item["probabilities"] = dict(answer.probabilities)
        if hasattr(answer, "confidence"):
            item["confidence"] = answer.confidence
        out[key] = item
    return out


def usage_from_sdk(response: Any) -> tuple[int | None, int | None]:
    usage = getattr(response, "usage", None)
    if usage is None:
        return None, None
    return getattr(usage, "input_tokens", None), getattr(usage, "output_tokens", None)


def sanitize_response_json(model: str, answers: dict[str, Any], input_tokens: int | None, output_tokens: int | None) -> dict[str, Any]:
    return {
        "model": model,
        "answers": answers,
        "usage": {
            "input_tokens": input_tokens,
            "output_tokens": output_tokens,
        },
    }


def measure_payload(state: dict[str, Any], questions: dict[str, Any], model: str) -> dict[str, Any]:
    body = {"state": state, "model": model, "questions": questions}
    encoded = json.dumps(body, ensure_ascii=False)
    estimates = estimate_tokens(len(encoded))
    return {
        "payload_chars": len(encoded),
        "payload_bytes_utf8": len(encoded.encode("utf-8")),
        "token_estimates": estimates,
        "request_token_budget_docs": 32000,
        "under_char_hint_150k": len(encoded) < 150000,
        "under_est_32k_div4": estimates["est_chars_div_4"] < 32000,
        "under_est_32k_div3": estimates["est_chars_div_3"] < 32000,
    }


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def format_pct(value: float) -> str:
    return f"{value * 100:.1f}%"


def render_report(
    *,
    gold: dict[str, Any],
    built: dict[str, Any],
    measure: dict[str, Any],
    run_results: list[dict[str, Any]],
    aggregate: dict[str, Any],
    defective: list[dict[str, Any]],
    listed_models: list[str] | None,
) -> str:
    questions = gold["questions"]
    noul_n = sum(1 for q in questions if q["type"] == "noul")
    choice_n = sum(1 for q in questions if q["type"] == "choice")
    lat = aggregate["latency_s"]
    inn = aggregate["input_tokens"]
    out = aggregate["output_tokens"]

    lines: list[str] = []
    lines.append("# Jev repository-comprehension benchmark")
    lines.append("")
    lines.append("Isolated TypeSafe Jev / System One experiment against the Milestone 1C")
    lines.append("`eo-map-carbon` snapshot. This is not a Carbon renderer milestone.")
    lines.append("")
    lines.append("Gold-set questions were written from the repository files **before** any Jev call.")
    lines.append("Questions were not silently retuned after seeing answers.")
    lines.append("")
    lines.append("## Setup")
    lines.append("")
    lines.append(f"- Model requested: `{DEFAULT_MODEL}` (SDK default is also `jev-latest`)")
    lines.append(f"- Models reported in completed runs: {', '.join(f'`{m}`' for m in aggregate['models']) or 'none'}")
    if listed_models:
        lines.append(f"- `client.models.list()`: {', '.join(f'`{m}`' for m in listed_models)}")
    lines.append(f"- Questions: {aggregate['question_count']} ({noul_n} Noul, {choice_n} Choice)")
    lines.append(f"- Runs attempted: {aggregate['runs_attempted']}")
    lines.append(f"- Runs completed: {aggregate['runs_completed']}")
    lines.append(f"- Shared state files: {', '.join(f'`{p}`' for p in built['included_files'])}")
    if built["dropped_files"]:
        lines.append(f"- Dropped to fit char budget: {', '.join(f'`{p}`' for p in built['dropped_files'])}")
    lines.append(f"- State JSON chars: {built['state_chars']}")
    lines.append(
        f"- Full request payload: {measure['payload_chars']} chars "
        f"(est. tokens chars/4={measure['token_estimates']['est_chars_div_4']}, "
        f"chars/3={measure['token_estimates']['est_chars_div_3']}; docs budget ~32k tokens / ~150k English chars)"
    )
    lines.append("- All questions for a run were sent in **one** System One request.")
    lines.append("")
    lines.append("## Headline results")
    lines.append("")
    lines.append(f"- Overall accuracy: **{format_pct(aggregate['overall_accuracy'])}**")
    lines.append(f"  ({aggregate['overall_correct']} / {aggregate['overall_total']} scored answers)")
    for row in aggregate["accuracy_per_run"]:
        lines.append(f"- Run {row['run']}: {format_pct(row['accuracy'])} ({row['correct']} / {aggregate['question_count']})")
    if lat["n"]:
        lines.append(
            f"- API latency seconds: min={lat['min']:.3f}, median={lat['median']:.3f}, max={lat['max']:.3f}"
        )
    if inn["n"]:
        lines.append(
            f"- Input tokens: min={inn['min']}, median={inn['median']}, max={inn['max']}"
        )
    if out["n"]:
        lines.append(
            f"- Output tokens: min={out['min']}, median={out['median']}, max={out['max']}"
        )
    lines.append(
        f"- Repeated answers stable: {'yes' if aggregate['repeated_answers_stable'] else 'no'} "
        f"({aggregate['stable_question_count']} / {aggregate['question_count']} questions identical across completed runs)"
    )
    lines.append("")
    lines.append("## Research questions")
    lines.append("")
    lines.append("1. **Can Jev accurately understand technical facts distributed across this small real codebase?**")
    lines.append(f"   Overall accuracy {format_pct(aggregate['overall_accuracy'])} on {aggregate['question_count']} gold facts.")
    lines.append("2. **Does its parallel-question architecture actually give very low latency for 30–40 judgments?**")
    if lat["n"]:
        lines.append(
            f"   One request of {aggregate['question_count']} questions returned in "
            f"{lat['min']:.3f}–{lat['max']:.3f}s (median {lat['median']:.3f}s)."
        )
    else:
        lines.append("   No completed latency samples.")
    lines.append("3. **Are probability/confidence values useful when it is uncertain?**")
    lines.append(
        f"   Low-confidence correct: {len(aggregate['low_confidence_correct'])}. "
        f"High-confidence incorrect: {len(aggregate['high_confidence_incorrect'])}."
    )
    lines.append("4. **Are repeated runs stable?**")
    lines.append(
        f"   {'Stable' if aggregate['repeated_answers_stable'] else 'Unstable'}: "
        f"{len(aggregate['disagreements'])} questions disagreed across runs."
    )
    lines.append("5. **Is this model interesting enough to investigate for future classification/verification workflows?**")
    lines.append("   See closeout notes in this report's discussion of errors and confidence.")
    lines.append("")
    lines.append("## Incorrect answers")
    lines.append("")
    if not aggregate["incorrect"]:
        lines.append("None.")
    else:
        for item in aggregate["incorrect"]:
            lines.append(
                f"- `{item['id']}` run {item['run']}: expected `{item['expected']}`, "
                f"predicted `{item['predicted']}`, confidence={item['confidence']}, noul={item['noul']}"
            )
    lines.append("")
    lines.append("## Low-confidence correct")
    lines.append("")
    if not aggregate["low_confidence_correct"]:
        lines.append("None.")
    else:
        for item in aggregate["low_confidence_correct"]:
            lines.append(
                f"- `{item['id']}` run {item['run']}: predicted `{item['predicted']}`, "
                f"confidence={item['confidence']}, noul={item['noul']}"
            )
    lines.append("")
    lines.append("## High-confidence incorrect")
    lines.append("")
    if not aggregate["high_confidence_incorrect"]:
        lines.append("None.")
    else:
        for item in aggregate["high_confidence_incorrect"]:
            lines.append(
                f"- `{item['id']}` run {item['run']}: expected `{item['expected']}`, "
                f"predicted `{item['predicted']}`, confidence={item['confidence']}, noul={item['noul']}"
            )
    lines.append("")
    lines.append("## Run-to-run disagreements")
    lines.append("")
    if not aggregate["disagreements"]:
        lines.append("None. Every completed run produced the same predicted label per question.")
    else:
        for item in aggregate["disagreements"]:
            lines.append(
                f"- `{item['id']}` expected `{item['expected']}`; predictions {item['predictions']}"
            )
    lines.append("")
    lines.append("## API errors / rate limits")
    lines.append("")
    if not aggregate["failures"]:
        lines.append("None.")
    else:
        for item in aggregate["failures"]:
            lines.append(f"- Run {item['run']}: {item['error_type']}: {item['error']}")
    lines.append("")
    lines.append("## Defective questions")
    lines.append("")
    if not defective:
        lines.append("None identified. Original score is the only score.")
    else:
        lines.append("These questions are kept in the original score. A revised score excluding them is shown only as a footnote.")
        for item in defective:
            lines.append(f"- `{item['id']}`: {item['reason']}")
        excluded = {item["id"] for item in defective}
        revised = revised_accuracy(run_results, excluded)
        lines.append("")
        lines.append(
            f"Revised overall accuracy excluding those ids: {format_pct(revised['overall_accuracy'])} "
            f"({revised['overall_correct']} / {revised['overall_total']})"
        )
    lines.append("")
    lines.append("## Gold set")
    lines.append("")
    lines.append("Each question has an expected answer and a source rationale in `gold_set.json`.")
    lines.append("Noul yes/no uses threshold 0.5 on the returned probability. Choice uses the selected option.")
    lines.append("Noul has no API confidence field; certainty is `abs(noul - 0.5) * 2`.")
    lines.append("")
    return "\n".join(lines) + "\n"


def run_once(client: Any, state: dict[str, Any], questions: dict[str, Any], model: str) -> dict[str, Any]:
    started = time.perf_counter()
    response = client.system_one(
        state=state,
        questions=questions,
        model=model,
        timeout=DEFAULT_TIMEOUT_S,
    )
    latency = time.perf_counter() - started
    answers = answers_from_sdk(response)
    input_tokens, output_tokens = usage_from_sdk(response)
    reported_model = getattr(response, "model", None) or model
    return {
        "ok": True,
        "latency_s": latency,
        "model": reported_model,
        "input_tokens": input_tokens,
        "output_tokens": output_tokens,
        "answers": answers,
        "response_json": sanitize_response_json(reported_model, answers, input_tokens, output_tokens),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Jev System One repo-comprehension benchmark")
    parser.add_argument("--runs", type=int, default=DEFAULT_RUNS)
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--measure-only", action="store_true")
    parser.add_argument("--results-dir", type=Path, default=HERE / "results")
    parser.add_argument("--report", type=Path, default=HERE / "REPORT.md")
    parser.add_argument("--pause-s", type=float, default=2.0, help="Pause between completed runs")
    args = parser.parse_args(argv)

    gold = load_gold()
    questions = api_questions(gold)
    built = build_state()
    measure = measure_payload(built["state"], questions, args.model)

    print(f"gold questions: {len(gold['questions'])}")
    print(f"included files: {built['included_files']}")
    print(f"dropped files: {built['dropped_files']}")
    print(f"state chars: {built['state_chars']}")
    print(f"payload chars: {measure['payload_chars']}")
    print(f"token estimates: {measure['token_estimates']}")

    if args.measure_only:
        write_json(args.results_dir / "measure.json", {"built": {k: v for k, v in built.items() if k != "state"}, "measure": measure})
        return 0

    if not os.environ.get("TYPESAFE_API_KEY"):
        print("TYPESAFE_API_KEY is not set", file=sys.stderr)
        return 2

    from typesafe_sdk import RetryPolicy, TypeSafeClient
    from typesafe_sdk import TypeSafeRateLimitError, TypeSafeAPIError, TypeSafeError

    listed_models: list[str] | None = None
    run_results: list[dict[str, Any]] = []
    results_dir = args.results_dir
    results_dir.mkdir(parents=True, exist_ok=True)
    session_dir = results_dir / utc_stamp()
    session_dir.mkdir(parents=True, exist_ok=True)

    write_json(session_dir / "measure.json", {"built": {k: v for k, v in built.items() if k != "state"}, "measure": measure})

    retry = RetryPolicy(max_retries=0, timeout=DEFAULT_TIMEOUT_S)
    stop_remaining = False
    with TypeSafeClient(timeout=DEFAULT_TIMEOUT_S, retry=retry, model=args.model) as client:
        try:
            listed = client.models.list()
            if hasattr(listed, "models"):
                listed_models = [getattr(m, "id", None) or getattr(m, "name", None) or str(m) for m in listed.models]
            elif isinstance(listed, list):
                listed_models = [getattr(m, "id", None) or str(m) for m in listed]
            else:
                listed_models = [str(listed)]
            print(f"available models: {listed_models}")
        except TypeSafeError as exc:
            print(f"models.list failed: {type(exc).__name__}: {exc}")

        for index in range(1, args.runs + 1):
            if stop_remaining:
                run_results.append(
                    {
                        "run": index,
                        "ok": False,
                        "error_type": "skipped_after_rate_limit",
                        "error": "Not sent; a previous run was rate-limited.",
                    }
                )
                continue
            print(f"run {index}/{args.runs}: one System One request, {len(questions)} questions")
            try:
                live = run_once(client, built["state"], questions, args.model)
            except TypeSafeRateLimitError as exc:
                run_results.append(
                    {
                        "run": index,
                        "ok": False,
                        "error_type": type(exc).__name__,
                        "error": str(exc),
                        "status": getattr(exc, "status", None),
                    }
                )
                print(f"run {index} rate-limited; preserving completed runs and stopping")
                stop_remaining = True
                continue
            except TypeSafeAPIError as exc:
                run_results.append(
                    {
                        "run": index,
                        "ok": False,
                        "error_type": type(exc).__name__,
                        "error": str(exc),
                        "status": getattr(exc, "status", None),
                    }
                )
                print(f"run {index} API error: {type(exc).__name__}: {exc}")
                if getattr(exc, "status", None) in {429, 529}:
                    stop_remaining = True
                continue
            except TypeSafeError as exc:
                run_results.append(
                    {
                        "run": index,
                        "ok": False,
                        "error_type": type(exc).__name__,
                        "error": str(exc),
                    }
                )
                print(f"run {index} failed: {type(exc).__name__}: {exc}")
                continue

            live["run"] = index
            live["score"] = score_run(gold["questions"], live["answers"], gold)
            run_results.append(live)
            write_json(session_dir / f"run-{index:02d}.json", live["response_json"])
            print(
                f"run {index} ok model={live['model']} latency={live['latency_s']:.3f}s "
                f"in={live['input_tokens']} out={live['output_tokens']} "
                f"accuracy={live['score']['accuracy']:.3f}"
            )
            if index < args.runs and args.pause_s > 0 and not stop_remaining:
                time.sleep(args.pause_s)

    aggregate = aggregate_runs(run_results, gold)
    defective: list[dict[str, Any]] = []
    report = render_report(
        gold=gold,
        built=built,
        measure=measure,
        run_results=run_results,
        aggregate=aggregate,
        defective=defective,
        listed_models=listed_models,
    )
    args.report.write_text(report, encoding="utf-8")
    summary = {
        "model": args.model,
        "listed_models": listed_models,
        "measure": measure,
        "included_files": built["included_files"],
        "dropped_files": built["dropped_files"],
        "aggregate": aggregate,
        "defective_questions": defective,
        "runs": [
            {
                "run": run["run"],
                "ok": run.get("ok"),
                "error_type": run.get("error_type"),
                "error": run.get("error"),
                "latency_s": run.get("latency_s"),
                "model": run.get("model"),
                "input_tokens": run.get("input_tokens"),
                "output_tokens": run.get("output_tokens"),
                "accuracy": (run.get("score") or {}).get("accuracy"),
                "answers": (run.get("score") or {}).get("answers"),
            }
            for run in run_results
        ],
    }
    write_json(session_dir / "summary.json", summary)
    write_json(HERE / "last_summary.json", summary)
    print(f"wrote {args.report}")
    print(f"wrote {session_dir}")
    print(f"overall accuracy={aggregate['overall_accuracy']:.3f} completed={aggregate['runs_completed']}")
    return 0 if aggregate["runs_completed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
