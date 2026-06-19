#!/bin/sh
# Build every native package this host has tooling for. Individual
# recipes live in packaging/<fmt>/build.sh and can be run directly.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

built=0
if command -v dpkg-deb >/dev/null 2>&1; then "$HERE/deb/build.sh";   built=1; fi
if command -v rpmbuild >/dev/null 2>&1; then "$HERE/rpm/build.sh";   built=1; fi
if command -v abuild   >/dev/null 2>&1; then "$HERE/apk/build.sh";   built=1; fi
if command -v pkgbuild >/dev/null 2>&1; then "$HERE/macos/build.sh"; built=1; fi

if [ "$built" = 0 ]; then
	echo "packages: no packaging tool found on this host" >&2
	echo "  need one of: dpkg-deb (deb), rpmbuild (rpm), abuild (apk), pkgbuild (macOS)" >&2
	exit 1
fi
