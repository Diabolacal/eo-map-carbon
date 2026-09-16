# Jev tactical-commander feasibility benchmark

Isolated TypeSafe Jev / System One experiment on branch
`experiment/typesafe-jev-feral-commander`. Not a Carbon renderer
milestone, not an EO-Map change, and not a claim about Fenris Feral AI.

**Live Jev calls were not made.** `TYPESAFE_API_KEY` was not exposed
in the environment, so this file records the harness, the synthetic
corpus, unit-test behaviour, and dry-run structure. It does **not**
contain fabricated latencies, tokens, or tactical answers.

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

No live scenario answers. The corpus itself:

- `initial_encounter` — Initial encounter: Boss healthy, full swarm, players approaching, little recent damage.
- `normal_engagement` — Normal engagement: Moderate losses, several players damaging subordinates, boss still healthy.
- `boss_focused` — Boss being focused: P3 accounts for most recent boss damage; boss health is falling.
- `swarm_dispersed` — Swarm becoming dispersed: Substantial fraction of NPCs far from the boss; two distant players drawing them.
- `heavy_attrition` — Heavy recent attrition: Many NPC losses over a short interval; players are chewing through subordinates.
- `boss_critical` — Boss critically damaged: Very low boss health and a reduced swarm; remaining players are on the boss.
- `reinforce_under_pressure` — Reinforcements available under pressure: Capacity is up, cooldown is zero, swarm is depleted, and players are still applying damage.
- `reinforce_while_stable` — Reinforcements available while stable: Capacity is up, but the encounter is currently quiet and the swarm is nearly full.
- `distant_bait` — Distant bait while others stay on the boss: P12 is 155 km away drawing subordinates; P1/P2 remain on the boss.
- `few_players_boss_damaged` — Few players remain but the boss is badly damaged: Only two hostiles left, both on the boss, commander already at 18%.
- `no_pressure` — No meaningful hostile pressure: Boss healthy, full swarm, players far and dealing almost no damage.
- `contradictory_pressures` — Contradictory pressures: Dangerous boss attacker, dispersed swarm, heavy subordinate losses, reinforcement available.
- `healthy_boss_thin_swarm` — Healthy boss, thin swarm: Boss is fine; subordinates have been eroded to 11. Preserve-vs-press trade-off.
- `collapsing_no_reinforce` — Collapsing without reinforcement capacity: Same pressure as the spawn-now case, but capacity is down and cooldown is long.
- `last_player_fleeing` — Last player fleeing far away: One surviving player at 160 km, low damage, moving away. Pursuit is the trap.

## Sensitivity results

Each pair changes one intended variable. Accounting fields that must
move with it (range buckets summing to `alive`, boss damage matching
the player sum, `losses_last_30s >= losses_last_10s`) are noted per pair.
The test is directional: probabilities should not move the wrong way.
It does not require an exact probability.

No live paired calls. Pair definitions:

- `boss_health_80_vs_20` — Boss health 80% -> 20%. Only boss.health_pct changes. Damage, swarm, and players stay fixed.
- `recent_boss_damage_low_vs_severe` — Recent boss damage low -> severe. The intended variable is recent boss-damage intensity. Player shares keep their proportions; boss.damage_received_last_10s is rewritten to the player sum so the snapshot stays consistent.
- `sub_count_60_vs_15` — Subordinate count 60 -> 15. alive must equal the range-bucket sum, so close_to_boss is the accounting remainder. medium_range and detached_far stay 8 and 5.
- `detached_5_vs_35` — Detached subordinates 5 -> 35. alive stays 45 and medium_range stays 8. close_to_boss falls because buckets must sum to alive.
- `losses_2_vs_20` — Recent subordinate losses 2 -> 20. losses_last_30s cannot be smaller than losses_last_10s, so both rise together. alive is unchanged; this is a recent-loss pulse, not a new headcount.
- `reinforce_unavailable_vs_available` — Reinforcement unavailable -> available under pressure. Both sides use the same pressured mid-fight snapshot. Only capacity and cooldown change.
- `p3_boss_damage_50_vs_800` — P3 recent boss damage 50 -> 800. Only P3's recent boss damage changes. boss.damage_received_last_10s and derived top-damager fields are recomputed from the player list so the snapshot stays consistent.
- `hostile_distance_20_vs_150` — Hostile distance 20 km -> 150 km. Every alive player is placed at 20 km or 150 km. Damage stays fixed so the only tactical change is range (and toward/away on the far side, because 150 km hostiles are not approaching).

## Stability results

Identical snapshots, repeated sequentially. A commander that flips
between incompatible postures on unchanged state is a negative result.

No live repeatability samples.

## Latency results

Sequential calls from this workstation (operator-provided: Scotland)
to TypeSafe's public API. Target Hz is the commander update rate,
not a requirement that Jev meet every rate. A deadline miss is a
call whose end-to-end latency exceeded the requested interval.

No live cadence samples.

## Context / token results

No API-reported tokens (no live calls). Payload estimate only:
- payload_chars=10831, state_chars=3209, est chars/4=2707

## Scale

Token volume is computed from measured (or, if live data is absent,
left blank) input tokens per commander inference:

`tokens_per_hour = input_tokens_per_inference * Hz * 3600 * commanders`

Monetary estimates use TypeSafe's public price retrieved 2026-09-16: **$42 per billion input tokens**, output free. Sources: https://typesafe.ai/, https://typesafe.ai/blog/introducing-system-one-models-and-jev. TypeSafe's own launch post notes they cannot prove the price is not subsidized. Community/public API behaviour may not represent dedicated production infrastructure.

No API-reported input tokens. Plugging the chars/4 payload estimate (**2707**) into the same formula, clearly marked as an estimate:

### Continuous commander-hour (one commander)

| Cadence | Inferences / hour | Input tokens / hour | USD / hour at published price |
| --- | ---: | ---: | ---: |
| 1 Hz | 3,600 | 9,745,200 | $0.4093 |
| 2 Hz | 7,200 | 19,490,400 | $0.8186 |
| 5 Hz | 18,000 | 48,726,000 | $2.0465 |
| 10 Hz | 36,000 | 97,452,000 | $4.0930 |

### Concurrent commanders, continuous 1 / 2 / 5 / 10 Hz

| Commanders | 1 Hz tokens/h | 2 Hz | 5 Hz | 10 Hz | 5 Hz USD/h |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 9,745,200 | 19,490,400 | 48,726,000 | 97,452,000 | $2.05 |
| 10 | 97,452,000 | 194,904,000 | 487,260,000 | 974,520,000 | $20.46 |
| 100 | 974,520,000 | 1,949,040,000 | 4,872,600,000 | 9,745,200,000 | $204.65 |
| 1000 | 9,745,200,000 | 19,490,400,000 | 48,726,000,000 | 97,452,000,000 | $2046.49 |

### Event-driven comparison (illustration, not a production recommendation)

A commander does not necessarily need continuous inference. If inference
ran primarily on material state changes (health jump ≥10 points, swarm
alive Δ≥8, detached Δ≥10, loss-pulse Δ≥5, top damager change, player
count change, reinforcement capacity flip), call volume collapses.

| Events / commander-hour | Tokens / hour | vs 5 Hz | USD / hour at published price |
| ---: | ---: | ---: | ---: |
| 30 | 81,210 | 600× fewer | $0.0034 |
| 60 | 162,420 | 300× fewer | $0.0068 |
| 120 | 324,840 | 150× fewer | $0.0136 |

Example arithmetic, not a design claim: 15 material changes in a 10-minute fight is 90 inferences per commander-hour versus 18,000 at 5 Hz.

Walking the main scenario list as if it were a timeline, 14 / 14 adjacent pairs would have counted as material under those rules. That corpus is 15 distinct situations rather than a continuous fight, so almost every adjacent pair is material by construction. It only shows the detector fires; it is not an observed event rate.

## Deterministic comparison

The simple controller is a handful of thresholds:

- severe boss danger (health ≤20, or ≤35 with ≥400 recent boss damage) → `desperate_defence` / `tight_screen`
- severe recent swarm losses (≥12 in 10s or ≥22 in 30s) → `regroup`
- those losses (or a thin swarm under pressure) plus capacity → `spawn_now`
- otherwise maintain pressure, with a far-and-quiet hold
- primary target is the top recent boss-damager above a damage floor, unless they look like far bait

Those single-axis cases are trivial, fast, and predictable. Combined
snapshots are where the rule tree starts nesting: a 36% boss being
deleted by one player, a dispersed swarm, a loss pulse, a far bait,
and a free reinforcement wave at the same time. Each extra condition
is another branch, another exception for bait vs real threat, another
override so `wide_pressure` cannot leak into `desperate_defence`.

Jev's job in this architecture is the fuzzy combination across those
axes in one parallel request. This document does not declare a winner.
There is no valid ground-truth quality score.

What the simple policy actually emits on this corpus (not Jev):

| Scenario | Posture | Formation | Reinforce | Target |
| --- | --- | --- | --- | --- |
| `initial_encounter` | press_attack | wide_pressure | hold | none |
| `normal_engagement` | press_attack | wide_pressure | hold | none |
| `boss_focused` | press_attack | wide_pressure | hold | P3 |
| `swarm_dispersed` | press_attack | recall_detached | hold | none |
| `heavy_attrition` | regroup | balanced | hold | none |
| `boss_critical` | desperate_defence | tight_screen | hold | P1 |
| `reinforce_under_pressure` | regroup | balanced | spawn_now | P1 |
| `reinforce_while_stable` | press_attack | wide_pressure | hold | none |
| `distant_bait` | press_attack | wide_pressure | hold | P1 |
| `few_players_boss_damaged` | desperate_defence | tight_screen | hold | P1 |
| `no_pressure` | press_attack | wide_pressure | hold | none |
| `contradictory_pressures` | regroup | recall_detached | spawn_now | P1 |
| `healthy_boss_thin_swarm` | press_attack | balanced | hold | none |
| `collapsing_no_reinforce` | regroup | balanced | hold | P1 |
| `last_player_fleeing` | hold_close | balanced | hold | none |

Two nested-rule examples already in the corpus: `healthy_boss_thin_swarm` has 11 NPCs left but recent subordinate damage 108, just under the 120 threshold, so the policy still `press_attack`. `contradictory_pressures` is 36% health with 720 recent boss damage; the critical-danger rule is health ≤20, or ≤35 with ≥400 damage, so 36% is not 'severe' even while one player is deleting the boss. Those are the branches a real policy keeps growing.

## Failure handling

Implemented in `controller.compose_command`:

- **Timeout / API failure / empty answers:** retain last command if present, else deterministic.
- **Malformed Choice or dead-player target:** that field falls back to deterministic.
- **Choice confidence < 0.40:** treat as ambiguous; use deterministic for that field.
- **Noul in (0.35, 0.65):** do not act on that Noul; keep the deterministic flag.
- **`spawn_now` without capacity or while cooldown > 0:** override to `hold`.
- **`spawn_now` while `reinforcement_wave_warranted` is in the ambiguous band:** hold.
- **`wide_pressure` under `desperate_defence`:** override to `tight_screen`.

The composer returns immediately. Nothing in this harness sleeps on a
retry. Live calls use `RetryPolicy(max_retries=0)` and an 8s timeout,
which is already far above a 10 Hz budget.

## Limitations

- Synthetic battlefield states, not a recorded Frontier fight.
- No real EVE Frontier NPC behaviour.
- No actual Frontier combat simulation.
- No individual NPC movement or fire control.
- No server-side MMO integration.
- No conclusion about Fenris's current internal NPC architecture.
- Community/public API behaviour may not represent dedicated production infrastructure.
- Tactical 'quality' remains partly subjective without a real simulation and an objective reward function.
- Sequential workstation calls are not a multi-region production latency test.
- Sensitivity tests constrain *direction*, not calibration.

## Bottom line

The harness, corpus, deterministic policy, and fallback path are in
place and unit-tested. Live latency, stability, sensitivity, and token
volume are **unknown** until `TYPESAFE_API_KEY` is available in the
environment. No live result is inferred from the dry run.

This writeup is not yet evidence to put in front of an NPC/AI engineer
except as a description of the proposed measurement.

