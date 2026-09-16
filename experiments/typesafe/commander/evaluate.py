"""Sensitivity, stability, cadence, and scale analysis. No API calls."""

from __future__ import annotations

import math
from statistics import mean, median, pstdev
from typing import Any

from .controller import compose_command, deterministic_command
from .scenarios import SensitivityPair, Scenario, material_change
from .state import Encounter

INCOMPATIBLE_POSTURES = {
    frozenset({"press_attack", "desperate_defence"}),
    frozenset({"press_attack", "regroup"}),
}
INCOMPATIBLE_FORMATIONS = {
    frozenset({"wide_pressure", "recall_detached"}),
    frozenset({"wide_pressure", "tight_screen"}),
}

# Official TypeSafe public pricing retrieved 2026-09-16 from
# https://typesafe.ai/ ("$42 Per Billion input tokens") and the 15 Sep 2026
# launch post https://typesafe.ai/blog/introducing-system-one-models-and-jev
# ("Input tokens: $0.042 / MTok ($42 per billion tokens). Output tokens: FREE").
PRICE_USD_PER_MILLION_INPUT = 0.042
PRICE_USD_PER_BILLION_INPUT = 42.0
PRICE_USD_PER_MILLION_OUTPUT = 0.0
PRICE_RETRIEVED_UTC_DATE = "2026-09-16"
PRICE_SOURCES = [
    "https://typesafe.ai/",
    "https://typesafe.ai/blog/introducing-system-one-models-and-jev",
]


def percentile(values: list[float], p: float) -> float | None:
    if not values:
        return None
    xs = sorted(values)
    if len(xs) == 1:
        return xs[0]
    rank = (p / 100.0) * (len(xs) - 1)
    lo = int(math.floor(rank))
    hi = int(math.ceil(rank))
    if lo == hi:
        return xs[lo]
    frac = rank - lo
    return xs[lo] * (1.0 - frac) + xs[hi] * frac


def latency_block(latencies: list[float], interval_s: float | None = None) -> dict[str, Any]:
    misses = 0
    if interval_s is not None:
        misses = sum(1 for x in latencies if x > interval_s)
    n = len(latencies)
    return {
        "n": n,
        "min": min(latencies) if n else None,
        "p50": percentile(latencies, 50) if n else None,
        "p95": percentile(latencies, 95) if n else None,
        "max": max(latencies) if n else None,
        "mean": mean(latencies) if n else None,
        "deadline_misses": misses,
        "deadline_miss_pct": (misses / n) if n else None,
    }


def compact_answers(answers: dict[str, Any]) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for qid, raw in answers.items():
        if not isinstance(raw, dict):
            out[qid] = raw
            continue
        if "noul" in raw:
            out[qid] = {"type": "noul", "noul": raw.get("noul")}
        else:
            item: dict[str, Any] = {
                "type": "choice",
                "choice": raw.get("choice"),
                "confidence": raw.get("confidence"),
            }
            if raw.get("probabilities"):
                item["probabilities"] = raw["probabilities"]
            out[qid] = item
    return out


def evaluate_review(scenario: Scenario, answers: dict[str, Any]) -> list[dict[str, Any]]:
    flags: list[dict[str, Any]] = []
    for check in scenario.review:
        qid = check["id"]
        raw = answers.get(qid) or {}
        kind = check["kind"]
        ok = True
        observed: Any = None
        if kind == "noul_below":
            observed = raw.get("noul")
            ok = observed is not None and float(observed) < float(check["threshold"])
        elif kind == "noul_above":
            observed = raw.get("noul")
            ok = observed is not None and float(observed) >= float(check["threshold"])
        elif kind == "choice_is":
            observed = raw.get("choice")
            ok = observed == check["value"]
        elif kind == "choice_not":
            observed = raw.get("choice")
            ok = observed != check["value"]
        elif kind == "choice_in":
            observed = raw.get("choice")
            ok = observed in check["values"]
        else:
            ok = False
        if not ok:
            flags.append(
                {
                    "scenario_id": scenario.id,
                    "id": qid,
                    "kind": kind,
                    "note": check["note"],
                    "observed": observed,
                    "check": {k: v for k, v in check.items() if k != "note"},
                }
            )
    return flags


def _moved_ok(base: float, variant: float, direction: str) -> bool:
    if direction == "not_decrease":
        return variant + 1e-9 >= base
    if direction == "not_increase":
        return variant <= base + 1e-9
    if direction == "increase":
        return variant > base + 1e-9
    if direction == "decrease":
        return variant + 1e-9 < base
    raise ValueError(direction)


def evaluate_sensitivity_pair(pair: SensitivityPair, base_answers: dict[str, Any], variant_answers: dict[str, Any]) -> dict[str, Any]:
    results = []
    for exp in pair.expectations:
        kind = exp["kind"]
        qid = exp["id"]
        direction = exp["direction"]
        if kind == "noul":
            b = (base_answers.get(qid) or {}).get("noul")
            v = (variant_answers.get(qid) or {}).get("noul")
            ok = b is not None and v is not None and _moved_ok(float(b), float(v), direction)
            results.append(
                {
                    "id": qid,
                    "kind": kind,
                    "direction": direction,
                    "base": b,
                    "variant": v,
                    "delta": (None if b is None or v is None else float(v) - float(b)),
                    "ok": ok,
                    "note": exp["note"],
                }
            )
        elif kind == "choice_prob":
            option = exp["option"]
            b_raw = base_answers.get(qid) or {}
            v_raw = variant_answers.get(qid) or {}
            b = (b_raw.get("probabilities") or {}).get(option)
            v = (v_raw.get("probabilities") or {}).get(option)
            ok = b is not None and v is not None and _moved_ok(float(b), float(v), direction)
            results.append(
                {
                    "id": qid,
                    "kind": kind,
                    "option": option,
                    "direction": direction,
                    "base": b,
                    "variant": v,
                    "delta": (None if b is None or v is None else float(v) - float(b)),
                    "base_choice": b_raw.get("choice"),
                    "variant_choice": v_raw.get("choice"),
                    "ok": ok,
                    "note": exp["note"],
                }
            )
        else:
            results.append({"id": qid, "kind": kind, "ok": False, "note": "unknown expectation kind"})
    return {
        "id": pair.id,
        "title": pair.title,
        "changed_field": pair.changed_field,
        "accounting_notes": pair.accounting_notes,
        "passed": all(item["ok"] for item in results),
        "expectations": results,
    }


def evaluate_stability(scenario_id: str, runs: list[dict[str, Any]]) -> dict[str, Any]:
    completed = [r for r in runs if r.get("ok")]
    noul_series: dict[str, list[float]] = {}
    choice_series: dict[str, list[str]] = {}
    for run in completed:
        for qid, raw in (run.get("answers") or {}).items():
            if not isinstance(raw, dict):
                continue
            if "noul" in raw:
                noul_series.setdefault(qid, []).append(float(raw["noul"]))
            elif "choice" in raw:
                choice_series.setdefault(qid, []).append(str(raw["choice"]))

    choice_rows = []
    agreements = 0
    for qid, values in choice_series.items():
        unique = sorted(set(values))
        agree = len(unique) == 1
        if agree:
            agreements += 1
        choice_rows.append(
            {
                "id": qid,
                "values": values,
                "unique": unique,
                "agreement": agree,
            }
        )
    noul_rows = []
    for qid, values in noul_series.items():
        noul_rows.append(
            {
                "id": qid,
                "n": len(values),
                "min": min(values),
                "max": max(values),
                "mean": mean(values),
                "stdev": pstdev(values) if len(values) > 1 else 0.0,
                "range": max(values) - min(values),
            }
        )

    posture_seq = choice_series.get("tactical_posture") or []
    formation_seq = choice_series.get("formation_intent") or []
    reinforce_seq = choice_series.get("reinforcement_action") or []
    target_seq = choice_series.get("primary_target") or []

    def _incompatible(seq: list[str], pairs: set[frozenset[str]]) -> int:
        count = 0
        for a, b in zip(seq, seq[1:]):
            if a != b and frozenset({a, b}) in pairs:
                count += 1
        return count

    incompatible_posture_switches = _incompatible(posture_seq, INCOMPATIBLE_POSTURES)
    incompatible_formation_switches = _incompatible(formation_seq, INCOMPATIBLE_FORMATIONS)
    reinforce_switches = sum(1 for a, b in zip(reinforce_seq, reinforce_seq[1:]) if a != b)
    none_flip = sum(
        1
        for a, b in zip(target_seq, target_seq[1:])
        if a != b and ("none" in {a, b})
    )

    choice_n = len(choice_rows)
    return {
        "scenario_id": scenario_id,
        "repeats_ok": len(completed),
        "repeats_attempted": len(runs),
        "choice_questions": choice_n,
        "choice_agreement_rate": (agreements / choice_n) if choice_n else None,
        "all_choices_stable": choice_n > 0 and agreements == choice_n,
        "choices": choice_rows,
        "nouls": noul_rows,
        "max_noul_range": max((row["range"] for row in noul_rows), default=0.0),
        "incompatible_posture_switches": incompatible_posture_switches,
        "incompatible_formation_switches": incompatible_formation_switches,
        "reinforcement_switches": reinforce_switches,
        "primary_target_none_flips": none_flip,
        "material_oscillation": bool(
            incompatible_posture_switches
            or incompatible_formation_switches
            or reinforce_switches
            or none_flip
        ),
    }


def cadence_summary(
    hz: float,
    rows: list[dict[str, Any]],
    wall_s: float,
) -> dict[str, Any]:
    interval = 1.0 / hz
    ok_rows = [r for r in rows if r.get("ok")]
    latencies = [float(r["latency_s"]) for r in ok_rows if r.get("latency_s") is not None]
    block = latency_block(latencies, interval)
    failures = [r for r in rows if not r.get("ok")]
    error_classes = {}
    for row in failures:
        key = row.get("error_class") or "error"
        error_classes[key] = error_classes.get(key, 0) + 1
    n = len(rows)
    achieved = (n / wall_s) if wall_s > 0 else None
    return {
        "target_hz": hz,
        "requested_interval_s": interval,
        "sample_count": n,
        "ok_count": len(ok_rows),
        "p50_s": block["p50"],
        "p95_s": block["p95"],
        "max_s": block["max"],
        "min_s": block["min"],
        "mean_s": block["mean"],
        "deadline_misses": block["deadline_misses"],
        "deadline_miss_pct": block["deadline_miss_pct"],
        "wall_s": wall_s,
        "achieved_hz": achieved,
        "sustainable": bool(
            block["n"]
            and block["p95"] is not None
            and block["p95"] <= interval
            and not any(cls in error_classes for cls in ("rate_limit", "timeout"))
        ),
        "api_failures": error_classes.get("api_error", 0) + error_classes.get("error", 0),
        "rate_limits": error_classes.get("rate_limit", 0),
        "timeouts": error_classes.get("timeout", 0),
        "malformed": error_classes.get("malformed", 0),
        "error_classes": error_classes,
    }


def token_stats(rows: list[dict[str, Any]]) -> dict[str, Any]:
    inn = [int(r["input_tokens"]) for r in rows if r.get("ok") and r.get("input_tokens") is not None]
    out = [int(r["output_tokens"]) for r in rows if r.get("ok") and r.get("output_tokens") is not None]
    return {
        "input": {
            "n": len(inn),
            "min": min(inn) if inn else None,
            "median": int(median(inn)) if inn else None,
            "max": max(inn) if inn else None,
            "mean": mean(inn) if inn else None,
        },
        "output": {
            "n": len(out),
            "min": min(out) if out else None,
            "median": int(median(out)) if out else None,
            "max": max(out) if out else None,
            "mean": mean(out) if out else None,
        },
    }


def scale_table(input_tokens: int) -> dict[str, Any]:
    cadences = (1, 2, 5, 10)
    per_hour = {}
    for hz in cadences:
        inf = hz * 3600
        tokens = inf * input_tokens
        per_hour[str(hz)] = {
            "inferences": inf,
            "input_tokens": tokens,
            "usd_if_published_price": tokens / 1_000_000_000 * PRICE_USD_PER_BILLION_INPUT,
        }
    commanders = (1, 10, 100, 1000)
    concurrent = {}
    for n in commanders:
        concurrent[str(n)] = {
            hz: {
                "inferences": per_hour[str(hz)]["inferences"] * n,
                "input_tokens": per_hour[str(hz)]["input_tokens"] * n,
                "usd_if_published_price": per_hour[str(hz)]["usd_if_published_price"] * n,
            }
            for hz in cadences
        }
    event_rates = (30, 60, 120)
    event_driven = {}
    for ev in event_rates:
        tokens = ev * input_tokens
        event_driven[str(ev)] = {
            "inferences_per_commander_hour": ev,
            "input_tokens": tokens,
            "usd_if_published_price": tokens / 1_000_000_000 * PRICE_USD_PER_BILLION_INPUT,
            "vs_5hz_token_ratio": (5 * 3600) / ev,
        }
    return {
        "input_tokens_per_inference": input_tokens,
        "price_usd_per_million_input": PRICE_USD_PER_MILLION_INPUT,
        "price_usd_per_billion_input": PRICE_USD_PER_BILLION_INPUT,
        "price_usd_per_million_output": PRICE_USD_PER_MILLION_OUTPUT,
        "price_retrieved_utc_date": PRICE_RETRIEVED_UTC_DATE,
        "price_sources": PRICE_SOURCES,
        "formula": "usd = input_tokens * inferences * (42 / 1e9); output treated as $0 per published pricing",
        "per_commander_hour_continuous": per_hour,
        "concurrent_commanders_continuous": concurrent,
        "event_driven_illustration": event_driven,
    }


def compare_deterministic(encounter: Encounter, answers: dict[str, Any] | None, error: str | None = None) -> dict[str, Any]:
    det = deterministic_command(encounter)
    applied = compose_command(encounter, answers, error=error)
    return {
        "deterministic": det.as_dict(),
        "applied": applied.as_dict(),
        "posture_agree": det.tactical_posture == applied.command.tactical_posture,
        "formation_agree": det.formation_intent == applied.command.formation_intent,
        "reinforce_agree": det.reinforcement_action == applied.command.reinforcement_action,
        "target_agree": det.primary_target == applied.command.primary_target,
    }


def event_driven_illustration(encounters: list[Encounter]) -> dict[str, Any]:
    if len(encounters) < 2:
        return {"pairs": 0, "material_pairs": 0, "reasons": []}
    material = 0
    details = []
    for prev, curr in zip(encounters, encounters[1:]):
        reasons = material_change(prev, curr)
        if reasons:
            material += 1
        details.append(
            {
                "from": prev.scenario_id,
                "to": curr.scenario_id,
                "material": bool(reasons),
                "reasons": reasons,
            }
        )
    return {
        "pairs": len(encounters) - 1,
        "material_pairs": material,
        "fraction": material / (len(encounters) - 1),
        "details": details,
    }
