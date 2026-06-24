#!/bin/sh
# Build a binary RPM (plus -devel) with rpmbuild from a clean tarball of
# HEAD. Run on an rpm-based distro (Fedora/RHEL/openSUSE) with rpm-build.
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/common.sh"

command -v rpmbuild >/dev/null 2>&1 || {
	echo "rpm: rpmbuild not found (Fedora/RHEL: dnf install rpm-build)" >&2
	exit 1
}

TOP=$(mktemp -d)
trap 'rm -rf "$TOP"' EXIT
mkdir -p "$TOP/SOURCES" "$TOP/SPECS" "$TOP/BUILD" "$TOP/RPMS" "$TOP/SRPMS"

source_tarball "$TOP/SOURCES/${PKG_NAME}-${PKG_VERSION}.tar.gz"
sed -e "s/@VERSION@/$PKG_VERSION/g" \
    "$(dirname -- "$0")/secret.spec.in" > "$TOP/SPECS/secret.spec"

rpmbuild --define "_topdir $TOP" -bb "$TOP/SPECS/secret.spec"

mkdir -p "$DIST_DIR"
find "$TOP/RPMS" -name '*.rpm' -exec cp -f {} "$DIST_DIR/" \;
echo "rpm: built artifacts in $DIST_DIR"
ls -1 "$DIST_DIR"/*.rpm 2>/dev/null || true
