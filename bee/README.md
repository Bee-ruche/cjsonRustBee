# The tooling of cjsonRustBee

- `reference/`: what the hive's judge reads of this project: `project.json` (the leaves, their bounds, the image
  the judge uses), `build-reference` (the reference's driver and the generator of cases), `build-unit` (one leaf's
  driver; it also holds the project's rule on what a leaf's file may hold: `port/LEAVES.md`). The judge is the
  hive's (`docs/reference-judge.md` of Bee-ruche/hive): it runs these scripts in containers without network.
- `check.sh`: the quick check of one leaf, for who writes it: the same scripts, on cases drawn now.
- `sandbox/`: where project code runs on your machine, and nowhere else.
- `../.github/workflows/judge-check.yml`: on each pull request, the hive's judge asks the tip itself, unit by unit,
  whether the machine works (the manifest, the reference and the generator, the control, cases drawn and framed).

## Sandbox

Two forms, the same as for every project of the hive:

- **The image** (`sandbox/Dockerfile`): the system and the compilers the judge uses for this project. `AGENTS.md`
  gives the two commands: build the image (with network; it holds nothing of the project), then run the check in
  it with `sandbox/docker.sh`: without network, the clone mounted read-only.
- **Without Docker** (`sandbox/sans-docker.sh <command>`, Linux): no network, none of your environment, an empty
  home; the work tree is the one writable place, but for its guarded paths, read-only there: `.git` (its history
  only), `.github`, `.claude`, `.mcp.json`, `AGENTS.md`, `CLAUDE.md`, what the project's settings protect (`bee/`,
  `reference/`, `harness/`, `port/src/lib.rs`, `port/blank.rs`, `port/LEAVES.md`, `README.md`, `LICENSE`,
  `.gitignore`), the settings (`hive/`) and `.gitattributes`. A leaf, `port/src/<leaf>.rs`, is the command's to write. The
  check writes in the sandbox's own `/tmp` only. After a run, treat what stays writable as untrusted: the rest of the
  tree, and any new path (an instructions file for an agent among them). `sandbox/test_sandbox.sh
  sandbox/sans-docker.sh <an empty folder>` checks each protection on a throwaway repository it makes there (read
  its first lines before running it as root; never for a worker, never on a machine that is not a throwaway). It shows the machine's own `/usr`: it serves this project only
  where `cc`, `rustc` and `ld.lld` live there. A compiler installed in your home folder is not shown inside (the
  home is hidden, on purpose): use the image then.

**The project's needs.** If the project's settings name files or documents (`needs` in `hive/project.json`), the
hive's tool fetches them into `~/.hive/needs/bee-ruche/cjsonrustbee/`, in the home the system's list of users gives
(not `$HOME`). Both forms show that folder at `/needs`, read-only, and nothing else of the home; no folder, no
`/needs`. Both refuse to run (status 125) when it is reached through a symbolic link (there or above it), is not a
folder, or when it or the folder above it is not yours or may be written by a group or others (the tool makes them
0755). `sans-docker.sh` binds it read-only, `nosuid` and `nodev`, every mount below it too (it can be neither renamed
nor removed, nor anything in it written), and refuses if what it bound is not the folder it checked. `docker.sh`
binds it read-only and runs the command as user 65534: it refuses a folder or a file of the needs that others
cannot read (folders 0755, files 0444, as the tool makes them), and a path with a comma or a quote, which docker's
`--mount` cannot take. Both find the home with `getent`, which macOS lacks:
there `docker.sh` shows no `/needs`. The project declares no `needs` today, so nothing changes for it until it does.

