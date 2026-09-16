from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))

from commander.client import measure_payload  # noqa: E402
from commander.controller import (  # noqa: E402
    AMBIGUOUS_CHOICE_CONFIDENCE,
    compose_command,
    deterministic_command,
    synthetic_answers,
)
from commander.evaluate import evaluate_sensitivity_pair, event_driven_illustration, scale_table  # noqa: E402
from commander.questions import build_questions, question_count  # noqa: E402
from commander.scenarios import (  # noqa: E402
    build_main_scenarios,
    build_sensitivity_pairs,
    material_change,
    scenario_by_id,
)
from commander.state import to_jev_state  # noqa: E402
from questions import load_gold  # noqa: E402
from score import score_run  # noqa: E402


class EncounterTests(unittest.TestCase):
    def test_all_main_scenarios_finalize(self) -> None:
        scenarios = build_main_scenarios()
        self.assertGreaterEqual(len(scenarios), 12)
        ids = [s.id for s in scenarios]
        self.assertEqual(len(ids), len(set(ids)))
        for scenario in scenarios:
            enc = scenario.encounter
            self.assertEqual(
                enc.swarm.close_to_boss + enc.swarm.medium_range + enc.swarm.detached_far,
                enc.swarm.alive,
            )
            self.assertTrue(enc.derived["boss_damage_matches_player_sum"])
            self.assertLessEqual(enc.swarm.alive, 60)
            self.assertLessEqual(enc.derived["players_alive"], 12)

    def test_derived_top_damager(self) -> None:
        focused = scenario_by_id()["boss_focused"].encounter
        self.assertEqual(focused.derived["top_boss_damager"]["id"], "P3")
        self.assertGreater(focused.derived["top_boss_damage_share"], 0.7)
        bait = scenario_by_id()["distant_bait"].encounter
        self.assertIn("P12", bait.derived["isolated_alive_player_ids"])
        self.assertEqual(bait.derived["top_boss_damager"]["id"], "P1")

    def test_dead_players_not_in_target_options(self) -> None:
        few = scenario_by_id()["few_players_boss_damaged"].encounter
        questions = build_questions(few)
        options = set(questions["primary_target"]["criteria"])
        self.assertIn("none", options)
        self.assertIn("P1", options)
        self.assertIn("P2", options)
        self.assertNotIn("P3", options)
        self.assertNotIn("P4", options)

    def test_question_set_size_and_no_fact_leak_labels(self) -> None:
        enc = scenario_by_id()["initial_encounter"].encounter
        questions = build_questions(enc)
        counts = question_count(questions)
        self.assertGreaterEqual(counts["total"], 10)
        self.assertLessEqual(counts["total"], 20)
        self.assertEqual(counts["noul"], 12)
        self.assertEqual(counts["choice"], 4)
        blob = json.dumps(questions)
        self.assertNotIn("expected", blob)
        self.assertNotIn("gold", blob)

    def test_payload_is_much_smaller_than_coding_tests(self) -> None:
        enc = scenario_by_id()["contradictory_pressures"].encounter
        measure = measure_payload(enc)
        self.assertLess(measure["payload_chars"], 20000)
        self.assertLess(measure["token_estimates"]["est_chars_div_4"], 8000)
        self.assertLess(measure["state_chars"], 8000)


class ControllerTests(unittest.TestCase):
    def test_critical_boss_protects(self) -> None:
        enc = scenario_by_id()["boss_critical"].encounter
        cmd = deterministic_command(enc)
        self.assertEqual(cmd.tactical_posture, "desperate_defence")
        self.assertEqual(cmd.formation_intent, "tight_screen")
        self.assertTrue(cmd.flags["boss_in_critical_danger"])
        self.assertTrue(cmd.flags["protect_boss_priority"])

    def test_stable_reinforce_holds(self) -> None:
        enc = scenario_by_id()["reinforce_while_stable"].encounter
        cmd = deterministic_command(enc)
        self.assertEqual(cmd.reinforcement_action, "hold")
        self.assertFalse(cmd.flags["reinforcement_wave_warranted"])

    def test_pressure_with_capacity_spawns(self) -> None:
        enc = scenario_by_id()["reinforce_under_pressure"].encounter
        cmd = deterministic_command(enc)
        self.assertEqual(cmd.reinforcement_action, "spawn_now")

    def test_no_capacity_never_spawns(self) -> None:
        enc = scenario_by_id()["collapsing_no_reinforce"].encounter
        cmd = deterministic_command(enc)
        self.assertEqual(cmd.reinforcement_action, "hold")
        self.assertFalse(enc.boss.reinforcement_capacity_available)

    def test_focused_player_is_primary_target(self) -> None:
        enc = scenario_by_id()["boss_focused"].encounter
        cmd = deterministic_command(enc)
        self.assertEqual(cmd.primary_target, "P3")

    def test_distant_bait_is_not_primary_target(self) -> None:
        enc = scenario_by_id()["distant_bait"].encounter
        cmd = deterministic_command(enc)
        self.assertNotEqual(cmd.primary_target, "P12")
        self.assertIn(cmd.primary_target, {"P1", "P2"})

    def test_timeout_retains_last_command(self) -> None:
        enc = scenario_by_id()["normal_engagement"].encounter
        last = deterministic_command(enc)
        applied = compose_command(enc, None, last=last, error="timeout")
        self.assertFalse(applied.used_jev)
        self.assertEqual(applied.source, "deterministic_fallback")
        self.assertEqual(applied.command.tactical_posture, last.tactical_posture)

    def test_timeout_without_last_uses_deterministic(self) -> None:
        enc = scenario_by_id()["boss_critical"].encounter
        applied = compose_command(enc, None, error="api_error")
        self.assertEqual(applied.command.tactical_posture, "desperate_defence")
        self.assertFalse(applied.used_jev)

    def test_malformed_answers_fall_back(self) -> None:
        enc = scenario_by_id()["initial_encounter"].encounter
        applied = compose_command(enc, {"tactical_posture": {"type": "choice", "choice": "not_a_real_posture"}})
        self.assertIn(applied.command.tactical_posture, {"press_attack", "hold_close", "regroup", "desperate_defence"})
        self.assertEqual(applied.field_sources["tactical_posture"], "deterministic")

    def test_low_confidence_choice_uses_deterministic(self) -> None:
        enc = scenario_by_id()["initial_encounter"].encounter
        det = deterministic_command(enc)
        applied = compose_command(
            enc,
            {
                "tactical_posture": {
                    "type": "choice",
                    "choice": "desperate_defence",
                    "confidence": AMBIGUOUS_CHOICE_CONFIDENCE - 0.05,
                    "probabilities": {
                        "press_attack": 0.3,
                        "hold_close": 0.3,
                        "regroup": 0.2,
                        "desperate_defence": 0.2,
                    },
                }
            },
        )
        self.assertEqual(applied.command.tactical_posture, det.tactical_posture)
        self.assertEqual(applied.field_sources["tactical_posture"], "deterministic_ambiguous")

    def test_jev_cannot_spawn_without_capacity(self) -> None:
        enc = scenario_by_id()["collapsing_no_reinforce"].encounter
        applied = compose_command(
            enc,
            {
                "reinforcement_action": {
                    "type": "choice",
                    "choice": "spawn_now",
                    "confidence": 0.99,
                    "probabilities": {"hold": 0.01, "spawn_now": 0.99},
                },
                "reinforcement_wave_warranted": {"type": "noul", "noul": 0.99},
            },
        )
        self.assertEqual(applied.command.reinforcement_action, "hold")
        self.assertEqual(applied.field_sources["reinforcement_action"], "deterministic_override")

    def test_dead_player_target_rejected(self) -> None:
        enc = scenario_by_id()["few_players_boss_damaged"].encounter
        applied = compose_command(
            enc,
            {
                "primary_target": {
                    "type": "choice",
                    "choice": "P5",
                    "confidence": 0.99,
                    "probabilities": {"P5": 1.0},
                }
            },
        )
        self.assertNotEqual(applied.command.primary_target, "P5")
        self.assertIn("malformed_or_dead_primary_target", applied.reasons)

    def test_always_returns_a_command(self) -> None:
        enc = scenario_by_id()["contradictory_pressures"].encounter
        for payload in (None, {}, {"nope": 1}, synthetic_answers(enc)):
            applied = compose_command(enc, payload if isinstance(payload, dict) else None, error=None if payload else "empty")
            self.assertIsNotNone(applied.command.tactical_posture)
            self.assertIsNotNone(applied.command.primary_target)


class SensitivityCorpusTests(unittest.TestCase):
    def test_pairs_change_the_intended_axis(self) -> None:
        pairs = build_sensitivity_pairs()
        self.assertGreaterEqual(len(pairs), 8)
        health = next(p for p in pairs if p.id == "boss_health_80_vs_20")
        self.assertEqual(health.base.encounter.boss.health_pct, 80)
        self.assertEqual(health.variant.encounter.boss.health_pct, 20)
        self.assertEqual(
            health.base.encounter.swarm.alive,
            health.variant.encounter.swarm.alive,
        )
        self.assertEqual(
            health.base.encounter.boss.damage_received_last_10s,
            health.variant.encounter.boss.damage_received_last_10s,
        )

        dmg = next(p for p in pairs if p.id == "p3_boss_damage_50_vs_800")
        base_p3 = next(p for p in dmg.base.encounter.players if p.id == "P3")
        var_p3 = next(p for p in dmg.variant.encounter.players if p.id == "P3")
        self.assertEqual(base_p3.damage_to_boss_last_10s, 50)
        self.assertEqual(var_p3.damage_to_boss_last_10s, 800)
        self.assertEqual(dmg.variant.encounter.derived["top_boss_damager"]["id"], "P3")

        far = next(p for p in pairs if p.id == "hostile_distance_20_vs_150")
        self.assertTrue(all(p.distance_from_boss_km == 20 for p in far.base.encounter.players if p.alive))
        self.assertTrue(all(p.distance_from_boss_km == 150 for p in far.variant.encounter.players if p.alive))

    def test_directional_helper_catches_wrong_way_move(self) -> None:
        pair = next(p for p in build_sensitivity_pairs() if p.id == "boss_health_80_vs_20")
        base = {
            "boss_in_critical_danger": {"noul": 0.2},
            "protect_boss_priority": {"noul": 0.3},
            "tactical_posture": {
                "choice": "press_attack",
                "probabilities": {"press_attack": 0.7, "desperate_defence": 0.1, "hold_close": 0.1, "regroup": 0.1},
            },
        }
        good = {
            "boss_in_critical_danger": {"noul": 0.9},
            "protect_boss_priority": {"noul": 0.8},
            "tactical_posture": {
                "choice": "desperate_defence",
                "probabilities": {"press_attack": 0.05, "desperate_defence": 0.8, "hold_close": 0.1, "regroup": 0.05},
            },
        }
        bad = {
            "boss_in_critical_danger": {"noul": 0.05},
            "protect_boss_priority": {"noul": 0.1},
            "tactical_posture": {
                "choice": "press_attack",
                "probabilities": {"press_attack": 0.9, "desperate_defence": 0.02, "hold_close": 0.04, "regroup": 0.04},
            },
        }
        self.assertTrue(evaluate_sensitivity_pair(pair, base, good)["passed"])
        self.assertFalse(evaluate_sensitivity_pair(pair, base, bad)["passed"])


class FallbackAndScaleTests(unittest.TestCase):
    def test_material_change_and_event_illustration(self) -> None:
        catalog = scenario_by_id()
        reasons = material_change(catalog["initial_encounter"].encounter, catalog["boss_critical"].encounter)
        self.assertIn("boss_health_delta_ge_10", reasons)
        self.assertTrue(reasons)
        event = event_driven_illustration([s.encounter for s in build_main_scenarios()])
        self.assertGreater(event["pairs"], 0)
        self.assertGreaterEqual(event["material_pairs"], 1)

    def test_scale_formula(self) -> None:
        table = scale_table(1000)
        self.assertEqual(table["per_commander_hour_continuous"]["1"]["input_tokens"], 1000 * 3600)
        self.assertEqual(table["concurrent_commanders_continuous"]["1000"][5]["input_tokens"], 1000 * 5 * 3600 * 1000)
        usd = table["per_commander_hour_continuous"]["1"]["usd_if_published_price"]
        self.assertAlmostEqual(usd, (1000 * 3600) / 1_000_000_000 * 42.0)

    def test_existing_gold_set_still_loads(self) -> None:
        gold = load_gold()
        self.assertGreaterEqual(len(gold["questions"]), 30)
        scored = score_run(
            [gold["questions"][0]],
            {gold["questions"][0]["id"]: {"type": "noul", "noul": 0.99}},
            gold,
        )
        self.assertEqual(scored["total"], 1)

    def test_jev_state_omits_review_notes(self) -> None:
        scenario = scenario_by_id()["contradictory_pressures"]
        state = to_jev_state(scenario.encounter)
        blob = json.dumps(state)
        self.assertNotIn("sanity", blob.lower())
        self.assertIn("note", state)
        self.assertIn("derived", state)
        self.assertTrue(scenario.review)


if __name__ == "__main__":
    unittest.main()
