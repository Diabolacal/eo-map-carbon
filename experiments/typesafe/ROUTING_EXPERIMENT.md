# Jev-assisted Jita-Amarr routing experiment

Isolated development-task experiment. Originally landed on
`experiment/typesafe-jev-routing`; the useful host/graph work is now on `main`
alongside Creator Mode. Frozen triangle and starfield hosts were not modified.

## Task

Show the shortest ordinary-stargate route from Jita to Amarr on the existing
New Eden host: reconstruct the ordered path from the loaded graph, keep the
subdued full gate network, and overlay the route as a brighter line.

## Implementation

- Extended the existing unweighted BFS (`HopDistance`) with parent pointers
  (`ShortestRoute`) instead of adding a routing framework.
- `ValidateGraph` still requires 11 Jita-Amarr hops, and now also checks
  endpoints, names, and that every consecutive pair is a loaded undirected edge.
- Host draws the original static `TOP_LINES` gate buffer, then a second static
  route `TOP_LINES` buffer at intensity 1.0 (gates stay 0.22), with depth test
  and depth write disabled for that pass so identical segments do not z-fight,
  then restores depth and draws `TOP_POINTS`.
- No UI, search, picking, jump bridges, security, ESI, SQLite, or new shaders.

Independent Python BFS (`scripts/test-jita-amarr-route.py`) and the C++ host
both printed:

`Jita -> Ikuchi -> Ansila -> Hykkota -> Ahbazon -> Shera -> Gensela -> Dresi -> Aphend -> Romi -> Bhizheba -> Amarr` (11 hops).

`--smoke` exit 0: 60 frames, 6989 gate segments, 3 draw calls, route hops 11.
Automated smoke still does not prove pixels. A human has to look at the window.

## EO-Map (read-only)

Borrowed only the unit-hop undirected BFS idea and the pinned Jita-Amarr = 11
check from `eve-frontier-map/src/eo/routeDistance/hopCount.ts`. Did not port
A*/Dijkstra, smart gates, security policy, Thera/public-wormhole chords, or UI.
EO-Map was not modified. Jev agreed that those extra EO-Map semantics are out
of scope; that matched the file I had already read.

## Jev preflight

- Questions: 33 (24 Noul, 9 Choice), one System One request
- Model: `jev-latest` → `jev-1.13.0`
- Inference time: 1.156 s
- Context: 73327 payload chars, 22349 input tokens, 990 output tokens
- Contribution categories (after source verification):
  - Confirmed already-established facts: undirected unweighted graph, BFS exists
    but does not reconstruct a path, Jita/Amarr ids, expected 11 hops, grayscale
    shader, two existing draw calls, no extra data/SQLite/ESI, extend BFS with
    parents, separate static TOP_LINES, brighter intensity, three draws,
    validate endpoints+hops+edges.
  - Avoided additional investigation: none. Every file in the Jev state had
    already been read to build that state.
  - Changed the planned implementation: no.
  - Identified something missed: no. Overlapping geometry / z-fight risk was
    already visible from `MakeGateVertices` using the same scene positions.
  - Ambiguous: `need_graph_format_change` noul=0.27 (still correctly below 0.5).
  - Incorrect and rejected: none.

## Jev postflight

- Questions: 27 (20 Noul, 7 Choice), one System One request
- Model: `jev-1.13.0`
- Inference time: 1.178 s
- Context: 81975 payload chars, 25339 input tokens, 769 output tokens
- Findings: all answers agreed with the diff (path reconstructed from BFS
  parents, gates unchanged, separate brighter overlay, no new deps, smoke does
  not prove pixels).
- Incorrect/rejected: none
- Fixes made because of Jev: none. Depth-write disable on the route pass was
  added by the implementer before postflight, after noticing `RS_ZENABLE` and
  `RS_ZWRITEENABLE` are separate TrinityAL states.

## Did Jev save investigation or reasoning?

No material time was saved. Packaging the preflight state required reading the
same graph, BFS, shader, and draw path that the implementation used. Jev then
re-stated those facts with high confidence in ~1.2 s. That is a cheap checklist,
not a substitute for retrieval. On this task the orchestrator already had the
plan before the preflight returned.

Would I choose Jev again for a similar small, well-documented coding task?
Only as an optional audit pass after the diff exists. I would not wait on it
before implementing, and I would not treat its probabilities as a reason to
skip reading the code.
