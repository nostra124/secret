#!/usr/bin/env bats
#
# Fault-injection: drive the defensive branches (OOM, fork/pipe/socket
# failures) that are otherwise unreachable. Only meaningful against a
# binary built with -DSECRET_FAULTS (see `make coverage`); skips cleanly
# otherwise.

bats_require_minimum_version 1.5.0

setup() {
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	SELF_CONFIG="$(mktemp -d "$BATS_TMPDIR/stores.XXXXXX")"
	HOME="$(mktemp -d "$BATS_TMPDIR/home.XXXXXX")"
	unset XDG_CONFIG_HOME
	export HOME XDG_SECRET_STORES="$SELF_CONFIG" SELF_QUIET=1
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	# Active only when the binary honours $SECRET_FAULT.
	if SECRET_FAULT=malloc "$SECRET_BIN" version >/dev/null 2>&1; then
		skip "binary not built with -DSECRET_FAULTS"
	fi
	cd "$SELF_CONFIG"
}
teardown() { cd "$BATS_TEST_DIRNAME"; rm -rf "$SELF_CONFIG" "$HOME"; }

@test "malloc failure hits the OOM guard" {
	SECRET_FAULT=malloc run "$SECRET_BIN" version
	[ "$status" -ne 0 ]
	[[ "$output" == *"out of memory"* ]]
}

@test "realloc failure hits the OOM guard (strlist growth)" {
	for i in 1 2 3 4 5 6 7 8 9 10; do mkdir -p "$SELF_CONFIG/store$i"; done
	SECRET_FAULT=realloc run "$SECRET_BIN" stores
	[ "$status" -ne 0 ]
	[[ "$output" == *"out of memory"* ]]
}

@test "fork failure in proc_run is handled (identity falls back)" {
	# hostname -f is spawned via proc_run; a fork failure must not crash.
	SECRET_FAULT=fork run "$SECRET_BIN" identity
	[ "$status" -eq 0 ]
	[ -n "$output" ]
}

@test "pipe failure in the capture path is handled" {
	SECRET_FAULT=pipe_out run "$SECRET_BIN" identity
	[ "$status" -eq 0 ]
}

@test "socket failure aborts serve" {
	SECRET_FAULT=socket run "$SECRET_BIN" serve
	[ "$status" -ne 0 ]
	[[ "$output" == *"socket"* ]]
}

@test "bind failure aborts serve" {
	SECRET_FAULT=bind run "$SECRET_BIN" serve
	[ "$status" -ne 0 ]
	[[ "$output" == *"bind"* ]]
}

@test "listen failure aborts serve" {
	SECRET_FAULT=listen run "$SECRET_BIN" serve
	[ "$status" -ne 0 ]
	[[ "$output" == *"listen"* ]]
}

# gpg-backed fault paths (pipe_in on encrypt, fork_tty on the editor).
@test "pipe_in failure during encrypt is handled" {
	command -v gpg >/dev/null || skip "gpg not installed"
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"; chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null
	"$SECRET_BIN" -q init vault
	SECRET_FAULT=pipe_in run bash -c "printf x | '$SECRET_BIN' -q set vault/k"
	[ "$status" -eq 0 ]   # encrypt fails internally; the command still returns
}

@test "fork_tty failure during edit is handled" {
	command -v gpg >/dev/null || skip "gpg not installed"
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg2.XXXXXX")"; chmod 700 "$GNUPGHOME"
	id="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$id" default default 2>/dev/null
	"$SECRET_BIN" -q init vault
	printf 'v\n' | "$SECRET_BIN" -q set vault/k
	EDITOR=true SECRET_FAULT=fork_tty run "$SECRET_BIN" -q edit vault/k
	[ "$status" -eq 0 ]
}
