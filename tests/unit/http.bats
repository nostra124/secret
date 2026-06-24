#!/usr/bin/env bats
#
# Unit tests for HTTP sharing: the uid-derived port, sharing groups, and
# pull-http. The deterministic cases (port, group add/list/del,
# validation) need no gpg or network. The full serve <-> pull-http
# round-trip needs gpg + git and a free TCP port, so it is gated behind
# $SECRET_SIT.

bats_require_minimum_version 1.5.0

setup() {
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/secret-stores.XXXXXX")"
	SAFE_CWD="$(mktemp -d "$BATS_TMPDIR/cwd.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	unset XDG_CACHE_HOME XDG_CONFIG_HOME XDG_DATA_HOME XDG_SHARE_HOME
	unset XDG_SOURCE_HOME XDG_BACKUP_HOME XDG_RUNTIME_DIR SECRET_HTTP_PORT
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
# port
# ---------------------------------------------------------------------------

@test "port prints a valid TCP port deterministically" {
	run "$SECRET_BIN" port
	[ "$status" -eq 0 ]
	[[ "$output" =~ ^[0-9]+$ ]]
	[ "$output" -ge 1 ] && [ "$output" -le 65535 ]
	# deterministic: a second call yields the same port
	run "$SECRET_BIN" port
	first="$output"
	run "$SECRET_BIN" port
	[ "$output" = "$first" ]
}

@test "port honours SECRET_HTTP_PORT" {
	SECRET_HTTP_PORT=51234 run "$SECRET_BIN" port
	[ "$status" -eq 0 ]
	[ "$output" = "51234" ]
}

@test "port is in the dynamic range by default" {
	run "$SECRET_BIN" port
	[ "$output" -ge 49152 ] && [ "$output" -le 65535 ]
}

# ---------------------------------------------------------------------------
# groups
# ---------------------------------------------------------------------------

@test "groups is empty initially" {
	run "$SECRET_BIN" groups
	[ "$status" -eq 0 ]
	[ -z "$output" ]
}

@test "group-add then groups lists the endpoint" {
	"$SECRET_BIN" group-add team http://peer.example:50000
	run "$SECRET_BIN" groups
	[ "$status" -eq 0 ]
	[[ "$output" == *"team	http://peer.example:50000"* ]]
}

@test "group-add is idempotent" {
	"$SECRET_BIN" group-add team http://peer.example
	"$SECRET_BIN" group-add team http://peer.example
	run "$SECRET_BIN" groups
	# exactly one line
	[ "$(echo "$output" | grep -c 'peer.example')" -eq 1 ]
}

@test "groups <name> lists only that group" {
	"$SECRET_BIN" group-add alpha http://a.example
	"$SECRET_BIN" group-add beta  http://b.example
	run "$SECRET_BIN" groups alpha
	[[ "$output" == *"a.example"* ]]
	[[ "$output" != *"b.example"* ]]
}

@test "group-del removes the endpoint and empty group" {
	"$SECRET_BIN" group-add team http://one.example
	"$SECRET_BIN" group-del team http://one.example
	run "$SECRET_BIN" groups
	[ -z "$output" ]
}

@test "a sharing group is not listed as a store" {
	"$SECRET_BIN" group-add team http://one.example
	run "$SECRET_BIN" stores
	[[ "$output" != *".groups"* ]]
}

# ---------------------------------------------------------------------------
# validation
# ---------------------------------------------------------------------------

@test "group-add without a group exits non-zero" {
	run "$SECRET_BIN" group-add
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a group"* ]]
}

@test "group-add without an endpoint exits non-zero" {
	run "$SECRET_BIN" group-add team
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify an endpoint"* ]]
}

@test "group-add rejects '..' in the group name" {
	run "$SECRET_BIN" group-add ../escape http://x
	[ "$status" -ne 0 ]
	[[ "$output" == *"path traversal"* ]]
}

@test "pull-http without a store exits non-zero" {
	run "$SECRET_BIN" pull-http
	[ "$status" -ne 0 ]
	[[ "$output" == *"please specify a store"* ]]
}

# ---------------------------------------------------------------------------
# Full serve <-> pull-http round-trip (gpg + git + a TCP port), SIT-gated.
# ---------------------------------------------------------------------------

@test "serve then pull-http clones a store over HTTP (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	command -v gpg >/dev/null || skip "gpg not installed"

	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"
	chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null

	A="$(mktemp -d "$BATS_TMPDIR/A.XXXXXX")"
	B="$(mktemp -d "$BATS_TMPDIR/B.XXXXXX")"
	port=48765
	XDG_SECRET_STORES="$A" "$SECRET_BIN" -q init bitcoin
	printf 's3cr3t-token' | XDG_SECRET_STORES="$A" "$SECRET_BIN" -q set bitcoin/api:key

	XDG_SECRET_STORES="$A" SECRET_HTTP_PORT="$port" SELF_QUIET=1 \
	    "$SECRET_BIN" serve >/dev/null 2>&1 &
	serve_pid=$!
	sleep 1

	XDG_SECRET_STORES="$B" "$SECRET_BIN" group-add team "http://127.0.0.1:$port"
	XDG_SECRET_STORES="$B" "$SECRET_BIN" -q pull-http bitcoin team

	run env XDG_SECRET_STORES="$B" "$SECRET_BIN" -q get bitcoin/api:key
	kill "$serve_pid" 2>/dev/null
	[ "$output" = "s3cr3t-token" ]
	rm -rf "$A" "$B"
}

@test "serve rejects non-GET and out-of-tree requests (SIT)" {
	[ -n "$SECRET_SIT" ] || skip "set SECRET_SIT=1 to run"
	command -v curl >/dev/null || skip "curl not installed"
	port=48766
	SECRET_HTTP_PORT="$port" SELF_QUIET=1 "$SECRET_BIN" serve >/dev/null 2>&1 &
	serve_pid=$!
	sleep 1
	post=$(curl -s -o /dev/null -w '%{http_code}' -X POST \
	       "http://127.0.0.1:$port/app/secret/x/info/refs")
	outside=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/elsewhere")
	missing=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/app/secret/x/nope")
	kill "$serve_pid" 2>/dev/null      # SIGTERM -> graceful shutdown
	[ "$post" = "405" ]
	[ "$outside" = "404" ]
	[ "$missing" = "404" ]
}
