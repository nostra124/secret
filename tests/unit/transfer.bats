#!/usr/bin/env bats
#
# Unit tests for the import / export / sources commands (the source
# provider plugin system). The deterministic cases here need no real
# backend: argument validation, unknown-source handling, provider
# listing, external-plugin discovery, and the store-existence guard are
# all exercised with a throwaway `secret-source-*` stub on $PATH.
#
# The full encrypt/decrypt round-trip through a provider needs gpg and
# is gated behind $SECRET_SIT (it generates a throwaway key), so the
# normal unit run stays fast and tool-free. The keyring provider's live
# round-trip belongs to the SIT suite (it needs a running Secret
# Service); see packaging/README and the PR notes.

bats_require_minimum_version 1.5.0

setup() {
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/secret-stores.XXXXXX")"
	SAFE_CWD="$(mktemp -d "$BATS_TMPDIR/cwd.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	PLUGDIR="$(mktemp -d "$BATS_TMPDIR/plugins.XXXXXX")"
	unset XDG_CACHE_HOME XDG_CONFIG_HOME XDG_DATA_HOME XDG_SHARE_HOME
	unset XDG_SOURCE_HOME XDG_BACKUP_HOME XDG_RUNTIME_DIR
	export HOME
	export XDG_SECRET_STORES="$SELF_CONFIG"
	export SELF_QUIET=1
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"

	# A toy external source plugin, always "available", that persists
	# records (param<TAB>base64) in $DEMO_FILE and records any forwarded
	# extra args in $ARGS_FILE.
	export DEMO_FILE="$PLUGDIR/backend.tsv"
	export ARGS_FILE="$PLUGDIR/args.txt"
	cat > "$PLUGDIR/secret-source-faketest" <<'PLUGIN'
#!/bin/sh
case "$1" in
  available) exit 0 ;;
  export)    shift 2; printf '%s' "$*" > "$ARGS_FILE"; cat > "$DEMO_FILE" ;;
  import)    shift 2; printf '%s' "$*" > "$ARGS_FILE"; [ -f "$DEMO_FILE" ] && cat "$DEMO_FILE" ;;
  *) exit 2 ;;
esac
PLUGIN
	chmod +x "$PLUGDIR/secret-source-faketest"
	export PATH="$PLUGDIR:$PATH"
	cd "$SAFE_CWD"
}

teardown() {
	cd "$BATS_TEST_DIRNAME"
	rm -rf "$SELF_CONFIG" "$SAFE_CWD" "$HOME" "$PLUGDIR"
}

# ---------------------------------------------------------------------------
# sources
# ---------------------------------------------------------------------------

@test "sources lists the built-in keyring provider" {
	run "$SECRET_BIN" sources
	[ "$status" -eq 0 ]
	[[ "$output" == *"keyring"* ]]
}

@test "sources prints name and availability columns" {
	run "$SECRET_BIN" sources
	[ "$status" -eq 0 ]
	# keyring line is either available or unavailable depending on host.
	echo "$output" | grep -E '^keyring	(available|unavailable)$'
}

@test "sources discovers an external secret-source-* on PATH" {
	run "$SECRET_BIN" sources
	[ "$status" -eq 0 ]
	[[ "$output" == *"faketest"* ]]
	# the stub always reports available
	echo "$output" | grep -E '^faketest	available$'
}

@test "sources lists the bundled keepass plugin (from libexec)" {
	# Discovered relative to the binary at ../libexec/secret/sources/;
	# availability depends on whether python3 has pykeepass.
	run "$SECRET_BIN" sources
	[ "$status" -eq 0 ]
	echo "$output" | grep -E '^keepass	(available|unavailable)$'
}

@test "external plugin receives forwarded extra args" {
	mkdir -p "$SELF_CONFIG/estore"
	run "$SECRET_BIN" export faketest estore --db /tmp/x.kdbx --group g
	[ "$status" -eq 0 ]
	[ "$(cat "$ARGS_FILE")" = "--db /tmp/x.kdbx --group g" ]
}

# ---------------------------------------------------------------------------
# argument validation
# ---------------------------------------------------------------------------

@test "export without args exits non-zero" {
	run "$SECRET_BIN" export
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a source"* ]]
}

@test "export with only a source exits non-zero" {
	run "$SECRET_BIN" export keyring
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

@test "import without args exits non-zero" {
	run "$SECRET_BIN" import
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a source"* ]]
}

@test "import with only a source exits non-zero" {
	run "$SECRET_BIN" import keyring
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

@test "unknown source exits non-zero with fatal" {
	run "$SECRET_BIN" import definitely-not-a-source mystore
	[ "$status" -ne 0 ]
	[[ "$output" == *"fatal"* ]]
	[[ "$output" == *"unknown source"* ]]
}

@test "export rejects '..' in the store name (FEAT-207)" {
	run "$SECRET_BIN" export faketest ../escape
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

# ---------------------------------------------------------------------------
# store-existence guard (provider available, store missing)
# ---------------------------------------------------------------------------

@test "export to an available source fails when the store is missing" {
	run "$SECRET_BIN" export faketest nostore
	[ "$status" -ne 0 ]
	[[ "$output" == *"store nostore does not exist"* ]]
}

@test "import from an available source fails when the store is missing" {
	run "$SECRET_BIN" import faketest nostore
	[ "$status" -ne 0 ]
	[[ "$output" == *"store nostore does not exist"* ]]
}

# ---------------------------------------------------------------------------
# Full encrypt round-trip through an external provider (gpg-gated).
# ---------------------------------------------------------------------------

@test "export then import round-trips a store through an external provider (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 (needs gpg) to run"
	command -v gpg >/dev/null || skip "gpg not installed"

	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"
	chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null

	"$SECRET_BIN" -q init vault
	printf 'tok-123'         | "$SECRET_BIN" -q set vault/api:key
	printf 'multi\nline\npw' | "$SECRET_BIN" -q set vault/db:pass

	"$SECRET_BIN" -q export faketest vault
	"$SECRET_BIN" -q destroy vault
	"$SECRET_BIN" -q init vault
	"$SECRET_BIN" -q import faketest vault

	run "$SECRET_BIN" -q get vault/api:key
	[ "$output" = "tok-123" ]
	run "$SECRET_BIN" -q get vault/db:pass
	[ "$output" = $'multi\nline\npw' ]
}

# ---------------------------------------------------------------------------
# KeePass plugin round-trip (gpg + a python with pykeepass), SIT-gated.
# ---------------------------------------------------------------------------

@test "keepass plugin round-trips a store (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	command -v gpg >/dev/null || skip "gpg not installed"
	# Find a python interpreter that can import pykeepass.
	PYK=""
	for p in python3 python3.12 python3.11; do
		command -v "$p" >/dev/null || continue
		if "$p" -c "import pykeepass" 2>/dev/null; then PYK="$p"; break; fi
	done
	[ -n "$PYK" ] || skip "no python with pykeepass"

	# Shim so the plugin's `#!/usr/bin/env python3` uses the working one.
	SHIM="$(mktemp -d "$BATS_TMPDIR/shim.XXXXXX")"
	ln -sf "$(command -v "$PYK")" "$SHIM/python3"
	export PATH="$SHIM:$PATH"
	export KEEPASS_DB="$BATS_TMPDIR/kp.$$.kdbx" KEEPASS_PASSWORD="dbpw"
	rm -f "$KEEPASS_DB"

	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"
	chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null

	"$SECRET_BIN" -q init vault
	printf 'tok-123'         | "$SECRET_BIN" -q set vault/api:key
	printf 'multi\nline\npw' | "$SECRET_BIN" -q set vault/db:pass

	"$SECRET_BIN" -q export keepass vault
	"$SECRET_BIN" -q destroy vault
	"$SECRET_BIN" -q init vault
	"$SECRET_BIN" -q import keepass vault

	run "$SECRET_BIN" -q get vault/api:key
	[ "$output" = "tok-123" ]
	run "$SECRET_BIN" -q get vault/db:pass
	[ "$output" = $'multi\nline\npw' ]
	rm -rf "$SHIM" "$KEEPASS_DB"
}

# ---------------------------------------------------------------------------
# Foreign keyring import via --query (needs a live Secret Service), SIT-gated.
# ---------------------------------------------------------------------------

@test "keyring --query imports foreign items with sanitised labels (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	for t in gpg secret-tool gnome-keyring-daemon dbus-run-session; do
		command -v "$t" >/dev/null || skip "$t not installed"
	done

	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"
	chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null
	"$SECRET_BIN" -q init vault

	out="$(dbus-run-session -- bash -c '
		printf kp | gnome-keyring-daemon --unlock --daemonize --components=secrets >/dev/null 2>&1
		sleep 1
		printf "github-token-xyz" | secret-tool store --label "GitHub API Token" app mybrowser service github
		"'"$SECRET_BIN"'" import keyring vault --query app=mybrowser --prefix imported >/dev/null 2>&1
		"'"$SECRET_BIN"'" -q get "vault/imported:github-api-token"
	')"
	[ "$out" = "github-token-xyz" ]
}
