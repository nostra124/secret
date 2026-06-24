#!/usr/bin/env bats
#
# SIT: assorted reachable branches — clipboard backends + auto-clear,
# group endpoint normalisation + pull-http, the root config path, and
# CLI flag-parsing edges.

bats_require_minimum_version 1.5.0

setup() {
	command -v gpg >/dev/null || skip "gpg not installed"
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/stores.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"; chmod 700 "$GNUPGHOME"
	unset XDG_CONFIG_HOME SECRET_CLIP_CMD SECRET_CLIP_TIME
	export HOME XDG_SECRET_STORES="$SELF_CONFIG" SELF_QUIET=1
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	ID="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$ID" default default 2>/dev/null
	"$SECRET_BIN" -q init vault
	printf 'topsecret' | "$SECRET_BIN" -q set vault/k
}
teardown() { rm -rf "$SELF_CONFIG" "$HOME" "$GNUPGHOME"; }

# ---- clipboard ------------------------------------------------------------

@test "clip via SECRET_CLIP_CMD with auto-clear fork" {
	clip="$BATS_TMPDIR/clip.$$"
	SECRET_CLIP_TIME=1 SECRET_CLIP_CMD="cat > $clip" "$SECRET_BIN" -q clip vault/k
	[ "$(cat "$clip")" = "topsecret" ]
}

@test "clip selects wl-copy under Wayland (and reports copy failure)" {
	command -v wl-copy >/dev/null || skip "wl-copy not installed"
	WAYLAND_DISPLAY=wl-test run "$SECRET_BIN" clip vault/k
	[ "$status" -ne 0 ]
	[[ "$output" == *"clipboard"* ]]
}

@test "clip fatals when SECRET_CLIP_CMD fails" {
	SECRET_CLIP_CMD=false run "$SECRET_BIN" clip vault/k
	[ "$status" -ne 0 ]
	[[ "$output" == *"clipboard copy failed"* ]]
}

@test "clip on a missing parameter errors" {
	run "$SECRET_BIN" clip vault/nope
	[ "$status" -ne 0 ]
	[[ "$output" == *"does not exist"* ]]
}

# ---- groups / pull-http ---------------------------------------------------

@test "pull-http normalises varied endpoint forms" {
	"$SECRET_BIN" group-add g1 https://h.example
	"$SECRET_BIN" group-add g1 host.example:9999
	"$SECRET_BIN" group-add g1 barehost
	# unreachable endpoints: the pull attempt fails, but endpoint_base_url
	# and pull_one execute for each form. Pull from all groups (no name).
	run "$SECRET_BIN" -q pull-http vault
	[ "$status" -eq 0 ]
}

@test "pull-http warns on a group with no endpoints" {
	run "$SECRET_BIN" pull-http vault emptygroup
	[ "$status" -eq 0 ]
	[[ "$output" == *"no endpoints"* ]]
}

# ---- root config path -----------------------------------------------------

@test "config falls back to /etc/secret as root with no XDG override" {
	[ "$(id -u)" -eq 0 ] || skip "not root"
	run env -u XDG_SECRET_STORES "$SECRET_BIN" stores
	[ "$status" -eq 0 ]
	[ -d /etc/secret ]
}

# ---- CLI flag parsing -----------------------------------------------------

@test "-- terminates flag parsing" {
	run "$SECRET_BIN" -- version
	[ "$status" -eq 0 ]
}

@test "unknown short flag is ignored" {
	run "$SECRET_BIN" -z version
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

@test "bundled flags -qd are parsed" {
	run "$SECRET_BIN" -qd version
	[ "$status" -eq 0 ]
	[[ "$output" == *"debug"* ]]
}
