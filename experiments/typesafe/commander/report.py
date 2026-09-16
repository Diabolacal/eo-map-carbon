"""Render COMMANDER_BENCHMARK.md from processed results."""

from __future__ import annotations

import json
from typing import Any

from .evaluate import PRICE_RETRIEVED_UTC_DATE, PRICE_SOURCES, PRICE_USD_PER_BILLION_INPUT
from .questions import CHOICE_IDS, NOUL_IDS, NOUL_SPECS, FORMATION_CRITERIA, POSTURE_CRITERIA, REINFORCE_CRITERIA
from .state import to_jev_state


def _fmt_s(value: float | None, digits: int = 3) -> str:
    if value is None:
        return "n/a"
    return f"{value:.{digits}f}"


def _fmt_pct(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value * 100:.1f}%"


def _fmt_int(value: int | float | None) -> str:
    if value is None:
        return "n/a"
    return f"{int(value):,}"


def _md_json(payload: Any) -> str:
    return "```json\n" + json.dumps(payload, indent=2, ensure_ascii=False) + "\n```"


def render_report(summary: dict[str, Any]) -> str:
    live = bool(summary.get("live"))
    measure = summary.get("measure") or {}
    tokens = summary.get("token_stats") or {}
    inn = (tokens.get("input") or {})
    representative_in = inn.get("median") or summary.get("representative_input_tokens")
    scale = summary.get("scale") or {}
    cadence = summary.get("cadence") or []
    scenarios = summary.get("scenarios") or []
    sensitivity = summary.get("sensitivity") or []
    stability = summary.get("stability") or []
    flags = summary.get("questionable") or []
    listed_models = summary.get("listed_models") or []
    models = summary.get("models") or []
    failures = summary.get("failures") or []
    det_notes = summary.get("deterministic_notes") or {}
    event = summary.get("event_driven") or {}
    example_state = summary.get("example_state")
    qcount = summary.get("question_count") or {}
    live_calls = summary.get("live_calls") or 0
    stop_reason = summary.get("stop_reason")

    lines: list[str] = []
    lines.append("# Jev tactical-commander feasibility benchmark")
    lines.append("")
    lines.append("Isolated TypeSafe Jev / System One experiment on branch")
    lines.append("`experiment/typesafe-jev-feral-commander`. Not a Carbon renderer")
    lines.append("milestone, not an EO-Map change, and not a claim about Fenris Feral AI.")
    lines.append("")
    if not live:
        lines.append("**Live Jev calls were not made.** `TYPESAFE_API_KEY` was not exposed")
        lines.append("in the environment, so this file records the harness, the synthetic")
        lines.append("corpus, unit-test behaviour, and dry-run structure. It does **not**")
        lines.append("contain fabricated latencies, tokens, or tactical answers.")
        lines.append("")
    else:
        lines.append(f"- Live calls made: {live_calls}")
        lines.append(f"- Model requested: `{summary.get('model', 'jev-latest')}`")
        if models:
            lines.append("- Models reported: " + ", ".join(f"`{m}`" for m in models))
        if listed_models:
            lines.append("- `client.models.list()`: " + ", ".join(f"`{m}`" for m in listed_models))
        if stop_reason:
            lines.append(f"- Suite stopped early: `{stop_reason}`")
        lines.append("")

    lines.append("## Scope")
    lines.append("")
    lines.append("This is a synthetic tactical-decision feasibility test. A compact")
    lines.append("numeric battlefield snapshot is handed to Jev; Jev returns typed")
    lines.append("higher-level judgements; a deterministic subordinate/fallback layer")
    lines.append("always has a valid command. Jev does not steer individual NPCs,")
    lines.append("fire, collide, or run a combat tick.")
    lines.append("")
    lines.append("Simulated:")
    lines.append("")
    lines.append("- 1 boss/commander, up to 60 subordinates, up to 12 player ships")
    lines.append("- optional reinforcement capacity and cooldown")
    lines.append("- scripted snapshots and one-variable paired states")
    lines.append("- a shallow threshold commander for architectural comparison")
    lines.append("- timeout / malformed / low-confidence fallback")
    lines.append("- sequential commander-cadence timing from this workstation")
    lines.append("")
    lines.append("Not simulated:")
    lines.append("")
    lines.append("- EVE Frontier NPC behaviour or Fenris's current internal AI")
    lines.append("- real Frontier combat numbers, logistics ships, or ship roles")
    lines.append("- per-NPC movement, firing, targeting mechanics, or collision")
    lines.append("- a closed-loop combat game whose next tick depends on Jev")
    lines.append("- server-side MMO integration or dedicated production inference")
    lines.append("- an objective 'AI quality' score (there is no reward function)")
    lines.append("")
    lines.append("The operator-provided test context is a workstation in Scotland")
    lines.append("calling TypeSafe's public community API. That is not network")
    lines.append("geolocation evidence.")
    lines.append("")

    lines.append("## Architecture")
    lines.append("")
    lines.append("```")
    lines.append("synthetic observable combat state")
    lines.append("        |")
    lines.append("        v")
    lines.append("Jev typed tactical decisions (Noul + Choice, one System One request)")
    lines.append("        |")
    lines.append("        v")
    lines.append("deterministic composer / fallback  -->  valid command every tick")
    lines.append("        |")
    lines.append("        v")
    lines.append("subordinate behaviour would consume the command (not simulated here)")
    lines.append("```")
    lines.append("")
    lines.append("Jev judges; it is not asked to rediscover facts the snapshot already")
    lines.append("contains. Exact health, distances, damage totals, alive counts, and")
    lines.append("top-damager identities are precomputed in `derived`.")
    lines.append("")
    lines.append("If Jev times out, the API fails, the payload is malformed, or a")
    lines.append("Choice is below the ambiguity threshold, the composer keeps the last")
    lines.append("valid command when one exists, otherwise the deterministic policy.")
    lines.append("The conceptual game tick never waits indefinitely on Jev. Spawn is")
    lines.append("overridden to hold when reinforcement capacity is not actually available.")
    lines.append("")

    lines.append("## State representation")
    lines.append("")
    lines.append("One representative compact state (`initial_encounter`):")
    lines.append("")
    if example_state is not None:
        lines.append(_md_json(example_state))
    else:
        lines.append("_Example state not recorded in this summary._")
    lines.append("")
    if measure:
        lines.append(
            f"- Serialized state: {measure.get('state_chars')} chars; "
            f"full request payload: {measure.get('payload_chars')} chars "
            f"(est. tokens chars/4={((measure.get('token_estimates') or {}).get('est_chars_div_4'))}, "
            f"chars/3={((measure.get('token_estimates') or {}).get('est_chars_div_3'))})."
        )
        lines.append(
            f"- Previous coding-task requests in this repo were ~22k–30k API input tokens "
            f"and ~73k–100k payload chars. This commander snapshot is intended to be much smaller."
        )
        lines.append("")

    lines.append("## Jev question set")
    lines.append("")
    lines.append(
        f"{qcount.get('total', 16)} questions per inference "
        f"({qcount.get('noul', 12)} Noul, {qcount.get('choice', 4)} Choice). "
        "All questions share one state and are sent in a single `POST /v1/systemone`."
    )
    lines.append("")
    lines.append("### Noul")
    lines.append("")
    for qid in NOUL_IDS:
        spec = NOUL_SPECS[qid]
        lines.append(f"- `{qid}` — {spec['instructions']}")
    lines.append("")
    lines.append("### Choice")
    lines.append("")
    lines.append("`tactical_posture`:")
    for key, desc in POSTURE_CRITERIA.items():
        lines.append(f"- `{key}` — {desc}")
    lines.append("")
    lines.append("`formation_intent`:")
    for key, desc in FORMATION_CRITERIA.items():
        lines.append(f"- `{key}` — {desc}")
    lines.append("")
    lines.append("`reinforcement_action`:")
    for key, desc in REINFORCE_CRITERIA.items():
        lines.append(f"- `{key}` — {desc}")
    lines.append("")
    lines.append("`primary_target`:")
    lines.append("- one option per currently alive synthetic player id")
    lines.append("- plus `none`")
    lines.append("")
    lines.append("Dead players remain in state with `alive: false` so recent deaths are")
    lines.append("visible, but they are not valid Choice options.")
    lines.append("")

    lines.append("## Scenario results")
    lines.append("")
    if not live:
        lines.append("No live scenario answers. The corpus itself:")
        lines.append("")
        for row in scenarios:
            lines.append(f"- `{row.get('id')}` — {row.get('title')}: {row.get('why')}")
        lines.append("")
    else:
        lines.append(
            f"{len(scenarios)} snapshots were sent. Soft review checks are directional "
            "sanity notes written before the live suite, not a gold accuracy score."
        )
        lines.append("")
        lines.append("| Scenario | Det posture | Jev posture | Det target | Jev target | Det reinforce | Jev reinforce | Review flags |")
        lines.append("| --- | --- | --- | --- | --- | --- | --- | ---: |")
        for row in scenarios:
            det = row.get("deterministic") or {}
            jev = row.get("jev_command") or {}
            lines.append(
                "| `{id}` | {dp} | {jp} | {dt} | {jt} | {dr} | {jr} | {fl} |".format(
                    id=row.get("id"),
                    dp=det.get("tactical_posture", ""),
                    jp=jev.get("tactical_posture", row.get("error", "")),
                    dt=det.get("primary_target", ""),
                    jt=jev.get("primary_target", ""),
                    dr=det.get("reinforcement_action", ""),
                    jr=jev.get("reinforcement_action", ""),
                    fl=row.get("review_flag_count", 0),
                )
            )
        lines.append("")
        lines.append("### Per-scenario notes")
        lines.append("")
        for row in scenarios:
            lines.append(f"#### `{row.get('id')}` — {row.get('title')}")
            lines.append("")
            lines.append(row.get("why", ""))
            lines.append("")
            if not row.get("ok"):
                lines.append(f"Call failed: `{row.get('error_type')}` {row.get('error')}")
                lines.append("")
                continue
            noul = row.get("noul") or {}
            interesting = [
                "boss_in_critical_danger",
                "swarm_overextended",
                "regroup_warranted",
                "reinforcement_wave_warranted",
                "focus_fire_warranted",
                "pursuit_warranted",
                "protect_boss_priority",
                "current_plan_still_sensible",
            ]
            bits = [f"`{k}`={noul.get(k)}" for k in interesting if k in noul]
            lines.append("Noul (subset): " + ", ".join(bits))
            lines.append("")
            choices = row.get("choices") or {}
            lines.append(
                "Choices: "
                + ", ".join(
                    f"`{k}`={v.get('choice')} (conf={v.get('confidence')})"
                    for k, v in choices.items()
                )
            )
            lines.append("")
            if row.get("review_flags"):
                lines.append("Questionable vs pre-written sanity notes:")
                for flag in row["review_flags"]:
                    lines.append(
                        f"- `{flag['id']}` {flag['kind']}: observed `{flag.get('observed')}` — {flag['note']}"
                    )
                lines.append("")
            else:
                lines.append("No pre-written sanity notes were violated.")
                lines.append("")

        if flags:
            lines.append("### Questionable or incorrect-looking calls")
            lines.append("")
            lines.append("These failed a pre-written directional sanity check. They are not")
            lines.append("scored as gold-set errors; the check itself can be too strict.")
            lines.append("")
            for flag in flags:
                lines.append(
                    f"- `{flag.get('scenario_id')}` `{flag.get('id')}`: "
                    f"observed `{flag.get('observed')}` — {flag.get('note')}"
                )
            lines.append("")
        else:
            lines.append("No pre-written sanity notes were violated on completed scenario calls.")
            lines.append("")

    lines.append("## Sensitivity results")
    lines.append("")
    lines.append("Each pair changes one intended variable. Accounting fields that must")
    lines.append("move with it (range buckets summing to `alive`, boss damage matching")
    lines.append("the player sum, `losses_last_30s >= losses_last_10s`) are noted per pair.")
    lines.append("The test is directional: probabilities should not move the wrong way.")
    lines.append("It does not require an exact probability.")
    lines.append("")
    if not live:
        lines.append("No live paired calls. Pair definitions:")
        lines.append("")
        for pair in sensitivity:
            lines.append(f"- `{pair.get('id')}` — {pair.get('title')}. {pair.get('accounting_notes', '')}")
        lines.append("")
    else:
        passed = sum(1 for p in sensitivity if p.get("passed"))
        lines.append(f"{passed} / {len(sensitivity)} pairs moved in the expected direction on every listed judgement.")
        lines.append("")
        lines.append("| Pair | Field | All expectations ok | Failures |")
        lines.append("| --- | --- | --- | --- |")
        for pair in sensitivity:
            fails = [e["id"] for e in (pair.get("expectations") or []) if not e.get("ok")]
            lines.append(
                f"| `{pair.get('id')}` | `{pair.get('changed_field')}` | "
                f"{'yes' if pair.get('passed') else 'no'} | {', '.join(f'`{x}`' for x in fails) or '—'} |"
            )
        lines.append("")
        for pair in sensitivity:
            lines.append(f"### `{pair.get('id')}`")
            lines.append("")
            lines.append(pair.get("title", ""))
            lines.append("")
            lines.append(pair.get("accounting_notes", ""))
            lines.append("")
            lines.append("| Judgement | Direction | Base | Variant | Delta | Ok |")
            lines.append("| --- | --- | --- | --- | --- | --- |")
            for exp in pair.get("expectations") or []:
                label = exp["id"] if "option" not in exp else f"{exp['id']}.{exp['option']}"
                lines.append(
                    f"| `{label}` | {exp.get('direction')} | {exp.get('base')} | "
                    f"{exp.get('variant')} | {exp.get('delta')} | {'yes' if exp.get('ok') else 'NO'} |"
                )
            lines.append("")
            for exp in pair.get("expectations") or []:
                if not exp.get("ok"):
                    lines.append(f"- Failed `{exp.get('id')}`: {exp.get('note')}")
            lines.append("")

    lines.append("## Stability results")
    lines.append("")
    lines.append("Identical snapshots, repeated sequentially. A commander that flips")
    lines.append("between incompatible postures on unchanged state is a negative result.")
    lines.append("")
    if not live:
        lines.append("No live repeatability samples.")
        lines.append("")
    else:
        for row in stability:
            lines.append(f"### `{row.get('scenario_id')}`")
            lines.append("")
            lines.append(
                f"- Repeats completed: {row.get('repeats_ok')} / {row.get('repeats_attempted')}"
            )
            lines.append(f"- Choice agreement rate: {_fmt_pct(row.get('choice_agreement_rate'))}")
            lines.append(f"- All choices stable: {row.get('all_choices_stable')}")
            lines.append(f"- Max Noul range (max-min): {row.get('max_noul_range')}")
            lines.append(f"- Incompatible posture switches: {row.get('incompatible_posture_switches')}")
            lines.append(f"- Incompatible formation switches: {row.get('incompatible_formation_switches')}")
            lines.append(f"- Reinforcement hold/spawn switches: {row.get('reinforcement_switches')}")
            lines.append(f"- Primary target none-flips: {row.get('primary_target_none_flips')}")
            lines.append(f"- Material oscillation: {row.get('material_oscillation')}")
            lines.append("")
            unstable = [c for c in (row.get("choices") or []) if not c.get("agreement")]
            if unstable:
                lines.append("Unstable choices:")
                for item in unstable:
                    lines.append(f"- `{item['id']}`: {item.get('unique')}")
                lines.append("")
            wide = [n for n in (row.get("nouls") or []) if n.get("range", 0) >= 0.15]
            if wide:
                lines.append("Noul ids with range ≥ 0.15:")
                for item in wide:
                    lines.append(
                        f"- `{item['id']}` range={item['range']:.3f} "
                        f"(min={item['min']:.3f}, max={item['max']:.3f})"
                    )
                lines.append("")

    lines.append("## Latency results")
    lines.append("")
    lines.append("Sequential calls from this workstation (operator-provided: Scotland)")
    lines.append("to TypeSafe's public API. Target Hz is the commander update rate,")
    lines.append("not a requirement that Jev meet every rate. A deadline miss is a")
    lines.append("call whose end-to-end latency exceeded the requested interval.")
    lines.append("")
    if not live or not cadence:
        lines.append("No live cadence samples.")
        lines.append("")
    else:
        lines.append("| Target Hz | Interval s | N | p50 s | p95 s | max s | Achieved Hz | Deadline misses | Miss % | Rate limits | Timeouts | API errors |")
        lines.append("| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
        for row in cadence:
            lines.append(
                "| {hz} | {iv} | {n} | {p50} | {p95} | {mx} | {ach} | {miss} | {mp} | {rl} | {to} | {ae} |".format(
                    hz=row.get("target_hz"),
                    iv=_fmt_s(row.get("requested_interval_s")),
                    n=row.get("sample_count"),
                    p50=_fmt_s(row.get("p50_s")),
                    p95=_fmt_s(row.get("p95_s")),
                    mx=_fmt_s(row.get("max_s")),
                    ach=_fmt_s(row.get("achieved_hz"), 2),
                    miss=row.get("deadline_misses"),
                    mp=_fmt_pct(row.get("deadline_miss_pct")),
                    rl=row.get("rate_limits"),
                    to=row.get("timeouts"),
                    ae=row.get("api_failures"),
                )
            )
        lines.append("")
        for row in cadence:
            hz = row.get("target_hz")
            p95 = row.get("p95_s")
            interval = row.get("requested_interval_s")
            if p95 is not None and interval is not None and p95 > interval:
                lines.append(
                    f"{hz} Hz is not sustainable sequentially from this test machine "
                    f"(p95 {_fmt_s(p95)}s > {interval:.3f}s interval)."
                )
            elif row.get("sustainable"):
                lines.append(
                    f"{hz} Hz was sustainable on this sample (p95 {_fmt_s(p95)}s ≤ {interval:.3f}s, no rate-limit/timeout class)."
                )
        lines.append("")

    if failures:
        lines.append("### API failures recorded")
        lines.append("")
        for item in failures:
            lines.append(
                f"- {item.get('phase')} `{item.get('scenario_id')}`: "
                f"{item.get('error_type')}: {item.get('error')}"
            )
        lines.append("")

    lines.append("## Context / token results")
    lines.append("")
    if live and inn.get("n"):
        lines.append(
            f"- API input tokens: min={inn.get('min')}, median={inn.get('median')}, "
            f"max={inn.get('max')} (n={inn.get('n')})"
        )
        out = tokens.get("output") or {}
        lines.append(
            f"- API output tokens: min={out.get('min')}, median={out.get('median')}, "
            f"max={out.get('max')} (n={out.get('n')})"
        )
        if measure:
            lines.append(
                f"- Representative payload: {measure.get('payload_chars')} chars, "
                f"state {measure.get('state_chars')} chars"
            )
        lines.append(
            "- Previous repo-comprehension run used 30,181 input tokens; Jita-Amarr "
            "preflight/postflight used 22,349 / 25,339. This commander path is the small-context case."
        )
    elif measure:
        lines.append("No API-reported tokens (no live calls). Payload estimate only:")
        lines.append(
            f"- payload_chars={measure.get('payload_chars')}, state_chars={measure.get('state_chars')}, "
            f"est chars/4={((measure.get('token_estimates') or {}).get('est_chars_div_4'))}"
        )
    lines.append("")

    lines.append("## Scale")
    lines.append("")
    lines.append("Token volume is computed from measured (or, if live data is absent,")
    lines.append("left blank) input tokens per commander inference:")
    lines.append("")
    lines.append("`tokens_per_hour = input_tokens_per_inference * Hz * 3600 * commanders`")
    lines.append("")
    lines.append(
        f"Monetary estimates use TypeSafe's public price retrieved {PRICE_RETRIEVED_UTC_DATE}: "
        f"**${PRICE_USD_PER_BILLION_INPUT:.0f} per billion input tokens**, output free. "
        f"Sources: {', '.join(PRICE_SOURCES)}. TypeSafe's own launch post notes they "
        "cannot prove the price is not subsidized. Community/public API behaviour may "
        "not represent dedicated production infrastructure."
    )
    lines.append("")
    est_only = bool(scale.get("estimate_only"))
    if scale.get("input_tokens_per_inference"):
        tok = scale["input_tokens_per_inference"]
        if est_only:
            lines.append(
                f"No API-reported input tokens. Plugging the chars/4 payload estimate "
                f"(**{tok}**) into the same formula, clearly marked as an estimate:"
            )
        else:
            lines.append(f"Measured/representative input tokens per inference: **{tok}**.")
        lines.append("")
        lines.append("### Continuous commander-hour (one commander)")
        lines.append("")
        lines.append("| Cadence | Inferences / hour | Input tokens / hour | USD / hour at published price |")
        lines.append("| --- | ---: | ---: | ---: |")
        for hz, row in (scale.get("per_commander_hour_continuous") or {}).items():
            lines.append(
                f"| {hz} Hz | {_fmt_int(row['inferences'])} | {_fmt_int(row['input_tokens'])} | "
                f"${row['usd_if_published_price']:.4f} |"
            )
        lines.append("")
        lines.append("### Concurrent commanders, continuous 1 / 2 / 5 / 10 Hz")
        lines.append("")
        lines.append("| Commanders | 1 Hz tokens/h | 2 Hz | 5 Hz | 10 Hz | 5 Hz USD/h |")
        lines.append("| ---: | ---: | ---: | ---: | ---: | ---: |")
        for n, row in (scale.get("concurrent_commanders_continuous") or {}).items():
            lines.append(
                f"| {n} | {_fmt_int(row[1]['input_tokens'])} | {_fmt_int(row[2]['input_tokens'])} | "
                f"{_fmt_int(row[5]['input_tokens'])} | {_fmt_int(row[10]['input_tokens'])} | "
                f"${row[5]['usd_if_published_price']:.2f} |"
            )
        lines.append("")
        lines.append("### Event-driven comparison (illustration, not a production recommendation)")
        lines.append("")
        lines.append("A commander does not necessarily need continuous inference. If inference")
        lines.append("ran primarily on material state changes (health jump ≥10 points, swarm")
        lines.append("alive Δ≥8, detached Δ≥10, loss-pulse Δ≥5, top damager change, player")
        lines.append("count change, reinforcement capacity flip), call volume collapses.")
        lines.append("")
        lines.append("| Events / commander-hour | Tokens / hour | vs 5 Hz | USD / hour at published price |")
        lines.append("| ---: | ---: | ---: | ---: |")
        for ev, row in (scale.get("event_driven_illustration") or {}).items():
            lines.append(
                f"| {ev} | {_fmt_int(row['input_tokens'])} | {row['vs_5hz_token_ratio']:.0f}× fewer | "
                f"${row['usd_if_published_price']:.4f} |"
            )
        lines.append("")
        lines.append(
            "Example arithmetic, not a design claim: 15 material changes in a 10-minute "
            "fight is 90 inferences per commander-hour versus 18,000 at 5 Hz."
        )
        if event:
            lines.append("")
            lines.append(
                f"Walking the main scenario list as if it were a timeline, "
                f"{event.get('material_pairs')} / {event.get('pairs')} adjacent pairs "
                "would have counted as material under those rules. That corpus is 15 "
                "distinct situations rather than a continuous fight, so almost every "
                "adjacent pair is material by construction. It only shows the detector "
                "fires; it is not an observed event rate."
            )
        lines.append("")
    else:
        lines.append("Scale table omitted because no representative input-token count is available.")
        lines.append("")

    lines.append("## Deterministic comparison")
    lines.append("")
    lines.append("The simple controller is a handful of thresholds:")
    lines.append("")
    lines.append("- severe boss danger (health ≤20, or ≤35 with ≥400 recent boss damage) → `desperate_defence` / `tight_screen`")
    lines.append("- severe recent swarm losses (≥12 in 10s or ≥22 in 30s) → `regroup`")
    lines.append("- those losses (or a thin swarm under pressure) plus capacity → `spawn_now`")
    lines.append("- otherwise maintain pressure, with a far-and-quiet hold")
    lines.append("- primary target is the top recent boss-damager above a damage floor, unless they look like far bait")
    lines.append("")
    lines.append("Those single-axis cases are trivial, fast, and predictable. Combined")
    lines.append("snapshots are where the rule tree starts nesting: a 36% boss being")
    lines.append("deleted by one player, a dispersed swarm, a loss pulse, a far bait,")
    lines.append("and a free reinforcement wave at the same time. Each extra condition")
    lines.append("is another branch, another exception for bait vs real threat, another")
    lines.append("override so `wide_pressure` cannot leak into `desperate_defence`.")
    lines.append("")
    lines.append("Jev's job in this architecture is the fuzzy combination across those")
    lines.append("axes in one parallel request. This document does not declare a winner.")
    lines.append("There is no valid ground-truth quality score.")
    lines.append("")
    controller_rows = summary.get("controller_corpus") or []
    if controller_rows:
        lines.append("What the simple policy actually emits on this corpus (not Jev):")
        lines.append("")
        lines.append("| Scenario | Posture | Formation | Reinforce | Target |")
        lines.append("| --- | --- | --- | --- | --- |")
        for row in controller_rows:
            cmd = row.get("command") or row
            lines.append(
                f"| `{row.get('id')}` | {cmd.get('tactical_posture')} | {cmd.get('formation_intent')} | "
                f"{cmd.get('reinforcement_action')} | {cmd.get('primary_target')} |"
            )
        lines.append("")
        lines.append(
            "Two nested-rule examples already in the corpus: `healthy_boss_thin_swarm` has 11 NPCs "
            "left but recent subordinate damage 108, just under the 120 threshold, so the policy still "
            "`press_attack`. `contradictory_pressures` is 36% health with 720 recent boss damage; the "
            "critical-danger rule is health ≤20, or ≤35 with ≥400 damage, so 36% is not 'severe' even "
            "while one player is deleting the boss. Those are the branches a real policy keeps growing."
        )
        lines.append("")
    if live and det_notes and det_notes.get("n"):
        lines.append("Observed agreements on completed live scenarios (Jev composed command vs simple policy):")
        lines.append("")
        lines.append(
            f"- posture agree {det_notes.get('posture_agree')}/{det_notes.get('n')}, "
            f"formation {det_notes.get('formation_agree')}/{det_notes.get('n')}, "
            f"reinforce {det_notes.get('reinforce_agree')}/{det_notes.get('n')}, "
            f"target {det_notes.get('target_agree')}/{det_notes.get('n')}"
        )
        lines.append("")
        for item in det_notes.get("disagreements") or []:
            lines.append(
                f"- `{item.get('id')}` {item.get('field')}: det `{item.get('det')}` vs Jev `{item.get('jev')}`"
            )
        lines.append("")

    lines.append("## Failure handling")
    lines.append("")
    lines.append("Implemented in `controller.compose_command`:")
    lines.append("")
    lines.append("- **Timeout / API failure / empty answers:** retain last command if present, else deterministic.")
    lines.append("- **Malformed Choice or dead-player target:** that field falls back to deterministic.")
    lines.append("- **Choice confidence < 0.40:** treat as ambiguous; use deterministic for that field.")
    lines.append("- **Noul in (0.35, 0.65):** do not act on that Noul; keep the deterministic flag.")
    lines.append("- **`spawn_now` without capacity or while cooldown > 0:** override to `hold`.")
    lines.append("- **`spawn_now` while `reinforcement_wave_warranted` is in the ambiguous band:** hold.")
    lines.append("- **`wide_pressure` under `desperate_defence`:** override to `tight_screen`.")
    lines.append("")
    lines.append("The composer returns immediately. Nothing in this harness sleeps on a")
    lines.append("retry. Live calls use `RetryPolicy(max_retries=0)` and an 8s timeout,")
    lines.append("which is already far above a 10 Hz budget.")
    lines.append("")

    lines.append("## Limitations")
    lines.append("")
    lines.append("- Synthetic battlefield states, not a recorded Frontier fight.")
    lines.append("- No real EVE Frontier NPC behaviour.")
    lines.append("- No actual Frontier combat simulation.")
    lines.append("- No individual NPC movement or fire control.")
    lines.append("- No server-side MMO integration.")
    lines.append("- No conclusion about Fenris's current internal NPC architecture.")
    lines.append("- Community/public API behaviour may not represent dedicated production infrastructure.")
    lines.append("- Tactical 'quality' remains partly subjective without a real simulation and an objective reward function.")
    lines.append("- Sequential workstation calls are not a multi-region production latency test.")
    lines.append("- Sensitivity tests constrain *direction*, not calibration.")
    lines.append("")

    lines.append("## Bottom line")
    lines.append("")
    if not live:
        lines.append("The harness, corpus, deterministic policy, and fallback path are in")
        lines.append("place and unit-tested. Live latency, stability, sensitivity, and token")
        lines.append("volume are **unknown** until `TYPESAFE_API_KEY` is available in the")
        lines.append("environment. No live result is inferred from the dry run.")
        lines.append("")
        lines.append("This writeup is not yet evidence to put in front of an NPC/AI engineer")
        lines.append("except as a description of the proposed measurement.")
    else:
        lines.extend(_bottom_line_live(summary, cadence, stability, sensitivity, flags, representative_in, scale))
    lines.append("")
    return "\n".join(lines) + "\n"


def _bottom_line_live(
    summary: dict[str, Any],
    cadence: list[dict[str, Any]],
    stability: list[dict[str, Any]],
    sensitivity: list[dict[str, Any]],
    flags: list[dict[str, Any]],
    representative_in: int | None,
    scale: dict[str, Any],
) -> list[str]:
    lines = []
    by_hz = {row.get("target_hz"): row for row in cadence}
    usable = []
    for hz in (1, 2, 5, 10):
        row = by_hz.get(hz) or by_hz.get(float(hz))
        if not row:
            continue
        p95 = row.get("p95_s")
        interval = row.get("requested_interval_s")
        if p95 is not None and interval is not None and p95 <= interval and row.get("ok_count"):
            usable.append(str(hz))
    if usable:
        lines.append(
            f"- **Latency:** sequential p95 stayed within the interval at {', '.join(h + ' Hz' for h in usable)} "
            "on this workstation and public API. Higher rates in the table either missed the deadline or were not sampled."
        )
    else:
        lines.append(
            "- **Latency:** none of the tested cadences kept p95 within the requested interval "
            "on this workstation against the public API. That does not by itself rule out a slower event-driven loop."
        )

    if stability:
        osc = [s for s in stability if s.get("material_oscillation")]
        stable = [s for s in stability if s.get("all_choices_stable")]
        if osc:
            lines.append(
                f"- **Stability:** material oscillation was observed on {len(osc)} / {len(stability)} "
                "repeated snapshots (incompatible posture/formation, hold/spawn flip, or none-flip)."
            )
        elif stable and len(stable) == len(stability):
            lines.append(
                f"- **Stability:** all Choice labels were identical across repeats on {len(stability)} "
                "snapshots. See the stability section for Noul ranges."
            )
        else:
            lines.append(
                "- **Stability:** mixed. Some Choice labels moved across identical input; see the stability section."
            )
    else:
        lines.append("- **Stability:** not sampled.")

    if sensitivity:
        passed = sum(1 for p in sensitivity if p.get("passed"))
        lines.append(
            f"- **Sensitivity:** {passed} / {len(sensitivity)} one-variable pairs moved every listed judgement "
            "in the expected direction. Failures are listed above and are not hidden."
        )
    else:
        lines.append("- **Sensitivity:** not sampled.")

    if flags:
        lines.append(
            f"- **Questionable calls:** {len(flags)} pre-written sanity notes were violated. "
            "They are listed in Scenario results rather than scored as gold-set accuracy."
        )
    else:
        lines.append("- **Questionable calls:** no pre-written sanity notes were violated on completed scenarios.")

    if representative_in:
        lines.append(
            f"- **Context:** representative API input tokens per commander inference: {representative_in}. "
            "That is roughly an order of magnitude below the earlier 20k–30k-token coding tests in this repo."
        )
    if scale.get("input_tokens_per_inference"):
        tok = scale["input_tokens_per_inference"]
        c5 = ((scale.get("concurrent_commanders_continuous") or {}).get("1000") or {}).get(5) or {}
        lines.append(
            f"- **Token volume:** {tok} input tokens/inference. One commander at 5 Hz is "
            f"{_fmt_int(((scale.get('per_commander_hour_continuous') or {}).get('5') or {}).get('input_tokens'))} "
            f"tokens/hour; 1,000 commanders at 5 Hz is {_fmt_int(c5.get('input_tokens'))} tokens/hour "
            f"(~${(c5.get('usd_if_published_price') or 0):.2f}/hour at the published $42/billion input price). "
            "Event-driven calling cuts that by the ratios in the scale table."
        )

    interesting = bool(usable) or (sensitivity and sum(1 for p in sensitivity if p.get("passed")) >= len(sensitivity) / 2)
    if interesting:
        lines.append(
            "- **Further investigation:** the measurement is interesting enough to show an NPC/AI engineer "
            "as a feasibility sketch — small state, batched typed judgements, measured cadence — not as a "
            "production recommendation. The next technical question is whether a real encounter stream "
            "(not scripted snapshots) still yields stable, directionally sensible orders when the "
            "deterministic layer owns movement and firing."
        )
    else:
        lines.append(
            "- **Further investigation:** keep this as an internal harness. The live numbers did not "
            "clearly support a commander-loop pitch without a slower event-driven design or a dedicated endpoint."
        )
    return lines
