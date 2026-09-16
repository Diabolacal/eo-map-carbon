"""Compact synthetic battlefield snapshots for the commander experiment.

Numeric fields are exact measured facts. Jev is asked to judge tactics, not
to rediscover counts, distances, or damage totals already present here.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from typing import Any, Literal

CLOSE_KM = 30.0
FAR_KM = 80.0
ISOLATION_KM = 60.0
PLAYER_TARGETS = ("boss", "subordinate", "none")
POSTURES = ("press_attack", "hold_close", "regroup", "desperate_defence")
FORMATIONS = ("tight_screen", "balanced", "wide_pressure", "recall_detached")

STATE_NOTE = (
    "Synthetic commander observation snapshot. Every numeric field is an exact "
    "measured fact. Judge higher-level tactics from these facts; do not re-count "
    "alive units, re-measure distances, or re-sum recent damage."
)


Target = Literal["boss", "subordinate", "none"]
Posture = Literal["press_attack", "hold_close", "regroup", "desperate_defence"]


@dataclass(frozen=True)
class Player:
    id: str
    alive: bool
    distance_from_boss_km: float
    damage_to_boss_last_10s: float
    damage_to_subordinates_last_10s: float
    current_target: str
    moving_toward_boss: bool
    nearest_other_player_km: float | None


@dataclass(frozen=True)
class Boss:
    health_pct: float
    damage_received_last_10s: float
    current_posture: str
    reinforcement_capacity_available: bool
    reinforcement_cooldown_s: float
    id: str = "boss"


@dataclass(frozen=True)
class Swarm:
    alive: int
    close_to_boss: int
    medium_range: int
    detached_far: int
    losses_last_10s: int
    losses_last_30s: int
    current_broad_order: str


@dataclass(frozen=True)
class Encounter:
    scenario_id: str
    tick_s: int
    boss: Boss
    swarm: Swarm
    players: tuple[Player, ...]
    derived: dict[str, Any] = field(default_factory=dict)


def _require(cond: bool, message: str) -> None:
    if not cond:
        raise ValueError(message)


def player_isolated(player: Player) -> bool:
    if not player.alive:
        return False
    if player.nearest_other_player_km is None:
        return True
    return player.nearest_other_player_km >= ISOLATION_KM


def compute_derived(boss: Boss, swarm: Swarm, players: tuple[Player, ...]) -> dict[str, Any]:
    alive = [p for p in players if p.alive]
    dead = [p for p in players if not p.alive]
    boss_dmg = [p for p in alive if p.damage_to_boss_last_10s > 0]
    sub_dmg = [p for p in alive if p.damage_to_subordinates_last_10s > 0]
    total_boss = sum(p.damage_to_boss_last_10s for p in alive)
    total_sub = sum(p.damage_to_subordinates_last_10s for p in alive)
    top_boss = max(boss_dmg, key=lambda p: p.damage_to_boss_last_10s, default=None)
    top_sub = max(sub_dmg, key=lambda p: p.damage_to_subordinates_last_10s, default=None)
    distances = [p.distance_from_boss_km for p in alive]
    isolated_ids = [p.id for p in alive if player_isolated(p)]
    close_frac = (swarm.close_to_boss / swarm.alive) if swarm.alive else 0.0
    detached_frac = (swarm.detached_far / swarm.alive) if swarm.alive else 0.0
    top_share = (top_boss.damage_to_boss_last_10s / total_boss) if top_boss and total_boss > 0 else 0.0
    return {
        "players_alive": len(alive),
        "players_dead": len(dead),
        "alive_player_ids": [p.id for p in alive],
        "dead_player_ids": [p.id for p in dead],
        "isolated_alive_player_ids": isolated_ids,
        "total_boss_damage_last_10s": round(total_boss, 1),
        "total_subordinate_damage_last_10s": round(total_sub, 1),
        "nearest_alive_player_km": round(min(distances), 1) if distances else None,
        "farthest_alive_player_km": round(max(distances), 1) if distances else None,
        "top_boss_damager": (
            {"id": top_boss.id, "damage_to_boss_last_10s": round(top_boss.damage_to_boss_last_10s, 1)}
            if top_boss
            else None
        ),
        "top_subordinate_damager": (
            {
                "id": top_sub.id,
                "damage_to_subordinates_last_10s": round(top_sub.damage_to_subordinates_last_10s, 1),
            }
            if top_sub
            else None
        ),
        "top_boss_damage_share": round(top_share, 3),
        "swarm_close_fraction": round(close_frac, 3),
        "swarm_detached_fraction": round(detached_frac, 3),
        "boss_damage_matches_player_sum": abs(boss.damage_received_last_10s - total_boss) < 0.51,
        "range_bands_km": {"close_lt": CLOSE_KM, "far_gt": FAR_KM, "isolated_gte": ISOLATION_KM},
    }


def finalize(
    scenario_id: str,
    tick_s: int,
    boss: Boss,
    swarm: Swarm,
    players: tuple[Player, ...] | list[Player],
) -> Encounter:
    players_t = tuple(players)
    ids = [p.id for p in players_t]
    _require(len(ids) == len(set(ids)), f"{scenario_id}: duplicate player ids")
    _require(0.0 <= boss.health_pct <= 100.0, f"{scenario_id}: health_pct out of range")
    _require(boss.current_posture in POSTURES, f"{scenario_id}: unknown boss posture")
    _require(swarm.current_broad_order in POSTURES, f"{scenario_id}: unknown swarm order")
    _require(swarm.alive >= 0, f"{scenario_id}: negative swarm alive")
    _require(
        swarm.close_to_boss + swarm.medium_range + swarm.detached_far == swarm.alive,
        f"{scenario_id}: swarm range buckets must sum to alive "
        f"({swarm.close_to_boss}+{swarm.medium_range}+{swarm.detached_far} != {swarm.alive})",
    )
    _require(swarm.losses_last_30s >= swarm.losses_last_10s, f"{scenario_id}: 30s losses < 10s losses")
    _require(swarm.losses_last_10s >= 0, f"{scenario_id}: negative losses")
    for player in players_t:
        _require(player.current_target in PLAYER_TARGETS, f"{scenario_id}: bad target for {player.id}")
        if not player.alive:
            _require(
                player.damage_to_boss_last_10s == 0 and player.damage_to_subordinates_last_10s == 0,
                f"{scenario_id}: dead player {player.id} still dealing damage",
            )
    derived = compute_derived(boss, swarm, players_t)
    _require(
        derived["boss_damage_matches_player_sum"],
        f"{scenario_id}: boss.damage_received_last_10s={boss.damage_received_last_10s} "
        f"!= player sum {derived['total_boss_damage_last_10s']}",
    )
    return Encounter(
        scenario_id=scenario_id,
        tick_s=tick_s,
        boss=boss,
        swarm=swarm,
        players=players_t,
        derived=derived,
    )


def to_jev_state(encounter: Encounter) -> dict[str, Any]:
    players_out = []
    for player in encounter.players:
        item = asdict(player)
        item["isolated"] = player_isolated(player)
        players_out.append(item)
    return {
        "note": STATE_NOTE,
        "scenario_id": encounter.scenario_id,
        "tick_s": encounter.tick_s,
        "boss": asdict(encounter.boss),
        "swarm": asdict(encounter.swarm),
        "players": players_out,
        "derived": encounter.derived,
    }


def replace_player(players: tuple[Player, ...], player_id: str, **changes: Any) -> tuple[Player, ...]:
    out = []
    found = False
    for player in players:
        if player.id == player_id:
            found = True
            data = asdict(player)
            data.update(changes)
            out.append(Player(**data))
        else:
            out.append(player)
    if not found:
        raise KeyError(player_id)
    return tuple(out)


def scale_player_boss_damage(players: tuple[Player, ...], new_total: float) -> tuple[Player, ...]:
    alive = [p for p in players if p.alive]
    current = sum(p.damage_to_boss_last_10s for p in alive)
    if current <= 0:
        if not alive or new_total <= 0:
            return players
        lead = alive[0]
        return replace_player(players, lead.id, damage_to_boss_last_10s=float(new_total))
    factor = new_total / current
    out = []
    for player in players:
        if player.alive:
            out.append(
                Player(
                    **{
                        **asdict(player),
                        "damage_to_boss_last_10s": round(player.damage_to_boss_last_10s * factor, 1),
                    }
                )
            )
        else:
            out.append(player)
    # Fix residual rounding so the sum matches new_total within 0.5.
    residual = round(new_total - sum(p.damage_to_boss_last_10s for p in out if p.alive), 1)
    if residual and out:
        for idx, player in enumerate(out):
            if player.alive:
                out[idx] = Player(
                    **{**asdict(player), "damage_to_boss_last_10s": round(player.damage_to_boss_last_10s + residual, 1)}
                )
                break
    return tuple(out)
