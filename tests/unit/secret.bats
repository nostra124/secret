#!/usr/bin/env bats
#
# Unit tests for bin/secret — the foundation secret-management tool
# (FEAT-029..044). Pinned to semver per FEAT-005's foundation API
# guarantee.
#
# Coverage scope: every subcommand whose happy path doesn't require
# real gpg / pass / qrencode / sudo / ssh / network. Subcommands that
# genuinely need those (pass-init, init/set/get for non-password-store
# stores that go through gpg-encrypt / gpg-decrypt, gpg-keys family,
# pull / push / sync) belong to the SIT suite that lands per FEAT-030.
#
# Sandboxing strategy: $XDG_SECRET_STORES is secret's own escape hatch
# (line 64 of bin/secret). When set, $SELF_CONFIG resolves to that
# path verbatim, bypassing the EUID branch entirely.

setup() {
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/secret-stores.XXXXXX")"
	SAFE_CWD="$(mktemp -d "$BATS_TMPDIR/cwd.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	# Reset every XDG_* var that could leak from the runner's
	# environment before pinning $XDG_SECRET_STORES to our sandbox.
	unset XDG_CACHE_HOME XDG_CONFIG_HOME XDG_DATA_HOME XDG_SHARE_HOME
	unset XDG_SOURCE_HOME XDG_BACKUP_HOME XDG_RUNTIME_DIR
	export HOME
	export XDG_SECRET_STORES="$SELF_CONFIG"
	export SELF_QUIET=1
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	cd "$SAFE_CWD"
}

teardown() {
	cd "$BATS_TEST_DIRNAME"
	rm -rf "$SELF_CONFIG" "$SAFE_CWD" "$HOME"
}

# ---------------------------------------------------------------------------
# Smoke
# ---------------------------------------------------------------------------

@test "secret binary exists and is executable" {
	[ -x "$SECRET_BIN" ]
}

@test "secret version returns a non-empty string" {
	run "$SECRET_BIN" version
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

@test "secret help prints usage" {
	run "$SECRET_BIN" help
	[ -n "$output" ]
}

@test "secret with no args prints help" {
	run "$SECRET_BIN"
	[ -n "$output" ]
}

@test "unknown subcommand exits non-zero with fatal message" {
	run "$SECRET_BIN" definitely-not-a-real-subcommand
	[ "$status" -ne 0 ]
	[[ "$output" == *"fatal"* ]]
}

# ---------------------------------------------------------------------------
# Help surface — every documented subcommand group should be discoverable
# ---------------------------------------------------------------------------

@test "secret help mentions pass-init (FEAT-041)" {
	run "$SECRET_BIN" help
	echo "$output" | grep -q pass-init
}

@test "help mentions store management commands" {
	run "$SECRET_BIN" help
	[[ "$output" == *"store management commands"* ]]
}

@test "help mentions store parameter commands" {
	run "$SECRET_BIN" help
	[[ "$output" == *"store parameter commands"* ]]
}

@test "help mentions account encryption commands" {
	run "$SECRET_BIN" help
	[[ "$output" == *"account encryption commands"* ]]
}

@test "help mentions sync commands group" {
	run "$SECRET_BIN" help
	[[ "$output" == *"yncronisation commands"* ]]  # script spells it 'syncronisation'
}

# ---------------------------------------------------------------------------
# Stores
# ---------------------------------------------------------------------------

@test "stores returns empty when no store and no password-store exist" {
	run "$SECRET_BIN" stores
	[ "$status" -eq 0 ]
	[ -z "$output" ]
}

@test "stores lists pre-seeded store directories" {
	mkdir -p "$SELF_CONFIG/alpha" "$SELF_CONFIG/beta"
	run "$SECRET_BIN" stores
	[ "$status" -eq 0 ]
	[[ "$output" == *"alpha"* ]]
	[[ "$output" == *"beta"* ]]
}

@test "stores reports password-store when ~/.password-store exists" {
	mkdir -p "$HOME/.password-store"
	run "$SECRET_BIN" stores
	[ "$status" -eq 0 ]
	[[ "$output" == *"password-store"* ]]
}

# ---------------------------------------------------------------------------
# exists
# ---------------------------------------------------------------------------

@test "exists returns 0 for a present store" {
	mkdir -p "$SELF_CONFIG/mystore"
	run "$SECRET_BIN" exists mystore
	[ "$status" -eq 0 ]
}

@test "exists returns non-zero for a missing store" {
	run "$SECRET_BIN" exists nope
	[ "$status" -ne 0 ]
}

@test "exists lowercases the store argument" {
	mkdir -p "$SELF_CONFIG/mystore"
	run "$SECRET_BIN" exists MyStore
	[ "$status" -eq 0 ]
}

@test "exists special-cases password-store backed by ~/.password-store" {
	mkdir -p "$HOME/.password-store"
	run "$SECRET_BIN" exists password-store
	[ "$status" -eq 0 ]
}

@test "exists without arg exits non-zero" {
	run "$SECRET_BIN" exists
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

# ---------------------------------------------------------------------------
# params
# ---------------------------------------------------------------------------

@test "params returns parameters of a store, .gpg suffix stripped, colons not slashes" {
	mkdir -p "$SELF_CONFIG/mystore/host"
	touch "$SELF_CONFIG/mystore/host/name.gpg"
	touch "$SELF_CONFIG/mystore/version.gpg"
	run "$SECRET_BIN" params mystore
	[ "$status" -eq 0 ]
	[[ "$output" == *"host:name"* ]]
	[[ "$output" == *"version"* ]]
	[[ "$output" != *".gpg"* ]]
	[[ "$output" != *"host/name"* ]]
}

@test "params on missing store returns empty success" {
	run "$SECRET_BIN" params no-such-store
	[ "$status" -eq 0 ]
	[ -z "$output" ]
}

@test "params without arg exits non-zero" {
	run "$SECRET_BIN" params
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

# ---------------------------------------------------------------------------
# has — file-existence check on <store>/<param>.gpg
# ---------------------------------------------------------------------------

@test "has returns 0 when the .gpg param file exists" {
	mkdir -p "$SELF_CONFIG/mystore"
	touch "$SELF_CONFIG/mystore/version.gpg"
	run "$SECRET_BIN" has mystore/version
	[ "$status" -eq 0 ]
}

@test "has returns non-zero when the param is missing" {
	mkdir -p "$SELF_CONFIG/mystore"
	run "$SECRET_BIN" has mystore/version
	[ "$status" -ne 0 ]
}

@test "has accepts colon as path separator (host:name -> host/name.gpg)" {
	mkdir -p "$SELF_CONFIG/mystore/host"
	touch "$SELF_CONFIG/mystore/host/name.gpg"
	run "$SECRET_BIN" has mystore/host:name
	[ "$status" -eq 0 ]
}

@test "has special-cases password-store backed by ~/.password-store" {
	mkdir -p "$HOME/.password-store"
	touch "$HOME/.password-store/work.gpg"
	run "$SECRET_BIN" has password-store/work
	[ "$status" -eq 0 ]
}

@test "has without slash separator returns non-zero" {
	run "$SECRET_BIN" has noseparator
	[ "$status" -ne 0 ]
}

@test "has without arg exits non-zero" {
	run "$SECRET_BIN" has
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

# ---------------------------------------------------------------------------
# ls — flat <store>/<param> listing
# ---------------------------------------------------------------------------

@test "ls returns store/param pairs across stores" {
	mkdir -p "$SELF_CONFIG/alpha" "$SELF_CONFIG/beta"
	touch "$SELF_CONFIG/alpha/key1.gpg" "$SELF_CONFIG/beta/key2.gpg"
	run "$SECRET_BIN" ls
	[ "$status" -eq 0 ]
	[[ "$output" == *"alpha/key1"* ]]
	[[ "$output" == *"beta/key2"* ]]
}

@test "ls with prefix filters by leading store name" {
	mkdir -p "$SELF_CONFIG/alpha" "$SELF_CONFIG/beta"
	touch "$SELF_CONFIG/alpha/key1.gpg" "$SELF_CONFIG/beta/key2.gpg"
	run "$SECRET_BIN" ls alpha
	[ "$status" -eq 0 ]
	[[ "$output" == *"alpha/key1"* ]]
	[[ "$output" != *"beta/key2"* ]]
}

# ---------------------------------------------------------------------------
# destroy — file removal (skip the password-store branch which would
# rm -rf $HOME/.password-store on the dev machine)
# ---------------------------------------------------------------------------

@test "destroy removes the store directory" {
	mkdir -p "$SELF_CONFIG/mystore"
	touch "$SELF_CONFIG/mystore/version.gpg"
	run "$SECRET_BIN" destroy mystore
	[ "$status" -eq 0 ]
	[ ! -d "$SELF_CONFIG/mystore" ]
}

@test "destroy of unknown store is a no-op" {
	run "$SECRET_BIN" destroy never-existed
	[ "$status" -eq 0 ]
}

@test "destroy without arg exits non-zero" {
	run "$SECRET_BIN" destroy
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

# ---------------------------------------------------------------------------
# Early-validation error paths on subcommands whose happy paths need
# external tools (gpg / pass / qrencode / $EDITOR / network)
# ---------------------------------------------------------------------------

@test "set without arg exits non-zero" {
	run "$SECRET_BIN" set
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

@test "set without slash separator exits non-zero" {
	run "$SECRET_BIN" set storealone
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<param>"* ]]
}

@test "get without arg exits non-zero" {
	run "$SECRET_BIN" get
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

@test "get without slash separator exits non-zero" {
	run "$SECRET_BIN" get storealone
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<param>"* ]]
}

@test "get on non-existent store exits non-zero" {
	run "$SECRET_BIN" get nope/version
	[ "$status" -ne 0 ]
	[[ "$output" == *"store nope does not exist"* ]]
}

@test "del without arg exits non-zero" {
	run "$SECRET_BIN" del
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

@test "qr without arg exits non-zero" {
	run "$SECRET_BIN" qr
	[ "$status" -ne 0 ]
}

@test "edit without arg exits non-zero" {
	run "$SECRET_BIN" edit
	[ "$status" -ne 0 ]
}

# pass-init's missing-dependency guard is exercised in the SIT suite
# (the per-distro Dockerfiles let us run with and without the gpg /
# pass packages installed). Faking it from a unit test requires a PATH
# manipulation that also blanks basename / mkdir / find / tr / sed and
# breaks the script's bootstrap, so the assertion belongs higher up
# the test pyramid.
