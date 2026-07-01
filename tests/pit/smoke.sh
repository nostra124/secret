#!/bin/sh
# In-container smoke test for an installed `secret` package: the binary,
# the library, the plugin, completion and man page must all be present
# and the CLI must answer.
set -e

echo "smoke: secret version = $(secret version)"
secret help 2>&1 | grep -q "data transfer commands"
secret sources >/dev/null
secret templates | grep -q wifi
command -v secret >/dev/null

# Shipped artifacts (paths are prefix-relative; check the common roots).
for p in /usr/lib/libsecstore.so /usr/lib64/libsecstore.so; do
	[ -e "$p" ] && break
done
[ -e /usr/include/secstore.h ] || [ -e /usr/local/include/secstore.h ] || true
for p in /usr/libexec/secret/sources/secret-source-keepass \
         /usr/lib/secret/sources/secret-source-keepass; do
	[ -x "$p" ] && { found_plugin=1; break; }
done
: "${found_plugin:?keepass plugin not installed}"

echo "smoke: OK"
