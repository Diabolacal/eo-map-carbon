"""Simple deterministic commander plus Jev fallback composition.

This policy is a comparison device, not ground truth. Thresholds are
intentionally shallow. Combined conditions are where the nesting starts.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any

from .state import Encounter, POSTURES, FORMATIONS, player_isolated

SAFE_POSTURE = "hold_close"
SAFE_FORMATION = "tight_screen"
SAFE_REINFORCE = "hold"
AMBIGUOUS_NOUL_BAND = 0.15
AMBIGUOUS_CHOICE_CONFIDENCE = 0.40


@dataclass(frozen=True)
class Command:
    tactical_posture: str
    formation_intent: str
    reinforcement_action: str
    primary_target: str
    flags: dict[str, bool]

    def as_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass(frozen=True)
class AppliedCommand:
    command: Command
    source: str
    reasons: tuple[str, ...]
    field_sources: dict[str, str]
    used_jev: bool

    def as_dict(self) -> dict[str, Any]:
        payload = asdict(self)
        payload["reasons"] = list(self.reasons)
        return payload


def _alive_players(encounter: Encounter) -> list[Any]:
    return [p for p in encounter.players if p.alive]


def deterministic_command(encounter: Encounter) -> Command:
    boss = encounter.boss
    swarm = encounter.swarm
    derived = encounter.derived
    alive = _alive_players(encounter)

    severe_boss = boss.health_pct <= 20.0 or (
        boss.health_pct <= 35.0 and boss.damage_received_last_10s >= 400.0
    )
    heavy_losses = swarm.losses_last_10s >= 12 or swarm.losses_last_30s >= 22
    dispersed = swarm.detached_far >= 20 or derived["swarm_detached_fraction"] >= 0.40
    low_swarm = swarm.alive <= 20
    has_pressure = (
        derived["total_boss_damage_last_10s"] >= 80
        or derived["total_subordinate_damage_last_10s"] >= 120
    )
    nearest = derived["nearest_alive_player_km"]
    distant_only = nearest is None or nearest >= 100.0
    can_spawn = boss.reinforcement_capacity_available and boss.reinforcement_cooldown_s <= 0

    # Nested combinations start here: danger, losses, dispersion, and remaining
    # hostility all interact. A real policy would keep growing branches.
    if severe_boss:
        posture = "desperate_defence"
    elif heavy_losses or (dispersed and low_swarm):
        posture = "regroup"
    elif (boss.health_pct <= 50 and has_pressure) or (low_swarm and has_pressure):
        posture = "hold_close"
    elif not has_pressure and distant_only:
        posture = "hold_close"
    else:
        posture = "press_attack"

    if posture == "desperate_defence" or severe_boss:
        formation = "tight_screen"
    elif dispersed:
        formation = "recall_detached"
    elif posture == "press_attack" and swarm.alive >= 40 and not heavy_losses:
        formation = "wide_pressure"
    elif posture in {"hold_close", "regroup"}:
        formation = "tight_screen" if severe_boss or low_swarm else "balanced"
    else:
        formation = "balanced"

    if posture in {"desperate_defence", "hold_close"} and formation == "wide_pressure":
        formation = "tight_screen"

    if can_spawn and (heavy_losses or severe_boss or (low_swarm and has_pressure)):
        reinforcement = "spawn_now"
    else:
        reinforcement = "hold"

    target = "none"
    top_boss = derived.get("top_boss_damager")
    top_sub = derived.get("top_subordinate_damager")
    by_id = {p.id: p for p in alive}
    if top_boss and top_boss["damage_to_boss_last_10s"] >= 80:
        candidate = by_id.get(top_boss["id"])
        if candidate is not None:
            far = candidate.distance_from_boss_km > 90
            bait = player_isolated(candidate) and candidate.distance_from_boss_km >= 100
            if bait and top_boss["damage_to_boss_last_10s"] < 300:
                target = "none"
            elif far and top_boss["damage_to_boss_last_10s"] < 300 and nearest is not None and nearest < 80:
                target = "none"
            else:
                target = candidate.id
    elif top_sub and top_sub["damage_to_subordinates_last_10s"] >= 150:
        candidate = by_id.get(top_sub["id"])
        if candidate is not None and candidate.distance_from_boss_km <= 80:
            target = candidate.id

    flags = {
        "boss_in_critical_danger": severe_boss,
        "swarm_overextended": dispersed,
        "regroup_warranted": posture == "regroup" or heavy_losses,
        "recall_detached_units": dispersed,
        "reinforcement_wave_warranted": reinforcement == "spawn_now",
        "focus_fire_warranted": target != "none",
        "pursuit_warranted": bool(
            target != "none"
            and by_id.get(target) is not None
            and by_id[target].distance_from_boss_km <= 80
            and not distant_only
        ),
        "preserve_swarm_priority": low_swarm or heavy_losses,
        "protect_boss_priority": severe_boss or posture == "desperate_defence",
        "current_plan_still_sensible": boss.current_posture == posture,
        "distant_bait_should_be_ignored": any(
            p.distance_from_boss_km >= 100 and player_isolated(p) for p in alive
        )
        and any(p.distance_from_boss_km <= 40 and p.damage_to_boss_last_10s >= 40 for p in alive),
        "pressure_can_be_maintained": posture == "press_attack",
    }
    return Command(
        tactical_posture=posture,
        formation_intent=formation,
        reinforcement_action=reinforcement,
        primary_target=target,
        flags=flags,
    )


def noul_ambiguous(value: float) -> bool:
    return abs(float(value) - 0.5) < AMBIGUOUS_NOUL_BAND


def _safe_command(encounter: Encounter, last: Command | None) -> Command:
    if last is not None:
        return last
    return deterministic_command(encounter)


def _choice_ok(raw: dict[str, Any] | None, allowed: set[str] | None = None) -> str | None:
    if not isinstance(raw, dict):
        return None
    if raw.get("type") not in {None, "choice"}:
        return None
    choice = raw.get("choice")
    if not isinstance(choice, str):
        return None
    if allowed is not None and choice not in allowed:
        return None
    return choice


def _choice_conf(raw: dict[str, Any] | None) -> float | None:
    if not isinstance(raw, dict):
        return None
    conf = raw.get("confidence")
    if conf is None:
        probs = raw.get("probabilities") or {}
        if probs:
            conf = max(float(v) for v in probs.values())
    try:
        return float(conf) if conf is not None else None
    except (TypeError, ValueError):
        return None


def _noul_ok(raw: dict[str, Any] | None) -> float | None:
    if not isinstance(raw, dict):
        return None
    if "noul" not in raw:
        return None
    try:
        value = float(raw["noul"])
    except (TypeError, ValueError):
        return None
    if not 0.0 <= value <= 1.0:
        return None
    return value


def compose_command(
    encounter: Encounter,
    jev_answers: dict[str, Any] | None,
    *,
    last: Command | None = None,
    error: str | None = None,
) -> AppliedCommand:
    """Jev is optional. The sim always gets a valid command back immediately.

    On timeout, API failure, or malformed payload: keep the last valid command
    if one exists, otherwise the deterministic policy. Never block waiting.
    """
    det = deterministic_command(encounter)
    reasons: list[str] = []
    field_sources = {
        "tactical_posture": "deterministic",
        "formation_intent": "deterministic",
        "reinforcement_action": "deterministic",
        "primary_target": "deterministic",
    }

    if error:
        reasons.append(error)
        retained = _safe_command(encounter, last)
        return AppliedCommand(
            command=retained,
            source="deterministic_fallback",
            reasons=tuple(reasons + ["retained_last" if last is not None else "used_deterministic"]),
            field_sources=field_sources,
            used_jev=False,
        )
    if not isinstance(jev_answers, dict) or not jev_answers:
        reasons.append("missing_or_empty_answers")
        retained = _safe_command(encounter, last)
        return AppliedCommand(
            command=retained,
            source="deterministic_fallback",
            reasons=tuple(reasons),
            field_sources=field_sources,
            used_jev=False,
        )

    alive_ids = set(encounter.derived["alive_player_ids"])
    posture = det.tactical_posture
    formation = det.formation_intent
    reinforcement = det.reinforcement_action
    target = det.primary_target

    raw_posture = jev_answers.get("tactical_posture")
    picked = _choice_ok(raw_posture, set(POSTURES))
    conf = _choice_conf(raw_posture)
    if picked is None:
        reasons.append("malformed_tactical_posture")
    elif conf is not None and conf < AMBIGUOUS_CHOICE_CONFIDENCE:
        reasons.append(f"ambiguous_tactical_posture_conf={conf:.2f}")
        field_sources["tactical_posture"] = "deterministic_ambiguous"
    else:
        posture = picked
        field_sources["tactical_posture"] = "jev"

    raw_form = jev_answers.get("formation_intent")
    picked = _choice_ok(raw_form, set(FORMATIONS))
    conf = _choice_conf(raw_form)
    if picked is None:
        reasons.append("malformed_formation_intent")
    elif conf is not None and conf < AMBIGUOUS_CHOICE_CONFIDENCE:
        reasons.append(f"ambiguous_formation_intent_conf={conf:.2f}")
        field_sources["formation_intent"] = "deterministic_ambiguous"
    else:
        formation = picked
        field_sources["formation_intent"] = "jev"

    raw_re = jev_answers.get("reinforcement_action")
    picked = _choice_ok(raw_re, {"hold", "spawn_now"})
    conf = _choice_conf(raw_re)
    noul_re = _noul_ok(jev_answers.get("reinforcement_wave_warranted"))
    can_spawn = (
        encounter.boss.reinforcement_capacity_available
        and encounter.boss.reinforcement_cooldown_s <= 0
    )
    if picked is None:
        reasons.append("malformed_reinforcement_action")
    elif conf is not None and conf < AMBIGUOUS_CHOICE_CONFIDENCE:
        reasons.append(f"ambiguous_reinforcement_action_conf={conf:.2f}")
        field_sources["reinforcement_action"] = "deterministic_ambiguous"
    elif picked == "spawn_now" and not can_spawn:
        reasons.append("jev_spawn_overridden_no_capacity")
        reinforcement = "hold"
        field_sources["reinforcement_action"] = "deterministic_override"
    elif picked == "spawn_now" and noul_re is not None and noul_ambiguous(noul_re):
        reasons.append("spawn_held_because_reinforcement_noul_ambiguous")
        reinforcement = "hold"
        field_sources["reinforcement_action"] = "deterministic_ambiguous"
    else:
        reinforcement = picked
        field_sources["reinforcement_action"] = "jev"

    raw_target = jev_answers.get("primary_target")
    allowed_targets = alive_ids | {"none"}
    picked = _choice_ok(raw_target, allowed_targets)
    conf = _choice_conf(raw_target)
    if picked is None:
        reasons.append("malformed_or_dead_primary_target")
    elif conf is not None and conf < AMBIGUOUS_CHOICE_CONFIDENCE:
        reasons.append(f"ambiguous_primary_target_conf={conf:.2f}")
        field_sources["primary_target"] = "deterministic_ambiguous"
    else:
        target = picked
        field_sources["primary_target"] = "jev"

    if posture == "desperate_defence" and formation == "wide_pressure":
        formation = "tight_screen"
        reasons.append("wide_pressure_overridden_in_desperate_defence")
        field_sources["formation_intent"] = "deterministic_override"

    flags = dict(det.flags)
    for qid, raw in jev_answers.items():
        noul = _noul_ok(raw)
        if noul is None:
            continue
        if noul_ambiguous(noul):
            reasons.append(f"ambiguous_noul:{qid}={noul:.2f}")
            continue
        flags[qid] = noul >= 0.5

    command = Command(
        tactical_posture=posture,
        formation_intent=formation,
        reinforcement_action=reinforcement,
        primary_target=target,
        flags=flags,
    )
    used = any(src == "jev" for src in field_sources.values())
    return AppliedCommand(
        command=command,
        source="jev_composed" if used else "deterministic_fallback",
        reasons=tuple(reasons) if reasons else ("jev_accepted",),
        field_sources=field_sources,
        used_jev=used,
    )


def synthetic_answers(encounter: Encounter) -> dict[str, Any]:
    """Deterministic stand-in answers for dry-run / unit tests. Not live Jev."""
    det = deterministic_command(encounter)
    answers: dict[str, Any] = {}
    for qid, flag in det.flags.items():
        answers[qid] = {"type": "noul", "noul": 0.86 if flag else 0.12}
    def _one_hot(options: list[str], winner: str) -> dict[str, Any]:
        mass = {opt: (0.91 if opt == winner else round(0.09 / max(len(options) - 1, 1), 4)) for opt in options}
        total = sum(mass.values())
        mass = {k: v / total for k, v in mass.items()}
        return {
            "type": "choice",
            "choice": winner,
            "confidence": 0.82,
            "probabilities": mass,
        }

    answers["tactical_posture"] = _one_hot(list(POSTURES), det.tactical_posture)
    answers["formation_intent"] = _one_hot(list(FORMATIONS), det.formation_intent)
    answers["reinforcement_action"] = _one_hot(["hold", "spawn_now"], det.reinforcement_action)
    target_opts = list(encounter.derived["alive_player_ids"]) + ["none"]
    answers["primary_target"] = _one_hot(target_opts, det.primary_target)
    return answers
