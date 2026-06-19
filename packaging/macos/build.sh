#!/bin/sh
# Build a macOS installer .pkg with pkgbuild/productbuild. Run on macOS
# with the Command Line Tools installed. Installs under /usr/local.
#
# Note: the package's value is the `secret` CLI (which statically embeds
# the library objects). libsecstore is shipped as built by `cc -shared`;
# for a first-class macOS dylib, rebuild lib with a .dylib rule.
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/common.sh"

command -v pkgbuild >/dev/null 2>&1 || {
	echo "macos: pkgbuild not found (run on macOS with the Command Line Tools)" >&2
	exit 1
}

STAGE=$(mktemp -d)
COMP=$(mktemp -d)
trap 'rm -rf "$STAGE" "$COMP"' EXIT

stage_install "$STAGE" /usr/local

pkgbuild --root "$STAGE" \
    --identifier de.driessel.secret \
    --version "$PKG_VERSION" \
    --install-location / \
    "$COMP/secret-component.pkg"

sed -e "s/@VERSION@/$PKG_VERSION/g" \
    "$(dirname -- "$0")/distribution.xml.in" > "$COMP/distribution.xml"

mkdir -p "$DIST_DIR"
PKG="$DIST_DIR/secret-${PKG_VERSION}.pkg"
productbuild --distribution "$COMP/distribution.xml" \
    --package-path "$COMP" \
    "$PKG"
echo "macos: built $PKG"
