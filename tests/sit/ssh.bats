#!/usr/bin/env bats
#
# SIT: SSH sync (push / pull / sync / remotes) against a localhost
# loopback peer (root@localhost over the system sshd). This drives the
# lib/sync.c and lib/account.c lines — account_online success,
# remote_store_exists, git remote add, and git push/pull over ssh — that
# no other layer can reach. Uses the real /root config so the local side
# and the ssh-invoked remote side share one store (a coherent self-peer).
#
# Skips cleanly if ssh/sshd cannot be brought up.

bats_require_minimum_version 1.5.0

setup() {
	for t in gpg ssh sshd ssh-keygen fping; do
		command -v "$t" >/dev/null || ls /usr/sbin/"$t" >/dev/null 2>&1 || skip "$t not installed"
	done
	BATS_TMPDIR=${BATS_TMPDIR:-$(mktemp -d)}
	export SECRET_BIN="$BATS_TEST_DIRNAME/../../bin/secret"
	export GNUPGHOME="$(mktemp -d "$BATS_TMPDIR/gnupg.XXXXXX")"; chmod 700 "$GNUPGHOME"
	# Real /root config so the ssh-invoked `secret` shares our store.
	export HOME=/root
	unset XDG_SECRET_STORES XDG_CONFIG_HOME
	ID="$("$SECRET_BIN" identity)"
	gpg --batch --pinentry-mode loopback --passphrase '' \
	    --quick-generate-key "$ID" default default 2>/dev/null

	# Bring up sshd + passwordless key auth to root@localhost.
	mkdir -p /root/.ssh; chmod 700 /root/.ssh
	[ -f /root/.ssh/id_ed25519 ] || ssh-keygen -q -t ed25519 -N '' -f /root/.ssh/id_ed25519
	grep -qf /root/.ssh/id_ed25519.pub /root/.ssh/authorized_keys 2>/dev/null || \
		cat /root/.ssh/id_ed25519.pub >> /root/.ssh/authorized_keys
	chmod 600 /root/.ssh/authorized_keys
	ssh-keygen -A >/dev/null 2>&1 || true
	mkdir -p /run/sshd
	pgrep -x sshd >/dev/null 2>&1 || /usr/sbin/sshd 2>/dev/null || true
	sleep 1
	ssh -o StrictHostKeyChecking=no -o BatchMode=yes -o ConnectTimeout=5 \
	    root@localhost true 2>/dev/null || skip "ssh loopback not available"

	# The remote side (`ssh ... secret`) must resolve the same binary.
	cp -f "$SECRET_BIN" /usr/local/bin/secret
	mkdir -p /root/.config/account/ssh
	: > /root/.config/account/ssh/root@localhost
	"$SECRET_BIN" -q init sitstore
	printf 's3cr3t' | "$SECRET_BIN" -q set sitstore/k
}
teardown() {
	rm -rf /root/.config/secret /root/.config/account "$GNUPGHOME" 2>/dev/null || true
}

@test "push a store to a loopback peer over ssh" {
	run "$SECRET_BIN" push sitstore root@localhost
	[ "$status" -eq 0 ]
	run "$SECRET_BIN" remotes
	[[ "$output" == *"root@localhost"* ]]
}

@test "pull a store from a loopback peer over ssh" {
	"$SECRET_BIN" -q push sitstore root@localhost
	run "$SECRET_BIN" pull sitstore root@localhost
	[ "$status" -eq 0 ]
}

@test "sync with a loopback peer" {
	run "$SECRET_BIN" sync root@localhost
	[ "$status" -eq 0 ]
}

@test "push with no account iterates the account list" {
	run "$SECRET_BIN" push sitstore
	[ "$status" -eq 0 ]
}

@test "pull from an unreachable account warns and exits 0" {
	: > /root/.config/account/ssh/nobody@192.0.2.1
	run "$SECRET_BIN" pull sitstore nobody@192.0.2.1
	[ "$status" -eq 0 ]
	rm -f /root/.config/account/ssh/nobody@192.0.2.1
}
