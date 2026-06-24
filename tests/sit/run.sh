#!/bin/sh
# SIT runner — system integration tests against the real tools on this
# host (gpg, git, ssh/sshd, pass, secret-tool + gnome-keyring, oathtool,
# qrencode, clipboard). Individual tests skip when their tool is absent.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"

command -v bats >/dev/null 2>&1 || { echo "sit: bats not installed" >&2; exit 1; }
echo "sit: building bin/secret"
make -f Makefile.in bin/secret >/dev/null

echo "sit: running tests/sit/*.bats"
SECRET_SIT=1 bats tests/sit/*.bats
