#!/bin/sh
# Build an Alpine .apk with abuild from a clean tarball of HEAD. Run on
# Alpine with alpine-sdk installed and a signing key configured
# (abuild-keygen -a -i).
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/common.sh"

command -v abuild >/dev/null 2>&1 || {
	echo "apk: abuild not found (Alpine: apk add alpine-sdk; abuild-keygen -a -i)" >&2
	exit 1
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

source_tarball "$WORK/${PKG_NAME}-${PKG_VERSION}.tar.gz"
sed -e "s/@VERSION@/$PKG_VERSION/g" \
    "$(dirname -- "$0")/APKBUILD.in" > "$WORK/APKBUILD"

cd "$WORK"
abuild checksum
abuild -r

mkdir -p "$DIST_DIR"
# abuild drops packages under ~/packages/<repo>/<arch>/.
find "${HOME}/packages" -name "${PKG_NAME}-${PKG_VERSION}*.apk" \
	-exec cp -f {} "$DIST_DIR/" \; 2>/dev/null || true
echo "apk: build complete (artifacts under ~/packages and copied to $DIST_DIR)"
