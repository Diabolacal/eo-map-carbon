# Jev repository-comprehension benchmark

Isolated TypeSafe Jev / System One experiment against the Milestone 1C
`eo-map-carbon` snapshot. This is not a Carbon renderer milestone.

Gold-set questions were written from the repository files **before** any Jev call.
Questions were not silently retuned after seeing answers.

## Setup

- Model requested: `jev-latest` (SDK default is also `jev-latest`)
- Models reported in completed runs: `jev-1.13.0`
- `client.models.list()`: `jev-latest`, `jev-preview`
- Questions: 37 (25 Noul, 12 Choice)
- Runs attempted: 5
- Runs completed: 5
- Shared state files: `AGENTS.md`, `docs/status.md`, `src/neweden_main.cpp`, `CMakeLists.txt`, `src/orbit_camera.h`, `src/new_eden_catalog.h`, `src/new_eden_gates.h`, `src/new_eden_catalog.cpp`, `src/new_eden_gates.cpp`, `README.md`
- State JSON chars: 87097
- Full request payload: 99799 chars (est. tokens chars/4=24949, chars/3=33266; docs budget ~32k tokens / ~150k English chars)
- All questions for a run were sent in **one** System One request.

## Headline results

- Overall accuracy: **100.0%**
  (185 / 185 scored answers)
- Run 1: 100.0% (37 / 37)
- Run 2: 100.0% (37 / 37)
- Run 3: 100.0% (37 / 37)
- Run 4: 100.0% (37 / 37)
- Run 5: 100.0% (37 / 37)
- API latency seconds: min=0.414, median=0.434, max=0.758
- Input tokens: min=30181, median=30181, max=30181
- Output tokens: min=1045, median=1045, max=1045
- Repeated answers stable: yes (37 / 37 questions identical across completed runs)

## Research questions

1. **Can Jev accurately understand technical facts distributed across this small real codebase?**
   Yes on this gold set: 100.0% (185 / 185). The 37 questions are explicit binary/closed-set facts that appear in `AGENTS.md`, `docs/status.md`, and the New Eden host. That is a real-repo comprehension check, not a test of catching subtle code/doc contradictions.
2. **Does its parallel-question architecture actually give very low latency for 30–40 judgments?**
   Yes. One request of 37 questions returned in 0.414–0.758s (median 0.434s). Run 1 was the slowest (0.758s); runs 2–5 were 0.414–0.436s. Input tokens were 30,181 against a documented ~32k request budget.
3. **Are probability/confidence values useful when it is uncertain?**
   This gold set never made the model uncertain. Every Choice came back at confidence 1.0 with all mass on the gold option. Noul yes-facts were 0.98–0.99; Noul no-facts were 0.04–0.11. There were no low-confidence correct answers and no high-confidence incorrect answers, so this run does not demonstrate useful “I don’t know” behaviour.
4. **Are repeated runs stable?**
   Yes. All 37 predicted labels were identical across five completed runs. Token counts were also identical.
5. **Is this model interesting enough to investigate for future classification/verification workflows?**
   Yes, as a fast batched checker for documented constraints (TrinityAL vs raw D3D, non-goals, primitive types, smoke vs human evidence). It is not yet shown to be useful for ambiguous review comments or implicit bugs. A follow-up would need questions whose gold answer is *not* restated in `AGENTS.md`.

## Observed probabilities (identical on all 5 runs)

Noul no-facts with the most residual yes-probability (still correctly below 0.5):

- `createdevice_creates_depth`: noul=0.11
- `wspace_included`: noul=0.10
- `second_vcpkg_manifest`: noul=0.10
- `dx11_point_size_controllable`: noul=0.09

All 12 Choice answers had `confidence=1.0`.

## Incorrect answers

None.

## Low-confidence correct

None.

## High-confidence incorrect

None.

## Run-to-run disagreements

None. Every completed run produced the same predicted label per question.

## API errors / rate limits

None.

## Defective questions

None identified. Original score is the only score.

## Gold set

Each question has an expected answer and a source rationale in `gold_set.json`.
Noul yes/no uses threshold 0.5 on the returned probability. Choice uses the selected option.
Noul has no API confidence field; certainty is `abs(noul - 0.5) * 2`.

