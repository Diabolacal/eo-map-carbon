"""Deterministic scenario corpus and one-variable sensitivity pairs."""

from __future__ import annotations

from dataclasses import asdict, dataclass, replace
from typing import Any, Callable

from .state import Boss, Encounter, Player, Swarm, finalize, replace_player, scale_player_boss_damage


@dataclass(frozen=True)
class Scenario:
    id: str
    title: str
    why: str
    encounter: Encounter
    review: tuple[dict[str, Any], ...] = ()


@dataclass(frozen=True)
class SensitivityPair:
    id: str
    title: str
    changed_field: str
    base: Scenario
    variant: Scenario
    accounting_notes: str
    expectations: tuple[dict[str, Any], ...]


def P(
    pid: str,
    *,
    dist: float,
    boss: float = 0.0,
    sub: float = 0.0,
    target: str = "none",
    toward: bool = True,
    near: float | None = 20.0,
    alive: bool = True,
) -> Player:
    if not alive:
        return Player(
            id=pid,
            alive=False,
            distance_from_boss_km=dist,
            damage_to_boss_last_10s=0.0,
            damage_to_subordinates_last_10s=0.0,
            current_target="none",
            moving_toward_boss=False,
            nearest_other_player_km=near,
        )
    return Player(
        id=pid,
        alive=True,
        distance_from_boss_km=float(dist),
        damage_to_boss_last_10s=float(boss),
        damage_to_subordinates_last_10s=float(sub),
        current_target=target,
        moving_toward_boss=toward,
        nearest_other_player_km=near,
    )


def _enc(
    sid: str,
    tick: int,
    boss: Boss,
    swarm: Swarm,
    players: list[Player] | tuple[Player, ...],
) -> Encounter:
    return finalize(sid, tick, boss, swarm, tuple(players))


def _sc(
    sid: str,
    title: str,
    why: str,
    encounter: Encounter,
    review: list[dict[str, Any]] | None = None,
) -> Scenario:
    return Scenario(id=sid, title=title, why=why, encounter=encounter, review=tuple(review or ()))


def _boss(
    health: float,
    dmg: float,
    posture: str = "press_attack",
    reinforce: bool = False,
    cooldown: float = 45.0,
) -> Boss:
    return Boss(
        health_pct=float(health),
        damage_received_last_10s=float(dmg),
        current_posture=posture,
        reinforcement_capacity_available=reinforce,
        reinforcement_cooldown_s=float(cooldown),
    )


def _swarm(
    alive: int,
    close: int,
    medium: int,
    far: int,
    loss10: int,
    loss30: int,
    order: str = "press_attack",
) -> Swarm:
    return Swarm(
        alive=alive,
        close_to_boss=close,
        medium_range=medium,
        detached_far=far,
        losses_last_10s=loss10,
        losses_last_30s=loss30,
        current_broad_order=order,
    )


def _noul_below(qid: str, threshold: float, note: str) -> dict[str, Any]:
    return {"kind": "noul_below", "id": qid, "threshold": threshold, "note": note}


def _noul_above(qid: str, threshold: float, note: str) -> dict[str, Any]:
    return {"kind": "noul_above", "id": qid, "threshold": threshold, "note": note}


def _choice_is(qid: str, value: str, note: str) -> dict[str, Any]:
    return {"kind": "choice_is", "id": qid, "value": value, "note": note}


def _choice_not(qid: str, value: str, note: str) -> dict[str, Any]:
    return {"kind": "choice_not", "id": qid, "value": value, "note": note}


def _choice_in(qid: str, values: list[str], note: str) -> dict[str, Any]:
    return {"kind": "choice_in", "id": qid, "values": values, "note": note}


def build_main_scenarios() -> list[Scenario]:
    initial_players = [
        P("P1", dist=42, boss=4, sub=3, target="none", toward=True, near=18),
        P("P2", dist=48, boss=2, sub=4, target="none", toward=True, near=16),
        P("P3", dist=55, boss=3, sub=2, target="none", toward=True, near=20),
        P("P4", dist=61, boss=1, sub=2, target="none", toward=True, near=22),
        P("P5", dist=67, boss=0, sub=3, target="none", toward=True, near=19),
        P("P6", dist=50, boss=2, sub=1, target="none", toward=True, near=15),
        P("P7", dist=72, boss=0, sub=2, target="none", toward=True, near=25),
        P("P8", dist=58, boss=0, sub=1, target="none", toward=True, near=17),
    ]
    initial_boss_dmg = sum(p.damage_to_boss_last_10s for p in initial_players)

    normal_players = [
        P("P1", dist=22, boss=40, sub=20, target="boss", toward=True, near=12),
        P("P2", dist=18, boss=10, sub=80, target="subordinate", toward=True, near=10),
        P("P3", dist=25, boss=50, sub=15, target="boss", toward=True, near=11),
        P("P4", dist=30, boss=20, sub=40, target="subordinate", toward=True, near=14),
        P("P5", dist=45, boss=15, sub=25, target="subordinate", toward=False, near=18),
        P("P6", dist=55, boss=15, sub=10, target="none", toward=True, near=20),
        P("P7", dist=70, boss=10, sub=8, target="none", toward=True, near=24),
        P("P8", dist=28, boss=0, sub=50, target="subordinate", toward=True, near=13),
    ]
    normal_boss_dmg = sum(p.damage_to_boss_last_10s for p in normal_players)

    focused_players = [
        P("P1", dist=20, boss=90, sub=5, target="boss", toward=True, near=10),
        P("P2", dist=24, boss=40, sub=70, target="subordinate", toward=True, near=12),
        P("P3", dist=16, boss=640, sub=10, target="boss", toward=True, near=9),
        P("P4", dist=32, boss=20, sub=35, target="subordinate", toward=True, near=15),
        P("P5", dist=40, boss=15, sub=20, target="subordinate", toward=False, near=18),
        P("P6", dist=52, boss=10, sub=8, target="none", toward=True, near=21),
        P("P7", dist=66, boss=5, sub=6, target="none", toward=True, near=25),
        P("P8", dist=29, boss=0, sub=40, target="subordinate", toward=True, near=14),
    ]
    focused_boss_dmg = sum(p.damage_to_boss_last_10s for p in focused_players)

    dispersed_players = [
        P("P1", dist=18, boss=30, sub=10, target="boss", toward=True, near=12),
        P("P2", dist=22, boss=20, sub=15, target="boss", toward=True, near=11),
        P("P3", dist=28, boss=15, sub=12, target="subordinate", toward=True, near=14),
        P("P4", dist=36, boss=10, sub=8, target="subordinate", toward=False, near=16),
        P("P5", dist=48, boss=8, sub=6, target="none", toward=False, near=20),
        P("P6", dist=70, boss=5, sub=10, target="subordinate", toward=False, near=28),
        P("P8", dist=130, boss=2, sub=60, target="subordinate", toward=False, near=70),
        P("P9", dist=142, boss=0, sub=40, target="subordinate", toward=False, near=70),
    ]
    dispersed_boss_dmg = sum(p.damage_to_boss_last_10s for p in dispersed_players)

    attrition_players = [
        P("P1", dist=16, boss=50, sub=70, target="subordinate", toward=True, near=9),
        P("P2", dist=19, boss=30, sub=90, target="subordinate", toward=True, near=8),
        P("P3", dist=21, boss=40, sub=80, target="subordinate", toward=True, near=10),
        P("P4", dist=26, boss=20, sub=55, target="subordinate", toward=True, near=12),
        P("P5", dist=34, boss=20, sub=40, target="subordinate", toward=True, near=15),
        P("P6", dist=44, boss=10, sub=20, target="none", toward=True, near=18),
        P("P7", dist=58, boss=10, sub=12, target="none", toward=False, near=22),
        P("P8", dist=24, boss=0, sub=65, target="subordinate", toward=True, near=11),
    ]
    attrition_boss_dmg = sum(p.damage_to_boss_last_10s for p in attrition_players)

    critical_players = [
        P("P1", dist=12, boss=280, sub=8, target="boss", toward=True, near=8),
        P("P2", dist=15, boss=140, sub=10, target="boss", toward=True, near=8),
        P("P3", dist=22, boss=0, sub=35, target="subordinate", toward=True, near=12),
        P("P4", dist=28, boss=0, sub=20, target="subordinate", toward=True, near=14),
        P("P5", dist=40, boss=0, sub=0, target="none", toward=False, near=18, alive=False),
        P("P6", dist=55, boss=0, sub=0, target="none", toward=False, near=22, alive=False),
    ]
    critical_boss_dmg = sum(p.damage_to_boss_last_10s for p in critical_players)

    pressure_players = [
        P("P1", dist=14, boss=120, sub=25, target="boss", toward=True, near=9),
        P("P2", dist=18, boss=80, sub=40, target="boss", toward=True, near=10),
        P("P3", dist=21, boss=50, sub=55, target="subordinate", toward=True, near=11),
        P("P4", dist=27, boss=30, sub=45, target="subordinate", toward=True, near=13),
        P("P5", dist=38, boss=20, sub=30, target="subordinate", toward=True, near=16),
        P("P6", dist=52, boss=10, sub=12, target="none", toward=True, near=20),
        P("P7", dist=74, boss=0, sub=8, target="none", toward=False, near=28),
        P("P8", dist=24, boss=0, sub=50, target="subordinate", toward=True, near=12),
    ]
    pressure_boss_dmg = sum(p.damage_to_boss_last_10s for p in pressure_players)

    stable_players = [
        P("P1", dist=36, boss=8, sub=6, target="none", toward=True, near=16),
        P("P2", dist=42, boss=6, sub=8, target="subordinate", toward=True, near=15),
        P("P3", dist=48, boss=7, sub=4, target="none", toward=True, near=18),
        P("P4", dist=54, boss=5, sub=5, target="none", toward=True, near=20),
        P("P5", dist=61, boss=4, sub=3, target="none", toward=True, near=22),
        P("P6", dist=50, boss=3, sub=2, target="none", toward=True, near=17),
        P("P7", dist=70, boss=2, sub=2, target="none", toward=True, near=24),
        P("P8", dist=44, boss=0, sub=4, target="subordinate", toward=True, near=16),
    ]
    stable_boss_dmg = sum(p.damage_to_boss_last_10s for p in stable_players)

    bait_players = [
        P("P1", dist=18, boss=120, sub=8, target="boss", toward=True, near=10),
        P("P2", dist=20, boss=80, sub=12, target="boss", toward=True, near=9),
        P("P3", dist=22, boss=35, sub=40, target="subordinate", toward=True, near=11),
        P("P4", dist=28, boss=5, sub=20, target="subordinate", toward=True, near=13),
        P("P5", dist=34, boss=0, sub=15, target="subordinate", toward=True, near=15),
        P("P6", dist=46, boss=0, sub=6, target="none", toward=True, near=18),
        P("P7", dist=60, boss=0, sub=4, target="none", toward=False, near=22),
        P("P12", dist=155, boss=0, sub=90, target="subordinate", toward=False, near=95),
    ]
    bait_boss_dmg = sum(p.damage_to_boss_last_10s for p in bait_players)

    few_players = [
        P("P1", dist=14, boss=200, sub=10, target="boss", toward=True, near=12),
        P("P2", dist=19, boss=60, sub=8, target="boss", toward=True, near=12),
        P("P3", dist=40, boss=0, sub=0, target="none", toward=False, near=20, alive=False),
        P("P4", dist=55, boss=0, sub=0, target="none", toward=False, near=22, alive=False),
        P("P5", dist=70, boss=0, sub=0, target="none", toward=False, near=25, alive=False),
        P("P6", dist=80, boss=0, sub=0, target="none", toward=False, near=30, alive=False),
    ]
    few_boss_dmg = sum(p.damage_to_boss_last_10s for p in few_players)

    quiet_players = [
        P("P1", dist=96, boss=3, sub=1, target="none", toward=False, near=22),
        P("P2", dist=104, boss=2, sub=2, target="none", toward=False, near=20),
        P("P3", dist=112, boss=2, sub=0, target="none", toward=False, near=24),
        P("P4", dist=118, boss=1, sub=1, target="none", toward=False, near=26),
        P("P5", dist=90, boss=0, sub=2, target="none", toward=False, near=28),
        P("P6", dist=125, boss=0, sub=0, target="none", toward=False, near=30),
        P("P7", dist=132, boss=0, sub=1, target="none", toward=False, near=32),
        P("P8", dist=108, boss=0, sub=1, target="none", toward=False, near=21),
    ]
    quiet_boss_dmg = sum(p.damage_to_boss_last_10s for p in quiet_players)

    contra_players = [
        P("P1", dist=12, boss=680, sub=10, target="boss", toward=True, near=10),
        P("P2", dist=20, boss=20, sub=25, target="subordinate", toward=True, near=11),
        P("P3", dist=26, boss=10, sub=30, target="subordinate", toward=True, near=13),
        P("P4", dist=34, boss=8, sub=18, target="subordinate", toward=False, near=16),
        P("P5", dist=48, boss=2, sub=8, target="none", toward=False, near=20),
        P("P6", dist=70, boss=0, sub=6, target="none", toward=False, near=28),
        P("P7", dist=88, boss=0, sub=4, target="subordinate", toward=False, near=35),
        P("P8", dist=145, boss=0, sub=50, target="subordinate", toward=False, near=80),
    ]
    contra_boss_dmg = sum(p.damage_to_boss_last_10s for p in contra_players)

    thin_players = [
        P("P1", dist=24, boss=12, sub=40, target="subordinate", toward=True, near=14),
        P("P2", dist=28, boss=10, sub=35, target="subordinate", toward=True, near=12),
        P("P3", dist=36, boss=8, sub=20, target="subordinate", toward=True, near=16),
        P("P4", dist=50, boss=6, sub=8, target="none", toward=True, near=20),
        P("P5", dist=62, boss=4, sub=5, target="none", toward=False, near=24),
    ]
    thin_boss_dmg = sum(p.damage_to_boss_last_10s for p in thin_players)

    flee_players = [
        P("P1", dist=160, boss=8, sub=4, target="none", toward=False, near=None),
        P("P2", dist=40, boss=0, sub=0, target="none", toward=False, near=20, alive=False),
        P("P3", dist=55, boss=0, sub=0, target="none", toward=False, near=22, alive=False),
    ]
    flee_boss_dmg = sum(p.damage_to_boss_last_10s for p in flee_players)

    return [
        _sc(
            "initial_encounter",
            "Initial encounter",
            "Boss healthy, full swarm, players approaching, little recent damage.",
            _enc(
                "initial_encounter",
                20,
                _boss(100, initial_boss_dmg, "press_attack", True, 0),
                _swarm(60, 56, 4, 0, 0, 0, "press_attack"),
                initial_players,
            ),
            [
                _noul_below("boss_in_critical_danger", 0.5, "Healthy boss, negligible damage."),
                _noul_below("reinforcement_wave_warranted", 0.6, "Full swarm, no losses."),
                _choice_not("tactical_posture", "desperate_defence", "Nothing justifies emergency defence."),
                _choice_not("reinforcement_action", "spawn_now", "No attrition yet."),
            ],
        ),
        _sc(
            "normal_engagement",
            "Normal engagement",
            "Moderate losses, several players damaging subordinates, boss still healthy.",
            _enc(
                "normal_engagement",
                90,
                _boss(78, normal_boss_dmg, "press_attack", False, 40),
                _swarm(51, 38, 10, 3, 4, 9, "press_attack"),
                normal_players,
            ),
            [
                _noul_below("boss_in_critical_danger", 0.5, "Boss at 78% is not critical."),
                _choice_not("tactical_posture", "desperate_defence", "Ordinary fight, not a last stand."),
                _choice_is("reinforcement_action", "hold", "No reinforcement capacity."),
            ],
        ),
        _sc(
            "boss_focused",
            "Boss being focused",
            "P3 accounts for most recent boss damage; boss health is falling.",
            _enc(
                "boss_focused",
                140,
                _boss(54, focused_boss_dmg, "hold_close", False, 30),
                _swarm(44, 36, 6, 2, 3, 7, "hold_close"),
                focused_players,
            ),
            [
                _noul_above("focus_fire_warranted", 0.5, "One player dominates recent boss damage."),
                _choice_is("primary_target", "P3", "P3 is the concentrated boss threat."),
                _choice_not("primary_target", "none", "A clear focus target exists."),
            ],
        ),
        _sc(
            "swarm_dispersed",
            "Swarm becoming dispersed",
            "Substantial fraction of NPCs far from the boss; two distant players drawing them.",
            _enc(
                "swarm_dispersed",
                160,
                _boss(72, dispersed_boss_dmg, "press_attack", False, 25),
                _swarm(48, 14, 10, 24, 5, 8, "press_attack"),
                dispersed_players,
            ),
            [
                _noul_above("swarm_overextended", 0.5, "Half the swarm is detached."),
                _noul_above("recall_detached_units", 0.5, "Far units are being drawn off."),
                _choice_in(
                    "formation_intent",
                    ["recall_detached", "tight_screen", "balanced"],
                    "Wide pressure would worsen dispersion.",
                ),
                _choice_not("primary_target", "P8", "Distant drawer should not become the focus."),
                _choice_not("primary_target", "P9", "Distant drawer should not become the focus."),
            ],
        ),
        _sc(
            "heavy_attrition",
            "Heavy recent attrition",
            "Many NPC losses over a short interval; players are chewing through subordinates.",
            _enc(
                "heavy_attrition",
                110,
                _boss(61, attrition_boss_dmg, "press_attack", False, 20),
                _swarm(27, 20, 5, 2, 18, 31, "press_attack"),
                attrition_players,
            ),
            [
                _noul_above("regroup_warranted", 0.5, "18 losses in 10s is severe."),
                _noul_above("preserve_swarm_priority", 0.5, "Remaining swarm is already halved."),
                _choice_not("tactical_posture", "press_attack", "Continuing a full press after a wipe pulse is questionable."),
            ],
        ),
        _sc(
            "boss_critical",
            "Boss critically damaged",
            "Very low boss health and a reduced swarm; remaining players are on the boss.",
            _enc(
                "boss_critical",
                210,
                _boss(14, critical_boss_dmg, "hold_close", False, 12),
                _swarm(16, 12, 3, 1, 6, 14, "hold_close"),
                critical_players,
            ),
            [
                _noul_above("boss_in_critical_danger", 0.5, "14% health under focused fire."),
                _noul_above("protect_boss_priority", 0.5, "Commander survivability is the live problem."),
                _choice_in(
                    "tactical_posture",
                    ["desperate_defence", "hold_close", "regroup"],
                    "Pressing the attack at 14% health is a questionable call.",
                ),
                _choice_not("tactical_posture", "press_attack", "Offensive posture with a dying boss."),
            ],
        ),
        _sc(
            "reinforce_under_pressure",
            "Reinforcements available under pressure",
            "Capacity is up, cooldown is zero, swarm is depleted, and players are still applying damage.",
            _enc(
                "reinforce_under_pressure",
                150,
                _boss(42, pressure_boss_dmg, "hold_close", True, 0),
                _swarm(29, 18, 6, 5, 14, 22, "hold_close"),
                pressure_players,
            ),
            [
                _noul_above("reinforcement_wave_warranted", 0.5, "Depleted swarm, pressure on, capacity free."),
                _choice_is("reinforcement_action", "spawn_now", "This is the case spawn is for."),
            ],
        ),
        _sc(
            "reinforce_while_stable",
            "Reinforcements available while stable",
            "Capacity is up, but the encounter is currently quiet and the swarm is nearly full.",
            _enc(
                "reinforce_while_stable",
                70,
                _boss(91, stable_boss_dmg, "press_attack", True, 0),
                _swarm(57, 50, 6, 1, 1, 2, "press_attack"),
                stable_players,
            ),
            [
                _noul_below("reinforcement_wave_warranted", 0.5, "No meaningful depletion."),
                _choice_is("reinforcement_action", "hold", "Spawning into a stable fight wastes the wave."),
            ],
        ),
        _sc(
            "distant_bait",
            "Distant bait while others stay on the boss",
            "P12 is 155 km away drawing subordinates; P1/P2 remain on the boss.",
            _enc(
                "distant_bait",
                175,
                _boss(70, bait_boss_dmg, "press_attack", False, 18),
                _swarm(40, 22, 6, 12, 4, 6, "press_attack"),
                bait_players,
            ),
            [
                _noul_above("distant_bait_should_be_ignored", 0.5, "P12 is a far draw, not the boss threat."),
                _noul_below("pursuit_warranted", 0.5, "Chasing 155 km abandons the boss fight."),
                _choice_not("primary_target", "P12", "The distant bait should not be primary."),
                _choice_in("primary_target", ["P1", "P2"], "Recent boss damage is on nearby P1/P2."),
            ],
        ),
        _sc(
            "few_players_boss_damaged",
            "Few players remain but the boss is badly damaged",
            "Only two hostiles left, both on the boss, commander already at 18%.",
            _enc(
                "few_players_boss_damaged",
                240,
                _boss(18, few_boss_dmg, "hold_close", False, 8),
                _swarm(22, 18, 3, 1, 3, 8, "hold_close"),
                few_players,
            ),
            [
                _noul_above("boss_in_critical_danger", 0.5, "18% health, two players still on the boss."),
                _noul_above("protect_boss_priority", 0.5, "Low remaining hostility does not make 18% safe."),
                _choice_not("tactical_posture", "press_attack", "A dying boss should not hunt."),
                _choice_in("primary_target", ["P1", "P2"], "One of the two remaining damagers."),
            ],
        ),
        _sc(
            "no_pressure",
            "No meaningful hostile pressure",
            "Boss healthy, full swarm, players far and dealing almost no damage.",
            _enc(
                "no_pressure",
                40,
                _boss(96, quiet_boss_dmg, "press_attack", True, 0),
                _swarm(60, 58, 2, 0, 0, 0, "press_attack"),
                quiet_players,
            ),
            [
                _noul_below("boss_in_critical_danger", 0.4, "No real threat."),
                _noul_below("pursuit_warranted", 0.5, "Chasing disengaged far players is a poor spend."),
                _choice_is("reinforcement_action", "hold", "Nothing to reinforce against."),
                _choice_not("tactical_posture", "desperate_defence", "Emergency defence with no pressure."),
                _choice_in("primary_target", ["none", "P1", "P2"], "none is the clean call; a nearby-ish id is merely soft."),
            ],
        ),
        _sc(
            "contradictory_pressures",
            "Contradictory pressures",
            "Dangerous boss attacker, dispersed swarm, heavy subordinate losses, reinforcement available.",
            _enc(
                "contradictory_pressures",
                190,
                _boss(36, contra_boss_dmg, "press_attack", True, 0),
                _swarm(24, 6, 5, 13, 16, 24, "press_attack"),
                contra_players,
            ),
            [
                _noul_above("boss_in_critical_danger", 0.4, "P1 is deleting the boss at 36%."),
                _noul_above("swarm_overextended", 0.5, "Most remaining NPCs are not close."),
                _choice_not("primary_target", "P8", "The far bait is not the boss killer."),
                _choice_is("primary_target", "P1", "P1 is the concentrated lethal threat."),
                _choice_in(
                    "reinforcement_action",
                    ["spawn_now", "hold"],
                    "Either call is arguable; spawn is the more natural read of capacity-plus-collapse.",
                ),
            ],
        ),
        _sc(
            "healthy_boss_thin_swarm",
            "Healthy boss, thin swarm",
            "Boss is fine; subordinates have been eroded to 11. Preserve-vs-press trade-off.",
            _enc(
                "healthy_boss_thin_swarm",
                200,
                _boss(88, thin_boss_dmg, "press_attack", False, 22),
                _swarm(11, 8, 2, 1, 6, 20, "press_attack"),
                thin_players,
            ),
            [
                _noul_below("boss_in_critical_danger", 0.5, "Boss is not the problem."),
                _noul_above("preserve_swarm_priority", 0.5, "11 NPCs left is a preservation problem."),
                _choice_not("tactical_posture", "desperate_defence", "Boss is at 88%."),
            ],
        ),
        _sc(
            "collapsing_no_reinforce",
            "Collapsing without reinforcement capacity",
            "Same pressure as the spawn-now case, but capacity is down and cooldown is long.",
            _enc(
                "collapsing_no_reinforce",
                150,
                _boss(42, pressure_boss_dmg, "hold_close", False, 90),
                _swarm(29, 18, 6, 5, 14, 22, "hold_close"),
                pressure_players,
            ),
            [
                _choice_is("reinforcement_action", "hold", "Capacity is not available."),
                _noul_below("reinforcement_wave_warranted", 0.55, "Warranted-but-impossible should still read as no."),
            ],
        ),
        _sc(
            "last_player_fleeing",
            "Last player fleeing far away",
            "One surviving player at 160 km, low damage, moving away. Pursuit is the trap.",
            _enc(
                "last_player_fleeing",
                260,
                _boss(40, flee_boss_dmg, "press_attack", False, 15),
                _swarm(35, 30, 4, 1, 1, 4, "press_attack"),
                flee_players,
            ),
            [
                _noul_below("pursuit_warranted", 0.5, "A 160 km chase for 8 recent damage is a poor spend."),
                _choice_in("primary_target", ["none", "P1"], "none is cleaner; P1 is the only alive id."),
                _choice_not("tactical_posture", "desperate_defence", "Boss at 40% with no real pressure."),
            ],
        ),
    ]


def _control_players() -> list[Player]:
    return [
        P("P1", dist=22, boss=20, sub=18, target="boss", toward=True, near=12),
        P("P2", dist=26, boss=15, sub=40, target="subordinate", toward=True, near=11),
        P("P3", dist=20, boss=50, sub=12, target="boss", toward=True, near=10),
        P("P4", dist=34, boss=12, sub=22, target="subordinate", toward=True, near=14),
        P("P5", dist=48, boss=10, sub=14, target="subordinate", toward=False, near=18),
        P("P6", dist=60, boss=8, sub=8, target="none", toward=True, near=22),
        P("P7", dist=72, boss=5, sub=6, target="none", toward=True, near=25),
        P("P8", dist=30, boss=0, sub=28, target="subordinate", toward=True, near=13),
    ]


def _control_encounter(sid: str, mutate: Callable[[Boss, Swarm, tuple[Player, ...]], tuple[Boss, Swarm, tuple[Player, ...]]]) -> Encounter:
    players = tuple(_control_players())
    total = sum(p.damage_to_boss_last_10s for p in players)
    boss = _boss(80, total, "press_attack", False, 20)
    swarm = _swarm(45, 32, 8, 5, 3, 6, "press_attack")
    boss, swarm, players = mutate(boss, swarm, players)
    total = sum(p.damage_to_boss_last_10s for p in players if p.alive)
    boss = replace(boss, damage_received_last_10s=float(total))
    return _enc(sid, 120, boss, swarm, players)


def _pair(
    pid: str,
    title: str,
    field: str,
    base_mut: Callable[[Boss, Swarm, tuple[Player, ...]], tuple[Boss, Swarm, tuple[Player, ...]]],
    var_mut: Callable[[Boss, Swarm, tuple[Player, ...]], tuple[Boss, Swarm, tuple[Player, ...]]],
    notes: str,
    expectations: list[dict[str, Any]],
) -> SensitivityPair:
    base = _sc(f"{pid}_base", f"{title} (base)", title, _control_encounter(f"{pid}_base", base_mut))
    variant = _sc(f"{pid}_variant", f"{title} (variant)", title, _control_encounter(f"{pid}_variant", var_mut))
    return SensitivityPair(
        id=pid,
        title=title,
        changed_field=field,
        base=base,
        variant=variant,
        accounting_notes=notes,
        expectations=tuple(expectations),
    )


def _exp_noul(qid: str, direction: str, note: str) -> dict[str, Any]:
    return {"kind": "noul", "id": qid, "direction": direction, "note": note}


def _exp_choice_prob(qid: str, option: str, direction: str, note: str) -> dict[str, Any]:
    return {"kind": "choice_prob", "id": qid, "option": option, "direction": direction, "note": note}


def build_sensitivity_pairs() -> list[SensitivityPair]:
    def identity(boss: Boss, swarm: Swarm, players: tuple[Player, ...]) -> tuple[Boss, Swarm, tuple[Player, ...]]:
        return boss, swarm, players

    pairs = [
        _pair(
            "boss_health_80_vs_20",
            "Boss health 80% -> 20%",
            "boss.health_pct",
            identity,
            lambda b, s, p: (replace(b, health_pct=20.0), s, p),
            "Only boss.health_pct changes. Damage, swarm, and players stay fixed.",
            [
                _exp_noul("boss_in_critical_danger", "not_decrease", "Lower health should not make critical danger less likely."),
                _exp_noul("protect_boss_priority", "not_decrease", "A 20% boss should not lower protect-self priority."),
                _exp_choice_prob(
                    "tactical_posture",
                    "desperate_defence",
                    "not_decrease",
                    "Desperate defence should not become less probable.",
                ),
                _exp_choice_prob(
                    "tactical_posture",
                    "press_attack",
                    "not_increase",
                    "Pressing the attack should not become more probable at 20% health.",
                ),
            ],
        ),
        _pair(
            "recent_boss_damage_low_vs_severe",
            "Recent boss damage low -> severe",
            "player boss-damage totals (and matching boss.damage_received_last_10s)",
            lambda b, s, p: (b, s, scale_player_boss_damage(p, 40.0)),
            lambda b, s, p: (b, s, scale_player_boss_damage(p, 520.0)),
            "The intended variable is recent boss-damage intensity. Player shares keep their proportions; "
            "boss.damage_received_last_10s is rewritten to the player sum so the snapshot stays consistent.",
            [
                _exp_noul("boss_in_critical_danger", "not_decrease", "A damage spike should not lower critical danger."),
                _exp_noul("focus_fire_warranted", "not_decrease", "More concentrated incoming damage should not lower focus-fire warrant."),
                _exp_noul("current_plan_still_sensible", "not_increase", "A sudden spike should not make the current plan more sensible."),
            ],
        ),
        _pair(
            "sub_count_60_vs_15",
            "Subordinate count 60 -> 15",
            "swarm.alive (close_to_boss is the remainder bucket)",
            lambda b, s, p: (b, replace(s, alive=60, close_to_boss=47, medium_range=8, detached_far=5), p),
            lambda b, s, p: (b, replace(s, alive=15, close_to_boss=2, medium_range=8, detached_far=5), p),
            "alive must equal the range-bucket sum, so close_to_boss is the accounting remainder. "
            "medium_range and detached_far stay 8 and 5.",
            [
                _exp_noul("preserve_swarm_priority", "not_decrease", "A thin swarm should not lower preservation priority."),
                _exp_noul("regroup_warranted", "not_decrease", "Losing most of the swarm should not make regroup less likely."),
                _exp_noul("pressure_can_be_maintained", "not_increase", "15 NPCs should not make continued pressure more plausible."),
            ],
        ),
        _pair(
            "detached_5_vs_35",
            "Detached subordinates 5 -> 35",
            "swarm.detached_far (close_to_boss is the remainder bucket)",
            lambda b, s, p: (b, replace(s, alive=45, close_to_boss=32, medium_range=8, detached_far=5), p),
            lambda b, s, p: (b, replace(s, alive=45, close_to_boss=2, medium_range=8, detached_far=35), p),
            "alive stays 45 and medium_range stays 8. close_to_boss falls because buckets must sum to alive.",
            [
                _exp_noul("swarm_overextended", "not_decrease", "More detached units should not lower overextension."),
                _exp_noul("recall_detached_units", "not_decrease", "Recall should not become less warranted."),
                _exp_choice_prob(
                    "formation_intent",
                    "recall_detached",
                    "not_decrease",
                    "Recall formation should not become less probable.",
                ),
                _exp_choice_prob(
                    "formation_intent",
                    "wide_pressure",
                    "not_increase",
                    "Wide pressure should not become more probable when the swarm is already scattered.",
                ),
            ],
        ),
        _pair(
            "losses_2_vs_20",
            "Recent subordinate losses 2 -> 20",
            "swarm.losses_last_10s (losses_last_30s stays >= 10s losses)",
            lambda b, s, p: (b, replace(s, losses_last_10s=2, losses_last_30s=6), p),
            lambda b, s, p: (b, replace(s, losses_last_10s=20, losses_last_30s=24), p),
            "losses_last_30s cannot be smaller than losses_last_10s, so both rise together. alive is unchanged; "
            "this is a recent-loss pulse, not a new headcount.",
            [
                _exp_noul("regroup_warranted", "not_decrease", "A loss pulse should not make regroup less likely."),
                _exp_noul("preserve_swarm_priority", "not_decrease", "Heavy recent deaths should not lower preservation."),
                _exp_noul("current_plan_still_sensible", "not_increase", "The current plan should not look more sensible after a wipe pulse."),
            ],
        ),
        _pair(
            "reinforce_unavailable_vs_available",
            "Reinforcement unavailable -> available under pressure",
            "boss.reinforcement_capacity_available",
            lambda b, s, p: (
                replace(b, health_pct=48.0, reinforcement_capacity_available=False, reinforcement_cooldown_s=80.0),
                replace(s, losses_last_10s=12, losses_last_30s=18, alive=32, close_to_boss=19, medium_range=8, detached_far=5),
                p,
            ),
            lambda b, s, p: (
                replace(b, health_pct=48.0, reinforcement_capacity_available=True, reinforcement_cooldown_s=0.0),
                replace(s, losses_last_10s=12, losses_last_30s=18, alive=32, close_to_boss=19, medium_range=8, detached_far=5),
                p,
            ),
            "Both sides use the same pressured mid-fight snapshot. Only capacity and cooldown change.",
            [
                _exp_noul(
                    "reinforcement_wave_warranted",
                    "not_decrease",
                    "Turning capacity on under pressure should not make a wave less warranted.",
                ),
                _exp_choice_prob(
                    "reinforcement_action",
                    "spawn_now",
                    "not_decrease",
                    "spawn_now should not become less probable when a wave becomes possible.",
                ),
            ],
        ),
        _pair(
            "p3_boss_damage_50_vs_800",
            "P3 recent boss damage 50 -> 800",
            "players[P3].damage_to_boss_last_10s",
            lambda b, s, p: (b, s, replace_player(p, "P3", damage_to_boss_last_10s=50.0)),
            lambda b, s, p: (b, s, replace_player(p, "P3", damage_to_boss_last_10s=800.0)),
            "Only P3's recent boss damage changes. boss.damage_received_last_10s and derived top-damager "
            "fields are recomputed from the player list so the snapshot stays consistent.",
            [
                _exp_choice_prob(
                    "primary_target",
                    "P3",
                    "not_decrease",
                    "P3 should become at least as plausible a focus target.",
                ),
                _exp_noul("focus_fire_warranted", "not_decrease", "An 800-damage outlier should not lower focus-fire warrant."),
                _exp_noul("boss_in_critical_danger", "not_decrease", "A huge incoming spike should not lower danger."),
            ],
        ),
        _pair(
            "hostile_distance_20_vs_150",
            "Hostile distance 20 km -> 150 km",
            "all alive players' distance_from_boss_km",
            lambda b, s, p: (b, s, tuple(Player(**{**asdict(x), "distance_from_boss_km": 20.0}) for x in p)),
            lambda b, s, p: (
                b,
                s,
                tuple(
                    Player(**{**asdict(x), "distance_from_boss_km": 150.0, "moving_toward_boss": False})
                    for x in p
                ),
            ),
            "Every alive player is placed at 20 km or 150 km. Damage stays fixed so the only tactical change "
            "is range (and toward/away on the far side, because 150 km hostiles are not approaching).",
            [
                _exp_noul("pursuit_warranted", "not_increase", "150 km hostiles should not make pursuit more warranted."),
                _exp_noul("distant_bait_should_be_ignored", "not_decrease", "Far hostiles should not make ignore-distant less likely."),
                _exp_noul("boss_in_critical_danger", "not_increase", "The same damage at much greater range should not raise immediate danger."),
            ],
        ),
    ]
    return pairs


def all_scenarios() -> list[Scenario]:
    out = list(build_main_scenarios())
    seen = {s.id for s in out}
    for pair in build_sensitivity_pairs():
        if pair.base.id not in seen:
            out.append(pair.base)
            seen.add(pair.base.id)
        if pair.variant.id not in seen:
            out.append(pair.variant)
            seen.add(pair.variant.id)
    return out


def scenario_by_id() -> dict[str, Scenario]:
    return {s.id: s for s in all_scenarios()}


def main_scenario_ids() -> list[str]:
    return [s.id for s in build_main_scenarios()]


def stability_scenario_ids() -> list[str]:
    return ["initial_encounter", "contradictory_pressures", "boss_critical"]


def cadence_scenario_id() -> str:
    return "normal_engagement"


def material_change(prev: Encounter, curr: Encounter) -> list[str]:
    """Reasons a commander might infer again rather than at a fixed Hz."""
    reasons: list[str] = []
    if abs(prev.boss.health_pct - curr.boss.health_pct) >= 10:
        reasons.append("boss_health_delta_ge_10")
    if abs(prev.swarm.alive - curr.swarm.alive) >= 8:
        reasons.append("swarm_alive_delta_ge_8")
    if abs(prev.swarm.detached_far - curr.swarm.detached_far) >= 10:
        reasons.append("detached_delta_ge_10")
    if abs(prev.swarm.losses_last_10s - curr.swarm.losses_last_10s) >= 5:
        reasons.append("losses10_delta_ge_5")
    if prev.derived["players_alive"] != curr.derived["players_alive"]:
        reasons.append("player_alive_count_changed")
    prev_top = (prev.derived.get("top_boss_damager") or {}).get("id")
    curr_top = (curr.derived.get("top_boss_damager") or {}).get("id")
    if prev_top != curr_top:
        reasons.append("top_boss_damager_changed")
    if prev.boss.reinforcement_capacity_available != curr.boss.reinforcement_capacity_available:
        reasons.append("reinforcement_capacity_changed")
    if prev.boss.current_posture != curr.boss.current_posture:
        reasons.append("posted_plan_changed")
    return reasons
