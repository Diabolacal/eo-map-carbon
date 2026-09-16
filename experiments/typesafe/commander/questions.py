"""Jev Noul/Choice set for one commander inference.

Questions are tactical/interpretive. Exact counts, distances, and damage
totals are supplied in state and are not asked again.
"""

from __future__ import annotations

from typing import Any

from .state import Encounter, to_jev_state

NOUL_IDS = (
    "boss_in_critical_danger",
    "swarm_overextended",
    "regroup_warranted",
    "recall_detached_units",
    "reinforcement_wave_warranted",
    "focus_fire_warranted",
    "pursuit_warranted",
    "preserve_swarm_priority",
    "protect_boss_priority",
    "current_plan_still_sensible",
    "distant_bait_should_be_ignored",
    "pressure_can_be_maintained",
)

CHOICE_IDS = (
    "tactical_posture",
    "formation_intent",
    "reinforcement_action",
    "primary_target",
)

POSTURE_CRITERIA = {
    "press_attack": "Keep applying offensive pressure on hostiles. Appropriate when the boss is not in immediate danger and the swarm can still spend itself usefully.",
    "hold_close": "Hold the swarm near the boss and stop chasing. Appropriate when pressure exists but overextending or exposing the boss would be worse than hunting.",
    "regroup": "Pull units back together and restore cohesion before committing again. Appropriate after dispersion or a sharp loss pulse.",
    "desperate_defence": "The commander is in immediate survival danger. Screen the boss and survive rather than hunt.",
}

FORMATION_CRITERIA = {
    "tight_screen": "Compact the remaining NPCs immediately around the boss.",
    "balanced": "Keep a close screen and still allow some local pressure on nearby hostiles.",
    "wide_pressure": "Spread subordinates to pressure multiple players. Poor if the swarm is already scattered or the boss is exposed.",
    "recall_detached": "The formation priority is pulling far units back toward the boss.",
}

REINFORCE_CRITERIA = {
    "hold": "Do not spawn a reinforcement wave now. Always choose this when capacity is unavailable.",
    "spawn_now": "Spawn a reinforcement wave immediately. Only appropriate if capacity is available and the current depletion/pressure warrants spending it.",
}

NOUL_SPECS: dict[str, dict[str, Any]] = {
    "boss_in_critical_danger": {
        "instructions": (
            "Given the exact measured `boss.health_pct`, `boss.damage_received_last_10s`, "
            "remaining swarm, and current player pressure, is the boss in critical tactical "
            "danger right now?"
        ),
        "criteria": {
            "true": "Commander survival is immediately at risk; protecting the boss should outrank applying pressure.",
            "false": "The boss can still absorb current pressure without an emergency defensive shift.",
        },
    },
    "swarm_overextended": {
        "instructions": (
            "Are subordinate NPCs overextended relative to the boss, given "
            "`swarm.detached_far`, `swarm.close_to_boss`, `derived.swarm_detached_fraction`, "
            "and player distances?"
        ),
        "criteria": {
            "true": "Too many subordinates are far from the boss relative to the threat geometry.",
            "false": "The swarm's spatial distribution is still acceptable.",
        },
    },
    "regroup_warranted": {
        "instructions": (
            "Should the commander order a regroup, considering `swarm.losses_last_10s`, "
            "`swarm.losses_last_30s`, dispersion, and remaining `swarm.alive`?"
        ),
        "criteria": {
            "true": "Cohesion should be restored before the swarm spends itself further.",
            "false": "A regroup is not warranted; the current commitment can continue.",
        },
    },
    "recall_detached_units": {
        "instructions": (
            "Should detached/far subordinates be recalled toward the boss rather than left "
            "on their current assignments?"
        ),
        "criteria": {
            "true": "Far units are misplaced and should be pulled back.",
            "false": "Detached units can stay on their current assignments.",
        },
    },
    "reinforcement_wave_warranted": {
        "instructions": (
            "Should a reinforcement wave be spawned now? Use remaining swarm strength, "
            "recent attrition, current pressure, `boss.reinforcement_capacity_available`, "
            "and `boss.reinforcement_cooldown_s`. If capacity is not available or cooldown "
            "is still running, the answer is no."
        ),
        "criteria": {
            "true": "Capacity is available and spending a wave now is tactically justified.",
            "false": "Either capacity is unavailable, or spending the wave now would be wasteful or premature.",
        },
    },
    "focus_fire_warranted": {
        "instructions": (
            "Is focus fire on a single player warranted, given `derived.top_boss_damager`, "
            "`derived.top_boss_damage_share`, and how concentrated the current threat is?"
        ),
        "criteria": {
            "true": "One (or a clearly dominant) player should be the shared focus.",
            "false": "Threat is too diffuse, too weak, or too far for a focus-fire order.",
        },
    },
    "pursuit_warranted": {
        "instructions": (
            "Is pursuing a distant or withdrawing player a sound tactical choice right now? "
            "A long chase that abandons the boss fight, or chasing a lone far player with "
            "little recent damage, is a poor choice."
        ),
        "criteria": {
            "true": "Closing on a withdrawing/distant hostile is worth the swarm's time.",
            "false": "Pursuit would pull the swarm into a poor position or waste it on a low-value far target.",
        },
    },
    "preserve_swarm_priority": {
        "instructions": (
            "Should the commander prioritise preserving remaining subordinates over continuing "
            "to apply pressure, given `swarm.alive` and recent losses?"
        ),
        "criteria": {
            "true": "The remaining swarm is scarce enough that conservation outranks hunting.",
            "false": "The swarm can still be spent to apply pressure.",
        },
    },
    "protect_boss_priority": {
        "instructions": (
            "Should the commander prioritise protecting itself over applying pressure on players?"
        ),
        "criteria": {
            "true": "Boss survivability should be the primary concern this tick.",
            "false": "The boss can accept current risk in order to keep pressure on players.",
        },
    },
    "current_plan_still_sensible": {
        "instructions": (
            "Is the current `boss.current_posture` / `swarm.current_broad_order` still a "
            "sensible overall plan given this snapshot?"
        ),
        "criteria": {
            "true": "The posted plan still matches the situation.",
            "false": "The posted plan should change.",
        },
    },
    "distant_bait_should_be_ignored": {
        "instructions": (
            "Is there a distant player whose current role is drawing the swarm away, such that "
            "the commander should ignore that player for now? Use `derived.isolated_alive_player_ids`, "
            "far distances, and compare their recent boss damage to nearer players."
        ),
        "criteria": {
            "true": "At least one far/isolated player is a draw and should not pull the swarm.",
            "false": "No such bait pattern is present, or the far player is actually the main threat.",
        },
    },
    "pressure_can_be_maintained": {
        "instructions": (
            "Can the swarm currently maintain offensive pressure without undue risk to the boss "
            "or the remaining subordinates?"
        ),
        "criteria": {
            "true": "Continuing to press is still affordable.",
            "false": "Continuing to press would be an undue risk.",
        },
    },
}


def primary_target_criteria(encounter: Encounter) -> dict[str, str]:
    criteria = {
        "none": "No currently alive player warrants focused attention. Use this when pressure is negligible, hostiles are too far to spend the swarm on, or threat is too diffuse to name one id.",
    }
    for player in encounter.players:
        if player.alive:
            criteria[player.id] = f"Alive player {player.id} should be the swarm's primary focus target."
    return criteria


def build_questions(encounter: Encounter) -> dict[str, Any]:
    questions: dict[str, Any] = {}
    for qid in NOUL_IDS:
        spec = NOUL_SPECS[qid]
        questions[qid] = {
            "type": "noul",
            "instructions": spec["instructions"],
            "criteria": spec["criteria"],
        }
    questions["tactical_posture"] = {
        "type": "choice",
        "instructions": (
            "What overall tactical posture should the commander adopt for this snapshot? "
            "Choose one. Do not invent an option outside the list."
        ),
        "criteria": POSTURE_CRITERIA,
    }
    questions["formation_intent"] = {
        "type": "choice",
        "instructions": (
            "What formation intent should the commander issue to subordinates? This is a "
            "broad order, not per-NPC steering."
        ),
        "criteria": FORMATION_CRITERIA,
    }
    questions["reinforcement_action"] = {
        "type": "choice",
        "instructions": (
            "Hold the reinforcement wave or spawn it now? If `boss.reinforcement_capacity_available` "
            "is false or `boss.reinforcement_cooldown_s` is still running, choose hold."
        ),
        "criteria": REINFORCE_CRITERIA,
    }
    questions["primary_target"] = {
        "type": "choice",
        "instructions": (
            "Which currently alive player should be the primary target? Options are the alive "
            "player ids in this snapshot plus `none`. Dead players are not valid choices. "
            "Prefer a nearby concentrated threat over a far player with little boss damage."
        ),
        "criteria": primary_target_criteria(encounter),
    }
    return questions


def question_count(questions: dict[str, Any]) -> dict[str, int]:
    noul = sum(1 for q in questions.values() if q["type"] == "noul")
    choice = sum(1 for q in questions.values() if q["type"] == "choice")
    return {"noul": noul, "choice": choice, "total": noul + choice}


def payload_body(encounter: Encounter, model: str) -> dict[str, Any]:
    return {
        "state": to_jev_state(encounter),
        "model": model,
        "questions": build_questions(encounter),
    }
