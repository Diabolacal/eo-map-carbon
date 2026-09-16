"""Run the synthetic Jev commander benchmark.

Order for live work: unit tests are separate; this script then probe ->
small repeat -> scenarios -> sensitivity -> stability -> cadence.

Does not print, log, or write TYPESAFE_API_KEY.
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
TYPESAFE = HERE.parent
if str(TYPESAFE) not in sys.path:
    sys.path.insert(0, str(TYPESAFE))
if str(HERE.parent) not in sys.path:
    sys.path.insert(0, str(HERE.parent))

from commander.client import (  # noqa: E402
    COMMANDER_TIMEOUT_S,
    DEFAULT_MODEL_NAME,
    InferenceBudget,
    infer_once,
    list_models,
    measure_payload,
    open_client,
)
from commander.controller import compose_command, deterministic_command, synthetic_answers  # noqa: E402
from commander.evaluate import (  # noqa: E402
    cadence_summary,
    compact_answers,
    compare_deterministic,
    evaluate_review,
    evaluate_sensitivity_pair,
    evaluate_stability,
    event_driven_illustration,
    scale_table,
    token_stats,
)
from commander.questions import build_questions, question_count  # noqa: E402
from commander.report import render_report  # noqa: E402
from commander.scenarios import (  # noqa: E402
    build_main_scenarios,
    build_sensitivity_pairs,
    cadence_scenario_id,
    scenario_by_id,
    stability_scenario_ids,
)
from commander.state import to_jev_state  # noqa: E402
from run_benchmark import write_json  # noqa: E402

RESULTS_ROOT = TYPESAFE / "results" / "commander"
SUMMARY_DIR = HERE / "summaries"
REPORT_PATH = TYPESAFE / "COMMANDER_BENCHMARK.md"
DEFAULT_BUDGET = 250
DEFAULT_CADENCE_SAMPLES = 40
DEFAULT_STABILITY_REPEATS = 8
CADENCE_HZ = (1, 2, 5, 10)


def utc_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def key_present() -> bool:
    return bool(os.environ.get("TYPESAFE_API_KEY"))


def dump_raw(session_dir: Path, name: str, payload: dict[str, Any]) -> None:
    raw = payload.get("response_json")
    if raw is None:
        return
    write_json(session_dir / name, raw)


def record_failure(bucket: list[dict[str, Any]], phase: str, result: dict[str, Any]) -> None:
    bucket.append(
        {
            "phase": phase,
            "scenario_id": result.get("scenario_id"),
            "error_type": result.get("error_type"),
            "error": result.get("error"),
            "error_class": result.get("error_class"),
            "status": result.get("status"),
        }
    )


def run_inference(
    client: Any | None,
    encounter: Any,
    *,
    dry_run: bool,
    model: str,
    budget: InferenceBudget,
    session_dir: Path,
    raw_name: str,
) -> dict[str, Any]:
    if dry_run:
        if not budget.allow():
            return {
                "ok": False,
                "skipped": True,
                "error_type": "budget_exhausted",
                "error": budget.stop_reason or "call budget exhausted",
                "error_class": "budget",
                "scenario_id": encounter.scenario_id,
                "dry_run": True,
            }
        answers = synthetic_answers(encounter)
        budget.used += 1
        return {
            "ok": True,
            "dry_run": True,
            "latency_s": 0.0,
            "model": "dry-run",
            "input_tokens": None,
            "output_tokens": None,
            "answers": answers,
            "scenario_id": encounter.scenario_id,
        }
    assert client is not None
    result = infer_once(client, encounter, model=model, budget=budget)
    if result.get("ok"):
        dump_raw(session_dir, raw_name, result)
    return result


def scenario_row(scenario: Any, result: dict[str, Any]) -> dict[str, Any]:
    det = deterministic_command(scenario.encounter)
    row: dict[str, Any] = {
        "id": scenario.id,
        "title": scenario.title,
        "why": scenario.why,
        "ok": bool(result.get("ok")),
        "latency_s": result.get("latency_s"),
        "input_tokens": result.get("input_tokens"),
        "output_tokens": result.get("output_tokens"),
        "model": result.get("model"),
        "deterministic": {
            "tactical_posture": det.tactical_posture,
            "formation_intent": det.formation_intent,
            "reinforcement_action": det.reinforcement_action,
            "primary_target": det.primary_target,
        },
    }
    if not result.get("ok"):
        row.update(
            {
                "error_type": result.get("error_type"),
                "error": result.get("error"),
                "error_class": result.get("error_class"),
                "review_flags": [],
                "review_flag_count": 0,
            }
        )
        return row
    answers = result["answers"]
    applied = compose_command(scenario.encounter, answers)
    compact = compact_answers(answers)
    noul = {k: v.get("noul") for k, v in compact.items() if v.get("type") == "noul"}
    choices = {k: v for k, v in compact.items() if v.get("type") == "choice"}
    flags = evaluate_review(scenario, answers)
    row.update(
        {
            "jev_command": {
                "tactical_posture": applied.command.tactical_posture,
                "formation_intent": applied.command.formation_intent,
                "reinforcement_action": applied.command.reinforcement_action,
                "primary_target": applied.command.primary_target,
            },
            "field_sources": applied.field_sources,
            "noul": noul,
            "choices": choices,
            "review_flags": flags,
            "review_flag_count": len(flags),
            "compare": compare_deterministic(scenario.encounter, answers),
        }
    )
    return row


def run_cadence(
    client: Any | None,
    encounter: Any,
    hz: float,
    samples: int,
    *,
    dry_run: bool,
    model: str,
    budget: InferenceBudget,
    session_dir: Path,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    interval = 1.0 / hz
    rows: list[dict[str, Any]] = []
    t0 = time.perf_counter()
    for index in range(samples):
        if not budget.allow():
            break
        call_started = time.perf_counter()
        result = run_inference(
            client,
            encounter,
            dry_run=dry_run,
            model=model,
            budget=budget,
            session_dir=session_dir,
            raw_name=f"cadence-{hz:g}hz-{index + 1:02d}.json",
        )
        rows.append(result)
        if result.get("error_class") == "rate_limit":
            break
        elapsed = time.perf_counter() - call_started
        remaining = interval - elapsed
        if remaining > 0 and not dry_run:
            time.sleep(remaining)
    wall = time.perf_counter() - t0
    return cadence_summary(hz, rows, wall), rows


def print_probe(result: dict[str, Any], encounter: Any) -> None:
    print(f"probe scenario={encounter.scenario_id} ok={result.get('ok')} model={result.get('model')}")
    print(f"latency_s={result.get('latency_s')} in={result.get('input_tokens')} out={result.get('output_tokens')}")
    if not result.get("ok"):
        print(f"error {result.get('error_type')}: {result.get('error')}")
        return
    compact = compact_answers(result["answers"])
    print(json.dumps(compact, indent=2, ensure_ascii=False))
    applied = compose_command(encounter, result["answers"])
    print(
        "composed "
        f"posture={applied.command.tactical_posture} "
        f"formation={applied.command.formation_intent} "
        f"reinforce={applied.command.reinforcement_action} "
        f"target={applied.command.primary_target} "
        f"source={applied.source}"
    )


def controller_corpus_rows() -> list[dict[str, Any]]:
    rows = []
    for scenario in build_main_scenarios():
        cmd = deterministic_command(scenario.encounter)
        rows.append(
            {
                "id": scenario.id,
                "title": scenario.title,
                "command": {
                    "tactical_posture": cmd.tactical_posture,
                    "formation_intent": cmd.formation_intent,
                    "reinforcement_action": cmd.reinforcement_action,
                    "primary_target": cmd.primary_target,
                },
            }
        )
    return rows


def build_offline_summary(
    *,
    model: str,
    measure: dict[str, Any],
    example_state: dict[str, Any],
    qcount: dict[str, int],
    reason: str,
) -> dict[str, Any]:
    est = ((measure.get("token_estimates") or {}).get("est_chars_div_4"))
    scale = scale_table(int(est)) if est else {}
    if scale:
        scale["estimate_only"] = True
        scale["estimate_note"] = "chars/4 of the serialized request; not API-reported input_tokens"
    main = build_main_scenarios()
    return {
        "live": False,
        "reason": reason,
        "model": model,
        "measure": measure,
        "example_state": example_state,
        "question_count": qcount,
        "scenarios": [{"id": s.id, "title": s.title, "why": s.why} for s in main],
        "sensitivity": [
            {
                "id": p.id,
                "title": p.title,
                "changed_field": p.changed_field,
                "accounting_notes": p.accounting_notes,
            }
            for p in build_sensitivity_pairs()
        ],
        "stability": [],
        "cadence": [],
        "controller_corpus": controller_corpus_rows(),
        "event_driven": event_driven_illustration([s.encounter for s in main]),
        "scale": scale,
        "live_calls": 0,
        "generated_utc": utc_stamp(),
    }


def build_summary(
    *,
    live: bool,
    model: str,
    listed_models: list[str] | None,
    measure: dict[str, Any],
    example_state: dict[str, Any],
    qcount: dict[str, int],
    probe: dict[str, Any] | None,
    repeats: list[dict[str, Any]],
    scenario_rows: list[dict[str, Any]],
    sensitivity_rows: list[dict[str, Any]],
    stability_rows: list[dict[str, Any]],
    cadence_rows: list[dict[str, Any]],
    failures: list[dict[str, Any]],
    live_calls: int,
    stop_reason: str | None,
    all_live_results: list[dict[str, Any]],
) -> dict[str, Any]:
    models = sorted({r.get("model") for r in all_live_results if r.get("ok") and r.get("model")})
    tok = token_stats(all_live_results)
    representative = (tok.get("input") or {}).get("median")
    scale = scale_table(int(representative)) if representative else {}
    questionable = []
    for row in scenario_rows:
        questionable.extend(row.get("review_flags") or [])
    agrees = {"n": 0, "posture_agree": 0, "formation_agree": 0, "reinforce_agree": 0, "target_agree": 0, "disagreements": []}
    for row in scenario_rows:
        cmp_ = row.get("compare")
        if not cmp_:
            continue
        agrees["n"] += 1
        mapping = {
            "posture_agree": ("tactical_posture", "posture_agree"),
            "formation_agree": ("formation_intent", "formation_agree"),
            "reinforce_agree": ("reinforcement_action", "reinforce_agree"),
            "target_agree": ("primary_target", "target_agree"),
        }
        for key, (field, flag) in mapping.items():
            if cmp_.get(flag):
                agrees[key] += 1
            else:
                agrees["disagreements"].append(
                    {
                        "id": row["id"],
                        "field": field,
                        "det": (cmp_.get("deterministic") or {}).get(field),
                        "jev": (cmp_.get("applied") or {}).get("command", {}).get(field),
                    }
                )
    main = build_main_scenarios()
    event = event_driven_illustration([s.encounter for s in main])
    return {
        "live": live,
        "model": model,
        "listed_models": listed_models,
        "models": models,
        "measure": measure,
        "example_state": example_state,
        "question_count": qcount,
        "probe": None
        if probe is None
        else {
            "ok": probe.get("ok"),
            "latency_s": probe.get("latency_s"),
            "input_tokens": probe.get("input_tokens"),
            "output_tokens": probe.get("output_tokens"),
            "model": probe.get("model"),
            "answers": compact_answers(probe["answers"]) if probe.get("ok") else None,
            "error_type": probe.get("error_type"),
            "error": probe.get("error"),
        },
        "small_repeat_n": len(repeats),
        "scenarios": scenario_rows,
        "sensitivity": sensitivity_rows,
        "stability": stability_rows,
        "cadence": cadence_rows,
        "questionable": questionable,
        "failures": failures,
        "token_stats": tok,
        "representative_input_tokens": representative,
        "scale": scale,
        "deterministic_notes": agrees,
        "event_driven": event,
        "controller_corpus": controller_corpus_rows(),
        "live_calls": live_calls,
        "stop_reason": stop_reason,
        "generated_utc": utc_stamp(),
    }


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Jev synthetic commander benchmark")
    parser.add_argument("--model", default=DEFAULT_MODEL_NAME)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--measure-only", action="store_true")
    parser.add_argument("--phase", choices=("probe", "repeat", "scenarios", "sensitivity", "stability", "cadence", "all"), default="all")
    parser.add_argument("--budget", type=int, default=DEFAULT_BUDGET)
    parser.add_argument("--cadence-samples", type=int, default=DEFAULT_CADENCE_SAMPLES)
    parser.add_argument("--stability-repeats", type=int, default=DEFAULT_STABILITY_REPEATS)
    parser.add_argument("--report", type=Path, default=REPORT_PATH)
    parser.add_argument("--results-dir", type=Path, default=RESULTS_ROOT)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    catalog = scenario_by_id()
    main_scenarios = build_main_scenarios()
    pairs = build_sensitivity_pairs()
    probe_scenario = catalog["initial_encounter"]
    cadence_encounter = catalog[cadence_scenario_id()].encounter
    qcount = question_count(build_questions(probe_scenario.encounter))
    measure = measure_payload(probe_scenario.encounter, args.model)
    example_state = to_jev_state(probe_scenario.encounter)

    print(f"questions: {qcount}")
    print(f"state chars: {measure['state_chars']}")
    print(f"payload chars: {measure['payload_chars']}")
    print(f"token estimates: {measure['token_estimates']}")
    print(f"main scenarios: {len(main_scenarios)}")
    print(f"sensitivity pairs: {len(pairs)}")

    if args.measure_only:
        write_json(args.results_dir / "measure.json", measure)
        return 0

    dry_run = bool(args.dry_run)
    if not dry_run and not key_present():
        print("TYPESAFE_API_KEY is not set; refusing to fabricate live results.", file=sys.stderr)
        print("Re-run with --dry-run for harness-only output, or export the key for a live suite.", file=sys.stderr)
        return 2

    live = not dry_run
    budget = InferenceBudget(args.budget)
    session_dir = args.results_dir / utc_stamp()
    session_dir.mkdir(parents=True, exist_ok=True)
    write_json(session_dir / "measure.json", measure)

    listed_models: list[str] | None = None
    client = None
    client_cm = None
    if live:
        client_cm = open_client(args.model, COMMANDER_TIMEOUT_S)
        client = client_cm.__enter__()
        listed_models = list_models(client)
        if listed_models:
            print(f"available models: {listed_models}")

    failures: list[dict[str, Any]] = []
    all_results: list[dict[str, Any]] = []
    probe_result: dict[str, Any] | None = None
    repeats: list[dict[str, Any]] = []
    scenario_rows: list[dict[str, Any]] = []
    sensitivity_rows: list[dict[str, Any]] = []
    stability_rows: list[dict[str, Any]] = []
    cadence_rows: list[dict[str, Any]] = []

    def still() -> bool:
        return budget.allow()

    try:
        want = args.phase
        if want in {"probe", "all"}:
            print("phase probe: one live request")
            probe_result = run_inference(
                client,
                probe_scenario.encounter,
                dry_run=dry_run,
                model=args.model,
                budget=budget,
                session_dir=session_dir,
                raw_name="probe.json",
            )
            all_results.append(probe_result)
            print_probe(probe_result, probe_scenario.encounter)
            if not probe_result.get("ok"):
                record_failure(failures, "probe", probe_result)
                if probe_result.get("error_class") in {"rate_limit", "timeout"} and live:
                    budget.halt(probe_result.get("error_class") or "probe_failed")
                    print("probe failed; not escalating")

        if want in {"repeat", "all"} and still() and (probe_result is None or probe_result.get("ok")):
            print("phase repeat: four more identical calls")
            for index in range(4):
                if not still():
                    break
                result = run_inference(
                    client,
                    probe_scenario.encounter,
                    dry_run=dry_run,
                    model=args.model,
                    budget=budget,
                    session_dir=session_dir,
                    raw_name=f"repeat-{index + 1:02d}.json",
                )
                repeats.append(result)
                all_results.append(result)
                print(
                    f"repeat {index + 1} ok={result.get('ok')} latency={result.get('latency_s')} "
                    f"in={result.get('input_tokens')}"
                )
                if not result.get("ok"):
                    record_failure(failures, "repeat", result)
                    if result.get("error_class") == "rate_limit":
                        break

        if want in {"scenarios", "all"} and still():
            print("phase scenarios")
            for scenario in main_scenarios:
                if not still():
                    break
                result = run_inference(
                    client,
                    scenario.encounter,
                    dry_run=dry_run,
                    model=args.model,
                    budget=budget,
                    session_dir=session_dir,
                    raw_name=f"scenario-{scenario.id}.json",
                )
                all_results.append(result)
                row = scenario_row(scenario, result)
                scenario_rows.append(row)
                if result.get("ok"):
                    print(
                        f"{scenario.id}: posture={row['jev_command']['tactical_posture']} "
                        f"target={row['jev_command']['primary_target']} "
                        f"reinf={row['jev_command']['reinforcement_action']} "
                        f"flags={row['review_flag_count']} "
                        f"lat={result.get('latency_s'):.3f}s in={result.get('input_tokens')}"
                    )
                else:
                    record_failure(failures, "scenarios", result)
                    print(f"{scenario.id} FAILED {result.get('error_type')}")
                    if result.get("error_class") == "rate_limit":
                        break

        if want in {"sensitivity", "all"} and still():
            print("phase sensitivity")
            answers_by_id: dict[str, dict[str, Any]] = {}
            for pair in pairs:
                if not still():
                    break
                pair_ok = True
                for side, scenario in (("base", pair.base), ("variant", pair.variant)):
                    if not still():
                        pair_ok = False
                        break
                    result = run_inference(
                        client,
                        scenario.encounter,
                        dry_run=dry_run,
                        model=args.model,
                        budget=budget,
                        session_dir=session_dir,
                        raw_name=f"sens-{pair.id}-{side}.json",
                    )
                    all_results.append(result)
                    if not result.get("ok"):
                        record_failure(failures, "sensitivity", result)
                        print(f"{pair.id} {side} FAILED {result.get('error_type')}")
                        pair_ok = False
                        if result.get("error_class") == "rate_limit":
                            break
                    else:
                        answers_by_id[scenario.id] = result["answers"]
                        print(
                            f"{pair.id} {side} ok lat={result.get('latency_s'):.3f}s in={result.get('input_tokens')}"
                        )
                if pair_ok and pair.base.id in answers_by_id and pair.variant.id in answers_by_id:
                    evaluated = evaluate_sensitivity_pair(
                        pair, answers_by_id[pair.base.id], answers_by_id[pair.variant.id]
                    )
                    sensitivity_rows.append(evaluated)
                    print(f"{pair.id} directional_ok={evaluated['passed']}")
                elif pair_ok:
                    sensitivity_rows.append(
                        {
                            "id": pair.id,
                            "title": pair.title,
                            "changed_field": pair.changed_field,
                            "accounting_notes": pair.accounting_notes,
                            "passed": False,
                            "expectations": [],
                            "error": "missing answers",
                        }
                    )
                if budget.stop_reason:
                    break

        if want in {"stability", "all"} and still():
            print("phase stability")
            for sid in stability_scenario_ids():
                if not still():
                    break
                scenario = catalog[sid]
                runs: list[dict[str, Any]] = []
                for index in range(args.stability_repeats):
                    if not still():
                        break
                    result = run_inference(
                        client,
                        scenario.encounter,
                        dry_run=dry_run,
                        model=args.model,
                        budget=budget,
                        session_dir=session_dir,
                        raw_name=f"stab-{sid}-{index + 1:02d}.json",
                    )
                    runs.append(result)
                    all_results.append(result)
                    if not result.get("ok"):
                        record_failure(failures, "stability", result)
                        if result.get("error_class") == "rate_limit":
                            break
                stability_rows.append(evaluate_stability(sid, runs))
                print(
                    f"stability {sid}: agree={stability_rows[-1].get('choice_agreement_rate')} "
                    f"oscillation={stability_rows[-1].get('material_oscillation')}"
                )
                if budget.stop_reason:
                    break

        if want in {"cadence", "all"} and still():
            print("phase cadence")
            for hz in CADENCE_HZ:
                if not still():
                    break
                print(f"cadence {hz} Hz x {args.cadence_samples}")
                summary_row, raw_rows = run_cadence(
                    client,
                    cadence_encounter,
                    hz,
                    args.cadence_samples,
                    dry_run=dry_run,
                    model=args.model,
                    budget=budget,
                    session_dir=session_dir,
                )
                cadence_rows.append(summary_row)
                all_results.extend(raw_rows)
                for raw in raw_rows:
                    if not raw.get("ok"):
                        record_failure(failures, f"cadence_{hz:g}hz", raw)
                print(
                    f"{hz} Hz p50={summary_row.get('p50_s')} p95={summary_row.get('p95_s')} "
                    f"max={summary_row.get('max_s')} achieved={summary_row.get('achieved_hz')} "
                    f"misses={summary_row.get('deadline_misses')}"
                )
                if budget.stop_reason:
                    break
    finally:
        if client_cm is not None:
            try:
                client_cm.__exit__(None, None, None)
            except Exception:
                pass

    live_calls = budget.used if live else 0
    if live:
        summary = build_summary(
            live=True,
            model=args.model,
            listed_models=listed_models,
            measure=measure,
            example_state=example_state,
            qcount=qcount,
            probe=probe_result,
            repeats=repeats,
            scenario_rows=scenario_rows,
            sensitivity_rows=sensitivity_rows,
            stability_rows=stability_rows,
            cadence_rows=cadence_rows,
            failures=failures,
            live_calls=live_calls,
            stop_reason=budget.stop_reason,
            all_live_results=all_results,
        )
        write_json(session_dir / "summary.json", summary)
        committed = summary
    else:
        debug = build_summary(
            live=False,
            model=args.model,
            listed_models=listed_models,
            measure=measure,
            example_state=example_state,
            qcount=qcount,
            probe=probe_result,
            repeats=repeats,
            scenario_rows=scenario_rows,
            sensitivity_rows=sensitivity_rows,
            stability_rows=stability_rows,
            cadence_rows=cadence_rows,
            failures=failures,
            live_calls=0,
            stop_reason=budget.stop_reason,
            all_live_results=[],
        )
        write_json(session_dir / "summary.json", debug)
        committed = build_offline_summary(
            model=args.model,
            measure=measure,
            example_state=example_state,
            qcount=qcount,
            reason="TYPESAFE_API_KEY was not set; dry-run only. No live answers, latencies, or API tokens.",
        )
    SUMMARY_DIR.mkdir(parents=True, exist_ok=True)
    write_json(SUMMARY_DIR / "latest.json", committed)
    summary = committed
    args.report.write_text(render_report(summary), encoding="utf-8")
    print(f"wrote {args.report}")
    print(f"wrote {SUMMARY_DIR / 'latest.json'}")
    print(f"live_calls={live_calls} budget_used={budget.used} stop={budget.stop_reason}")
    if live and live_calls == 0:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
