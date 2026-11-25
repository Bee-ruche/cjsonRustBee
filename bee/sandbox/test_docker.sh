#!/usr/bin/env bash
# Proves what bee/sandbox/docker.sh (the sandbox's Docker form) does with the project's needs, on throwaway work trees
# built here. Docker itself is not run: in a mount namespace of the test's own (unshare -rm), a stand-in for docker
# records the arguments it is given, and the home the system's list of users gives (root's, under unshare -r) is a
# fresh tmpfs holding a copy of a fixture home. Nothing of the machine is read or changed.
#
#   test_docker.sh <path of the docker.sh to test> <existing empty folder for the fixtures>
#
# Needs bash, util-linux (unshare, mount), coreutils. Exit status: 0 if no check failed.
set -u
[ $# -eq 2 ] && [ -f "$1" ] && [ -d "$2" ] || { echo "usage: $0 <docker.sh> <folder>" >&2; exit 2; }
launcher=$(realpath "$1"); base=$(realpath "$2")
uid=$(id -u); npass=0; nfail=0; nskip=0; fixtures=0

pass() { printf 'PASS  %-3s %s\n' "$1" "$2"; npass=$((npass + 1)); }
fail() { printf 'FAIL  %-3s %s   <-- %s\n' "$1" "$2" "${3:-}"; nfail=$((nfail + 1)); }
skip() { printf 'SKIP  %-3s %s   (%s)\n' "$1" "$2" "$3"; nskip=$((nskip + 1)); }
verdict() { if [ "$3" -eq 0 ]; then pass "$1" "$2"; else fail "$1" "$2" "${4:-}"; fi; }
first_line() { printf '%s' "$1" | head -n 1 | cut -c1-110; }

# fixture [name of the work tree]: $top/home (the caller's $HOME, with a decoy of the needs), $top/<name> (the work
# tree, holding the launcher), the stand-in for docker, and $top/home-with-needs (a home of the system's list with the
# project's needs, 0755 and 0444 as the hive's tool makes them).
fixture() {
  fixtures=$((fixtures + 1)); top=$base/fixture-$fixtures; home=$top/home; repo=$top/${1:-repo}
  mkdir -p "$home/.hive/needs/bee-ruche/cjsonrustbee" "$repo/bee/sandbox"
  echo "decoy" >"$home/.hive/needs/bee-ruche/cjsonrustbee/decoy.txt"
  cp "$launcher" "$repo/bee/sandbox/docker.sh"; chmod +x "$repo/bee/sandbox/docker.sh"
  printf '#!/bin/sh\nprintf "%%s\\n" "$@" >%q\nexit 7\n' "$top/docker-args" >"$top/stand-in-docker"; chmod 755 "$top/stand-in-docker"
  pw=$top/home-with-needs; own=$pw/.hive/needs/bee-ruche/cjsonrustbee
  mkdir -p "$own/sub" "$pw/.hive/needs/bee-ruche/other"; echo "of another project" >"$pw/.hive/needs/bee-ruche/other/x.txt"
  echo "the project's data" >"$own/data.bin"; echo "inner" >"$own/sub/inner.txt"
  chmod 755 "$pw/.hive" "$pw/.hive/needs" "$pw/.hive/needs/bee-ruche" "$own" "$own/sub"; chmod 444 "$own/data.bin" "$own/sub/inner.txt"
}
# run_form <home folder> <setup> <command…>: the launcher of the fixture, from its work tree, with the fixture's $HOME;
# the setup is run first in the test's namespace, with $h the home and $f the project's folder in it. Sets $out, $rc
# (99, 98: the namespace or the setup failed) and $args (what docker was given, one per line; empty: not run).
run_form() {
  local folder=$1 setup=$2; shift 2
  rm -f "$top/docker-args"
  out=$(cd "$repo" && HOME=$home timeout -k 5 60 unshare -rm --propagation private bash -c '
    h=$(getent passwd "$(id -u)" | cut -d: -f6) && [ -d "$h" ] && mount -t tmpfs -o mode=755 tmpfs "$h" && cp -a "$1/." "$h/" || exit 99
    if [ -e /usr/bin/docker ]; then mount --bind "$2" /usr/bin/docker
    else mount -t tmpfs -o mode=755 tmpfs /usr/local/bin && cp "$2" /usr/local/bin/docker; fi || exit 99
    f=$h/.hive/needs/bee-ruche/cjsonrustbee; eval "$3" || exit 98; shift 3
    exec bee/sandbox/docker.sh "$@"' bee-test "$folder" "$top/stand-in-docker" "$setup" "$@" 2>&1); rc=$?
  args=$(cat "$top/docker-args" 2>/dev/null)
}
# The arguments docker is given without needs, for a work tree at $repo and the command `sh -c 'echo a  b'`.
plain_args() {
  printf '%s\n' run --rm --network none --cap-drop ALL --security-opt no-new-privileges --user 65534:65534 \
    --tmpfs /tmp:rw,exec,size=512m -e HOME=/tmp ${1:+--mount} ${1:+"type=bind,source=$1,target=/needs,readonly"} \
    --mount "type=bind,source=$repo,target=/work,readonly" -w /work bee-cjson sh -c 'echo a  b'
}
# refused <id> <description> <expected reason> <home folder> <setup>: no docker run, status 125, the reason.
refused() {
  run_form "$4" "$5" true
  verdict "$1" "$2" "$([ "$rc" -eq 125 ] && [ -z "$args" ] && grep -q "$3" <<<"$out"; echo $?)" "rc $rc, docker given: $(first_line "$args"): $(first_line "$out")"
}

echo "launcher: $launcher"
echo "user: uid $uid; kernel $(uname -r); $(unshare --version)"
echo
echo "== k. the Docker form and the project's needs"
fixture
mkdir -p "$top/home-without-needs/.hive/needs/bee-ruche/other"
run_form "$top/home-without-needs" "" sh -c 'echo a  b'
if [ "$rc" -eq 99 ]; then
  skip k "the Docker form" "no mount namespace of its own for the test: $(first_line "$out")"
  echo; echo "result: $npass passed, $nfail failed, $nskip skipped"; exit 0
fi
verdict k1 "no needs' folder: docker is given the plain line, the command's arguments unchanged, and its status comes back" \
  "$([ "$rc" -eq 7 ] && [ "$args" = "$(plain_args)" ]; echo $?)" "rc $rc: $(printf '%s' "$args" | tr '\n' ' ' | cut -c1-110)"
run_form "$pw" "" sh -c 'echo a  b'
verdict k2 "a needs' folder: one more read-only bind of it at /needs, from the home the system names (not \$HOME's decoy)" \
  "$([ "$rc" -eq 7 ] && [ "$args" = "$(plain_args /root/.hive/needs/bee-ruche/cjsonrustbee)" ]; echo $?)" "rc $rc: $(printf '%s' "$args" | grep needs | tr '\n' ' ' | cut -c1-110)"
mkdir -p "$top/linked-needs" "$top/linked-hive/needs/bee-ruche/cjsonrustbee"; chmod -R 755 "$top/linked-needs" "$top/linked-hive"
refused k3 "a needs' folder that is a link (to ~/.ssh, say): docker is not run" "not a plain folder" "$pw" 'rm -rf "$f" && ln -s /root/.ssh-like "$f" && mkdir -m 755 /root/.ssh-like'
refused k4 "a dangling link there: docker is not run" "not a plain folder" "$pw" 'rm -rf "$f" && ln -s /root/no-such-folder "$f"'
refused k5 "a link above it (.hive): docker is not run" "not a plain folder" "$pw" 'rm -rf "$h/.hive" && ln -s '"$(printf %q "$top/linked-hive")"' "$h/.hive"'
# A home of the system's list whose path holds a comma or a quote: a stand-in for getent says so, over a fresh /home.
for odd in "/home/a,b" "/home/a,readonly=false" '/home/a"b'; do
  printf '#!/bin/sh\necho %q\n' "root:x:0:0:root:$odd:/bin/sh" >"$top/stand-in-getent"; chmod 755 "$top/stand-in-getent"
  refused k6 "a home whose path is $odd (docker's --mount would split it): docker is not run" "a comma, a quote" "$pw" \
    "mount -t tmpfs -o mode=755 tmpfs /home && mkdir -p -m 755 $(printf %q "$odd/.hive/needs/bee-ruche/cjsonrustbee") && mount --bind $(printf %q "$top/stand-in-getent") /usr/bin/getent"
done
refused k7 "a needs' folder others cannot read (0700): docker is not run" "cannot be read by others" "$pw" 'chmod 700 "$f"'
refused k8 "a file of the needs others cannot read (0600): docker is not run" "cannot be read by others" "$pw" 'chmod 600 "$f/sub/inner.txt"'
refused k9 "a folder of the needs others cannot list (0711): docker is not run" "cannot be read by others" "$pw" 'chmod 711 "$f/sub"'
refused k10 "a needs' folder others may write to (0777): docker is not run" "a group or others may write" "$pw" 'chmod 777 "$f"'
refused k11 "the folder above it, writable by its group (0775): docker is not run" "a group or others may write" "$pw" 'chmod 775 "${f%/*}"'
if [ "$uid" -eq 0 ]; then
  other=$top/home-of-another-owner; cp -a "$pw" "$other"; chown -R 65533:65533 "$other/.hive/needs/bee-ruche/cjsonrustbee"
  refused k12 "a needs' folder that is not the user's own: docker is not run" "is not your own" "$pw" "mount --bind $(printf %q "$other") \"\$h\""
else
  skip k12 "a needs' folder that is not the user's own" "not root: the test cannot give a folder to another user"
fi
fixture "work,tree"
refused k13 "a work tree whose path holds a comma: docker is not run" "a comma, a quote" "$pw" ""

echo
echo "result: $npass passed, $nfail failed, $nskip skipped"
[ "$nfail" -eq 0 ]
