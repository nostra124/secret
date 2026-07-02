---
id: FEAT-229
type: feature
priority: high
status: open
milestone: 0.18.0
---

# mTLS peer sync, replacing git-over-ssh + HTTP sharing

## Description

**As a** user with `secret` on several devices
**I want** my daemons to replicate encrypted stores directly over
mutually-authenticated TLS
**So that** sync is a single authenticated channel between my own nodes,
not a mix of ssh push and dumb-HTTP pull.

Per the 2026-07-01 design round this **replaces** the existing sync
surface (decision 1). It is the *only* network surface in the redesign,
and it carries **GPG-encrypted git repos only** — no plaintext, no
signing.

## Implementation

1. `secstored` gains an **mTLS** listener (OpenSSL, already linked): each
   node has an X.509 identity (cert + key); peers authenticate **both**
   ends.
2. **TOFU cert pinning** (decision 2): on first contact a peer's cert is
   recorded alongside the existing account keys (`~/.config/account/…`);
   later mismatches are refused. No CA required.
3. Replicate stores as encrypted git repos over the mTLS channel
   (bidirectional pull; keep the same on-disk/on-wire git object format).
4. **Retire** `serve`, `pull-http`, `groups`/`group-add`/`group-del`,
   and git-over-ssh `push`/`pull`/`sync`; migrate the uid-derived-port
   machinery as needed. Update `docs/sharing.md`.

## Compatibility

Removing the `serve` / `pull-http` / `groups` / ssh-sync verbs changes
the CLI contract pinned by `tests/unit/secret.bats`, so this is a
**major** version bump. Peers still on the shell version or plain git
can no longer sync — accepted, as the redesign supersedes the shell
implementation.

## Acceptance Criteria

1. Two `secstored` instances replicate a store over mTLS; a store set on
   one appears on the other.
2. A peer presenting an unpinned/changed cert is refused; a pinned peer
   succeeds.
3. Only ciphertext crosses the wire (asserted by inspecting the stream /
   the on-disk objects).
4. The retired verbs are gone and `docs/sharing.md` documents mTLS sync.
5. The version is bumped major and the bats contract updated
   accordingly.
