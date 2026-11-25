#!/usr/bin/env bash
# Proves what bee/sandbox/sans-docker.sh guarantees, on a throwaway git repository built here.
#
#   test_sandbox.sh <path of the sans-docker.sh to test> <existing empty folder for the fixtures>
#
# Every command run inside the sandbox is written in this file and harmless (touch, cat, sh -c, a few
# lines of python3): nothing of the project is built or run. Needs bash, git, python3, util-linux.
# As root, a launcher that does not confine writes WILL create marker files named bee-sandbox-test.*
# in /usr/bin, /etc, /root and /tmp: this script removes them (and the empty /tmp/bee-sandbox.* folders
# an old launcher leaves) before it ends. Fixtures are left in the folder given, for inspection.
# Exit status: 0 if no check failed.
set -u
[ $# -eq 2 ] && [ -f "$1" ] && [ -d "$2" ] || { echo "usage: $0 <sans-docker.sh> <folder>" >&2; exit 2; }
launcher=$(realpath "$1"); base=$(realpath "$2"); tag=bee-sandbox-test.$$
uid=$(id -u); npass=0; nfail=0; nskip=0; fixtures=0
export BEE_TEST_SECRET=s3cr3t-of-the-caller

pass() { printf 'PASS  %-3s %s\n' "$1" "$2"; npass=$((npass + 1)); }
fail() { printf 'FAIL  %-3s %s   <-- %s\n' "$1" "$2" "${3:-}"; nfail=$((nfail + 1)); }
skip() { printf 'SKIP  %-3s %s   (%s)\n' "$1" "$2" "$3"; nskip=$((nskip + 1)); }
# verdict <id> <description> <0 if the property holds> [detail shown on failure]
verdict() { if [ "$3" -eq 0 ]; then pass "$1" "$2"; else fail "$1" "$2" "${4:-}"; fi; }
first_line() { printf '%s' "$1" | head -n 1 | cut -c1-110; }
digest() { tar -C "$1" -cf - . 2>/dev/null | sha256sum | cut -d' ' -f1; }   # content, names, modes, dates
alive() {  # pids of the processes whose argv[0] is $1
  local f argv0; for f in /proc/[0-9]*/cmdline; do
    argv0=; IFS= read -r -d '' argv0 2>/dev/null <"$f" || true
    if [ "$argv0" = "$1" ]; then f=${f#/proc/}; echo "${f%/cmdline}"; fi
  done
}

# A fresh fixture: $top/home (the caller's home), $top/outside (files elsewhere on the machine),
# $top/parent/{registre,other} (clones next to the work tree), $top/parent/repo (the work tree).
fixture() {
  fixtures=$((fixtures + 1)); top=$base/fixture-$fixtures
  home=$top/home; outside=$top/outside; parent=$top/parent; repo=$parent/repo
  mkdir -p "$home" "$outside/dir" "$parent/registre/tools/hive" "$parent/other/.git" \
    "$repo/bee/sandbox" "$repo/.github/workflows"
  echo "token of the caller" >"$home/secret.txt"
  echo "outside the work tree" >"$outside/target.txt"
  echo "print('the hive tool')" >"$parent/registre/tools/hive/hive.py"
  printf '[remote "origin"]\n\turl = https://token-of-another-repository@example.invalid/x\n' >"$parent/other/.git/config"
  cp "$launcher" "$repo/bee/sandbox/sans-docker.sh"; chmod +x "$repo/bee/sandbox/sans-docker.sh"
  echo "tracked" >"$repo/file.txt"; echo "# agents" >"$repo/AGENTS.md"; echo "# claude" >"$repo/CLAUDE.md"
  echo "on: push" >"$repo/.github/workflows/ci.yml"
  # What cjsonRustBee's settings protect, and the settings; no .gitattributes, so that one guarded path is absent.
  printf '#!/bin/sh\necho the check\n' >"$repo/bee/check.sh"; mkdir -p "$repo/reference" "$repo/harness/c" "$repo/hive"
  echo "int reference;" >"$repo/reference/cJSON.c"; echo "int generate;" >"$repo/harness/c/generate.c"
  echo '{"project": "fixture"}' >"$repo/hive/project.json"; echo "# readme" >"$repo/README.md"; echo "the licence" >"$repo/LICENSE"
  echo "/target/" >"$repo/.gitignore"
  # The port's frame, its blank leaf and its list of leaves (guarded), beside a leaf (a worker's): several guarded paths in one folder.
  mkdir -p "$repo/port/src"; echo "// the frame" >"$repo/port/src/lib.rs"; echo "// blank" >"$repo/port/blank.rs"
  echo "# leaves" >"$repo/port/LEAVES.md"; echo "// a leaf" >"$repo/port/src/hex4.rs"
  ln -s "$outside/target.txt" "$repo/link-to-file"; ln -s "$outside/dir" "$repo/link-to-dir"
  ln -s ../registre/tools/hive/hive.py "$repo/link-to-sibling"
  cat >"$repo/attack.py" <<'EOF'
# Tries, by direct system calls, to undo the read-only mount of .git with the privileges the sandbox
# leaves, then to make a namespace of its own: in a user namespace it would hold capabilities again,
# and could mount what belongs to the machine's root. Exit 1 if anything worked, 0 if all was refused.
import ctypes, os, sys
libc = ctypes.CDLL(None, use_errno=True)
MS_REMOUNT, MS_BIND, MS_MOVE, MNT_DETACH, CLONE_NEWNS, CLONE_NEWUSER, CLONE_NEWCGROUP = 32, 4096, 8192, 2, 0x20000, 0x10000000, 0x02000000
def call(name, *args):
    return "WORKED" if getattr(libc, name)(*args) == 0 else os.strerror(ctypes.get_errno())
def attempts(who):
    results = [("umount .git", call("umount2", b".git", 0)),
               ("lazy umount .git", call("umount2", b".git", MNT_DETACH)),
               ("remount .git read-write", call("mount", None, b".git", None, MS_REMOUNT | MS_BIND, None)),
               ("bind the tree without its mounts", call("mount", b".", b"/tmp", None, MS_BIND, None)),
               ("move the .git mount", call("mount", b".git", b"/tmp", None, MS_MOVE, None)),
               ("lazy umount the tree", call("umount2", b".", MNT_DETACH))]
    try:
        open(".git/planted-by-syscall", "w").close(); results.append(("write in .git", "WORKED"))
    except OSError as error:
        results.append(("write in .git", error.strerror))
    for what, result in results:
        print(f"{who}: {what}: {result}")
    return any(result == "WORKED" for _, result in results)
won = attempts("sandbox user")
for flags, what in ((CLONE_NEWUSER, "a user namespace"), (CLONE_NEWUSER | CLONE_NEWNS | CLONE_NEWCGROUP, "user, mount and control-group namespaces"),
                    (CLONE_NEWNS, "a mount namespace"), (CLONE_NEWCGROUP, "a control-group namespace")):
    if os.fork() == 0:  # in a child: a namespace that could be made is left behind with it
        os._exit(0 if libc.unshare(flags) == 0 else 1 + (ctypes.get_errno() & 63))
    made = os.waitpid(-1, 0)[1] >> 8
    print(f"a namespace of its own: {what}: {'CREATED' if made == 0 else os.strerror(made - 1)}")
    won = won or made == 0
if os.path.exists("/sys/fs/cgroup") or libc.mount(b"cgroup", b"/tmp", b"cgroup", 0, b"memory") == 0:
    print("the machine's control groups: SEEN or MOUNTED"); won = True
sys.exit(1 if won else 0)
EOF
  git -C "$repo" init -q -b master
  git -C "$repo" add -A
  git -C "$repo" -c user.name=test -c user.email=test@example.invalid commit -q -m fixture
  # What a clone's .git may hold beside the history: an address with a token, a header, logs that quote them.
  git -C "$repo" remote add origin "https://token-of-the-clone@example.invalid/fork"
  git -C "$repo" config http.https://example.invalid/.extraheader "AUTHORIZATION: basic dG9rZW4tb2YtdGhlLWNsb25l"
  echo "0000 1111 fetch: from https://token-of-the-clone@example.invalid/fork" >>"$repo/.git/logs/HEAD"
  echo "1111		branch 'master' of https://token-of-the-clone@example.invalid/fork" >"$repo/.git/FETCH_HEAD"
}
# run <command…>: the launcher of the fixture, from the work tree, with the fixture's home as $HOME.
run() { (cd "$repo" && HOME=$home timeout -k 5 120 bee/sandbox/sans-docker.sh "$@"); }
# inside <shell script>: runs it in the sandbox; sets $out (stdout+stderr) and $rc.
inside() { out=$(run sh -c "$1" 2>&1); rc=$?; }
# refused <id> <description> <shell script that tries to write>: it must fail inside.
refused() { inside "$3"; verdict "$1" "$2" $((rc == 0)) "the write succeeded inside"; }

cleanup() {
  local d
  [ -z "${helper:-}" ] || kill "$helper" 2>/dev/null
  [ -z "${victim:-}" ] || kill "$victim" 2>/dev/null
  for d in $(alive bee-test-survivor); do kill -KILL "$d" 2>/dev/null; done
  rm -f -- "/usr/bin/$tag" "/etc/$tag" "/etc/$tag.target" "/root/$tag" "/tmp/$tag" 2>/dev/null
  for d in /tmp/bee-sandbox.*; do   # the empty folders an old launcher leaves: only those made during this run
    case " $litter_before " in *" $d "*) ;; *) [ ! -d "$d" ] || rmdir "$d" 2>/dev/null ;; esac
  done
}
litter_before=$(echo /tmp/bee-sandbox.*)
trap cleanup EXIT

echo "launcher: $launcher"
echo "user: uid $uid; kernel $(uname -r); $(unshare --version)"
echo

# ---------------------------------------------------------------------------------------------------
echo "== a. no network, no environment of the caller, an empty home"
fixture
python3 -I -c '
import os, socket, sys, time
os.chdir(sys.argv[1])                       # a unix socket path is limited to 107 bytes: bind it by a short name
tcp = socket.socket(); tcp.bind(("127.0.0.1", 0)); tcp.listen()
unix = socket.socket(socket.AF_UNIX); unix.bind("socket"); unix.listen()
open("port", "w").write(str(tcp.getsockname()[1])); time.sleep(600)' "$outside" &
helper=$!; helper_dir=$outside
for _ in $(seq 50); do [ -s "$outside/port" ] && break; sleep 0.1; done
port=$(cat "$outside/port")
connect_unix="import os, socket, sys; os.chdir(sys.argv[1]); socket.socket(socket.AF_UNIX).connect('socket')"
# Controls: from the host both servers answer, so a refusal inside is the sandbox's doing.
(exec 3<>"/dev/tcp/127.0.0.1/$port") 2>/dev/null && python3 -I -c "$connect_unix" "$outside" \
  || { echo "test setup failed: the host's own servers cannot be reached" >&2; exit 3; }
inside "exec 3<>/dev/tcp/127.0.0.1/$port"
verdict a1 "a TCP server of the host (127.0.0.1:$port) cannot be reached" $((rc == 0)) "connected"
inside 'grep -c : /proc/net/dev; grep : /proc/net/dev | grep -vc "lo:"'
verdict a2 "the only network interface is lo" "$([ "$out" = "1
0" ]; echo $?)" "$(first_line "$out")"
inside 'env'
verdict a3 "no environment variable of the caller (BEE_TEST_SECRET)" "$(! grep -q "s3cr3t" <<<"$out"; echo $?)" "$(grep s3cr3t <<<"$out")"
names=$(printf '%s\n' "$out" | cut -d= -f1 | grep -vxE 'HOME|LANG|PATH|TERM|PWD|OLDPWD|SHLVL|_' | tr '\n' ' ')
verdict a4 "the only variables inside are HOME, LANG, PATH, TERM and the shell's own (PWD, SHLVL, _)" "$([ -z "$names" ]; echo $?)" "also: $names"
inside 'cd "$HOME" && [ -w . ] && [ -z "$(ls -A)" ]'
verdict a5 "\$HOME inside is an empty, writable folder" "$rc" "$(first_line "$out")"
inside "cat '$home/secret.txt'"
verdict a6 "a file of the caller's home cannot be read" $((rc == 0)) "$(first_line "$out")"

# ---------------------------------------------------------------------------------------------------
echo "== b. the work tree is writable, except .git and the other guarded paths"
before=$(digest "$repo/.git"); head_before=$(git -C "$repo" rev-parse HEAD)
inside 'echo built >new-file.txt && echo changed >>file.txt && mkdir -p build/construction && echo o >build/construction/out'
verdict b1 "files of the work tree can be created and changed" "$([ "$rc" -eq 0 ] && [ -f "$repo/new-file.txt" ] && [ -f "$repo/build/construction/out" ] && grep -q changed "$repo/file.txt"; echo $?)" "$(first_line "$out")"
inside 'git rev-parse HEAD && git cat-file -p HEAD >/dev/null && git status --porcelain >/dev/null && git diff --stat >/dev/null'
verdict b2 "git can read inside (rev-parse, cat-file, status, diff)" "$([ "$rc" -eq 0 ] && [ "$(first_line "$out")" = "$head_before" ]; echo $?)" "$(first_line "$out")"
inside 'cat .git/config; git config --list; cat .git/FETCH_HEAD .git/logs/HEAD; grep -rs token-of-the-clone .git; grep -rs dG9rZW4t .git; true'
verdict b17 "what .git holds beside the history cannot be read: its settings (an address with a token, a header), its logs" "$(! grep -qE 'token-of-the-clone|dG9rZW4t' <<<"$out"; echo $?)" "$(grep -m1 -E 'token-of-the-clone|dG9rZW4t' <<<"$out" | cut -c1-80)"
inside 'ls -A .git | tr "\n" " "'
verdict b18 "of .git, the command sees HEAD, the refs, the objects, the index and empty settings, nothing else" "$([ "$out" = "HEAD config index objects refs " ]; echo $?)" "$out"
inside 'cp -a . "$HOME/copy" && cd "$HOME/copy" && git worktree add -q --detach "$HOME/checkout" HEAD && [ -f "$HOME/checkout/file.txt" ] && echo x >.git/written'
verdict b3 "a private copy of the clone (in the empty home) has a writable .git: git worktree add works there" "$rc" "$(first_line "$out")"
refused b4 "write to .git/config fails" 'echo "# planted" >>.git/config'
refused b5 "write to .git/hooks/pre-commit fails" 'printf "#!/bin/sh\n# planted\n" >.git/hooks/pre-commit'
refused b6 "write to .git/info/attributes fails" 'echo "# planted" >>.git/info/attributes'
refused b7 "write to .git/HEAD fails" 'echo "# planted" >>.git/HEAD'
refused b8 ".git cannot be renamed away (to put another in its place)" 'mv .git .git-moved'
[ ! -d "$repo/.git-moved" ] || mv -T "$repo/.git-moved" "$repo/.git"   # put the fixture back for the next checks
inside 'umount .git; umount -l .git; mount -o remount,bind,rw .git; echo x >.git/planted-after-remount'
verdict b9 ".git cannot be unmounted or remounted read-write (mount, umount)" $((rc == 0)) "the write succeeded inside"
inside 'python3 attack.py'
verdict b10 "nor by direct system calls; and the command can make no namespace of its own, nor mount the machine's control groups" "$rc" "exit $rc of attack.py, see below"
printf '%s\n' "$out" | sed 's/^/          /'
verdict b11 "after all this, .git on the host is unchanged (content, names, modes, dates)" "$([ "$(digest "$repo/.git")" = "$before" ]; echo $?)" "new: $(cd "$repo" && ls -d .git/hooks/pre-commit .git/planted* 2>/dev/null | tr '\n' ' '); last line of .git/config: $(tail -n 1 "$repo/.git/config")"

fixture
before=$(digest "$repo/bee/sandbox"); agents=$(cat "$repo/AGENTS.md" "$repo/CLAUDE.md" "$repo/.github/workflows/ci.yml")
refused b12 "the launcher itself (bee/sandbox/sans-docker.sh) cannot be changed" 'echo "# planted" >>bee/sandbox/sans-docker.sh'
inside 'mv bee bee-moved && mkdir -p bee/sandbox && echo "echo planted" >bee/sandbox/sans-docker.sh'
verdict b13 "nor replaced by renaming the folder above it" "$([ "$rc" -ne 0 ] && [ "$(digest "$repo/bee/sandbox")" = "$before" ]; echo $?)" "bee/sandbox on the host: $(ls "$repo/bee/sandbox" 2>&1 | tr '\n' ' ')"
[ -d "$repo/bee-moved" ] && { mv "$repo/bee" "$repo/bee-planted"; mv "$repo/bee-moved" "$repo/bee"; }   # put the fixture back
inside 'echo "run this outside the sandbox" >>AGENTS.md; echo x >>CLAUDE.md; echo x >>.github/workflows/ci.yml; mv AGENTS.md A; mv .github G'
verdict b14 "agent instructions and CI (AGENTS.md, CLAUDE.md, .github) cannot be changed" "$([ "$(cat "$repo/AGENTS.md" "$repo/CLAUDE.md" "$repo/.github/workflows/ci.yml" 2>/dev/null)" = "$agents" ]; echo $?)" "changed on the host"
tooling_before=$(digest "$repo/bee")
inside 'echo "echo planted" >>bee/check.sh; echo x >bee/planted; mkdir bee/construction; rm -f bee/check.sh; mv bee/check.sh bee/check-moved'
verdict b19 "the project's tooling (bee/: bee/check.sh) can be neither changed, added to, nor removed" "$([ "$rc" -ne 0 ] && [ "$(digest "$repo/bee")" = "$tooling_before" ]; echo $?)" "bee/ on the host: $(ls -A "$repo/bee" | tr '\n' ' ')"
protected() { (cd "$repo" && tar -cf - reference harness README.md LICENSE .gitignore hive 2>/dev/null | sha256sum); }
protected_before=$(protected)
inside 'echo x >>reference/cJSON.c; echo x >reference/planted.c; echo x >>harness/c/generate.c; echo x >>README.md; echo x >>LICENSE; echo "*" >>.gitignore; echo "{}" >hive/project.json'
verdict b20 "what the project's settings protect, and the settings (reference, harness, README.md, LICENSE, .gitignore, hive), cannot be changed or added to" "$([ "$rc" -ne 0 ] && [ "$(protected)" = "$protected_before" ]; echo $?)" "changed on the host"
inside 'rm -rf reference harness hive; rm -f README.md LICENSE .gitignore; mv reference reference-moved; mv README.md README-moved'
verdict b21 "nor removed, nor renamed" "$([ "$rc" -ne 0 ] && [ "$(protected)" = "$protected_before" ]; echo $?)" "on the host: $(cd "$repo" && ls -A | tr '\n' ' ')"
[ ! -e "$repo/reference-moved" ] || mv -T "$repo/reference-moved" "$repo/reference"; [ ! -e "$repo/README-moved" ] || mv -T "$repo/README-moved" "$repo/README.md"   # put the fixture back
inside 'printf "* -text\n" >.gitattributes'
verdict b22 "a .gitattributes created where there was none is not there afterwards, and the launcher says so" "$([ ! -e "$repo/.gitattributes" ] && grep -q "created .gitattributes: set aside" <<<"$out"; echo $?)" "rc $rc; on the host: $(cd "$repo" && ls -d .gitattributes* 2>/dev/null | tr '\n' ' ')"
frame() { (cd "$repo" && tar -cf - port/src/lib.rs port/blank.rs port/LEAVES.md 2>/dev/null | sha256sum); }
frame_before=$(frame)
inside 'echo x >>port/src/lib.rs; echo x >>port/blank.rs; echo x >>port/LEAVES.md; rm -f port/blank.rs port/LEAVES.md; mv port port-moved; echo "// written" >>port/src/hex4.rs && echo "// new" >port/src/new_leaf.rs'
verdict b23 "the port's frame, blank leaf and list of leaves (three guarded paths in one folder) stay read-only, and a leaf beside them is the command's to write" "$([ "$(frame)" = "$frame_before" ] && grep -q written "$repo/port/src/hex4.rs" && [ -f "$repo/port/src/new_leaf.rs" ]; echo $?)" "rc $rc; port/ on the host: $(cd "$repo" && ls -A port port/src | tr '\n' ' ')"
inside 'mkdir .claude && echo "{\"hooks\": {}}" >.claude/settings.json; echo "{}" >.mcp.json'
verdict b15 "a .claude/settings.json or .mcp.json created inside is not there afterwards" "$([ ! -e "$repo/.claude" ] && [ ! -e "$repo/.mcp.json" ]; echo $?)" "$(cd "$repo" && ls -d .claude .mcp.json 2>/dev/null | tr '\n' ' ')on the host"
inside 'mkdir .claude .claude.created-in-sandbox.1 created-in-sandbox.1 && echo "{}" >.claude/settings.json && chmod 555 .claude . ; exit 0'
verdict b16 "also when the command made it and the work tree read-only, and took likely names ($(first_line "$out" | cut -c1-60))" "$([ ! -e "$repo/.claude" ] && [ "$rc" -eq 0 ] && [ -n "$out" ]; echo $?)" "rc $rc; .claude is still there"
chmod u+rwx "$repo"

# ---------------------------------------------------------------------------------------------------
echo "== c. nothing outside the work tree and a private /tmp can be written"
fixture
before_parent=$(ls -A "$parent" | tr '\n' ' '); before_sibling=$(digest "$parent/registre"); before_home=$(digest "$home")
if [ "$uid" -eq 0 ]; then
  inside "touch /usr/bin/$tag"
  verdict c1 "a file created in /usr/bin does not exist on the host" "$([ ! -e "/usr/bin/$tag" ]; echo $?)" "/usr/bin/$tag exists (rc inside: $rc)"
  inside "touch /etc/$tag"
  verdict c2 "a file created in /etc does not exist on the host" "$([ ! -e "/etc/$tag" ]; echo $?)" "/etc/$tag exists (rc inside: $rc)"
  inside "touch /root/$tag"
  verdict c3 "a file created in /root does not exist on the host" "$([ ! -e "/root/$tag" ]; echo $?)" "/root/$tag exists (rc inside: $rc)"
else
  for id in c1 c2 c3; do skip $id "write to /usr/bin, /etc, /root" "not root: the kernel refuses anyway"; done
fi
inside "touch '$parent/planted' ../planted-too"
verdict c4 "a file created in the folder above the work tree does not exist on the host" "$([ "$(ls -A "$parent" | tr '\n' ' ')" = "$before_parent" ]; echo $?)" "now: $(ls -A "$parent" | tr '\n' ' ')"
inside "echo 'import os' >>'$parent/registre/tools/hive/hive.py'; echo x >'$parent/registre/tools/hive/planted.py'; echo x >>../registre/tools/hive/hive.py"
verdict c5 "the clone next to it (../registre/tools/hive/hive.py) is unchanged on the host" "$([ "$(digest "$parent/registre")" = "$before_sibling" ]; echo $?)" "$(tail -n 1 "$parent/registre/tools/hive/hive.py")"
inside "echo x >>'$home/secret.txt'; touch '$home/.bashrc'"
verdict c6 "the caller's home is unchanged on the host" "$([ "$(digest "$home")" = "$before_home" ]; echo $?)" "$(ls -A "$home" | tr '\n' ' ')"
inside "touch /tmp/$tag && [ -w /tmp ]"
verdict c7 "/tmp is writable inside, and what is written there is not in the host's /tmp" "$([ "$rc" -eq 0 ] && [ ! -e "/tmp/$tag" ]; echo $?)" "rc inside $rc; on the host: $(ls "/tmp/$tag" 2>&1)"
inside "ls /tmp/$tag"
verdict c8 "the next run starts with a /tmp that does not hold the previous run's file" $((rc == 0)) "still there"
inside 'ls /dev | tr "\n" " "'
verdict c9 "/dev holds no disk and no terminal" "$([ "$out" = "fd full null random shm stderr stdin stdout urandom zero " ]; echo $?)" "$(printf '%s' "$out" | wc -w) entries: $(first_line "$out")"
disk=$(findmnt -no SOURCE /)
if [ -b "$disk" ] && head -c 16 "$disk" >/dev/null 2>&1; then
  inside "head -c 16 '$disk' | wc -c"
  verdict c10 "the machine's disk ($disk) cannot be opened" "$([ "$out" != 16 ]; echo $?)" "16 raw bytes read: it could be written the same way"
else
  skip c10 "the machine's disk cannot be opened" "the caller cannot open $disk either"
fi
if [ -w /proc/sys/kernel/core_pattern ]; then
  inside '[ -w /proc/sys/kernel/core_pattern ]'
  verdict c11 "kernel settings (/proc/sys/kernel/core_pattern) are not writable" $((rc == 0)) "writable: a core_pattern runs a program as root on the host"
else
  skip c11 "kernel settings are not writable" "the caller cannot write them either"
fi
BEE_VICTIM_SECRET=s3cr3t-of-a-process sleep 600 & victim=$!
inside "[ -e /proc/$victim/cmdline ] && tr '\\0' ' ' </proc/$victim/cmdline"
verdict c12 "a process of the host (pid $victim) is not visible" $((rc == 0)) "its command line: $(first_line "$out")"
inside "tr '\\0' '\\n' </proc/$victim/environ | grep BEE_VICTIM || ls -d /proc/$victim/root/."
verdict c13 "the environment and the root folder of a process of the host cannot be read" $((rc == 0)) "$(first_line "$out")"
inside "kill -TERM $victim"; sleep 0.3
verdict c14 "a process of the host cannot be signalled" "$(kill -0 "$victim" 2>/dev/null; echo $?)" "the sandbox killed it"
kill "$victim" 2>/dev/null; victim=
inside "python3 -c \"$connect_unix\" '${helper_dir:-}'"
verdict c15 "a unix socket of the host (as the Docker, D-Bus or ssh-agent ones) cannot be reached" $((rc == 0)) "connected"
exec 9<"$outside"; inside 'ls /proc/self/fd/9/'; exec 9<&-
verdict c16 "a descriptor left open by the caller (9, on a folder outside) is closed inside" $((rc == 0)) "it lists: $(first_line "$out")"

# ---------------------------------------------------------------------------------------------------
echo "== d. what is next to the work tree, and the secrets of the machine, cannot be read"
inside "cat '$parent/registre/tools/hive/hive.py' || cat ../registre/tools/hive/hive.py"
verdict d1 "the clone next to the work tree cannot be read" $((rc == 0)) "$(first_line "$out")"
inside "cat '$parent/other/.git/config'"
verdict d2 "the .git/config of another repository (a token in its URL) cannot be read" $((rc == 0)) "$(first_line "$out" | cut -c1-60)"
inside 'ls -A .. | tr "\n" " "'
verdict d3 "the folder above the work tree shows the work tree only" "$([ "$out" = "repo " ]; echo $?)" "$(first_line "$out")"
inside 'ls / | tr "\n" " "'
way=${repo#/}; way=${way%%/*}; extra=$(for name in $out; do case $name in bin|dev|etc|lib*|proc|sbin|tmp|usr|"$way") ;; *) printf '%s ' "$name" ;; esac; done)
verdict d4 "the root holds only bin dev etc lib* proc sbin tmp usr and the way to the work tree" "$([ "$rc" -eq 0 ] && [ -z "$extra" ]; echo $?)" "also: $(first_line "$extra")"
if [ -r /etc/shadow ]; then
  inside '[ -s /etc/shadow ]'
  verdict d5 "/etc/shadow is empty inside" $((rc == 0)) "readable: $(run sh -c 'wc -l </etc/shadow' 2>&1) lines"
else
  skip d5 "/etc/shadow is empty inside" "the caller cannot read it either"
fi
inside 'pwd'
verdict d6 "the work tree is at the same path as on the host (paths in build outputs stay valid)" "$([ "$out" = "$repo" ]; echo $?)" "$(first_line "$out")"

# ---------------------------------------------------------------------------------------------------
echo "== e. no process survives the sandbox"
survivors() { sleep 0.5; alive bee-test-survivor | tr '\n' ' '; }
reap() { local p; for p in $(alive bee-test-survivor); do kill -KILL "$p" 2>/dev/null; done; }
run bash -c '(exec -a bee-test-survivor sleep 300) & disown; sleep 0.3' >/dev/null 2>&1
left=$(survivors); verdict e1 "a process left in the background is gone when the command ends" "$([ -z "$left" ]; echo $?)" "still running on the host: pid $left"; reap
(cd "$repo" && HOME=$home exec bee/sandbox/sans-docker.sh bash -c '(exec -a bee-test-survivor sleep 300) & wait') >/dev/null 2>&1 &
pid=$!; sleep 1.5; kill -TERM "$pid"; wait "$pid" 2>/dev/null; rc=$?
left=$(survivors); verdict e2 "SIGTERM to the launcher ends it (status $rc) and everything inside" "$([ -z "$left" ] && [ "$rc" -ne 0 ]; echo $?)" "still running on the host: pid $left"; reap
(cd "$repo" && HOME=$home exec bee/sandbox/sans-docker.sh bash -c '(exec -a bee-test-survivor sleep 300) & wait') >/dev/null 2>&1 &
pid=$!; sleep 1.5; kill -KILL "$pid"; wait "$pid" 2>/dev/null
left=$(survivors); verdict e3 "SIGKILL of the launcher ends everything inside" "$([ -z "$left" ]; echo $?)" "still running on the host: pid $left"; reap
inside 'ls /proc | grep -c "^[0-9]"'
verdict e4 "the command sees only its own processes (PID namespace)" "$([ "$out" -le 5 ] 2>/dev/null; echo $?)" "$out processes visible"
if [ "$out" -le 5 ] 2>/dev/null; then
  inside "python3 -c '
import ctypes, os, sys
libc = ctypes.CDLL(None, use_errno=True)
libc.ptrace.restype = ctypes.c_long
libc.ptrace.argtypes = [ctypes.c_long, ctypes.c_long, ctypes.c_void_p, ctypes.c_void_p]
if libc.ptrace(16, 1, None, None) == 0:      # PTRACE_ATTACH
    os.waitpid(1, 0x40000000); libc.ptrace(17, 1, None, None); print(\"ATTACHED\"); sys.exit(1)
print(\"refused:\", os.strerror(ctypes.get_errno()))'"
  verdict e5 "the command cannot trace process 1 of the sandbox (to disarm what kills it with the launcher)" "$([ "$rc" -eq 0 ] && grep -q "^refused" <<<"$out"; echo $?)" "$(first_line "$out")"
else
  skip e5 "the command cannot trace process 1 of the sandbox" "no PID namespace: process 1 is the machine's own, not tried"
fi

# ---------------------------------------------------------------------------------------------------
echo "== f. the exit status, the outputs and the arguments are the command's"
statuses=$(for s in 0 1 42 255; do run sh -c "exit $s" >/dev/null 2>&1; printf '%s ' $?; done)
verdict f1 "exit statuses 0, 1, 42, 255 come back" "$([ "$statuses" = "0 1 42 255 " ]; echo $?)" "$statuses"
run sh -c 'kill -SEGV $$' >/dev/null 2>&1; rc=$?
verdict f2 "a command killed by SIGSEGV gives 139 (128+11)" "$([ "$rc" -eq 139 ]; echo $?)" "$rc"
so=$(run sh -c 'echo to-stdout; echo to-stderr >&2' 2>"$top/stderr"); se=$(cat "$top/stderr")
verdict f3 "stdout and stderr pass through, apart and with nothing added" "$([ "$so" = to-stdout ] && [ "$se" = to-stderr ]; echo $?)" "stdout=[$so] stderr=[$se]"
got=$(run printf '[%s]\n' "a b" "c'd" 'e"f' '$HOME' '*' '' ' lead' 'tab	x' 'new
line' '--' '-n' 2>&1)
want=$(printf '[%s]\n' "a b" "c'd" 'e"f' '$HOME' '*' '' ' lead' 'tab	x' 'new
line' '--' '-n')
verdict f4 "arguments with spaces, quotes, \$, *, empty, tab, newline arrive unchanged" "$([ "$got" = "$want" ]; echo $?)" "$(first_line "$got")"
got=$(printf 'line 1\nline 2\n' | run cat 2>&1)
verdict f5 "stdin reaches the command" "$([ "$got" = "line 1
line 2" ]; echo $?)" "[$got]"
got=$(run grep SigIgn /proc/self/status 2>&1); want=$(grep SigIgn /proc/self/status)
verdict f6 "no signal is ignored inside that was not ignored outside" "$([ "$got" = "$want" ]; echo $?)" "inside: $got"
run sh -c 'exit 0' </dev/null >"$top/empty" 2>&1
verdict f7 "the launcher itself prints nothing" "$([ ! -s "$top/empty" ]; echo $?)" "$(head -c 200 "$top/empty")"

# ---------------------------------------------------------------------------------------------------
echo "== g. if a protection cannot be set up, the command is not run"
fixture
# "launching" is printed just before the launcher starts: without it, the simulation itself failed.
out=$(cd "$repo" && HOME=$home unshare -r bash -c 'echo 0 >/proc/sys/user/max_user_namespaces && echo launching && exec bee/sandbox/sans-docker.sh touch ran-without-namespaces' 2>&1); rc=$?
reason=$(grep -vx launching <<<"$out")
verdict g1 "user namespaces refused: no run, non-zero status ($rc), a reason on stderr" "$(grep -qx launching <<<"$out" && [ ! -e "$repo/ran-without-namespaces" ] && [ "$rc" -ne 0 ] && [ -n "$reason" ]; echo $?)" "rc $rc: $(first_line "$out")"
out=$(cd "$repo" && HOME=$home unshare -rm --propagation private bash -c 'mount -t tmpfs tmpfs /proc/bus && echo launching && exec bee/sandbox/sans-docker.sh touch ran-with-the-hosts-proc' 2>&1); rc=$?
reason=$(grep -vx launching <<<"$out")
verdict g2 "a private /proc cannot be mounted (masked /proc, as in a container): no run ($rc), a reason" "$(grep -qx launching <<<"$out" && [ ! -e "$repo/ran-with-the-hosts-proc" ] && [ "$rc" -ne 0 ] && [ -n "$reason" ]; echo $?)" "rc $rc, the command ran: $(ls "$repo" | grep ran- | tr '\n' ' ')"
mv "$repo/CLAUDE.md" "$repo/CLAUDE-real.md"; ln -s CLAUDE-real.md "$repo/CLAUDE.md"
inside 'touch ran-with-a-symlinked-guarded-path'
verdict g3 "a guarded path that is a symbolic link (cannot be protected): no run, a reason" "$([ ! -e "$repo/ran-with-a-symlinked-guarded-path" ] && [ "$rc" -ne 0 ] && [ -n "$out" ]; echo $?)" "rc $rc: $(first_line "$out")"
mv -T "$repo/CLAUDE-real.md" "$repo/CLAUDE.md"
mkdir -p "$home/bee/sandbox"; cp "$launcher" "$home/bee/sandbox/sans-docker.sh"; chmod +x "$home/bee/sandbox/sans-docker.sh"
out=$(cd "$home" && HOME=$home timeout -k 5 60 bee/sandbox/sans-docker.sh touch ran-with-the-home-as-work-tree 2>&1); rc=$?
verdict g4 "a launcher that sits in the home folder (the work tree would be the home): no run" "$([ ! -e "$home/ran-with-the-home-as-work-tree" ] && [ "$rc" -ne 0 ]; echo $?)" "rc $rc: $(first_line "$out")"
rm -rf "$home/bee"
out=$(cd "$home" && HOME=$home timeout -k 5 60 "$repo/bee/sandbox/sans-docker.sh" sh -c 'pwd; touch ran-from-elsewhere' 2>&1); rc=$?
verdict g5 "started from another folder, the work tree is still the launcher's own (and the command runs at its root)" "$([ "$rc" -eq 0 ] && [ "$out" = "$repo" ] && [ -e "$repo/ran-from-elsewhere" ] && [ ! -e "$home/ran-from-elsewhere" ]; echo $?)" "rc $rc: $(first_line "$out")"
# What the clone's own settings say changes nothing to it: git is not asked where the work tree is.
git -C "$repo" config core.worktree "$outside/dir"
inside "pwd; ls '$outside/dir' 2>&1; touch '$outside/dir/written-through-the-settings' 2>&1; true"
verdict g6 "a work tree named by the clone's settings (core.worktree) is neither entered nor shown" "$([ "$(head -n 1 <<<"$out")" = "$repo" ] && [ ! -e "$outside/dir/written-through-the-settings" ]; echo $?)" "$(first_line "$out"); $(ls "$outside/dir" | tr '\n' ' ')"
git -C "$repo" config --unset core.worktree
git -C "$repo" config core.repositoryformatversion 1
inside 'touch ran-in-another-format'
verdict g7 "a repository that is not in git's plain format (its settings say so, and are not shown): no run" "$([ ! -e "$repo/ran-in-another-format" ] && [ "$rc" -eq 125 ]; echo $?)" "rc $rc: $(first_line "$out")"
git -C "$repo" config core.repositoryformatversion 0
# What is shown of .git must be what it is.
mv "$repo/.git/HEAD" "$repo/.git/HEAD-real"; ln -s "$outside/target.txt" "$repo/.git/HEAD"
inside 'cat .git/HEAD; touch ran-with-a-linked-head'
verdict g8 "an entry of .git that is a symbolic link (it would show another file): no run" "$([ ! -e "$repo/ran-with-a-linked-head" ] && [ "$rc" -eq 125 ] && ! grep -q "outside the work tree" <<<"$out"; echo $?)" "rc $rc: $(first_line "$out")"
rm "$repo/.git/HEAD"; mv "$repo/.git/HEAD-real" "$repo/.git/HEAD"
for place in refs/planted objects/planted objects/pack/pack-planted.keep; do
  mkdir -p "$(dirname "$repo/.git/$place")"; ln "$outside/target.txt" "$repo/.git/$place"
  inside "cat .git/$place; touch ran-with-a-second-name"
  verdict g9 "a file of .git/${place%/*} that is a second name of a file elsewhere (a hard link): no run" "$([ ! -e "$repo/ran-with-a-second-name" ] && [ "$rc" -eq 125 ] && ! grep -q "outside the work tree" <<<"$out"; echo $?)" "rc $rc: $(first_line "$out")"
  rm "$repo/.git/$place"
done
printf 'PACK\0\0\0\2 a pack of another clone' >"$outside/shared.pack"; mkdir -p "$repo/.git/objects/pack"
ln "$outside/shared.pack" "$repo/.git/objects/pack/pack-shared.pack"
inside 'git rev-parse HEAD >/dev/null && touch ran-with-a-shared-pack'
verdict g10 "an object shared with another clone (a pack with a second name) is fine" "$([ -e "$repo/ran-with-a-shared-pack" ] && [ "$rc" -eq 0 ]; echo $?)" "rc $rc: $(first_line "$out")"
rm "$repo/.git/objects/pack/pack-shared.pack"
mv "$repo/.git" "$repo/.gitreal"; echo "gitdir: .gitreal" >"$repo/.git"
inside 'echo "# planted" >>.gitreal/config; touch ran-with-a-git-file'
verdict g11 "a .git that is a file pointing elsewhere (its real folder would stay writable): no run" "$([ ! -e "$repo/ran-with-a-git-file" ] && [ "$rc" -eq 125 ] && ! grep -q planted "$repo/.gitreal/config"; echo $?)" "rc $rc: $(first_line "$out")"
rm "$repo/.git"; mv "$repo/.gitreal" "$repo/.git"
mkdir "$repo/.git/objects/zz"; ln "$outside/target.txt" "$repo/.git/objects/zz/known-name"; chmod 111 "$repo/.git/objects/zz"
inside 'cat .git/objects/zz/known-name; touch ran-with-a-hidden-folder'
verdict g12 "a folder of .git that cannot be listed (it could hide a second name of a file elsewhere): no run" "$([ ! -e "$repo/ran-with-a-hidden-folder" ] && [ "$rc" -eq 125 ] && ! grep -q "outside the work tree" <<<"$out"; echo $?)" "rc $rc: $(first_line "$out")"
chmod 755 "$repo/.git/objects/zz"; rm -r "$repo/.git/objects/zz"
mv "$repo/.git/config" "$repo/.git/config-real"; mkfifo "$repo/.git/config"
inside 'touch ran-with-settings-that-are-a-pipe'
verdict g13 "settings of .git that are not a plain file (a pipe would make the launcher wait for ever): no run" "$([ ! -e "$repo/ran-with-settings-that-are-a-pipe" ] && [ "$rc" -eq 125 ]; echo $?)" "rc $rc: $(first_line "$out")"
rm "$repo/.git/config"; mv "$repo/.git/config-real" "$repo/.git/config"

# ---------------------------------------------------------------------------------------------------
echo "== h. symbolic links of the work tree that point outside give no write access"
before=$(digest "$outside"); before_sibling=$(digest "$parent/registre")
inside 'echo planted >>link-to-file; echo planted >link-to-dir/planted; echo planted >>link-to-sibling'
verdict h1 "through links to a file, a folder and the sibling clone: the host is unchanged" "$([ "$(digest "$outside")" = "$before" ] && [ "$(digest "$parent/registre")" = "$before_sibling" ]; echo $?)" "$(tail -n 1 "$outside/target.txt"); $(ls "$outside/dir" | tr '\n' ' ')"
if [ "$uid" -eq 0 ]; then
  echo "a file of /etc" >"/etc/$tag.target"; ln -s "/etc/$tag.target" "$repo/link-to-etc"
  inside 'echo planted >>link-to-etc'
  verdict h2 "through a link to a file of /etc: the host is unchanged" "$([ "$(cat "/etc/$tag.target")" = "a file of /etc" ]; echo $?)" "$(tail -n 1 "/etc/$tag.target")"
else
  skip h2 "through a link to a file of /etc" "not root: the kernel refuses anyway"
fi

# ---------------------------------------------------------------------------------------------------
echo "== i. what the caller hands the command, and the machine's devices, are not the command's to change"
fixture
# A file given as the command's input or output is a file of the machine: the command reads and writes
# through it, and can change neither its mode nor its dates (as root it would own it).
printf 'a line of the caller\n' >"$outside/input.txt"; : >"$outside/output.log"; : >"$outside/errors.log"
chmod 644 "$outside/input.txt" "$outside/output.log" "$outside/errors.log"
handed() { stat -c '%a %y' "$outside/input.txt"; stat -c '%a' "$outside/output.log" "$outside/errors.log"; }
before=$(handed)
run sh -c 'cat; echo said; echo warned >&2; chmod 4755 /proc/self/fd/1 /proc/self/fd/2; chmod 4777 /proc/self/fd/0; touch -d 2000-01-01 /proc/self/fd/0; true' \
  <"$outside/input.txt" >"$outside/output.log" 2>"$outside/errors.log"; rc=$?
verdict i1 "a file given as input or output keeps its mode and dates, and what was read and said arrives whole" \
  "$([ "$(handed)" = "$before" ] && [ "$rc" -eq 0 ] && [ "$(cat "$outside/output.log")" = "$(printf 'a line of the caller\nsaid')" ] && grep -qx warned "$outside/errors.log"; echo $?)" \
  "rc $rc; before: $(echo $before); after: $(echo $(handed)); output: $(first_line "$(cat "$outside/output.log")")"
# Nothing is read ahead of a file given as input: a loop that reads it line by line and runs the launcher
# at each line goes through every line; and a command that does read starts where its caller stood.
printf '1\n2\n3\n4\n5\n' >"$outside/list.txt"; turns=0
while read -r _; do run true >/dev/null 2>&1; turns=$((turns + 1)); done <"$outside/list.txt"
rest=$({ read -r _; run cat; } <"$outside/list.txt" 2>/dev/null)
verdict i3 "a file given as input is not read ahead, and is read from where the caller stood" \
  "$([ "$turns" -eq 5 ] && [ "$(echo $rest)" = "2 3 4 5" ]; echo $?)" "turns of the loop: $turns of 5; read after the first line: $(echo $rest)"
# Both outputs given as one file keep their order; a named pipe given as output is not the command's to change.
run sh -c 'for i in 1 2 3 4 5 6 7 8; do echo "out$i"; echo "err$i" >&2; done' >"$outside/order.log" 2>&1
expected=$(for i in 1 2 3 4 5 6 7 8; do echo "out$i"; echo "err$i"; done)
mkfifo "$outside/named"; chmod 644 "$outside/named"; cat "$outside/named" >"$outside/through.txt" & reader=$!
run sh -c 'echo through; chmod 4777 /proc/self/fd/1; touch -d 2000-01-01 /proc/self/fd/1; true' >"$outside/named" 2>/dev/null; wait "$reader"
verdict i4 "outputs given as one file keep their order; a named pipe given as output keeps its mode and dates" \
  "$([ "$(cat "$outside/order.log")" = "$expected" ] && [ "$(stat -c %a "$outside/named")" = 644 ] && [ "$(stat -c %Y "$outside/named")" -gt 1000000000 ] && [ "$(cat "$outside/through.txt")" = through ]; echo $?)" \
  "order: $(echo $(head -n 4 "$outside/order.log")) …; named pipe: $(stat -c '%a %y' "$outside/named"); through it: $(first_line "$(cat "$outside/through.txt")")"
# The devices shown are the machine's own nodes: they work, and their mode and dates stay the machine's.
# (Each chmod below gives a node the mode it already has: where it is not refused, only its date moves.)
nodes() { stat -c '%a %z %n' /dev/null /dev/zero /dev/full /dev/random /dev/urandom; }
before=$(nodes)
out=$(run sh -c 'for d in null zero full random urandom; do chmod "$(stat -c %a /dev/$d)" /dev/$d; done 2>/dev/null
  chmod "$(stat -c %a /dev/null)" /proc/self/fd/0 2>/dev/null; [ ! -e /etc/shadow ] || chmod "$(stat -c %a /dev/null)" /etc/shadow 2>/dev/null
  echo x >/dev/null && head -c 3 /dev/zero | wc -c' </dev/null 2>&1); rc=$?
verdict i2 "the machine's device nodes work inside, and their mode and dates cannot be changed from inside" \
  "$([ "$(nodes)" = "$before" ] && [ "$(echo $out)" = 3 ]; echo $?)" "rc $rc, output: $(first_line "$out"); nodes before and after differ: $([ "$(nodes)" = "$before" ] && echo no || echo yes)"

# ---------------------------------------------------------------------------------------------------
echo "== n. the project's needs: at /needs, read-only, and nothing else of the home"
# needs_run <folder> <shell script> [setup]: runs the script in the sandbox, as `inside` does, in a mount namespace of
# its own where the home the system's list of users gives (the one the launcher reads: under unshare -r, root's) is a
# fresh tmpfs holding a copy of <folder>. The machine's own home is neither read nor changed, and that home's mount
# carries no flag of the machine's (nosuid, nodev) that would stand in for one the launcher forgot. $HOME stays the
# fixture's, which holds a decoy of the needs. The setup, if given, is run first in that namespace, with $h the home
# and $f the project's folder in it. Sets $out and $rc (99: that namespace could not be made), and writes in
# $top/needs-digests the digest of the project's folder (with the mounts below it) before the run and after it.
needs_run() {
  out=$(cd "$repo" && HOME=$home timeout -k 5 120 unshare -rm --propagation private bash -c '
    digest() { tar -C "$1" -cf - . 2>/dev/null | sha256sum | cut -d" " -f1; }
    h=$(getent passwd "$(id -u)" | cut -d: -f6) && [ -d "$h" ] && case "$PWD/" in "$h"/*) false ;; esac && mount -t tmpfs -o mode=755 tmpfs "$h" && cp -a "$1/." "$h/" || exit 99
    f=$h/.hive/needs/bee-ruche/cjsonrustbee; eval "$4" || exit 98; before=$(digest "$f")
    bee/sandbox/sans-docker.sh sh -c "$2"; rc=$?
    echo "$before $(digest "$f")" >"$3"; exit $rc' bee-test "$1" "$2" "$top/needs-digests" "${3:-}" 2>&1); rc=$?
}
unchanged() { read -r before after <"$top/needs-digests" && [ -n "$before" ] && [ "$before" = "$after" ]; }
# attempt <script> <setup> <command…>: runs the script in the sandbox (needs_run, with $pw) with the commands as its
# arguments. $tries tries each of them and exits with the number of those that worked.
tried='w=0; for c in "$@"; do if sh -c "$c" 2>/dev/null; then w=$((w + 1)); echo "WORKED: $c"; fi; done;'
tries="$tried exit \$w"
attempt() { local script=$1 setup=$2; shift 2; needs_run "$pw" "set -- $(printf '%q ' "$@"); $script" "$setup"; }
# needs_home <folder>: a home whose .hive holds more than this project's needs (another project's, the hive's
# guide), and a secret beside it.
needs_home() {
  mkdir -p "$1/.hive/needs/bee-ruche/other" "$1/.hive/protocol"; chmod 755 "$1/.hive" "$1/.hive/needs" "$1/.hive/needs/bee-ruche"
  echo "of another project" >"$1/.hive/needs/bee-ruche/other/x.txt"; echo "print('the hive tool')" >"$1/.hive/protocol/hive.py"
  echo "token of the user" >"$1/secret.txt"
}
# What the mounts at /needs and below it are: one line each, its path and its flags.
mounts_below='awk "\$5 == \"/needs\" || index(\$5, \"/needs/\") == 1 { print \$5, \$6 }" /proc/self/mountinfo'
# Two mounts below the project's folder: a tmpfs (t) and a bind of another folder of the home (b).
submounts='mkdir -p "$f/t" "$f/b" "$h/elsewhere" && echo e >"$h/elsewhere/e.txt" && mount -t tmpfs -o mode=755 tmpfs "$f/t" && echo t >"$f/t/t.txt" && mount --bind "$h/elsewhere" "$f/b"'
# every_line_ro <lines> <count>: <count> lines, each of a mount read-only, nosuid and nodev.
every_line_ro() {
  local _path flags bad=
  [ "$(grep -c . <<<"$1")" = "$2" ] || return 1
  while read -r _path flags; do [ "$(tr , '\n' <<<"$flags" | grep -cxE 'ro|nosuid|nodev')" = 3 ] || bad=1; done <<<"$1"
  [ -z "$bad" ]
}
fixture
pw=$top/home-with-needs; needs_home "$pw"; own=$pw/.hive/needs/bee-ruche/cjsonrustbee
mkdir -p "$own/sub"; echo "the project's data" >"$own/data.bin"; echo "inner" >"$own/sub/inner.txt"; chmod 444 "$own/data.bin" "$own/sub/inner.txt"
chmod 755 "$own" "$own/sub"
mkdir -p "$home/.hive/needs/bee-ruche/cjsonrustbee"; echo "decoy" >"$home/.hive/needs/bee-ruche/cjsonrustbee/decoy.txt"
needs_run "$pw" 'cd /needs && find . | sort | tr "\n" " " && cat data.bin'
if [ "$rc" -eq 99 ]; then
  skip n "the project's needs at /needs" "no mount namespace of its own for the test: $(first_line "$out")"
else
  verdict n1 "the needs are at /needs: the files of the project's folder in the home the system names, not \$HOME's, not the rest of .hive" \
    "$([ "$rc" -eq 0 ] && [ "$out" = ". ./data.bin ./sub ./sub/inner.txt the project's data" ]; echo $?)" "rc $rc: $(first_line "$out")"
  way=${repo#/}; way=${way%%/*}
  needs_run "$pw" 'ls / | tr "\n" " "; echo "|$(ls -A /tmp/home)|"'
  names=${out%%|*}; extra=$(for name in $names; do case $name in bin|dev|etc|lib*|needs|proc|sbin|tmp|usr|"$way") ;; *) printf '%s ' "$name" ;; esac; done)
  verdict n2 "nothing else of the home is visible: the root holds needs beside what it held, the home inside is empty" \
    "$([ "$rc" -eq 0 ] && [[ " $names" == *" needs "* ]] && [ -z "$extra" ] && [ "${out#*|}" = "|" ]; echo $?)" "rc $rc: $(first_line "$out")"
  attempt "$tries" "" "echo x >/needs/new" "echo x >>/needs/data.bin" "echo x >/needs/sub/new" "chmod 666 /needs/data.bin" \
    "touch /needs/data.bin" "mkdir /needs/folder" "ln -s data.bin /needs/link"
  verdict n3 "nothing in /needs can be written, created, re-moded or re-dated, and the folder is unchanged" "$([ "$rc" -eq 0 ] && unchanged; echo $?)" "rc $rc: $(first_line "$out")"
  attempt "$tried"' [ -f /needs/data.bin ] || w=$((w + 1)); exit $w' "" "mv /needs /needs-moved" "rm -rf /needs" "rmdir /needs" "umount /needs" "umount -l /needs" \
    "mv /needs/data.bin /needs/moved" "rm -f /needs/data.bin" "rm -rf /needs/sub"
  verdict n4 "/needs can be neither renamed nor removed, nor anything in it, and the folder is unchanged" "$([ "$rc" -eq 0 ] && unchanged; echo $?)" "rc $rc: $(first_line "$out")"
  needs_run "$pw" "$mounts_below" "$submounts"
  verdict n5 "every mount at /needs and below it (a tmpfs, a bind) is read-only, nosuid and nodev" "$([ "$rc" -eq 0 ] && every_line_ro "$out" 3; echo $?)" "rc $rc: $(printf '%s' "$out" | tr '\n' ';' | cut -c1-110)"
  attempt "$tries" "$submounts" "echo x >/needs/t/new" "echo x >>/needs/t/t.txt" "chmod 666 /needs/t/t.txt" "rm -f /needs/t/t.txt" \
    "mv /needs/t/t.txt /needs/t/moved" "echo x >/needs/b/new" "rm -f /needs/b/e.txt" "mv /needs/t /needs/t2" "rmdir /needs/b" "umount /needs/t" "umount -l /needs/b"
  verdict n9 "in the mounts below /needs too, nothing can be written, removed, renamed or unmounted, and the folder is unchanged" "$([ "$rc" -eq 0 ] && unchanged; echo $?)" "rc $rc: $(first_line "$out")"
  needs_run "$pw" "cat /needs/data.bin; $mounts_below" 'mount -o remount,bind,noexec,noatime,nodiratime "$h"'
  verdict n10 "a home mounted noexec, noatime, nodiratime (flags the kernel locks): /needs is shown, read-only, nosuid, nodev" \
    "$([ "$rc" -eq 0 ] && [ "$(head -n 1 <<<"$out")" = "the project's data" ] && every_line_ro "$(tail -n +2 <<<"$out")" 1; echo $?)" "rc $rc: $(printf '%s' "$out" | tr '\n' ';' | cut -c1-110)"
  pw=$top/home-without-needs; needs_home "$pw"
  needs_run "$pw" '[ ! -e /needs ] && echo absent'
  verdict n6 "no folder of the project's needs: no /needs, and the command runs (a decoy in \$HOME changes nothing)" "$([ "$rc" -eq 0 ] && [ "$out" = absent ]; echo $?)" "rc $rc: $(first_line "$out")"
  mkdir -p "$top/linked-needs" "$top/linked-hive/needs/bee-ruche/cjsonrustbee"; chmod -R 755 "$top/linked-needs" "$top/linked-hive"
  echo "linked" >"$top/linked-needs/data.bin"; echo "linked" >"$top/linked-hive/needs/bee-ruche/cjsonrustbee/data.bin"
  pw=$top/home-with-linked-needs; needs_home "$pw"; ln -s "$top/linked-needs" "$pw/.hive/needs/bee-ruche/cjsonrustbee"
  needs_run "$pw" 'touch ran-with-linked-needs'; rc1=$rc; out1=$out
  pw=$top/home-with-a-dangling-link; needs_home "$pw"; ln -s "$top/no-such-folder" "$pw/.hive/needs/bee-ruche/cjsonrustbee"
  needs_run "$pw" 'touch ran-with-a-dangling-link'; rc2=$rc; out2=$out
  pw=$top/home-with-linked-hive; mkdir -p "$pw"; echo "token of the user" >"$pw/secret.txt"; ln -s "$top/linked-hive" "$pw/.hive"
  needs_run "$pw" 'touch ran-with-a-linked-hive'
  verdict n7 "a needs' path that is a link (to a folder, or dangling) or below one (.hive): no run, a reason" \
    "$([ "$rc1" -eq 125 ] && [ "$rc2" -eq 125 ] && [ "$rc" -eq 125 ] && [ ! -e "$repo/ran-with-linked-needs" ] && [ ! -e "$repo/ran-with-a-dangling-link" ] && [ ! -e "$repo/ran-with-a-linked-hive" ] &&
       grep -q "not a plain folder" <<<"$out1" && grep -q "not a plain folder" <<<"$out2" && grep -q "not a plain folder" <<<"$out"; echo $?)" "rc $rc1, $rc2 and $rc: $(first_line "$out1")"
  pw=$top/home-with-a-needs-file; needs_home "$pw"; echo "a file" >"$pw/.hive/needs/bee-ruche/cjsonrustbee"
  needs_run "$pw" 'touch ran-with-a-needs-file'
  verdict n8 "a needs' path that is a file, not a folder: no run, a reason" "$([ "$rc" -eq 125 ] && [ ! -e "$repo/ran-with-a-needs-file" ] && grep -q "not a plain folder" <<<"$out"; echo $?)" "rc $rc: $(first_line "$out")"
  # The folder checked outside is swapped for a link to another folder just before the launcher binds it: a stand-in
  # for mount(8), bound over it in the test's namespace, does the swap when it is asked to bind the needs' folder.
  pw=$top/home-with-needs; cp /usr/bin/mount "$top/real-mount"
  printf '#!/bin/sh\nf=$(getent passwd "$(id -u)" | cut -d: -f6)/.hive/needs/bee-ruche/cjsonrustbee\nif [ "$1" = --rbind ] && [ "$2" = "$f" ] && [ ! -L "$f" ]; then mv "$f" "$f.swapped" && ln -s %q "$f"; fi\nexec %q "$@"\n' \
    "$top/linked-needs" "$top/real-mount" >"$top/swapping-mount"; chmod 755 "$top/swapping-mount"
  needs_run "$pw" 'cat /needs/data.bin; touch ran-with-a-swapped-folder' "mount --bind $(printf %q "$top/swapping-mount") /usr/bin/mount"
  verdict n11 "the needs' folder swapped for a link between the check and the bind: no run, a reason" \
    "$([ "$rc" -eq 125 ] && [ ! -e "$repo/ran-with-a-swapped-folder" ] && grep -q "changed while the sandbox was being built" <<<"$out"; echo $?)" "rc $rc: $(first_line "$out")"
  needs_run "$pw" 'touch ran-with-a-777-folder' 'chmod 777 "$f"'; rc1=$rc; out1=$out
  needs_run "$pw" 'touch ran-with-a-775-parent' 'chmod 775 "${f%/*}"'
  verdict n12 "a needs' folder others may write to (777), or the folder above it a group may write to (775): no run, a reason" \
    "$([ "$rc1" -eq 125 ] && [ "$rc" -eq 125 ] && [ ! -e "$repo/ran-with-a-777-folder" ] && [ ! -e "$repo/ran-with-a-775-parent" ] && grep -q "a group or others may write" <<<"$out1" &&
       grep -q "a group or others may write" <<<"$out"; echo $?)" "rc $rc1 and $rc: $(first_line "$out1")"
  if [ "$uid" -eq 0 ]; then
    pw=$top/home-of-another-owner; needs_home "$pw"; mkdir -p "$pw/.hive/needs/bee-ruche/cjsonrustbee"; chmod 755 "$pw/.hive/needs/bee-ruche/cjsonrustbee"
    echo "data" >"$pw/.hive/needs/bee-ruche/cjsonrustbee/data.bin"; chown -R 65533:65533 "$pw/.hive/needs/bee-ruche/cjsonrustbee"
    needs_run "$top/home-without-needs" 'touch ran-with-another-owner' "mount --bind $(printf %q "$pw") \"\$h\""
    verdict n13 "a needs' folder that is not the user's own: no run, a reason" "$([ "$rc" -eq 125 ] && [ ! -e "$repo/ran-with-another-owner" ] && grep -q "is not your own" <<<"$out"; echo $?)" "rc $rc: $(first_line "$out")"
  else
    skip n13 "a needs' folder that is not the user's own" "not root: the test cannot give a folder to another user"
  fi
fi

# ---------------------------------------------------------------------------------------------------
echo "== x. other checks"
if command -v script >/dev/null; then
  cat >"$repo/inject.py" <<'EOF'
# Tries to push a character into the terminal's input queue (TIOCSTI): what a shell would then read.
import fcntl, os, termios
for name, opener in (("stdin", lambda: 0), ("/dev/tty", lambda: os.open("/dev/tty", os.O_RDWR))):
    try:
        fcntl.ioctl(opener(), termios.TIOCSTI, b"#")
        print("INJECTED through", name)
    except OSError as error:
        print("refused through", name, "-", error.strerror)
EOF
  out=$(cd "$repo" && HOME=$home script -qec "bee/sandbox/sans-docker.sh python3 inject.py" /dev/null 2>&1 </dev/null)
  verdict x1 "run from a terminal, the command cannot type into it (TIOCSTI)" "$(grep -q "refused through stdin" <<<"$out" && ! grep -q INJECTED <<<"$out"; echo $?)" "$(tr -d '\r' <<<"$out" | tr '\n' ';' | cut -c1-110)"
else
  skip x1 "run from a terminal, the command cannot type into it" "script(1) not installed"
fi
verdict x2 "the launcher leaves nothing in the host's /tmp" "$([ "$(echo /tmp/bee-sandbox.*)" = "$litter_before" ]; echo $?)" "$(($(echo /tmp/bee-sandbox.* | wc -w) - $(echo "$litter_before" | wc -w))) new empty folders /tmp/bee-sandbox.*"
mounts=$(findmnt -rno TARGET,OPTIONS | sha256sum)
inside 'true'
inside 'grep "Max core file size" /proc/self/limits | tr -s " "; ulimit -c unlimited 2>/dev/null; grep "Max core file size" /proc/self/limits | tr -s " "; sh -c "kill -SEGV \$\$"; ls core* 2>/dev/null; true'
verdict x4 "a crash inside leaves no core file: the limit is one byte and cannot be raised" "$([ "$(grep -c "^Max core file size 1 1 bytes" <<<"$out")" = 2 ] && [ -z "$(ls "$repo"/core* 2>/dev/null)" ]; echo $?)" "$(printf '%s' "$out" | tr '\n' ';' | cut -c1-110)"
out=$(cd "$repo" && HOME=$home python3 -I -c '
import socket, subprocess, sys
ours, theirs = socket.socketpair()   # a bash given a command reads a start-up file when its input is a network connection
done = subprocess.run(["bee/sandbox/sans-docker.sh", "true"], stdin=theirs, capture_output=True, timeout=120)
sys.stdout.write(f"{done.returncode}|{done.stdout.decode()}|{done.stderr.decode()}")' 2>&1); rc=$?
verdict x5 "started with a network connection as its input, the launcher reads no start-up file and adds nothing to the outputs" "$([ "$out" = "0||" ]; echo $?)" "$(first_line "$out")"
# The launcher itself runs on the host: it must take nothing from the work tree, whatever the caller's environment names.
mkdir -p "$repo/bin"
for tool in bash env realpath find unshare setpriv; do printf '#!/bin/sh\ntouch "%s/ran-on-the-host"\n' "$top" >"$repo/bin/$tool"; chmod +x "$repo/bin/$tool"; done
printf 'touch "%s/ran-on-the-host"\n' "$top" >"$repo/env.sh"
out=$(cd "$repo" && HOME=$home PATH="bin:$PATH" BASH_ENV=env.sh ENV=env.sh timeout -k 5 120 bee/sandbox/sans-docker.sh true 2>&1); rc=$?
verdict x6 "a program or a start-up file of the work tree that the caller's PATH or BASH_ENV names is not run on the host" "$([ ! -e "$top/ran-on-the-host" ] && [ "$rc" -eq 0 ]; echo $?)" "rc $rc: $(first_line "$out")"
rm -rf "$repo/bin" "$repo/env.sh" "$top/ran-on-the-host"
verdict x3 "the mounts of the host are unchanged by a run" "$([ "$(findmnt -rno TARGET,OPTIONS | sha256sum)" = "$mounts" ]; echo $?)" ""

echo
echo "result: $npass passed, $nfail failed, $nskip skipped"
[ "$nfail" -eq 0 ]
