#!/bin/sh
# common.sh — shared metadata and helpers for secret's native package
# recipes (deb / rpm / apk / macOS pkg). Sourced by each build.sh.
#
# Every recipe stages files with `make -f Makefile.in install-dist`
# (DESTDIR + prefix), then wraps the staged tree with the platform's
# packager. Output lands in dist/ at the repo root.
set -eu

# Resolve the repo root from this file's location (packaging/common.sh).
COMMON_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$COMMON_DIR" in
	*/packaging/*) REPO_ROOT=$(CDPATH= cd -- "$COMMON_DIR/../.." && pwd) ;;
	*/packaging)   REPO_ROOT=$(CDPATH= cd -- "$COMMON_DIR/.." && pwd) ;;
	*)             REPO_ROOT=$(CDPATH= cd -- "$COMMON_DIR" && pwd) ;;
esac

PKG_NAME=secret
PKG_VERSION=$(cat "$REPO_ROOT/VERSION")
PKG_MAINTAINER="René Driessel <rene@driessel.de>"
PKG_LICENSE="GPL-3.0-or-later"
PKG_HOMEPAGE="https://github.com/nostra124/secret"
PKG_SUMMARY="Multi-store GPG-encrypted secret manager"
DIST_DIR="$REPO_ROOT/dist"

export REPO_ROOT PKG_NAME PKG_VERSION PKG_MAINTAINER PKG_LICENSE \
       PKG_HOMEPAGE PKG_SUMMARY DIST_DIR

# Stage an install into $1 with prefix $2 (default /usr).
stage_install() {
	_dest=$1
	_prefix=${2:-/usr}
	make -C "$REPO_ROOT" -f Makefile.in install-dist \
	     DESTDIR="$_dest" prefix="$_prefix" >/dev/null
}

# Create a reproducible source tarball ($1 = output path) with a
# name-version/ prefix from the committed tree.
source_tarball() {
	git -C "$REPO_ROOT" archive --format=tar.gz \
	    --prefix="${PKG_NAME}-${PKG_VERSION}/" -o "$1" HEAD
}
