# Building PaperMono tools

Use this guide when planning or changing tools in this PaperMono-only fork. It complements the
[roadmap](papermono-roadmap.md) and repository development rules. These are decision-making principles,
not a required architecture, new framework, or additional approval gate.

## Start with observable behavior

Scope one independently useful increment. Describe what the user sees and what happens during rapid input,
slow hardware work, errors, and leaving the screen. Resolve choices that affect this increment; defer the rest.
Discuss new visual designs before implementing them. Name the existing component and verify its capabilities
in the pinned source, rather than assuming an API or a newer reference project behaves the same way.

## Keep responsibilities clear and local

- Give state, input, rendering, and hardware resources clear owners. Use the simplest separation the feature needs.
- Process accepted inputs in order. Decide explicitly whether intermediate visual states can be skipped.
  Slow rendering must not silently discard user actions.
- Treat feedback as its own behavior: define when it starts, ends, repeats, and overlaps. Distinguish a held
  press, transient acknowledgement, and persistent navigation selection.
- Avoid holding input/state locks during display refresh, network requests, or storage work. Keep shared-state
  handoffs short and protect activity lifetime when navigating away.
- Reuse activities, theme components, HAL access, and existing libraries before adding code. Borrow narrowly
  from CrossPlay. Official M5Stack libraries are welcome when they do not duplicate existing peripheral ownership.
- Keep changes local until another actual feature needs shared code. Do not introduce a service, event bus,
  task, queue, or abstraction solely for anticipated future use.

## Make limits and recovery explicit

Prefer bounded storage where practical; justify allocations and queue sizes. Decide what happens at input,
precision, capacity, and hardware limits. Never silently drop a required action to preserve visual feedback.
Preserve the reader, navigation, sleep behavior, and hardware ownership. For recording or uploads, define
resource cleanup and data retention before starting background work; calculator choices are not a template
for durable data processing.

## Verify, then simplify

Keep a small runnable check for non-trivial state transitions, boundaries, and recovery. Test the feature's
observable behavior rather than mirroring every helper. Build the relevant firmware target after final code
changes; distinguish host checks from device evidence.

On PaperMono, check normal use, rapid/repeated input, idle feedback, errors, and exit/reader return. Measure
latency or resource use before claiming improvement. A successful build does not prove timing or appearance.

Before finishing, inspect the final flow as if it were designed today. Delete superseded approaches, unused
flags, and temporary paths. Update documentation to describe the final behavior, with unresolved device checks
stated separately. Avoid preserving the history of each implementation attempt in the design.

## Tiny feature checklist

- What does this increment do, and what stays out of scope?
- Which existing component or implementation will it reuse?
- What happens during fast input, slow work, errors, and exit?
- Who owns state/resources, and what are the bounds and cleanup rules?
- What host check and PaperMono experiment demonstrate success?

## Calculator example

The [calculator plan](papermono-calculator-input-plan.md) records the concrete behavior. Input updates the
calculator immediately; rendering uses the latest snapshot, skipping intermediate frames. Separate key
feedback tracks independent expiry, and navigation retains its own selection. E-paper refreshes remain serial.
A replay queue was unnecessary for this interaction. The lesson is to decide feedback and busy-state behavior
early—not to require snapshots or per-key timers in every future tool.
