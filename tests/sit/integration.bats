#!/usr/bin/env bats
#
# SIT: integration paths that need real gpg (but not ssh/pass) — the
# recipient (gpg-key) commands, init variants, the gpg-backed parameter
# commands (del / ins / rem / edit / gen / qr value), and help branches.
# These cover the lib/recipients.c, lib/init.c and lib/param.c lines the
# tool-free unit suite cannot reach.

bats_require_minimum_version 1.5.0

setup() {
	command -v gpg >/dev/null || skip "gpg not installed"
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/stores.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"; chmod 700 "$GNUPGHOME"
	unset XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_HOME
	export HOME XDG_SECRET_STORES="$SELF_CONFIG" SELF_QUIET=1
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	ID="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$ID" default default 2>/dev/null
	cd "$SELF_CONFIG"
}
teardown() {
	cd "$BATS_TEST_DIRNAME"
	rm -rf "$SELF_CONFIG" "$HOME" "$GNUPGHOME"
}

# ---- init -----------------------------------------------------------------

@test "init bootstraps a GPG identity when the keyring is empty" {
	# Fresh, empty keyring: `init` (no store) must generate a key.
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/empty.XXXXXX")"; chmod 700 "$GNUPGHOME"
	run "$SECRET_BIN" -q init
	[ "$status" -eq 0 ]
	run "$SECRET_BIN" gpg-keys
	[ "$status" -eq 0 ]
}

@test "init re-initialises an existing store (re-encrypt loop)" {
	"$SECRET_BIN" -q init vault
	printf 'v' | "$SECRET_BIN" -q set vault/k
	run "$SECRET_BIN" -q init vault
	[ "$status" -eq 0 ]
	run "$SECRET_BIN" -q get vault/k
	[ "$output" = "v" ]
}

# ---- recipients -----------------------------------------------------------

@test "gpg-keys <store> <param> extracts the encrypting key uid" {
	"$SECRET_BIN" -q init vault
	printf 'secret' | "$SECRET_BIN" -q set vault/api:key
	run "$SECRET_BIN" gpg-keys vault api:key
	[ "$status" -eq 0 ]
	[[ "$output" == *"$ID"* ]]
}

@test "add-gpg-key / has-gpg-key / del-gpg-key round-trip" {
	"$SECRET_BIN" -q init vault
	printf 'secret' | "$SECRET_BIN" -q set vault/k
	# register a second 'account' public key (reuse our own key material)
	mkdir -p "$HOME/.config/account/gpg"
	gpg --armor --export "$ID" > "$HOME/.config/account/gpg/peer@host.pub"

	run "$SECRET_BIN" has-gpg-key vault peer@host
	[ "$status" -ne 0 ]
	"$SECRET_BIN" -q add-gpg-key vault peer@host
	run "$SECRET_BIN" has-gpg-key vault peer@host
	[ "$status" -eq 0 ]
	"$SECRET_BIN" -q del-gpg-key vault peer@host
	run "$SECRET_BIN" has-gpg-key vault peer@host
	[ "$status" -ne 0 ]
}

# ---- parameter commands (gpg-backed) --------------------------------------

@test "del removes a parameter and commits" {
	"$SECRET_BIN" -q init vault
	printf 'x' | "$SECRET_BIN" -q set vault/gone
	run "$SECRET_BIN" has vault/gone
	[ "$status" -eq 0 ]
	"$SECRET_BIN" -q del vault/gone
	run "$SECRET_BIN" has vault/gone
	[ "$status" -ne 0 ]
}

@test "ins prepends + dedups, rem removes matching lines" {
	"$SECRET_BIN" -q init vault
	printf 'b\na\n' | "$SECRET_BIN" -q set vault/list
	printf 'c\n'    | "$SECRET_BIN" -q ins vault/list
	run "$SECRET_BIN" -q get vault/list
	[[ "$output" == *"a"* ]] && [[ "$output" == *"b"* ]] && [[ "$output" == *"c"* ]]
	printf 'a\n'    | "$SECRET_BIN" -q rem vault/list
	run "$SECRET_BIN" -q get vault/list
	[[ "$output" != *"a"* ]]
	[[ "$output" == *"b"* ]]
}

@test "edit re-encrypts via \$EDITOR" {
	"$SECRET_BIN" -q init vault
	printf 'first\n' | "$SECRET_BIN" -q set vault/note
	ed="$BATS_TMPDIR/ed.$$"; printf '#!/bin/sh\nprintf "appended\\n" >> "$1"\n' > "$ed"; chmod +x "$ed"
	EDITOR="$ed" "$SECRET_BIN" -q edit vault/note
	run "$SECRET_BIN" -q get vault/note
	[[ "$output" == *"appended"* ]]
}

@test "gen creates a 33-char secret then is idempotent" {
	"$SECRET_BIN" -q init vault >/dev/null 2>&1
	run "$SECRET_BIN" -q gen vault/rnd
	[ "${#output}" -eq 33 ]
	first="$output"
	run "$SECRET_BIN" -q gen vault/rnd
	[ "$output" = "$first" ]
}

@test "qr encodes a plain parameter value" {
	command -v qrencode >/dev/null || skip "qrencode not installed"
	"$SECRET_BIN" -q init vault
	printf 'plainvalue' | "$SECRET_BIN" -q set vault/p
	run "$SECRET_BIN" qr vault/p
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

# ---- help branches --------------------------------------------------------

@test "help setup / identity print their topic help" {
	run "$SECRET_BIN" help setup
	[ "$status" -eq 0 ]; [[ "$output" == *"setup"* ]]
	run "$SECRET_BIN" help identity
	[ "$status" -eq 0 ]; [[ "$output" == *"identity"* ]]
}

@test "help for a known-but-helpless command says so" {
	run "$SECRET_BIN" help stores
	[ "$status" -eq 0 ]
	[[ "$output" == *"No help found"* ]]
}

# ---- remotes (git remote enumeration) -------------------------------------

@test "remotes lists a configured git remote" {
	"$SECRET_BIN" -q init vault
	( cd "$SELF_CONFIG/vault" && git remote add peer ssh://peer.example/x )
	run "$SECRET_BIN" remotes
	[ "$status" -eq 0 ]
	[[ "$output" == *"peer"* ]]
}
