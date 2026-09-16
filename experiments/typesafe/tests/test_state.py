from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))

from build_state import build_state, estimate_tokens  # noqa: E402
from questions import api_questions, load_gold  # noqa: E402


class StateTests(unittest.TestCase):
    def test_builds_named_files(self) -> None:
        built = build_state()
        self.assertIn("AGENTS.md", built["included_files"])
        self.assertIn("src/neweden_main.cpp", built["included_files"])
        self.assertIn("files", built["state"])
        self.assertGreater(built["state_chars"], 10000)
        self.assertLess(built["state_chars"], 150000)

    def test_payload_fits_docs_char_hint(self) -> None:
        built = build_state()
        gold = load_gold()
        questions = api_questions(gold)
        body = {"state": built["state"], "model": "jev-latest", "questions": questions}
        encoded = json.dumps(body, ensure_ascii=False)
        estimates = estimate_tokens(len(encoded))
        self.assertLess(len(encoded), 150000)
        self.assertLess(estimates["est_chars_div_4"], 32000)

    def test_api_questions_omit_gold_labels(self) -> None:
        payload = json.dumps(api_questions(load_gold()))
        self.assertNotIn('"expected"', payload)
        self.assertNotIn('"source"', payload)


if __name__ == "__main__":
    unittest.main()
