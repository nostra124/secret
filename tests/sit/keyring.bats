#!/usr/bin/env bats
#
# SIT: the GNOME keyring provider's namespaced export/import round-trip
# (lib/source_keyring.c), driven against a real headless
# gnome-keyring-daemon. Complements the foreign --query import covered in
# tests/unit/transfer.bats.

bats_require_minimum_version 1.5.0

setup() {
	for t in gpg secret-tool gnome-keyring-daemon dbus-run-session; do
		command -v "$t" >/dev/null || skip "$t not installed"
	done
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/stores.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"; chmod 700 "$GNUPGHOME"
	unset XDG_CONFIG_HOME
	export HOME XDG_SECRET_STORES="$SELF_CONFIG" SELF_QUIET=1
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	ID="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$ID" default default 2>/dev/null
	"$SECRET_BIN" -q init vault
	printf 'tok-123'         | "$SECRET_BIN" -q set vault/api:key
	printf 'multi\nline\npw' | "$SECRET_BIN" -q set vault/db:pass
}
teardown() { rm -rf "$SELF_CONFIG" "$HOME" "$GNUPGHOME"; }

@test "export to keyring then import round-trips namespaced items" {
	out="$(dbus-run-session -- bash -c '
		printf kp | gnome-keyring-daemon --unlock --daemonize --components=secrets >/dev/null 2>&1
		sleep 1
		"'"$SECRET_BIN"'" export keyring vault >/dev/null 2>&1
		# wipe the on-disk values, then import them back from the keyring
		"'"$SECRET_BIN"'" -q destroy vault
		"'"$SECRET_BIN"'" -q init vault >/dev/null 2>&1
		"'"$SECRET_BIN"'" import keyring vault >/dev/null 2>&1
		printf "%s|%s" "$("'"$SECRET_BIN"'" -q get vault/api:key)" \
		               "$("'"$SECRET_BIN"'" -q get vault/db:pass)"
	')"
	[ "$out" = $'tok-123|multi\nline\npw' ]
}

@test "sources reports keyring available when secret-tool is present" {
	run "$SECRET_BIN" sources
	[ "$status" -eq 0 ]
	echo "$output" | grep -E '^keyring	available'
}
