"""TypeSafe System One client wrapper for commander inferences.

Reuses the parent experiment's answer/usage sanitizers. Never logs the API key.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
TYPESAFE = HERE.parent
if str(TYPESAFE) not in sys.path:
    sys.path.insert(0, str(TYPESAFE))

from build_state import estimate_tokens  # noqa: E402
from run_benchmark import (  # noqa: E402
    DEFAULT_MODEL,
    answers_from_sdk,
    sanitize_response_json,
    usage_from_sdk,
)

from .questions import build_questions, payload_body
from .state import Encounter, to_jev_state

COMMANDER_TIMEOUT_S = 8.0
DEFAULT_MODEL_NAME = DEFAULT_MODEL


def measure_payload(encounter: Encounter, model: str = DEFAULT_MODEL_NAME) -> dict[str, Any]:
    body = payload_body(encounter, model)
    encoded = json.dumps(body, ensure_ascii=False)
    state_encoded = json.dumps(body["state"], ensure_ascii=False)
    estimates = estimate_tokens(len(encoded))
    return {
        "payload_chars": len(encoded),
        "payload_bytes_utf8": len(encoded.encode("utf-8")),
        "state_chars": len(state_encoded),
        "question_count": len(body["questions"]),
        "token_estimates": estimates,
        "request_token_budget_docs": 32000,
        "under_char_hint_150k": len(encoded) < 150000,
        "under_est_32k_div4": estimates["est_chars_div_4"] < 32000,
    }


def classify_error(exc: BaseException) -> str:
    name = type(exc).__name__
    status = getattr(exc, "status", None)
    if "RateLimit" in name or status == 429:
        return "rate_limit"
    if "Timeout" in name or name.endswith("TimeoutError"):
        return "timeout"
    if "ResponseValidation" in name or "malformed" in str(exc).lower():
        return "malformed"
    if status in {529, 500, 502, 503}:
        return "api_error"
    if status is not None:
        return "api_error"
    return "error"


class InferenceBudget:
    def __init__(self, max_calls: int = 250) -> None:
        self.max_calls = max_calls
        self.used = 0
        self.stop_reason: str | None = None

    def allow(self) -> bool:
        return self.stop_reason is None and self.used < self.max_calls

    def halt(self, reason: str) -> None:
        self.stop_reason = reason


def infer_once(
    client: Any,
    encounter: Encounter,
    *,
    model: str = DEFAULT_MODEL_NAME,
    timeout_s: float = COMMANDER_TIMEOUT_S,
    budget: InferenceBudget | None = None,
) -> dict[str, Any]:
    if budget is not None and not budget.allow():
        return {
            "ok": False,
            "skipped": True,
            "error_type": "budget_exhausted",
            "error": budget.stop_reason or "call budget exhausted",
            "error_class": "budget",
            "scenario_id": encounter.scenario_id,
        }

    questions = build_questions(encounter)
    state = to_jev_state(encounter)
    started = time.perf_counter()
    try:
        response = client.system_one(
            state=state,
            questions=questions,
            model=model,
            timeout=timeout_s,
        )
        latency = time.perf_counter() - started
        answers = answers_from_sdk(response)
        input_tokens, output_tokens = usage_from_sdk(response)
        reported_model = getattr(response, "model", None) or model
        if budget is not None:
            budget.used += 1
        if not answers:
            return {
                "ok": False,
                "error_type": "empty_answers",
                "error": "response contained no answers",
                "error_class": "malformed",
                "latency_s": latency,
                "scenario_id": encounter.scenario_id,
                "model": reported_model,
                "input_tokens": input_tokens,
                "output_tokens": output_tokens,
            }
        return {
            "ok": True,
            "latency_s": latency,
            "model": reported_model,
            "input_tokens": input_tokens,
            "output_tokens": output_tokens,
            "answers": answers,
            "scenario_id": encounter.scenario_id,
            "response_json": sanitize_response_json(reported_model, answers, input_tokens, output_tokens),
        }
    except Exception as exc:
        latency = time.perf_counter() - started
        error_class = classify_error(exc)
        if budget is not None:
            budget.used += 1
            if error_class == "rate_limit":
                budget.halt("rate_limit")
            elif getattr(exc, "status", None) in {429, 529}:
                budget.halt(f"http_{getattr(exc, 'status', None)}")
        return {
            "ok": False,
            "latency_s": latency,
            "error_type": type(exc).__name__,
            "error": str(exc),
            "error_class": error_class,
            "status": getattr(exc, "status", None),
            "scenario_id": encounter.scenario_id,
        }


def open_client(model: str, timeout_s: float = COMMANDER_TIMEOUT_S) -> Any:
    from typesafe_sdk import RetryPolicy, TypeSafeClient

    retry = RetryPolicy(max_retries=0, timeout=timeout_s)
    return TypeSafeClient(timeout=timeout_s, retry=retry, model=model)


def list_models(client: Any) -> list[str] | None:
    try:
        listed = client.models.list()
    except Exception as exc:
        print(f"models.list failed: {type(exc).__name__}: {exc}")
        return None
    if hasattr(listed, "models"):
        return [getattr(m, "id", None) or getattr(m, "name", None) or str(m) for m in listed.models]
    if isinstance(listed, list):
        return [getattr(m, "id", None) or str(m) for m in listed]
    return [str(listed)]
