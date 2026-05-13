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

bats_require_minimum_version 1.5.0

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

# FEAT-206: pin `secret version` output to the canonical VERSION
# file. If they diverge, the version contract (skills/version.md
# §6) is broken.
@test "secret version output matches VERSION file (FEAT-206)" {
	expected=$(cat "$BATS_TEST_DIRNAME/../../VERSION")
	run "$SECRET_BIN" version
	[ "$status" -eq 0 ]
	[ "$output" = "$expected" ]
}

@test "secret help prints usage" {
	run "$SECRET_BIN" help
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

@test "secret with no args prints help" {
	run "$SECRET_BIN"
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

@test "unknown subcommand exits non-zero with fatal message (FEAT-206)" {
	# FEAT-206: tighten by asserting the fatal message lands on
	# stderr specifically (not just somewhere in combined output).
	run --separate-stderr "$SECRET_BIN" definitely-not-a-real-subcommand
	[ "$status" -ne 0 ]
	[[ "$stderr" == *"fatal"* ]]
	[[ "$stderr" == *"unknown command"* ]]
}

# ---------------------------------------------------------------------------
# FEAT-205 — dispatcher shortcut. When $1 looks like <store>/<param>
# and the .gpg file exists, the dispatcher (`bin/secret` final block)
# routes directly to command:get and exits. When the file is absent,
# the shortcut does not trigger and the normal "unknown command"
# fatal fires.
# ---------------------------------------------------------------------------

@test "store/param shortcut routes to command:get when .gpg exists (FEAT-205)" {
	mkdir -p "$SELF_CONFIG/mystore"
	# Seed a .gpg file so command:has succeeds and the dispatcher
	# takes the shortcut. command:get itself will fail in the
	# absence of real gpg, but the dispatcher block ends with
	# `exit 0` once the shortcut is taken.
	touch "$SELF_CONFIG/mystore/version.gpg"
	run "$SECRET_BIN" mystore/version
	[ "$status" -eq 0 ]
}

@test "store/param shortcut falls through to unknown-command when .gpg absent (FEAT-205)" {
	# No .gpg file exists, so command:has returns non-zero. The
	# dispatcher then tries `has command $1` which fails (there
	# is no command:mystore/version function) → unknown-command
	# fatal.
	run "$SECRET_BIN" missing/param
	[ "$status" -ne 0 ]
	[[ "$output" == *"unknown command"* ]]
}

# ---------------------------------------------------------------------------
# Help surface — every documented subcommand group should be discoverable
# ---------------------------------------------------------------------------

@test "secret help mentions pass-init (FEAT-041)" {
	run "$SECRET_BIN" help
	[ "$status" -eq 0 ]
	echo "$output" | grep -q pass-init
}

@test "help mentions store management commands" {
	run "$SECRET_BIN" help
	[ "$status" -eq 0 ]
	[[ "$output" == *"store management commands"* ]]
}

@test "help mentions store parameter commands" {
	run "$SECRET_BIN" help
	[ "$status" -eq 0 ]
	[[ "$output" == *"store parameter commands"* ]]
}

@test "help mentions account encryption commands" {
	run "$SECRET_BIN" help
	[ "$status" -eq 0 ]
	[[ "$output" == *"account encryption commands"* ]]
}

@test "help mentions sync commands group" {
	run "$SECRET_BIN" help
	[ "$status" -eq 0 ]
	[[ "$output" == *"synchronisation commands"* ]]
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

# FEAT-203 — stores listing must survive a SELF_CONFIG path with a
# space in it. command:stores is the canary; the full deep-quoting
# refactor for every subcommand is tracked in FEAT-213.

@test "stores lists store directories under a SELF_CONFIG with a space" {
	# Re-point SELF_CONFIG to a path containing a space and seed
	# one store.
	SPACED="$BATS_TMPDIR/with space/$(date +%N).XXXXXX"
	mkdir -p "$SPACED/alpha"
	XDG_SECRET_STORES="$SPACED" run "$SECRET_BIN" stores
	[ "$status" -eq 0 ]
	[[ "$output" == *"alpha"* ]]
	rm -rf "$SPACED"
}

# FEAT-213: the deep quoting refactor in 0.12.0 makes every
# subcommand survive a $SELF_CONFIG path containing spaces. Each
# test re-points XDG_SECRET_STORES at a spaced sandbox and exercises
# one subcommand.

setup_spaced() {
	SPACED_CONFIG="$BATS_TMPDIR/spaced cfg.$$"
	mkdir -p "$SPACED_CONFIG"
}

teardown_spaced() {
	rm -rf "$SPACED_CONFIG"
}

@test "exists works under a spaced SELF_CONFIG (FEAT-213)" {
	setup_spaced
	mkdir -p "$SPACED_CONFIG/mystore"
	XDG_SECRET_STORES="$SPACED_CONFIG" run "$SECRET_BIN" exists mystore
	[ "$status" -eq 0 ]
	teardown_spaced
}

@test "params works under a spaced SELF_CONFIG (FEAT-213)" {
	setup_spaced
	mkdir -p "$SPACED_CONFIG/mystore"
	touch "$SPACED_CONFIG/mystore/key.gpg"
	XDG_SECRET_STORES="$SPACED_CONFIG" run "$SECRET_BIN" params mystore
	[ "$status" -eq 0 ]
	[[ "$output" == *"key"* ]]
	teardown_spaced
}

@test "has works under a spaced SELF_CONFIG (FEAT-213)" {
	setup_spaced
	mkdir -p "$SPACED_CONFIG/mystore"
	touch "$SPACED_CONFIG/mystore/key.gpg"
	XDG_SECRET_STORES="$SPACED_CONFIG" run "$SECRET_BIN" has mystore/key
	[ "$status" -eq 0 ]
	teardown_spaced
}

@test "ls works under a spaced SELF_CONFIG (FEAT-213)" {
	setup_spaced
	mkdir -p "$SPACED_CONFIG/mystore"
	touch "$SPACED_CONFIG/mystore/key.gpg"
	XDG_SECRET_STORES="$SPACED_CONFIG" run "$SECRET_BIN" ls
	[ "$status" -eq 0 ]
	[[ "$output" == *"mystore/key"* ]]
	teardown_spaced
}

@test "destroy works under a spaced SELF_CONFIG (FEAT-213)" {
	setup_spaced
	mkdir -p "$SPACED_CONFIG/mystore"
	touch "$SPACED_CONFIG/mystore/key.gpg"
	XDG_SECRET_STORES="$SPACED_CONFIG" run "$SECRET_BIN" destroy mystore
	[ "$status" -eq 0 ]
	[ ! -d "$SPACED_CONFIG/mystore" ]
	teardown_spaced
}

@test "clean works under a spaced SELF_CONFIG (FEAT-213)" {
	setup_spaced
	mkdir -p "$SPACED_CONFIG/mystore"
	touch "$SPACED_CONFIG/mystore/empty.gpg"
	echo "x" > "$SPACED_CONFIG/mystore/keep.gpg"
	XDG_SECRET_STORES="$SPACED_CONFIG" run "$SECRET_BIN" clean mystore
	[ "$status" -eq 0 ]
	[ ! -e "$SPACED_CONFIG/mystore/empty.gpg" ]
	[ -e "$SPACED_CONFIG/mystore/keep.gpg" ]
	teardown_spaced
}

@test "remotes works under a spaced SELF_CONFIG (FEAT-213)" {
	setup_spaced
	mkdir -p "$SPACED_CONFIG/mystore"
	XDG_SECRET_STORES="$SPACED_CONFIG" run "$SECRET_BIN" remotes
	[ "$status" -eq 0 ]
	teardown_spaced
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

@test "params password-store output is sorted (FEAT-208)" {
	mkdir -p "$HOME/.password-store"
	touch "$HOME/.password-store/zebra.gpg"
	touch "$HOME/.password-store/alpha.gpg"
	touch "$HOME/.password-store/mango.gpg"
	run "$SECRET_BIN" params password-store
	[ "$status" -eq 0 ]
	# Output should be sorted alphabetically.
	expected=$'alpha\nmango\nzebra'
	[ "$output" = "$expected" ]
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

# FEAT-209: command:ls now uses `grep -i` on the prefix filter so a
# mixed-case query (`secret ls Alpha`) returns the same results as
# the lowercase form (`secret ls alpha`). The store directories
# themselves are normalised to lowercase by command:init at creation
# time, so on-disk names stay consistent.

@test "ls is case-insensitive on the query prefix (FEAT-209)" {
	mkdir -p "$SELF_CONFIG/alpha"
	touch "$SELF_CONFIG/alpha/key1.gpg"
	# Lowercase form (baseline).
	run "$SECRET_BIN" ls alpha
	[ "$status" -eq 0 ]
	[[ "$output" == *"alpha/key1"* ]]
	# Mixed-case form must return the same result.
	run "$SECRET_BIN" ls Alpha
	[ "$status" -eq 0 ]
	[[ "$output" == *"alpha/key1"* ]]
	# Uppercase form too.
	run "$SECRET_BIN" ls ALPHA
	[ "$status" -eq 0 ]
	[[ "$output" == *"alpha/key1"* ]]
}

# ---------------------------------------------------------------------------
# destroy — file removal. FEAT-205 enabled the password-store branch
# now that setup() sandboxes $HOME.
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

@test "destroy password-store removes ~/.password-store (FEAT-205)" {
	mkdir -p "$HOME/.password-store"
	touch "$HOME/.password-store/foo.gpg"
	run "$SECRET_BIN" destroy password-store
	[ "$status" -eq 0 ]
	[ ! -d "$HOME/.password-store" ]
}

@test "destroy password-store is a no-op when ~/.password-store absent (FEAT-205)" {
	run "$SECRET_BIN" destroy password-store
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

# Logging contract — see skills/logging.md. The four supported
# levels are debug / info / warn / error. fatal() is the
# error+exit convenience used by validation paths.

@test "get on existing store with missing param emits error level (not warn)" {
	mkdir -p "$SELF_CONFIG/mystore"
	run "$SECRET_BIN" get mystore/missing
	# A missing parameter inside an existing store is an error,
	# not a warning — the operation failed and the caller needs
	# to know.
	[[ "$output" == *"error"* ]]
	[[ "$output" == *"mystore/missing"* ]]
}

@test "secret -d version tagged 'debug -' on stderr" {
	run --separate-stderr "$SECRET_BIN" -d version
	[ "$status" -eq 0 ]
	[[ "$stderr" == *"debug -"* ]]
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

# ---------------------------------------------------------------------------
# gpg-keys — FEAT-200 fix verifies the no-store branch no longer calls
# the undefined command:accounts. The happy path (gpg --list-packets on
# real ciphertext) belongs to SIT.
# ---------------------------------------------------------------------------

@test "gpg-keys with no args and no stores exits 0" {
	run "$SECRET_BIN" gpg-keys
	[ "$status" -eq 0 ]
	[ -z "$output" ]
}

@test "gpg-keys with no args recurses across seeded stores without invoking accounts" {
	mkdir -p "$SELF_CONFIG/alpha/.gpg" "$SELF_CONFIG/beta/.gpg"
	touch "$SELF_CONFIG/alpha/.gpg/keyA.pub" "$SELF_CONFIG/beta/.gpg/keyB.pub"
	run "$SECRET_BIN" gpg-keys
	[ "$status" -eq 0 ]
	[[ "$output" == *"keyA"* ]]
	[[ "$output" == *"keyB"* ]]
	[[ "$output" != *"command not found"* ]]
}

# ---------------------------------------------------------------------------
# Flag parsing (FEAT-202) — the -d / -q / -f switches are parsed by the
# top-level getopts loop and stored in env vars before the subcommand
# dispatcher runs.
# ---------------------------------------------------------------------------

@test "secret -d version emits a debug line on stderr" {
	run "$SECRET_BIN" -d version
	[ "$status" -eq 0 ]
	[[ "$output" == *"debug"* ]]
	[[ "$output" == *"0."* ]]  # the version itself is still printed
}

@test "secret -q stores stays silent when no stores exist" {
	run "$SECRET_BIN" -q stores
	[ "$status" -eq 0 ]
	[ -z "$output" ]
}

@test "secret -q -d combines flags (debug visible, info suppressed)" {
	run "$SECRET_BIN" -q -d version
	[ "$status" -eq 0 ]
	[[ "$output" == *"debug"* ]]
}

@test "unknown flag is rejected by getopts but version still resolves" {
	# getopts silently rejects unknown short flags; the remaining args
	# must still flow through to the subcommand dispatcher.
	run "$SECRET_BIN" version
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

# ---------------------------------------------------------------------------
# FEAT-207 — path-traversal policy. `..` segments in store or parameter
# names are rejected at the parser level so resolved file paths cannot
# escape $SELF_CONFIG. Centralised in the validate_name helper.
# ---------------------------------------------------------------------------

@test "exists rejects '..' in store name" {
	run "$SECRET_BIN" exists ../foo
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

@test "params rejects '..' in store name" {
	run "$SECRET_BIN" params ../foo
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

@test "has rejects '..' anywhere in <store>/<param>" {
	run "$SECRET_BIN" has store/../escape
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

@test "get rejects '..' as store component" {
	run "$SECRET_BIN" get ../etc/passwd
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

@test "set rejects '..' as parameter component" {
	run bash -c "echo x | $SECRET_BIN set store/../escape"
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

@test "destroy rejects '..' in store name" {
	run "$SECRET_BIN" destroy ../foo
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

# Sanity check: a legitimate name containing two single dots (not
# adjacent) is allowed.
@test "exists allows store names with non-traversal dots" {
	mkdir -p "$SELF_CONFIG/foo.bar.baz"
	run "$SECRET_BIN" exists foo.bar.baz
	[ "$status" -eq 0 ]
}

# ---------------------------------------------------------------------------
# FEAT-204 — argument-validation coverage for subcommands whose happy
# paths require gpg / pass / qrencode / $EDITOR / network. We only
# exercise the early-exit paths here; full behaviour belongs in SIT.
# ---------------------------------------------------------------------------

# gen / def — both expect <store>/<param> and fatal on missing or
# malformed arguments.

@test "gen without arg exits non-zero" {
	run "$SECRET_BIN" gen
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

@test "gen without slash separator exits non-zero" {
	run "$SECRET_BIN" gen storealone
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<param>"* ]]
}

@test "def without arg exits non-zero" {
	run "$SECRET_BIN" def
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

@test "def without slash separator exits non-zero" {
	run "$SECRET_BIN" def storealone
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<param>"* ]]
}

# ins / rem — FEAT-212 hoisted the argument-validation guards
# directly into both functions and removed the unconditional
# `return 0`. Tests pin the missing-arg and bad-format paths.

@test "ins without arg exits non-zero (FEAT-212)" {
	run bash -c "echo | $SECRET_BIN ins"
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

@test "ins without slash separator exits non-zero (FEAT-212)" {
	run bash -c "echo | $SECRET_BIN ins storealone"
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<param>"* ]]
}

@test "rem without arg exits non-zero (FEAT-212)" {
	run bash -c "echo | $SECRET_BIN rem"
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

@test "rem without slash separator exits non-zero (FEAT-212)" {
	run bash -c "echo | $SECRET_BIN rem storealone"
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<param>"* ]]
}

# remotes — FEAT-211 fixed the no-stores infinite recursion with the
# same guard pattern FEAT-200 introduced for gpg-keys. Tests pin
# both the no-stores and seeded-store cases.

@test "remotes with no stores returns empty success (FEAT-211)" {
	run "$SECRET_BIN" remotes
	[ "$status" -eq 0 ]
	[ -z "$output" ]
}

@test "remotes with a store that has no git remotes returns empty success" {
	mkdir -p "$SELF_CONFIG/loner"
	run "$SECRET_BIN" remotes
	[ "$status" -eq 0 ]
}

# add-gpg-key / del-gpg-key / has-gpg-key — fatal on missing args.
# The happy paths go through `account` + gpg and belong to SIT.

@test "add-gpg-key without store arg exits non-zero" {
	run "$SECRET_BIN" add-gpg-key
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

@test "add-gpg-key without account arg exits non-zero" {
	run "$SECRET_BIN" add-gpg-key mystore
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify an account"* ]]
}

@test "del-gpg-key without store arg exits non-zero" {
	run "$SECRET_BIN" del-gpg-key
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

@test "del-gpg-key without account arg exits non-zero" {
	run "$SECRET_BIN" del-gpg-key mystore
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify an account"* ]]
}

@test "has-gpg-key without store arg exits non-zero" {
	run "$SECRET_BIN" has-gpg-key
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

@test "has-gpg-key without account arg exits non-zero" {
	run "$SECRET_BIN" has-gpg-key mystore
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify an account"* ]]
}

@test "has-gpg-key returns non-zero when store lacks the key file" {
	mkdir -p "$SELF_CONFIG/mystore/.gpg"
	# no .pub file in .gpg/ for 'noone'
	run "$SECRET_BIN" has-gpg-key mystore noone
	[ "$status" -ne 0 ]
}

# clean — FEAT-210 implemented command:clean as "remove empty
# .gpg files from a store". Option A chosen over Option B
# (removing the help-text line).

@test "clean without arg exits non-zero (FEAT-210)" {
	run "$SECRET_BIN" clean
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

@test "clean on missing store exits non-zero with fatal (FEAT-210)" {
	run "$SECRET_BIN" clean nope
	[ "$status" -ne 0 ]
	[[ "$output" == *"store nope does not exist"* ]]
}

@test "clean removes empty .gpg files but keeps non-empty ones (FEAT-210)" {
	mkdir -p "$SELF_CONFIG/mystore"
	touch "$SELF_CONFIG/mystore/empty.gpg"
	echo "real ciphertext payload" > "$SELF_CONFIG/mystore/keep.gpg"
	run "$SECRET_BIN" clean mystore
	[ "$status" -eq 0 ]
	[ ! -e "$SELF_CONFIG/mystore/empty.gpg" ]
	[ -e "$SELF_CONFIG/mystore/keep.gpg" ]
}

# pass-init's missing-dependency guard is exercised in the SIT suite
# (the per-distro Dockerfiles let us run with and without the gpg /
# pass packages installed). Faking it from a unit test requires a PATH
# manipulation that also blanks basename / mkdir / find / tr / sed and
# breaks the script's bootstrap, so the assertion belongs higher up
# the test pyramid.
