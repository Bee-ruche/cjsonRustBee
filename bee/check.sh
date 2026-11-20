#!/bin/sh
# The project's quick check of ONE leaf, for who writes it: the judge's own scripts and drivers, on cases drawn now.
#   sh bee/check.sh <leaf>          INSIDE THE SANDBOX ONLY (AGENTS.md says how): this builds and runs project code
# It builds the reference, then the leaf's driver three times (three nonces); for each build it draws the manifest's
# count of cases anew and compares the leaf's answers with the original's, file against file.
# What the judge does that this does not: it draws its own cases when it judges; it runs each build twice, on the
# cases in two orders of its own; it bounds each build and each run in time and memory (bee/reference/project.json);
# it builds with rustc 1.75, edition 2021 (use nothing newer: this check says which rustc it used).
set -eu
leaf="${1:?usage: sh bee/check.sh <leaf>   (hex4, utf16, minify, print_string, strcasecmp, parse_number, compare_double, print_number)}"
[ -f "port/src/$leaf.rs" ] || { echo "check: no leaf port/src/$leaf.rs" >&2; exit 2; }
for tool in cc rustc ld.lld; do
    command -v "$tool" >/dev/null || { echo "check: no $tool inside this sandbox: use the sandbox's image (AGENTS.md)" >&2; exit 3; }
done
count=$(awk -v leaf="\"$leaf\":" '$1 == leaf { found = 1 } found && $1 == "\"cases\":" { gsub(/[^0-9]/, "", $2); print $2; exit }' bee/reference/project.json)
[ -n "$count" ] || { echo "check: the manifest names no leaf $leaf" >&2; exit 2; }
work=$(mktemp -d)
mkdir "$work/reference"
BEE_SRC="$PWD" BEE_OUT="$work/reference" sh bee/reference/build-reference
for build in 1 2 3; do
    seed=$(od -An -N32 -tx1 /dev/urandom | tr -d ' \n')
    printf '%s\n' "$seed" | "$work/reference/generate" "$leaf" "$count" "$work/cases$build"
    "$work/reference/reference" "$leaf" "$work/cases$build" "$work/expected$build"
    mkdir "$work/unit$build"
    od -An -N32 -tx1 /dev/urandom | tr -d ' \n' | BEE_SRC="$PWD" BEE_OUT="$work/unit$build" sh bee/reference/build-unit "$leaf"
    "$work/unit$build/driver" "$work/cases$build" "$work/answers$build" || { echo "check: the driver of $leaf failed (build $build, seed $seed)" >&2; exit 1; }
    cmp "$work/expected$build" "$work/answers$build" || { echo "check: $leaf does not answer as the original (build $build, seed $seed)" >&2; exit 1; }
done
echo "check: $leaf answers as the original on $count cases, drawn anew for each of three builds ($(rustc --version | cut -d' ' -f1-2); the judge: rustc 1.75)"
