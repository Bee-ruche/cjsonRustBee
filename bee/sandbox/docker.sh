#!/bin/bash -p
# The sandbox's Docker form: runs a command of the project in the image of bee/sandbox/Dockerfile (bee-cjson),
# without network, without capabilities, as an unprivileged user (65534), the work tree (this git clone) mounted
# read-only at /work, an empty /tmp as its home, and, if the hive's tool fetched the project's needs, their folder
# (~/.hive/needs/bee-ruche/cjsonrustbee, in the home the system's list of users gives) read-only at /needs. Nothing
# else of the machine.
#
# Usage: bee/sandbox/docker.sh <command> [arguments…]      (after: docker build -t bee-cjson bee/sandbox)
#   e.g. bee/sandbox/docker.sh sh bee/check.sh hex4
#
# If the needs' folder cannot be shown as it must be, the command is NOT run: exit status 125 and the reason on
# stderr. Otherwise the exit status is docker's (the command's). The work tree is the folder this launcher sits in
# (<work tree>/bee/sandbox/): git is not asked. Needs bash, coreutils, findutils and docker; getent to show the needs
# (without it, as on macOS or Git Bash, there is no /needs). It takes nothing from the work tree: bash is started with
# -p (no start-up file, no function of the environment); every tool of this script is found in the system's own
# folders; docker is the caller's: found on the caller's PATH and run with it, less any folder of it that is relative
# or in the work tree (Docker Desktop's docker is a script that runs docker.exe from the PATH; credential helpers and
# plugins are found there too), and its own settings (DOCKER_HOST, DOCKER_CONTEXT, DOCKER_CONFIG) are the caller's.
set -euo pipefail
caller_path=$PATH
PATH=/usr/sbin:/usr/bin:/sbin:/bin

refuse() { echo "bee-sandbox: $*: the command was NOT run." >&2; exit 125; }
# Docker reads --mount as comma-separated values: a path holding a comma, a quote or a control character would end
# the field or add options of its own (readonly=false…).
plain() { [[ $1 != *[,\"[:cntrl:]]* ]] || refuse "$2 ($1) holds a comma, a quote or a control character, which docker's --mount cannot take"; }

[ $# -ge 1 ] || { echo "usage: $0 <command> [arguments…]" >&2; exit 2; }
here=$(realpath -- "${BASH_SOURCE[0]}")
[[ $here == /?*/bee/sandbox/* ]] || refuse "this launcher is not in <work tree>/bee/sandbox/"
repo=${here%/bee/sandbox/*}
plain "$repo" "the work tree's path"
docker_path=
IFS=: read -r -a parts <<<"$caller_path"
for part in ${parts[@]+"${parts[@]}"}; do
  if [[ $part == /* && $part != "$repo" && $part != "$repo"/* ]]; then docker_path+=${docker_path:+:}$part; fi
done
docker=$(PATH=$docker_path command -v docker) && [[ $docker == /* ]] \
  || refuse "no docker on your PATH (its folders that are absolute and outside the work tree: ${docker_path:-none})"

# The project's needs, as sans-docker.sh finds and checks them: a link there or above it, or a folder (or the one
# above it) that is not the user's own or that a group or others may write to, is refused. The command runs as 65534
# here: every folder of the needs must be readable and searchable by others, every file readable by others (the
# hive's tool makes them 0755 and 0444).
needs=$(getent passwd "$(id -u)" 2>/dev/null | cut -d: -f6) || needs=
needs=${needs:+${needs%/}/.hive/needs/bee-ruche/cjsonrustbee}
mounts=()
if [ -n "$needs" ] && { [ -e "$needs" ] || [ -L "$needs" ]; }; then
  [ "$(realpath -- "$needs")" = "$needs" ] && [ -d "$needs" ] || refuse "the needs' folder is not a plain folder"
  for path in "$needs" "${needs%/*}"; do
    [ "$(stat -c %u -- "$path")" = "$(id -u)" ] && [ $((0$(stat -c %a -- "$path") & 022)) = 0 ] \
      || refuse "$path is not your own, or a group or others may write to it"
  done
  plain "$needs" "the needs' folder"
  unread=$(find "$needs" \( \( -type d ! -perm -005 \) -o \( ! -type d ! -perm -004 \) \) -print -quit 2>&1) || unread=$needs
  [ -z "$unread" ] || refuse "${unread%%$'\n'*} cannot be read by others, and the command runs as another user (folders 0755, files 0444)"
  mounts=(--mount "type=bind,source=$needs,target=/needs,readonly")
fi

# (MSYS_NO_PATHCONV: on Git Bash for Windows, the paths of the line, /work among them, would otherwise be rewritten
# into Windows paths on their way to docker.exe. Nothing else reads it.)
exec env PATH="$docker_path" MSYS_NO_PATHCONV=1 "$docker" run --rm --network none --cap-drop ALL --security-opt no-new-privileges --user 65534:65534 \
  --tmpfs /tmp:rw,exec,size=512m -e HOME=/tmp ${mounts[@]+"${mounts[@]}"} \
  --mount "type=bind,source=$repo,target=/work,readonly" -w /work bee-cjson "$@"
