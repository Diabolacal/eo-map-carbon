from __future__ import annotations

import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))

from questions import load_gold  # noqa: E402
from score import aggregate_runs, noul_certainty, revised_accuracy, score_run  # noqa: E402


class GoldSetTests(unittest.TestCase):
    def test_gold_set_size_and_labels(self) -> None:
        gold = load_gold()
        questions = gold["questions"]
        self.assertGreaterEqual(len(questions), 30)
        self.assertLessEqual(len(questions), 40)
        for item in questions:
            self.assertIn(item["type"], {"noul", "choice"})
            self.assertTrue(item["instructions"])
            self.assertTrue(item["source"])
            self.assertIn("expected", item)


class ScoreTests(unittest.TestCase):
    def test_noul_true_and_false(self) -> None:
        gold = load_gold()
        question = next(q for q in gold["questions"] if q["id"] == "uses_trinityal")
        hit = score_run([question], {"uses_trinityal": {"type": "noul", "noul": 0.97}}, gold)
        self.assertTrue(hit["answers"][0]["correct"])
        miss = score_run([question], {"uses_trinityal": {"type": "noul", "noul": 0.05}}, gold)
        self.assertFalse(miss["answers"][0]["correct"])
        self.assertTrue(miss["answers"][0]["high_confidence_incorrect"])

    def test_choice_and_low_confidence_correct(self) -> None:
        gold = load_gold()
        question = next(q for q in gold["questions"] if q["id"] == "renderer_stack")
        scored = score_run(
            [question],
            {
                "renderer_stack": {
                    "type": "choice",
                    "choice": "trinityal_dx11",
                    "probabilities": {
                        "trinityal_dx11": 0.4,
                        "raw_direct3d": 0.3,
                        "opengl_or_sdl": 0.2,
                        "threejs_or_webview": 0.1,
                    },
                    "confidence": 0.2,
                }
            },
            gold,
        )
        self.assertTrue(scored["answers"][0]["correct"])
        self.assertTrue(scored["answers"][0]["low_confidence_correct"])

    def test_noul_certainty(self) -> None:
        self.assertAlmostEqual(noul_certainty(0.5), 0.0)
        self.assertAlmostEqual(noul_certainty(1.0), 1.0)
        self.assertAlmostEqual(noul_certainty(0.0), 1.0)

    def test_aggregate_disagreement_and_revised_score(self) -> None:
        gold = {
            "noul_yes_threshold": 0.5,
            "choice_low_confidence": 0.5,
            "choice_high_confidence": 0.8,
            "noul_low_certainty": 0.6,
            "noul_high_certainty": 0.8,
            "questions": [
                {
                    "id": "a",
                    "type": "noul",
                    "expected": True,
                    "source": "test",
                    "instructions": "A?",
                }
            ],
        }
        run_a = {
            "run": 1,
            "ok": True,
            "latency_s": 0.2,
            "model": "jev-latest",
            "input_tokens": 10,
            "output_tokens": 2,
            "score": score_run(gold["questions"], {"a": {"noul": 0.9}}, gold),
        }
        run_b = {
            "run": 2,
            "ok": True,
            "latency_s": 0.4,
            "model": "jev-latest",
            "input_tokens": 10,
            "output_tokens": 2,
            "score": score_run(gold["questions"], {"a": {"noul": 0.1}}, gold),
        }
        agg = aggregate_runs([run_a, run_b], gold)
        self.assertEqual(agg["runs_completed"], 2)
        self.assertEqual(len(agg["disagreements"]), 1)
        self.assertFalse(agg["repeated_answers_stable"])
        self.assertAlmostEqual(agg["latency_s"]["median"], 0.3)
        revised = revised_accuracy([run_a, run_b], {"a"})
        self.assertEqual(revised["overall_total"], 0)


if __name__ == "__main__":
    unittest.main()
