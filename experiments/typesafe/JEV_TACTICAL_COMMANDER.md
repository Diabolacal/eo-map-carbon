# Could Jev work as a tactical NPC commander?

I came across TypeSafe's Jev/System One model and it reminded me of the way NPC decision logic has been described to me before, where you have deterministic behaviour deciding what an NPC does based on the state around it. Rather than just send somebody a link and say this looks interesting, I thought I would actually test the idea.

The specific question was fairly simple. Could something like Jev sit above normal deterministic NPC behaviour and act as a higher level commander? So instead of asking a model to fly a ship, fire weapons or make decisions every game tick, one boss NPC could look at the state of a fight and occasionally decide what the group should be doing.

I used a synthetic encounter with one boss, up to 60 subordinate NPCs and up to 12 players. The normal NPCs are not AI controlled in this test. There is no combat simulation. The only thing being tested is the commander's tactical decision layer.

One important clarification before going any further. This lives in my `eo-map-carbon` repository because I already had the Jev test harness there from another experiment. The NPC benchmark itself is Python making System One API calls from the command line. It is not running NPCs inside Carbon, it is not wired into a Carbon host, and Carbon is not involved in the simulation loop. I don't want to make that sound more impressive than it is.

The full benchmark and raw processed results are in [COMMANDER_BENCHMARK.md](./COMMANDER_BENCHMARK.md). The actual harness is in [commander/](./commander/) and the processed live result set is in [commander/summaries/latest.json](./commander/summaries/latest.json).

## What I actually gave the model

I deliberately kept the state numeric and fairly boring. The model does not get told a player is "high DPS" or that the swarm is "in trouble". It gets things the game would already know, such as boss health, recent damage, how many NPCs are alive, how many are far away from the boss, recent losses, player distances, who is currently attacking what and which player has dealt the most recent damage.

A representative request was about 3,448 input tokens. That is quite small compared with the earlier source code tests I had done with Jev, which were around 22,000 to 30,000 input tokens.

For each battlefield state I sent 16 questions in the same System One request. Twelve were probability style Noul questions, things like whether the boss was in critical danger, whether the swarm was overextended, whether regrouping was warranted and whether a reinforcement wave made sense. Four were Choices covering tactical posture, formation, reinforcement action and primary target.

The important part is that I wasn't asking the model to rediscover facts the game already knows. It wasn't asked "is P3 40 km away?" because that would be pointless. It was asked whether the facts around it justified a tactical decision.

There was also a deliberately simple deterministic controller sitting underneath it. If the API failed, timed out or gave an ambiguous answer, the commander still had a valid deterministic order. That matters because I don't think you would ever want an external model to be the thing keeping a game simulation alive.

## The live test

I ran 220 real Jev calls against TypeSafe's public community API from my PC in Scotland on 16 September 2026. Every completed request came back as `jev-1.13.0`.

Those 220 calls covered 15 scripted battlefield situations, eight one-variable sensitivity tests, repeated identical states for stability testing and 160 cadence calls split across 1, 2, 5 and 10 Hz.

The median request used 3,448 input tokens and 479 output tokens. There were no API failures, rate limits or timeouts during the run.

## Latency

The latency was probably the bit I was most interested in because this falls apart fairly quickly if the answer takes seconds.

| Target rate | Median | p95 | Maximum | Deadline misses |
| --- | ---: | ---: | ---: | ---: |
| 1 Hz | 242 ms | 299 ms | 646 ms | 0 / 40 |
| 2 Hz | 240 ms | 270 ms | 291 ms | 0 / 40 |
| 5 Hz | 240 ms | 264 ms | 716 ms | 40 / 40 |
| 10 Hz | 238 ms | 276 ms | 408 ms | 40 / 40 |

So from my machine, using the public API, 1 or 2 decisions per second worked fine. Five and ten did not. Median latency sat at roughly 240 ms no matter how quickly I tried to call it, which puts sequential throughput at about 4 Hz from here.

That obviously says nothing about what a colocated or dedicated production endpoint could do, but it does at least give a real number rather than a marketing latency figure.

## Did the decisions make any sense?

Mostly, yes, although not enough that I would let the raw answer directly control anything important.

In a boss-focused scenario it identified P3, the player doing most of the recent damage, as the primary target. In a bait scenario where one player was far away trying to draw the swarm out, it targeted the nearer threat and gave pursuit only a 0.14 probability. In a deliberately messy scenario with the boss taking damage, the swarm spread out and reinforcements available, it came back with a fairly coherent set of decisions to regroup, recall detached units, spawn reinforcements and focus the main threat.

The sensitivity tests were interesting as well. Dropping boss health from 80% to 20% moved `boss_in_critical_danger` from 0.19 to 0.83. Increasing detached NPCs from 5 to 35 moved `swarm_overextended` from 0.26 to 0.82. Increasing recent losses from 2 to 20 moved `regroup_warranted` from 0.34 to 0.72. Turning reinforcement capacity on under pressure moved the probability of `spawn_now` from 0.00 to 0.99.

It was also stable. I repeated three identical states eight times each and the Choice outputs were identical every time. One of the probability outputs did move across the 0.5 threshold on identical input, from 0.44 to 0.55, so I definitely would not build hard game logic around a single 0.5 cut-off.

## The bits that went wrong

There were some obvious problems, which is probably the more useful part of the test.

The biggest one was reinforcements. If reinforcement capacity was available, Jev had a strong bias towards `spawn_now`, including situations where the fight was stable. In the completely low-pressure case it still wanted to spawn another wave and that one made it through the deterministic composer.

It also never selected `none` as the primary target in any of the 15 scenarios. Even when the only player was 160 km away and the separate pursuit judgement correctly said chasing them was a bad idea, the target Choice still picked that player.

There were also cases where its own answers did not really agree with each other. At 20% boss health the critical-danger probability went to 0.83, but tactical posture still stayed on `press_attack` at 0.83 confidence. In a heavy attrition case `regroup_warranted` was 0.77 while the raw posture Choice weakly picked `press_attack`.

That is why I think the interesting architecture, if there is one, is Jev making fuzzy tactical judgements inside a deterministic envelope rather than Jev simply being given control. The deterministic layer caught some of these bad calls. It did not catch all of them.

## Cost and scale

Using the measured median of 3,448 input tokens per inference and TypeSafe's published price at the time of the test of $42 per billion input tokens, one continuously active commander at 1 Hz works out at about 12.4 million input tokens per hour, roughly $0.52 at list price.

At 5 Hz one commander would be about $2.61 per hour. Ten simultaneous commanders would be around $26 per hour, 100 around $261 and 1,000 around $2,607. So I don't think "put this on every NPC and call it constantly" makes much sense even before you get into infrastructure and latency.

The numbers look completely different if the commander only asks for a new judgement when something meaningful changes. Thirty decisions per hour at this request size is only about 103,000 input tokens. Sixty is about 207,000. That is the version I find more interesting for something rare like a boss NPC commanding a larger group.

The pricing above is just arithmetic against TypeSafe's public list price on 16 September 2026. It is not a production quote and I have no idea what dedicated infrastructure would look like.

## What I think this shows

I don't think this shows that an LLM should replace normal NPC behaviour trees, and it definitely doesn't show that this belongs in EVE Frontier. I haven't simulated EVE Frontier combat at all.

What it does show, I think, is that a fairly small snapshot of a battlefield can be turned into a bundle of higher level tactical judgements in roughly a quarter of a second using the public API. Those decisions were repeatable and they reacted quite strongly to changes in health, losses, dispersion and reinforcement capacity. They were also imperfect in some very obvious ways.

The part I find interesting is the hierarchy. One inference could potentially make a decision for a boss and then ordinary fast deterministic logic could carry that decision out across 60 subordinate NPCs. It could be periodic at 1 or 2 Hz, or possibly just event driven when the fight materially changes. I think that is a more realistic question than trying to make every NPC an LLM agent.

There are still huge unanswered questions. This was 15 frozen synthetic situations rather than a real encounter stream, it used the public community API from Scotland, there was no closed-loop combat simulation and I have no idea how this maps onto the actual Feral AI architecture. The target and reinforcement biases would also need understood before I would trust the outputs.

So yeah, I think it is interesting enough to show somebody who actually works on NPC behaviour, mostly because there are now some real numbers and some real failures to talk about rather than just saying "this new model looks fast".

TypeSafe: https://typesafe.ai/

TypeSafe System One / Jev launch post: https://typesafe.ai/blog/introducing-system-one-models-and-jev
