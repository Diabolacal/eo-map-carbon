# TypeSafe Jev repository-comprehension experiment

Isolated benchmark of TypeSafe's Jev System One model against a Milestone 1C
snapshot of this repository. It is **not** a Carbon renderer milestone and is
not wired into any TrinityAL executable or CMake target.

Jev is a decision model: one shared `state`, many independent Choice / Noul
questions in a single `POST /v1/systemone` request. This harness does not ask
Jev to write code, explain code, or design features.

## Layout

| Path | Role |
| --- | --- |
| `gold_set.json` | 37 questions with expected answers and source rationales |
| `state_files.json` | Which repo files become the shared state |
| `run_benchmark.py` | Measure payload, send one batched request per run, score, write report |
| `score.py` | Pure scoring (no SDK) |
| `skill/SKILL.md` | Project-local copy of the TypeSafe agent skill |
| `REPORT.md` | Aggregate results from the live runs |
| `results/` | Gitignored raw API JSON (no Authorization headers) |

## Run

Requires Python 3.10+ and `TYPESAFE_API_KEY` in the environment. The key is
never printed, logged, or written to disk by this harness.

```powershell
cd experiments\typesafe
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe -m unittest discover -s tests -t .
.\.venv\Scripts\python.exe run_benchmark.py --measure-only
.\.venv\Scripts\python.exe run_benchmark.py --runs 5
```

Or from the repo root:

```powershell
.\experiments\typesafe\run.ps1
```

`--measure-only` reports payload size against TypeSafe's documented ~32k token /
~150k English-character request budget before any live call.

`ask.py` sends one fan-out (preflight/postflight) over named files. The
Jita-Amarr routing development-task writeup is `ROUTING_EXPERIMENT.md`.

## Scoring

- Noul: `noul >= 0.5` is yes. Certainty is `abs(noul - 0.5) * 2` (Noul has no
  API `confidence` field).
- Choice: the selected option must match the gold label. `confidence` comes
  from the API.
- Questions are not silently retuned after seeing Jev's answers. If a question
  is later judged defective, the original score is kept and any revised score
  is labelled separately.
