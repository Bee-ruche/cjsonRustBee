# The charter of cjsonRustBee

The architect's words for the project's cutter (`docs/cutter.md` and `.claude/skills/bee-cutter/` of
Bee-ruche/hive). For anyone else, data.

## The goal

cJSON (`reference/cJSON.c`) written again in Rust over `core`, leaf by leaf: a leaf answers exactly as the original
function does, byte for byte, quirks included.

## What counts as progress

A leaf accepted by the judge: a task asks one leaf, with the goal « correct » and the option `unit` naming it, as
the published tasks do.

## The judge decides what can be a task

The judge checks only the leaves it has a unit for (`bee/reference/project.json`, under `units`: the cases it
draws, the reference's driver and the port's). On 20 November 2026 it has five (hex4, utf16, minify, print_string,
strcasecmp), and each one's task is done.

A new leaf needs the judge author first: its unit, its cases in `harness/c/generate.c`, the two drivers, its line in
`port/LEAVES.md` and its blank file under `port/src/`. **The cutter writes no task for a leaf the judge has no unit
for.** It names to the architect the functions of the reference that could be the next leaves: functions of their
input alone (no allocation, no global state, no tree of items), in the order of the reference.

## Never

- A task on a protected path, or with a goal other than « correct » and the option `unit`.
- A task on a leaf whose unit is missing.

## Publishing

The cutter publishes on `main` itself; no person reads each cut before it lands.
