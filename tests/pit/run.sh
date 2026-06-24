#!/bin/sh
# PIT runner — package integration tests. Builds each native package and
# installs it into a clean container of the matching distro, then runs
# tests/pit/smoke.sh against the installed binary. Requires docker (or
# podman) with registry access; each format is skipped if its base image
# cannot be pulled.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"

OCI=${OCI:-}
for c in docker podman; do
	if command -v "$c" >/dev/null 2>&1 && "$c" info >/dev/null 2>&1; then OCI=$c; break; fi
done
[ -n "$OCI" ] || { echo "pit: no usable docker/podman" >&2; exit 1; }
echo "pit: using $OCI"

fail=0
ran=0

# Pass the host proxy + CA bundle into containers so their package
# managers work behind a TLS-intercepting proxy. CA mount is best-effort.
CA=${SECRET_PIT_CA:-/root/.ccr/ca-bundle.crt}
proxy_env="-e HTTP_PROXY -e HTTPS_PROXY -e NO_PROXY -e http_proxy -e https_proxy -e no_proxy"
ca_mount=""
[ -f "$CA" ] && ca_mount="-v $CA:/secret-proxy-ca.crt:ro"

# ---- deb: build on host (dpkg-deb), install in debian ----------------
if command -v dpkg-deb >/dev/null 2>&1; then
	echo "pit: building + testing .deb (debian)"
	packaging/deb/build.sh
	deb=$(ls -1 dist/secret_*_*.deb | head -1)
	ran=1
	# shellcheck disable=SC2086
	"$OCI" run --rm $proxy_env $ca_mount -v "$PWD/dist:/dist:ro" -v "$PWD/tests/pit:/pit:ro" \
		debian:stable-slim sh -c '
			[ -f /secret-proxy-ca.crt ] && cp /secret-proxy-ca.crt \
				/usr/local/share/ca-certificates/proxy.crt 2>/dev/null && \
				update-ca-certificates >/dev/null 2>&1 || true
			apt-get update -qq >/dev/null 2>&1
			dpkg -i /dist/'"$(basename "$deb")"' >/dev/null 2>&1 || \
				apt-get install -f -y -qq >/dev/null 2>&1
			dpkg -i /dist/'"$(basename "$deb")"' >/dev/null 2>&1
			sh /pit/smoke.sh
		' && echo "pit: deb OK" || { echo "pit: deb FAILED"; fail=1; }
fi

# ---- rpm: build + install inside fedora ------------------------------
echo "pit: building + testing .rpm (fedora)"
# shellcheck disable=SC2086
"$OCI" run --rm $proxy_env $ca_mount -v "$PWD:/src:ro" fedora:latest sh -c '
	[ -f /secret-proxy-ca.crt ] && cp /secret-proxy-ca.crt \
		/etc/pki/ca-trust/source/anchors/proxy.crt && update-ca-trust >/dev/null 2>&1 || true
	dnf install -y -q gcc make rpm-build git >/dev/null 2>&1 || { echo SKIP-NET; exit 0; }
	cp -a /src /build && cd /build
	packaging/rpm/build.sh >/dev/null 2>&1
	dnf install -y -q ./dist/secret-*.rpm >/dev/null 2>&1
	sh tests/pit/smoke.sh
' | tee /tmp/pit-rpm.log; grep -q "smoke: OK" /tmp/pit-rpm.log && echo "pit: rpm OK" || \
	{ grep -q SKIP-NET /tmp/pit-rpm.log && echo "pit: rpm SKIPPED (no mirror access)" || { echo "pit: rpm FAILED"; fail=1; }; }

# ---- apk: build + install inside alpine ------------------------------
echo "pit: building + testing .apk (alpine)"
# shellcheck disable=SC2086
"$OCI" run --rm $proxy_env $ca_mount -v "$PWD:/src:ro" alpine:latest sh -c '
	[ -f /secret-proxy-ca.crt ] && cat /secret-proxy-ca.crt >> /etc/ssl/certs/ca-certificates.crt 2>/dev/null || true
	apk add --no-cache alpine-sdk gnupg git >/dev/null 2>&1 || { echo SKIP-NET; exit 0; }
	adduser -D builder 2>/dev/null; addgroup builder abuild 2>/dev/null || true
	cp -a /src /build && chown -R builder /build && cd /build
	su builder -c "abuild-keygen -a -n >/dev/null 2>&1; packaging/apk/build.sh" >/dev/null 2>&1
	apk add --allow-untrusted dist/secret-*.apk >/dev/null 2>&1
	sh tests/pit/smoke.sh
' | tee /tmp/pit-apk.log; grep -q "smoke: OK" /tmp/pit-apk.log && echo "pit: apk OK" || \
	{ grep -q SKIP-NET /tmp/pit-apk.log && echo "pit: apk SKIPPED (no mirror access)" || { echo "pit: apk FAILED"; fail=1; }; }

[ "$ran" -eq 1 ] || { echo "pit: nothing ran" >&2; exit 1; }
[ "$fail" -eq 0 ] && echo "pit: OK (deb verified; rpm/apk where mirrors reachable)"
exit "$fail"
