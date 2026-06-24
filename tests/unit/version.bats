#!/usr/bin/env bats
#
# Unit: the version-string resolution fallback chain (lib/config.c) —
# VERSION, then .rpk/version, then share/<self>/version, then the
# hardcoded default. Exercised by relocating a copy of the binary into a
# layout missing the higher-priority files. No external tools needed.

bats_require_minimum_version 1.5.0

setup() {
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	WORK="$(mktemp -d "$BATS_TMPDIR/ver.XXXXXX")"
	mkdir -p "$WORK/bin"
	cp "$SECRET_BIN" "$WORK/bin/secret"
}
teardown() { rm -rf "$WORK"; }

@test "version falls back to .rpk/version when VERSION is absent" {
	mkdir -p "$WORK/.rpk"; echo "9.9.9" > "$WORK/.rpk/version"
	run "$WORK/bin/secret" version
	[ "$output" = "9.9.9" ]
}

@test "version falls back to share/<self>/version" {
	mkdir -p "$WORK/share/secret"; echo "7.7.7" > "$WORK/share/secret/version"
	run "$WORK/bin/secret" version
	[ "$output" = "7.7.7" ]
}

@test "version uses the hardcoded default when no version file exists" {
	run "$WORK/bin/secret" version
	[ "$output" = "0.8" ]
}
