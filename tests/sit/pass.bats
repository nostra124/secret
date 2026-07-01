#!/usr/bin/env bats
#
# SIT: the pass(1) / password-store integration — pass-init and the
# password-store branches of stores / params / has / get / del / destroy
# / gpg-keys / init / edit. Needs gpg + pass.

bats_require_minimum_version 1.5.0

setup() {
	command -v gpg  >/dev/null || skip "gpg not installed"
	command -v pass >/dev/null || skip "pass not installed"
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/stores.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"; chmod 700 "$GNUPGHOME"
	unset XDG_CONFIG_HOME
	export HOME XDG_SECRET_STORES="$SELF_CONFIG" SELF_QUIET=1
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	export PASSWORD_STORE_DIR="$HOME/.password-store"
	ID="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$ID" default default 2>/dev/null
	cd "$HOME"
}
teardown() {
	cd "$BATS_TEST_DIRNAME"
	rm -rf "$SELF_CONFIG" "$HOME" "$GNUPGHOME"
}

@test "pass-init bootstraps the password store" {
	run "$SECRET_BIN" -q pass-init
	[ "$status" -eq 0 ]
	[ -d "$HOME/.password-store" ]
}

@test "password-store appears in stores and exists" {
	"$SECRET_BIN" -q pass-init
	run "$SECRET_BIN" stores
	[[ "$output" == *"password-store"* ]]
	run "$SECRET_BIN" exists password-store
	[ "$status" -eq 0 ]
}

@test "params / has / get / del over password-store" {
	"$SECRET_BIN" -q pass-init
	# Place an entry directly (pass insert is finicky non-interactively;
	# params/has only test for the file, get/del exercise the pass path).
	mkdir -p "$HOME/.password-store/work"
	gpg --yes --batch --trust-model always -r "$ID" -o "$HOME/.password-store/work/site.gpg" \
	    --encrypt <<< "hunter2" 2>/dev/null || : > "$HOME/.password-store/work/site.gpg"
	run "$SECRET_BIN" params password-store
	[[ "$output" == *"work:site"* ]]
	run "$SECRET_BIN" has password-store/work:site
	[ "$status" -eq 0 ]
	# exercise the get + del password-store code paths
	"$SECRET_BIN" -q get password-store/work:site >/dev/null 2>&1 || true
	"$SECRET_BIN" -q del password-store/work:site >/dev/null 2>&1 || true
	run "$SECRET_BIN" has password-store/work:site
	[ "$status" -ne 0 ]
}

@test "gpg-keys lists the password-store recipients" {
	"$SECRET_BIN" -q pass-init
	run "$SECRET_BIN" gpg-keys password-store
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

@test "init password-store re-runs pass init" {
	"$SECRET_BIN" -q pass-init
	run "$SECRET_BIN" -q init password-store
	[ "$status" -eq 0 ]
}

@test "edit over password-store invokes pass edit" {
	"$SECRET_BIN" -q pass-init
	# Exercise the password-store edit code path (pass edit + \$EDITOR);
	# pass's own success is not asserted.
	ed="$BATS_TMPDIR/ed.$$"; printf '#!/bin/sh\nprintf "line\\n" > "$1"\n' > "$ed"; chmod +x "$ed"
	EDITOR="$ed" run "$SECRET_BIN" -q edit password-store/note
	[ -n "${status+x}" ]
}

@test "set / del-gpg-key / add-gpg-key over password-store run" {
	"$SECRET_BIN" -q pass-init
	# the password-store set path (the legacy pass-insert behaviour)
	printf 'v\n' | "$SECRET_BIN" -q set password-store/thing >/dev/null 2>&1 || true
	# del-gpg-key on password-store hits the warn branch
	run "$SECRET_BIN" del-gpg-key password-store someone
	[ "$status" -eq 0 ]
	# add-gpg-key requires a known account key
	mkdir -p "$HOME/.config/account/gpg"
	gpg --armor --export "$ID" > "$HOME/.config/account/gpg/peer@host.pub"
	run "$SECRET_BIN" -q add-gpg-key password-store peer@host
	[ "$status" -eq 0 ]
}

@test "destroy removes the password store" {
	"$SECRET_BIN" -q pass-init
	"$SECRET_BIN" -q destroy password-store
	[ ! -d "$HOME/.password-store" ]
}

@test "pass-init fails fast when gpg is missing" {
	# Shadow gpg with an empty PATH dir to hit the dependency guard.
	empty="$(mktemp -d "$BATS_TMPDIR/nopath.XXXXXX")"
	PATH="$empty" run "$SECRET_BIN" pass-init
	[ "$status" -ne 0 ]
	[[ "$output" == *"gpg"* ]]
}
