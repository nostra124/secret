#!/usr/bin/env bats
#
# Unit tests for the structured-entry template system (templates / new /
# otp). The deterministic cases need no gpg: listing, field display, and
# every `new`/`otp` validation path fatals before any store write. The
# full create + TOTP round-trip needs gpg (and oathtool) and is gated
# behind $SECRET_SIT.

bats_require_minimum_version 1.5.0

setup() {
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/secret-stores.XXXXXX")"
	SAFE_CWD="$(mktemp -d "$BATS_TMPDIR/cwd.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
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
# templates
# ---------------------------------------------------------------------------

@test "templates lists the built-in templates" {
	run "$SECRET_BIN" templates
	[ "$status" -eq 0 ]
	for t in login wifi mfa passkey wallet; do
		[[ "$output" == *"$t"* ]]
	done
}

@test "templates marks mfa and passkey as extensions" {
	run "$SECRET_BIN" templates
	[ "$status" -eq 0 ]
	echo "$output" | grep -E '^mfa .*\[extension\]'
	echo "$output" | grep -E '^passkey .*\[extension\]'
}

@test "templates <name> shows the fields" {
	run "$SECRET_BIN" templates wifi
	[ "$status" -eq 0 ]
	[[ "$output" == *"ssid"* ]]
	[[ "$output" == *"password"* ]]
	[[ "$output" == *"[secret]"* ]]
	[[ "$output" == *"default: wpa2"* ]]
}

@test "templates <unknown> exits non-zero with fatal" {
	run "$SECRET_BIN" templates not-a-template
	[ "$status" -ne 0 ]
	[[ "$output" == *"unknown template"* ]]
}

# ---------------------------------------------------------------------------
# new — validation (fatals before any store write, so gpg is not needed)
# ---------------------------------------------------------------------------

@test "new without a template exits non-zero" {
	run "$SECRET_BIN" new
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a template"* ]]
}

@test "new with a template but no store/entry exits non-zero" {
	run "$SECRET_BIN" new login
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<entry>"* ]]
}

@test "new with an unknown template exits non-zero" {
	run "$SECRET_BIN" new not-a-template store/entry
	[ "$status" -ne 0 ]
	[[ "$output" == *"unknown template"* ]]
}

@test "new rejects '..' in the entry (FEAT-207)" {
	run "$SECRET_BIN" new login store/../escape
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

@test "new --attach on a non-extension template is rejected" {
	run "$SECRET_BIN" new login accounts/github --attach
	[ "$status" -ne 0 ]
	[[ "$output" == *"not an extension"* ]]
}

@test "new with an unknown option is rejected" {
	run "$SECRET_BIN" new login accounts/github --bogus
	[ "$status" -ne 0 ]
	[[ "$output" == *"unknown option"* ]]
}

# ---------------------------------------------------------------------------
# otp — validation
# ---------------------------------------------------------------------------

@test "otp without an argument exits non-zero" {
	run "$SECRET_BIN" otp
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<entry>"* ]]
}

@test "otp -c without an entry still exits non-zero" {
	run "$SECRET_BIN" otp -c
	[ "$status" -ne 0 ]
	[[ "$output" == *"format <store>/<entry>"* ]]
}

@test "clip without an argument exits non-zero" {
	run "$SECRET_BIN" clip
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a parameter"* ]]
}

# ---------------------------------------------------------------------------
# Full create + TOTP round-trip (gpg + oathtool), SIT-gated.
# ---------------------------------------------------------------------------

@test "new materialises template fields and otp generates a code (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	command -v gpg >/dev/null || skip "gpg not installed"
	command -v oathtool >/dev/null || skip "oathtool not installed"

	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"
	chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null

	# login: optional 'notes' skipped, password auto-generated.
	"$SECRET_BIN" -q new login accounts/github username=octocat >/dev/null 2>&1
	run "$SECRET_BIN" -q get accounts/github:username
	[ "$output" = "octocat" ]
	run "$SECRET_BIN" -q get accounts/github:password
	[ "${#output}" -eq 33 ]

	# standalone mfa + otp.
	"$SECRET_BIN" -q new mfa accounts/github-totp secret=JBSWY3DPEHPK3PXP >/dev/null 2>&1
	run "$SECRET_BIN" -q otp accounts/github-totp
	[ "$status" -eq 0 ]
	[[ "$output" =~ ^[0-9]{6}$ ]]

	# mfa attached to the login entry — otp finds <entry>/mfa/secret.
	"$SECRET_BIN" -q new mfa accounts/github --attach secret=JBSWY3DPEHPK3PXP >/dev/null 2>&1
	run "$SECRET_BIN" -q get accounts/github:mfa:secret
	[ "$output" = "JBSWY3DPEHPK3PXP" ]
	run "$SECRET_BIN" -q otp accounts/github
	[ "$status" -eq 0 ]
	[[ "$output" =~ ^[0-9]{6}$ ]]
}

# ---------------------------------------------------------------------------
# Template-bound functionality: clipboard + wifi/mfa QR (gpg-gated).
# ---------------------------------------------------------------------------

setup_sit_store() {
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"
	chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null
}

@test "clip copies a parameter value to the clipboard (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	command -v gpg >/dev/null || skip "gpg not installed"
	setup_sit_store
	"$SECRET_BIN" -q init vault
	printf 'hunter2-secret' | "$SECRET_BIN" -q set vault/pw
	CLIP="$BATS_TMPDIR/clip.$$"
	SECRET_CLIP_TIME=0 SECRET_CLIP_CMD="cat > $CLIP" "$SECRET_BIN" -q clip vault/pw
	[ "$(cat "$CLIP")" = "hunter2-secret" ]
	rm -f "$CLIP"
}

@test "otp -c copies the generated code (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	command -v gpg >/dev/null || skip "gpg not installed"
	command -v oathtool >/dev/null || skip "oathtool not installed"
	setup_sit_store
	"$SECRET_BIN" -q new mfa acc/gh secret=JBSWY3DPEHPK3PXP >/dev/null 2>&1
	CLIP="$BATS_TMPDIR/clip.$$"
	SECRET_CLIP_TIME=0 SECRET_CLIP_CMD="cat > $CLIP" "$SECRET_BIN" -q otp acc/gh -c
	[[ "$(cat "$CLIP")" =~ ^[0-9]{6}$ ]]
	rm -f "$CLIP"
}

@test "qr builds the WIFI: payload for a wifi entry (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	command -v gpg >/dev/null || skip "gpg not installed"
	command -v qrencode >/dev/null || skip "qrencode not installed"
	setup_sit_store
	"$SECRET_BIN" -q new wifi net/home ssid="Cafe Net" password='p@ss;w"rd' \
	    security=wpa3 hidden=true >/dev/null 2>&1
	expected='WIFI:T:WPA;S:Cafe Net;P:p@ss\;w\"rd;H:true;;'
	diff <("$SECRET_BIN" qr net/home) <(qrencode -t utf8 "$expected")
}
