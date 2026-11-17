# The tooling of cjsonRustBee

- `reference/`: what the hive's judge reads of this project: `project.json` (the leaves, their bounds, the image
  the judge uses), `build-reference` (the reference's driver and the generator of cases), `build-unit` (one leaf's
  driver; it also holds the project's rule on what a leaf's file may hold: `port/LEAVES.md`). The judge is the
  registry's (`docs/reference-judge.md` there): it runs these scripts in containers without network.
- `check.sh`: the quick check of one leaf, for who writes it: the same scripts, on cases drawn now.
- `sandbox/`: where project code runs on your machine, and nowhere else.

## Sandbox

Two forms, the same as for every project of the hive:

- **The image** (`sandbox/Dockerfile`): the system and the compilers the judge uses for this project. `AGENTS.md`
  gives the two commands: build the image (with network; it holds nothing of the project), then run the check in
  it without network, the clone mounted read-only.
- **Without Docker** (`sandbox/sans-docker.sh <command>`, Linux): no network, none of your environment, an empty
  home; the work tree is the one writable place. It shows the machine's own `/usr`: it serves this project only
  where `cc`, `rustc` and `ld.lld` live there. A compiler installed in your home folder is not shown inside (the
  home is hidden, on purpose): use the image then.
