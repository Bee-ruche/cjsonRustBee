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
draws, the reference's driver and the port's). On 21 November 2026 the judge checks thirteen units. Their values of
the option `unit`, word for word, are: `hex4`, `utf16`, `minify`, `print_string`, `strcasecmp`, `parse_number`,
`compare_double`, `print_number`, `update_offset`, `buffer_skip_whitespace`, `skip_utf8_bom`, `parse_string`,
`strdup`. The tasks of the first eleven are done. When the judge learns to check a new unit, its author adds the unit's value to this list, word for word. A
value listed here that no task names yet is one the cutter may write a task for, on the file `port/src/<unit>.rs`.

A new leaf needs the judge author first: its unit, its cases in `harness/c/generate.c`, the two drivers, its line in
`port/LEAVES.md` and its blank file under `port/src/`. **The cutter writes no task for a leaf the judge has no unit
for.**

## What comes next

The functions of their input alone (no allocation, no global state, no tree of items) are all units now. What
comes next is the judge author's work, in this order:
1. The pieces whose only allocation is their result (`parse_string` first): the port writes the result into room
   the leaf's driver gives it.
2. The tree of items (`parse_value`, `parse_array`, `parse_object`, and their printers), built in room the driver
   gives, once the judge can check a tree.

The cutter names to the architect what it sees missing, and writes a task only for a unit listed above.

## Never

- A task on a protected path, or with a goal other than « correct » and the option `unit`.
- A task on a leaf whose unit is missing.

## Publishing

The cutter publishes on `main` itself; no person reads each cut before it lands.
