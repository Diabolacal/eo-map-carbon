# Jev tactical-commander feasibility benchmark

Isolated TypeSafe Jev / System One experiment. Originally recorded on
`experiment/typesafe-jev-feral-commander`; that work now lives on `main`.
Not a Carbon renderer milestone, not an EO-Map change, and not a claim
about Fenris Feral AI.

This revision is the **live** run. It replaces the previous dry-run/offline
writeup on `e8d7b09`, which existed only because `TYPESAFE_API_KEY` was
unavailable. No dry-run answers are treated as Jev output below.

## Live run identity (measured)

- Session directory (gitignored raw dumps): `experiments/typesafe/results/commander/20260916T191117Z/`
- Processed summary: `experiments/typesafe/commander/summaries/latest.json`
- Generated UTC: `20260916T191252Z`
- Live calls: **220** sequential `POST /v1/systemone` (budget 250; none skipped)
- Phases: probe (1) → small repeat (4) → 15 scenarios → 8×2 sensitivity → 3×8 stability → 4×40 cadence
- Model requested: `jev-latest`
- Model returned on every completed call: **`jev-1.13.0`**
- `client.models.list()`: `jev-latest`, `jev-preview`
- API failures / rate limits / timeouts: **none**
- Probe: 0.247 s, 3418 input tokens, 481 output tokens, 16 answers with Noul/Choice types

How to read the rest of this file:

- **Measured live:** latency, API `input_tokens` / `output_tokens`, returned model, raw Noul/Choice answers.
- **Derived:** p50/p95, directional sensitivity, composed command after fallback, token-volume scale, published-price USD.
- **Synthetic assumptions:** scripted snapshots, pre-written sanity notes, deterministic thresholds. These are not Frontier combat.

## Scope

This is a synthetic tactical-decision feasibility test. A compact
numeric battlefield snapshot is handed to Jev; Jev returns typed
higher-level judgements; a deterministic subordinate/fallback layer
always has a valid command. Jev does not steer individual NPCs,
fire, collide, or run a combat tick.

Simulated:

- 1 boss/commander, up to 60 subordinates, up to 12 player ships
- optional reinforcement capacity and cooldown
- scripted snapshots and one-variable paired states
- a shallow threshold commander for architectural comparison
- timeout / malformed / low-confidence fallback
- sequential commander-cadence timing from this workstation

Not simulated:

- EVE Frontier NPC behaviour or Fenris's current internal AI
- real Frontier combat numbers, logistics ships, or ship roles
- per-NPC movement, firing, targeting mechanics, or collision
- a closed-loop combat game whose next tick depends on Jev
- server-side MMO integration or dedicated production inference
- an objective 'AI quality' score (there is no reward function)

The operator-provided test context is a workstation in Scotland
calling TypeSafe's public community API. That is not network
geolocation evidence.

## Architecture

```
synthetic observable combat state
        |
        v
Jev typed tactical decisions (Noul + Choice, one System One request)
        |
        v
deterministic composer / fallback  -->  valid command every tick
        |
        v
subordinate behaviour would consume the command (not simulated here)
```

Jev judges; it is not asked to rediscover facts the snapshot already
contains. Exact health, distances, damage totals, alive counts, and
top-damager identities are precomputed in `derived`.

If Jev times out, the API fails, the payload is malformed, or a
Choice is below the ambiguity threshold, the composer keeps the last
valid command when one exists, otherwise the deterministic policy.
The conceptual game tick never waits indefinitely on Jev. Spawn is
overridden to hold when reinforcement capacity is not actually available.

## State representation

One representative compact state (`initial_encounter`):

```json
{
  "note": "Synthetic commander observation snapshot. Every numeric field is an exact measured fact. Judge higher-level tactics from these facts; do not re-count alive units, re-measure distances, or re-sum recent damage.",
  "scenario_id": "initial_encounter",
  "tick_s": 20,
  "boss": {
    "health_pct": 100.0,
    "damage_received_last_10s": 12.0,
    "current_posture": "press_attack",
    "reinforcement_capacity_available": true,
    "reinforcement_cooldown_s": 0.0,
    "id": "boss"
  },
  "swarm": {
    "alive": 60,
    "close_to_boss": 56,
    "medium_range": 4,
    "detached_far": 0,
    "losses_last_10s": 0,
    "losses_last_30s": 0,
    "current_broad_order": "press_attack"
  },
  "players": [
    {
      "id": "P1",
      "alive": true,
      "distance_from_boss_km": 42.0,
      "damage_to_boss_last_10s": 4.0,
      "damage_to_subordinates_last_10s": 3.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 18,
      "isolated": false
    },
    {
      "id": "P2",
      "alive": true,
      "distance_from_boss_km": 48.0,
      "damage_to_boss_last_10s": 2.0,
      "damage_to_subordinates_last_10s": 4.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 16,
      "isolated": false
    },
    {
      "id": "P3",
      "alive": true,
      "distance_from_boss_km": 55.0,
      "damage_to_boss_last_10s": 3.0,
      "damage_to_subordinates_last_10s": 2.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 20,
      "isolated": false
    },
    {
      "id": "P4",
      "alive": true,
      "distance_from_boss_km": 61.0,
      "damage_to_boss_last_10s": 1.0,
      "damage_to_subordinates_last_10s": 2.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 22,
      "isolated": false
    },
    {
      "id": "P5",
      "alive": true,
      "distance_from_boss_km": 67.0,
      "damage_to_boss_last_10s": 0.0,
      "damage_to_subordinates_last_10s": 3.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 19,
      "isolated": false
    },
    {
      "id": "P6",
      "alive": true,
      "distance_from_boss_km": 50.0,
      "damage_to_boss_last_10s": 2.0,
      "damage_to_subordinates_last_10s": 1.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 15,
      "isolated": false
    },
    {
      "id": "P7",
      "alive": true,
      "distance_from_boss_km": 72.0,
      "damage_to_boss_last_10s": 0.0,
      "damage_to_subordinates_last_10s": 2.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 25,
      "isolated": false
    },
    {
      "id": "P8",
      "alive": true,
      "distance_from_boss_km": 58.0,
      "damage_to_boss_last_10s": 0.0,
      "damage_to_subordinates_last_10s": 1.0,
      "current_target": "none",
      "moving_toward_boss": true,
      "nearest_other_player_km": 17,
      "isolated": false
    }
  ],
  "derived": {
    "players_alive": 8,
    "players_dead": 0,
    "alive_player_ids": [
      "P1",
      "P2",
      "P3",
      "P4",
      "P5",
      "P6",
      "P7",
      "P8"
    ],
    "dead_player_ids": [],
    "isolated_alive_player_ids": [],
    "total_boss_damage_last_10s": 12.0,
    "total_subordinate_damage_last_10s": 18.0,
    "nearest_alive_player_km": 42.0,
    "farthest_alive_player_km": 72.0,
    "top_boss_damager": {
      "id": "P1",
      "damage_to_boss_last_10s": 4.0
    },
    "top_subordinate_damager": {
      "id": "P2",
      "damage_to_subordinates_last_10s": 4.0
    },
    "top_boss_damage_share": 0.333,
    "swarm_close_fraction": 0.933,
    "swarm_detached_fraction": 0.0,
    "boss_damage_matches_player_sum": true,
    "range_bands_km": {
      "close_lt": 30.0,
      "far_gt": 80.0,
      "isolated_gte": 60.0
    }
  }
}
```

- Serialized state: 3209 chars; full request payload: 10831 chars (est. tokens chars/4=2707, chars/3=3610).
- Previous coding-task requests in this repo were ~22k–30k API input tokens and ~73k–100k payload chars. This commander snapshot is intended to be much smaller.

- Live API-reported tokens on the same `initial_encounter` shape: probe used **3418 input / 481 output**. Across all 220 calls, median input was **3448** (min 2703 on the short `last_player_fleeing` snapshot, max 3463). The chars/4 estimate of 2707 under-counted the API tokenizer.

## Jev question set

16 questions per inference (12 Noul, 4 Choice). All questions share one state and are sent in a single `POST /v1/systemone`.

### Noul

- `boss_in_critical_danger` — Given the exact measured `boss.health_pct`, `boss.damage_received_last_10s`, remaining swarm, and current player pressure, is the boss in critical tactical danger right now?
- `swarm_overextended` — Are subordinate NPCs overextended relative to the boss, given `swarm.detached_far`, `swarm.close_to_boss`, `derived.swarm_detached_fraction`, and player distances?
- `regroup_warranted` — Should the commander order a regroup, considering `swarm.losses_last_10s`, `swarm.losses_last_30s`, dispersion, and remaining `swarm.alive`?
- `recall_detached_units` — Should detached/far subordinates be recalled toward the boss rather than left on their current assignments?
- `reinforcement_wave_warranted` — Should a reinforcement wave be spawned now? Use remaining swarm strength, recent attrition, current pressure, `boss.reinforcement_capacity_available`, and `boss.reinforcement_cooldown_s`. If capacity is not available or cooldown is still running, the answer is no.
- `focus_fire_warranted` — Is focus fire on a single player warranted, given `derived.top_boss_damager`, `derived.top_boss_damage_share`, and how concentrated the current threat is?
- `pursuit_warranted` — Is pursuing a distant or withdrawing player a sound tactical choice right now? A long chase that abandons the boss fight, or chasing a lone far player with little recent damage, is a poor choice.
- `preserve_swarm_priority` — Should the commander prioritise preserving remaining subordinates over continuing to apply pressure, given `swarm.alive` and recent losses?
- `protect_boss_priority` — Should the commander prioritise protecting itself over applying pressure on players?
- `current_plan_still_sensible` — Is the current `boss.current_posture` / `swarm.current_broad_order` still a sensible overall plan given this snapshot?
- `distant_bait_should_be_ignored` — Is there a distant player whose current role is drawing the swarm away, such that the commander should ignore that player for now? Use `derived.isolated_alive_player_ids`, far distances, and compare their recent boss damage to nearer players.
- `pressure_can_be_maintained` — Can the swarm currently maintain offensive pressure without undue risk to the boss or the remaining subordinates?

### Choice

`tactical_posture`:
- `press_attack` — Keep applying offensive pressure on hostiles. Appropriate when the boss is not in immediate danger and the swarm can still spend itself usefully.
- `hold_close` — Hold the swarm near the boss and stop chasing. Appropriate when pressure exists but overextending or exposing the boss would be worse than hunting.
- `regroup` — Pull units back together and restore cohesion before committing again. Appropriate after dispersion or a sharp loss pulse.
- `desperate_defence` — The commander is in immediate survival danger. Screen the boss and survive rather than hunt.

`formation_intent`:
- `tight_screen` — Compact the remaining NPCs immediately around the boss.
- `balanced` — Keep a close screen and still allow some local pressure on nearby hostiles.
- `wide_pressure` — Spread subordinates to pressure multiple players. Poor if the swarm is already scattered or the boss is exposed.
- `recall_detached` — The formation priority is pulling far units back toward the boss.

`reinforcement_action`:
- `hold` — Do not spawn a reinforcement wave now. Always choose this when capacity is unavailable.
- `spawn_now` — Spawn a reinforcement wave immediately. Only appropriate if capacity is available and the current depletion/pressure warrants spending it.

`primary_target`:
- one option per currently alive synthetic player id
- plus `none`

Dead players remain in state with `alive: false` so recent deaths are
visible, but they are not valid Choice options.

## Scenario results

**Measured:** 15/15 snapshots returned a full 16-answer payload from `jev-1.13.0`.

Two layers must not be collapsed:

1. **Raw Jev** — the Noul probabilities and Choice labels in the API body.
2. **Composed command** — `controller.compose_command`, which substitutes the deterministic policy when Choice confidence is below 0.40, when a Noul sits in (0.35, 0.65) and would gate spawn, or when spawn is requested without capacity.

The table's "composed" columns are what a game tick would actually consume. Review flags are against **raw** Jev, because that is the model behaviour.

| Scenario | Composed posture | Raw posture (conf) | Composed target | Raw target | Composed reinf | Raw reinf (conf) | Raw review flags |
| --- | --- | --- | --- | --- | --- | --- | ---: |
| `initial_encounter` | press_attack | press_attack (0.90) | P1 | P1 | **hold** | spawn_now (0.95) | 1 |
| `normal_engagement` | press_attack | press_attack (0.87) | P3 | P3 | hold | hold (1.00) | 0 |
| `boss_focused` | press_attack | press_attack (0.34, ambiguous) | P3 | P3 | hold | hold (1.00) | 0 |
| `swarm_dispersed` | regroup | regroup (0.43) | P1 | P1 | hold | hold (1.00) | 0 |
| `heavy_attrition` | regroup | press_attack (0.34, ambiguous) | P1 | P1 | hold | hold (1.00) | 2 |
| `boss_critical` | hold_close | hold_close (0.42) | P1 | P1 | hold | hold (1.00) | 0 |
| `reinforce_under_pressure` | hold_close | hold_close (0.40) | P1 | P1 | spawn_now | spawn_now (0.99) | 0 |
| `reinforce_while_stable` | press_attack | press_attack (0.95) | P1 | P1 | **hold** | spawn_now (0.97) | 2 |
| `distant_bait` | press_attack | press_attack (0.74) | P1 | P1 | hold | hold (1.00) | 0 |
| `few_players_boss_damaged` | desperate_defence | hold_close (0.37, ambiguous) | P1 | P1 | hold | hold (1.00) | 0 |
| `no_pressure` | press_attack | press_attack (0.92) | P1 | P1 | **spawn_now** | spawn_now (0.92) | 1 |
| `contradictory_pressures` | regroup | regroup (0.58) | P1 | P1 | spawn_now | spawn_now (0.97) | 0 |
| `healthy_boss_thin_swarm` | press_attack | press_attack (0.55) | P1 | P1 | hold | hold (1.00) | 0 |
| `collapsing_no_reinforce` | regroup | hold_close (0.30, ambiguous) | P1 | P1 | hold | hold (1.00) | 0 |
| `last_player_fleeing` | press_attack | press_attack (0.48) | P1 | P1 | hold | hold (1.00) | 0 |

`formation_intent` was below the 0.40 confidence floor on 10/15 scenarios, so the composer used the deterministic formation there. That Choice is the noisiest of the four.

### Sensible raw calls (not cherry-picked; these just worked)

- **`boss_focused`:** `focus_fire_warranted` 0.83, `primary_target` P3 at 0.97. The one player who accounted for most recent boss damage is named. Capacity is down, so `reinforcement_action` is hold at confidence 1.0.
- **`distant_bait`:** `primary_target` P1 (0.97), not P12. `distant_bait_should_be_ignored` 0.65, `pursuit_warranted` 0.14. This is the case the experiment was most worried about, and the raw answers go the right way.
- **`contradictory_pressures`:** `regroup` (0.58), `recall_detached` (0.72), `spawn_now` (0.97), target P1 (0.99). `boss_in_critical_danger` 0.69, `swarm_overextended` 0.78, `pursuit_warranted` 0.13. Combined conditions land on a coherent bundle rather than a single threshold.
- **`reinforce_under_pressure`:** `reinforcement_wave_warranted` 0.74 and `spawn_now` 0.99, with `regroup_warranted` 0.72. This is the spawn-now case the questions were written for.
- **`collapsing_no_reinforce`:** same pressure snapshot with capacity down → `reinforcement_wave_warranted` 0.08 and `hold` at 1.0. Capacity is treated as a hard fact.
- **`swarm_dispersed`:** `swarm_overextended` 0.77, `regroup` 0.43, `recall_detached` 0.71. It does not chase P8/P9.

### Questionable or internally strained raw calls

These are the ones I would actually put on a slide as "look at this, not just the happy path."

1. **Spawn whenever capacity is free.** Whenever `reinforcement_capacity_available` is true, raw `spawn_now` is 0.92–0.99 except when the Noul is used as a soft brake:
   - `initial_encounter`: spawn_now 0.95 despite 60/60 NPCs and 12 recent boss damage. Noul 0.39 is in the ambiguous band, so the composer **holds**. Raw Jev still wants the wave.
   - `reinforce_while_stable`: spawn_now 0.97, Noul 0.58 (also ambiguous) → composer holds. Pre-written note wanted Noul below 0.5.
   - `no_pressure`: spawn_now 0.92, Noul 0.30 (outside the ambiguous band) → **composed command is spawn_now** into a fight with no pressure. This is the leak that actually reaches a game tick.
2. **Almost never chooses `none` as primary target.** Raw `primary_target` is some player id on all 15 snapshots, including `initial_encounter` (P1, 4 recent boss damage), `no_pressure` (P1, 3 damage, 96 km), and `last_player_fleeing` (P1 at 160 km, 8 damage, `pursuit_warranted` 0.14). The Noul says "do not chase"; the Choice still names the only/nearest id. The deterministic policy chose `none` on 8/15 of the same snapshots.
3. **`heavy_attrition` raw posture is `press_attack` at confidence 0.34** while `regroup_warranted` is 0.77. The composer discards the low-confidence Choice and uses deterministic `regroup`. Raw Jev is internally split: the Noul wants regroup, the Choice (weakly) wants to press. `preserve_swarm_priority` 0.49 sits 0.01 under the 0.5 sanity note; that particular flag is too sharp to treat as a real miss.
4. **`healthy_boss_thin_swarm`:** `regroup_warranted` 0.78 and `preserve_swarm_priority` 0.61, but `tactical_posture` is still `press_attack` at 0.55. Eleven NPCs left, boss at 88%. The Nouls and the Choice disagree.
5. **`boss_critical` (14% health):** `boss_in_critical_danger` 0.86, but the Choice is `hold_close` (0.42) rather than `desperate_defence`. Not a sanity-note failure (`hold_close` was an allowed defensive label), but the mass on `desperate_defence` stays small even here. The 80%→20% sensitivity pair shows the same pattern (see below).
6. **`last_player_fleeing`:** `pursuit_warranted` 0.14 is the right Noul, `press_attack` 0.48 plus `primary_target` P1 0.52 is a weak chase of the only remaining ship at 160 km. The composer keeps both because confidence is above 0.40.

Review-flag count is 6. Two of those are the 0.49 preserve Noul and the raw `press_attack` on `heavy_attrition` (already overridden by fallback). The spawn-now bias is the systematic issue; the missing `none` target is the second.

## Sensitivity results

**Measured:** 8 pairs, 16 additional live calls. Directional test: the named probability must not move the wrong way. Exact values are not required.

**5 / 8 pairs passed every listed judgement.** The three failures are tiny deltas. The more important observation is a pair that *passed* the directional test but did not change the Choice.

| Pair | Intended change | All listed ok? | What actually moved |
| --- | --- | --- | --- |
| `boss_health_80_vs_20` | health 80 → 20 | yes | `boss_in_critical_danger` 0.19 → **0.83**. `protect_boss_priority` 0.36 → 0.52. Choice stayed **`press_attack`** (0.90 → 0.83). `desperate_defence` only 0.01 → 0.04. |
| `recent_boss_damage_low_vs_severe` | recent boss damage 40 → 520 | **no** | Danger 0.14 → 0.17, focus 0.58 → 0.62 (right way, small). `current_plan_still_sensible` 0.64 → 0.68 (**+0.04**, wrong way). |
| `sub_count_60_vs_15` | alive 60 → 15 | yes | `regroup_warranted` 0.36 → 0.70, `preserve_swarm_priority` 0.19 → 0.45, `pressure_can_be_maintained` 0.56 → 0.42. |
| `detached_5_vs_35` | detached 5 → 35 | **no** | `swarm_overextended` 0.26 → **0.82**. Formation `recall_detached` 0.37 → **0.89**. Noul `recall_detached_units` 0.62 → 0.61 (**−0.01**). |
| `losses_2_vs_20` | losses10 2 → 20 | yes | `regroup_warranted` 0.34 → 0.72. Preserve 0.18 → 0.34. Plan-sensible 0.68 → 0.60. |
| `reinforce_unavailable_vs_available` | capacity off → on, under pressure | yes | Warranted 0.07 → 0.66. `spawn_now` probability **0.00 → 0.99**. Cleanest pair in the set. |
| `p3_boss_damage_50_vs_800` | P3 boss damage 50 → 800 | yes | P3 already 0.96 at 50 damage, 0.98 at 800. Focus 0.68 → 0.85. Ceiling effect: P3 was already the target. |
| `hostile_distance_20_vs_150` | all hostiles 20 km → 150 km | **no** | Pursuit 0.13 → 0.12. Ignore-distant 0.07 → 0.09 (almost no bait signal even at 150 km, because *every* player moved out together). Danger 0.22 → 0.23 (**+0.01**). |

Failed listed judgements, without hiding them:

- `current_plan_still_sensible` rose 0.04 after a damage spike. Wrong direction, small, and the posted plan on that control snapshot was already `press_attack`.
- `recall_detached_units` fell 0.01 while the matching Choice probability jumped 0.52. The Noul and the Choice disagree in magnitude; the Choice is the one that reacted.
- `boss_in_critical_danger` rose 0.01 when hostiles moved from 20 km to 150 km. Noise relative to a 0.01 step; not evidence that range is ignored, given pursuit also did not rise.

The pair I would actually discuss with an NPC engineer is **health 80 → 20**: the danger Noul moves like a calibrated detector, the posture Choice does not leave `press_attack`. Fallback would not save this, because `press_attack` confidence stays high (0.83).

## Stability results

**Measured:** three snapshots, 8 sequential identical calls each (24 calls). Plus the probe+4 repeats on `initial_encounter` earlier in the session, which agreed with the later block.

| Snapshot | Choice agreement | Material oscillation | Max Noul range | Note |
| --- | ---: | --- | ---: | --- |
| `initial_encounter` | 4/4 = 100% | no | 0.08 (`reinforcement_wave_warranted` 0.31–0.39) | Raw spawn_now every time; composer holds. |
| `contradictory_pressures` | 4/4 = 100% | no | 0.11 (`distant_bait_should_be_ignored` 0.44–0.55) | That one Noul **crosses 0.5** on identical input. Choices do not flip. |
| `boss_critical` | 4/4 = 100% | no | 0.07 (`protect_boss_priority` 0.59–0.66) | Always `hold_close` + P1 + hold. |

No incompatible posture/formation switches, no hold/spawn flips, no none-flips. Repeated decisions are stable at the Choice layer. The 0.44–0.55 Noul band on `distant_bait_should_be_ignored` is the only identical-input threshold crossing; a 0.5 hard gate on that Noul would chatter, a Choice would not.

## Latency results

**Measured** sequential calls from this workstation (operator-provided: Scotland) to TypeSafe's public API. A deadline miss is end-to-end latency greater than the requested interval. 40 samples per target rate.

| Target Hz | Interval s | N | p50 s | p95 s | max s | Achieved Hz | Deadline misses | Miss % | Rate limits | Timeouts | API errors |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1.000 | 40 | 0.242 | 0.299 | 0.646 | 1.00 | 0 | 0.0% | 0 | 0 | 0 |
| 2 | 0.500 | 40 | 0.240 | 0.270 | 0.291 | 2.00 | 0 | 0.0% | 0 | 0 | 0 |
| 5 | 0.200 | 40 | 0.240 | 0.264 | 0.716 | 3.92 | 40 | 100.0% | 0 | 0 | 0 |
| 10 | 0.100 | 40 | 0.238 | 0.276 | 0.408 | 4.05 | 40 | 100.0% | 0 | 0 | 0 |

p50 is ~240 ms at every target rate. That is the sequential ceiling from this machine on this payload: about **4 Hz** if you issue the next call immediately. 1 Hz and 2 Hz keep p95 inside the interval. 5 Hz and 10 Hz miss every deadline because 240 ms > 200 ms and > 100 ms. One 5 Hz sample took 0.716 s; that is why max is not just "slightly over."

This is not evidence about a colocated production endpoint.

## Context / token results

**Measured** API usage on 220 live calls:

| | min | median | max |
| --- | ---: | ---: | ---: |
| input_tokens | 2703 | **3448** | 3463 |
| output_tokens | 424 | **479** | 483 |

- State JSON: 3209 chars on `initial_encounter`; full request payload 10831 chars.
- 16 tactical questions (12 Noul, 4 Choice).
- Previous coding experiments in this repo: 30,181 input tokens (repo-comprehension) and 22,349 / 25,339 (Jita-Amarr preflight/postflight). This commander snapshot is roughly **9× smaller** than the 30k-token coding test on API-reported input tokens (3448 vs 30181).

## Scale

**Derived** from the measured median **3448 input tokens/inference**.

`tokens_per_hour = 3448 * Hz * 3600 * commanders`

USD uses TypeSafe's public list price retrieved 2026-09-16: **$42 per billion input tokens**, output free. Sources: [typesafe.ai](https://typesafe.ai/), [15 Sep 2026 launch post](https://typesafe.ai/blog/introducing-system-one-models-and-jev). That post says they cannot prove the price is not subsidized. These are list-price arithmetic, not invoices, and they assume the public API.

### Continuous commander-hour (one commander)

| Cadence | Inferences / hour | Input tokens / hour | USD / hour at published list price |
| --- | ---: | ---: | ---: |
| 1 Hz | 3,600 | 12,412,800 | $0.52 |
| 2 Hz | 7,200 | 24,825,600 | $1.04 |
| 5 Hz | 18,000 | 62,064,000 | $2.61 |
| 10 Hz | 36,000 | 124,128,000 | $5.21 |

### Concurrent commanders, continuous

| Commanders | 1 Hz tokens/h | 5 Hz tokens/h | 10 Hz tokens/h | 5 Hz USD/h |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 12.4M | 62.1M | 124M | $2.61 |
| 10 | 124M | 621M | 1.24B | $26.07 |
| 100 | 1.24B | 6.21B | 12.4B | $261 |
| 1,000 | 12.4B | 62.1B | 124B | $2,607 |

1,000 continuously active commanders at 5 Hz is 18 million inferences/hour and ~62 billion input tokens/hour. At the published list price that is about $2.6k/hour. The same 1,000 commanders at 1 Hz is about $521/hour.

### Event-driven comparison (illustration, not a production cadence)

| Events / commander-hour | Tokens / hour | vs 5 Hz | USD / hour |
| ---: | ---: | ---: | ---: |
| 30 | 103,440 | 600× fewer | $0.004 |
| 60 | 206,880 | 300× fewer | $0.009 |
| 120 | 413,760 | 150× fewer | $0.017 |

15 material changes in a 10-minute fight is 90 inferences per commander-hour versus 18,000 at 5 Hz. Walking this corpus as if it were a timeline, 14/14 adjacent pairs trip the material-change detector; that is by construction (15 distinct situations), not an observed fight rate.

## Deterministic comparison

No winner. The simple policy is a handful of thresholds:

- health ≤20, or ≤35 with ≥400 recent boss damage → `desperate_defence` / `tight_screen`
- ≥12 losses in 10 s or ≥22 in 30 s → `regroup`
- those losses (or a thin swarm under pressure) plus capacity → `spawn_now`
- otherwise press, with a far-and-quiet hold
- primary target is the top recent boss-damager above a damage floor, unless they look like far bait

Where that is enough, it is trivial and predictable: `collapsing_no_reinforce` never spawns; `boss_focused` names P3; `distant_bait` does not name P12.

Where combinations nest: `healthy_boss_thin_swarm` has 11 NPCs but 108 recent subordinate damage, just under the 120 floor, so the policy still `press_attack`. `contradictory_pressures` is 36% health with 720 boss damage; the critical-danger rule is ≤20, or ≤35 with ≥400, so 36% is not "severe" even while P1 is deleting the boss. Each extra exception is another branch.

Composed Jev vs that policy on the 15 live snapshots: posture 11/15, formation 13/15, reinforce 14/15, target 7/15. The target disagreement is almost entirely "Jev names P1, the threshold policy says `none`." Formation agreement is high because low-confidence Jev formation is replaced by the policy. Reinforce agreement is high because ambiguous spawn Nouls are held — except `no_pressure`, where the spawn leaked.

Jev's useful contribution on this corpus is the parallel Noul bundle on combined states (`contradictory_pressures`, `swarm_dispersed`, `distant_bait`) plus a calibrated danger Noul. Its least useful contribution is treating "capacity available" as "spawn now" and refusing `none` as a target.

## Failure handling

Observed in this live run, not just described:

- **Timeout / API failure / rate limit:** none of 220 calls.
- **Choice confidence < 0.40:** used on `tactical_posture` and/or `formation_intent` in 10 scenarios (`boss_focused`, `heavy_attrition`, `few_players_boss_damaged`, `collapsing_no_reinforce`, and most formations). The tick still got a valid command.
- **Spawn + ambiguous `reinforcement_wave_warranted`:** held `initial_encounter` and `reinforce_while_stable`.
- **Spawn without capacity:** never requested; when capacity was false, raw Choice was `hold` at 1.0.
- **`no_pressure` spawn** was *not* caught, because the Noul (0.30) was outside the ambiguous band.

The composer did work. It is not a complete guard against a high-confidence bad Choice.

## Limitations

- Synthetic battlefield states, not a recorded Frontier fight.
- No real EVE Frontier NPC behaviour.
- No actual Frontier combat simulation.
- No individual NPC movement or fire control.
- No server-side MMO integration.
- No conclusion about Fenris's current internal NPC architecture.
- Community/public API behaviour may not represent dedicated production infrastructure.
- Tactical "quality" remains partly subjective without a real simulation and an objective reward function.
- Sequential workstation calls from Scotland are not a multi-region production latency test.
- Sensitivity tests constrain *direction*, not calibration. A 0.01 miss is reported; it should not be over-read.
- Pre-written sanity notes are not a gold set. Two flags are threshold-sharp (`preserve_swarm_priority` 0.49).

## Bottom line

- **Cadence:** from this test machine, **1 Hz and 2 Hz are sustainable** (p95 0.30 s and 0.27 s). **5 Hz and 10 Hz are not** sequentially: p50 is already ~240 ms, so every sample misses a 200 ms / 100 ms deadline. Unpaced sequential throughput is about **4 Hz**. That is interesting for an async commander loop at 1–2 Hz, not for a 10 Hz tick.
- **Stability:** Choice labels were identical across 8 repeats on all three snapshots. No tactical oscillation. One Noul (`distant_bait_should_be_ignored` on the contradictory snapshot) crossed 0.5 on identical input (0.44–0.55).
- **Sensitivity:** 5/8 pairs passed every listed directional check. The three failures are ±0.01 to +0.04. The Nouls for danger, regroup, overextension, and spawn-capacity move in the obvious direction, often by a lot. The posture Choice did **not** leave `press_attack` when boss health went 80 → 20, even though the danger Noul went 0.19 → 0.83.
- **Questionable decisions:** systematic raw `spawn_now` whenever capacity is available, including `no_pressure` where the composed command also spawned; systematic refusal to choose `none` as a target; raw `press_attack` against a 0.77 regroup Noul on heavy attrition (fallback caught that one). `distant_bait` targeting was not one of the failures.
- **Context:** median **3448 input tokens**, **479 output**, 16 questions, ~11k payload chars. About an order of magnitude below the 20k–30k-token coding tests in this repo.
- **Token volume at 3448/inference:** one commander at 1 Hz is 12.4M tokens/hour (~$0.52/h at the 2026-09-16 published $42/B list price). 10 / 100 / 1,000 commanders at 5 Hz are 0.62B / 6.2B / 62B tokens/hour (~$26 / $261 / $2,607 per hour). Event-driven calling changes that by two orders of magnitude. List price, public API, not a production quote.
- **Show an NPC/AI engineer?** Yes, as a **measured feasibility sketch**, not as a design. The useful evidence is: small structured state, 16 parallel tactical questions, ~240 ms public-API latency, stable Choices, Nouls that move with health/losses/capacity/dispersion, and a fallback that keeps the sim running. The useful caveats on the same slide are spawn-when-idle, never-`none` targeting, and danger-Noul versus press-attack Choice. That is enough to ask whether a Fenris commander loop wants this shape of judgement, not enough to claim it belongs in one.

What remains unanswered: colocated/dedicated-endpoint latency; behaviour on a real encounter stream rather than 15 frozen snapshots; whether spawn/target biases survive better question wording; how often a production sim would actually call (1 Hz vs event-driven); and anything about Fenris's current Feral implementation.
