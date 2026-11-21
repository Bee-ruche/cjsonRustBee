# The sandbox of cjsonRustBee, as an image: what building and checking a leaf needs, and nothing of yours.
# The same system and the same compilers as the hive's judge uses for this project (Bee-ruche/hive's
# tools/judge/Dockerfile.base with rustc and lld): what builds here builds there.
#
#   docker build -t bee-cjson bee/sandbox        (with network; only while `git status --porcelain bee/sandbox` prints nothing)
#   then the `docker run` line of AGENTS.md: no network, no capability, the clone mounted, an unprivileged user.
FROM mirror.gcr.io/library/ubuntu@sha256:24737cef17f4e22b1977df7d9a05e77c4e28b7da5b8911f8d8ccda136bf3573c

RUN apt-get update -q \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
      gcc libc6-dev rustc lld \
 && rm -rf /var/lib/apt/lists/*

USER 65534:65534
WORKDIR /
