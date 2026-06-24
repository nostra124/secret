#!/bin/sh
# Build a .deb from the staged install tree using dpkg-deb. No fakeroot
# required (--root-owner-group forces 0:0 ownership in the archive).
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/common.sh"

command -v dpkg-deb >/dev/null 2>&1 || {
	echo "deb: dpkg-deb not found (Debian/Ubuntu: apt-get install dpkg-dev)" >&2
	exit 1
}

ARCH=$(dpkg --print-architecture 2>/dev/null || echo all)
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
chmod 0755 "$STAGE"   # package root should be world-readable, not mktemp's 0700

stage_install "$STAGE" /usr

# Installed-Size is in KiB, rounded; computed before DEBIAN/ is added.
INSTALLED_KB=$(du -ks "$STAGE" | cut -f1)

mkdir -p "$STAGE/DEBIAN"
sed -e "s/@VERSION@/$PKG_VERSION/g" \
    -e "s/@ARCH@/$ARCH/g" \
    -e "s/@INSTALLED_SIZE@/$INSTALLED_KB/g" \
    -e "s|@MAINTAINER@|$PKG_MAINTAINER|g" \
    "$(dirname -- "$0")/control.in" > "$STAGE/DEBIAN/control"

mkdir -p "$DIST_DIR"
DEB="$DIST_DIR/${PKG_NAME}_${PKG_VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$STAGE" "$DEB" >/dev/null
echo "deb: built $DEB"
