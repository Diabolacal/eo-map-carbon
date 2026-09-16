"""Score Jev System One answers against the gold set.

This module has no TypeSafe SDK dependency so the gold-set contract can be
reviewed and unit-tested without an API key.
"""

from __future__ import annotations

from statistics import mean, median
from typing import Any

NOUL_YES_THRESHOLD = 0.5


def noul_certainty(noul: float) -> float:
    """Map a Noul probability to [0, 1] certainty. 0.5 -> 0.0, 0 or 1 -> 1.0."""
    return abs(float(noul) - 0.5) * 2.0


def predicted_noul(noul: float, threshold: float = NOUL_YES_THRESHOLD) -> bool:
    return float(noul) >= threshold


def score_answer(question: dict[str, Any], raw: dict[str, Any], gold: dict[str, Any]) -> dict[str, Any]:
    qtype = question["type"]
    expected = question["expected"]
    result: dict[str, Any] = {
        "id": question["id"],
        "type": qtype,
        "expected": expected,
        "source": question.get("source", ""),
        "correct": False,
        "predicted": None,
        "confidence": None,
        "noul": None,
        "noul_certainty": None,
        "probabilities": None,
        "choice": None,
        "low_confidence_correct": False,
        "high_confidence_incorrect": False,
    }

    if qtype == "noul":
        noul = float(raw["noul"])
        predicted = predicted_noul(noul, gold.get("noul_yes_threshold", NOUL_YES_THRESHOLD))
        certainty = noul_certainty(noul)
        correct = predicted == bool(expected)
        result.update(
            {
                "noul": noul,
                "predicted": predicted,
                "noul_certainty": certainty,
                "confidence": certainty,
                "correct": correct,
                "low_confidence_correct": correct
                and certainty < float(gold.get("noul_low_certainty", 0.6)),
                "high_confidence_incorrect": (not correct)
                and certainty >= float(gold.get("noul_high_certainty", 0.8)),
            }
        )
        return result

    if qtype == "choice":
        choice = raw.get("choice")
        probabilities = raw.get("probabilities") or {}
        confidence = raw.get("confidence")
        if confidence is None and probabilities:
            confidence = max(float(v) for v in probabilities.values())
        confidence_f = float(confidence) if confidence is not None else None
        correct = choice == expected
        result.update(
            {
                "choice": choice,
                "predicted": choice,
                "probabilities": probabilities,
                "confidence": confidence_f,
                "correct": correct,
                "low_confidence_correct": correct
                and confidence_f is not None
                and confidence_f < float(gold.get("choice_low_confidence", 0.5)),
                "high_confidence_incorrect": (not correct)
                and confidence_f is not None
                and confidence_f >= float(gold.get("choice_high_confidence", 0.8)),
            }
        )
        return result

    raise ValueError(f"unsupported question type: {qtype}")


def score_run(
    questions: list[dict[str, Any]],
    answers: dict[str, Any],
    gold: dict[str, Any],
) -> dict[str, Any]:
    scored = []
    missing = []
    for question in questions:
        raw = answers.get(question["id"])
        if raw is None:
            missing.append(question["id"])
            scored.append(
                {
                    "id": question["id"],
                    "type": question["type"],
                    "expected": question["expected"],
                    "source": question.get("source", ""),
                    "correct": False,
                    "predicted": None,
                    "missing": True,
                }
            )
            continue
        item = score_answer(question, raw, gold)
        item["missing"] = False
        scored.append(item)

    total = len(questions)
    correct = sum(1 for item in scored if item.get("correct"))
    return {
        "total": total,
        "correct": correct,
        "incorrect": total - correct,
        "accuracy": (correct / total) if total else 0.0,
        "missing": missing,
        "answers": scored,
    }


def aggregate_runs(run_results: list[dict[str, Any]], gold: dict[str, Any]) -> dict[str, Any]:
    questions = gold["questions"]
    completed = [run for run in run_results if run.get("ok")]
    failed = [run for run in run_results if not run.get("ok")]

    per_run_accuracy = [
        {"run": run["run"], "accuracy": run["score"]["accuracy"], "correct": run["score"]["correct"]}
        for run in completed
    ]

    by_id: dict[str, list[dict[str, Any]]] = {q["id"]: [] for q in questions}
    for run in completed:
        for item in run["score"]["answers"]:
            by_id[item["id"]].append({"run": run["run"], **item})

    disagreements = []
    stable = []
    incorrect = []
    low_conf_correct = []
    high_conf_incorrect = []
    for question in questions:
        qid = question["id"]
        items = by_id[qid]
        preds = [item.get("predicted") for item in items]
        unique = {repr(p) for p in preds}
        record = {
            "id": qid,
            "type": question["type"],
            "expected": question["expected"],
            "predictions": preds,
            "unique_predictions": sorted(unique),
        }
        if len(unique) > 1:
            disagreements.append(record)
        else:
            stable.append(qid)

        for item in items:
            brief = {
                "id": qid,
                "run": item["run"],
                "expected": question["expected"],
                "predicted": item.get("predicted"),
                "confidence": item.get("confidence"),
                "noul": item.get("noul"),
                "choice": item.get("choice"),
                "probabilities": item.get("probabilities"),
            }
            if not item.get("correct"):
                incorrect.append(brief)
            if item.get("low_confidence_correct"):
                low_conf_correct.append(brief)
            if item.get("high_confidence_incorrect"):
                high_conf_incorrect.append(brief)

    latencies = [run["latency_s"] for run in completed if run.get("latency_s") is not None]
    input_tokens = [run["input_tokens"] for run in completed if run.get("input_tokens") is not None]
    output_tokens = [run["output_tokens"] for run in completed if run.get("output_tokens") is not None]
    models = sorted({run.get("model") for run in completed if run.get("model")})

    overall_correct = sum(run["score"]["correct"] for run in completed)
    overall_total = sum(run["score"]["total"] for run in completed)

    return {
        "question_count": len(questions),
        "runs_attempted": len(run_results),
        "runs_completed": len(completed),
        "runs_failed": len(failed),
        "failures": [
            {"run": run["run"], "error": run.get("error"), "error_type": run.get("error_type")}
            for run in failed
        ],
        "models": models,
        "accuracy_per_run": per_run_accuracy,
        "overall_correct": overall_correct,
        "overall_total": overall_total,
        "overall_accuracy": (overall_correct / overall_total) if overall_total else 0.0,
        "stable_question_ids": stable,
        "stable_question_count": len(stable),
        "disagreements": disagreements,
        "incorrect": incorrect,
        "low_confidence_correct": low_conf_correct,
        "high_confidence_incorrect": high_conf_incorrect,
        "latency_s": _stats(latencies),
        "input_tokens": _stats(input_tokens),
        "output_tokens": _stats(output_tokens),
        "repeated_answers_stable": len(disagreements) == 0 and len(completed) > 1,
    }


def _stats(values: list[float]) -> dict[str, float | int | None]:
    if not values:
        return {"n": 0, "min": None, "median": None, "max": None, "mean": None}
    return {
        "n": len(values),
        "min": min(values),
        "median": median(values),
        "max": max(values),
        "mean": mean(values),
    }


def revised_accuracy(
    run_results: list[dict[str, Any]],
    exclude_ids: set[str],
) -> dict[str, Any]:
    """Original scores stay intact. This is only for explicitly defective questions."""
    completed = [run for run in run_results if run.get("ok")]
    per_run = []
    overall_correct = 0
    overall_total = 0
    for run in completed:
        kept = [item for item in run["score"]["answers"] if item["id"] not in exclude_ids]
        correct = sum(1 for item in kept if item.get("correct"))
        total = len(kept)
        per_run.append({"run": run["run"], "accuracy": (correct / total) if total else 0.0, "correct": correct, "total": total})
        overall_correct += correct
        overall_total += total
    return {
        "excluded_ids": sorted(exclude_ids),
        "accuracy_per_run": per_run,
        "overall_accuracy": (overall_correct / overall_total) if overall_total else 0.0,
        "overall_correct": overall_correct,
        "overall_total": overall_total,
    }
