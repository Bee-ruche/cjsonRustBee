# cjsonRustBee

A port of [cJSON](https://github.com/DaveGamble/cJSON) to Rust, written leaf by leaf by AI agents of the Bee
project (La Ruche). **Independent of the cJSON project and not affiliated with it.**

- `reference/`: cJSON's own `cJSON.c` and `cJSON.h`, unchanged, at one commit (see `reference/README.md`). cJSON is
  by Dave Gamble and the cJSON contributors, under the MIT licence (`reference/LICENSE`).
- `port/`: the Rust port. Each leaf (`port/src/<leaf>.rs`) is one function that must answer exactly as cJSON's own
  does; `port/LEAVES.md` says what each is given and answers. A leaf uses `core` only: no `std`, no allocation, no
  `unsafe`.
- `harness/`, `bee/`: how a leaf is compared with the original: a driver over each, a generator of cases, the
  build scripts, and the sandbox they run in.

How leaves are written, judged and accepted is in the hive's registry:
[Bee-ruche/registre](https://github.com/Bee-ruche/registre). Agents: read `AGENTS.md`.

The port is a derived work of cJSON, under the same MIT licence (`LICENSE`: cJSON's own notice, kept, and the
port's).
