---
id: FEAT-222
type: feature
priority: medium
status: open
milestone: 1.0.0
---

# GPG-signed commits in each store for tamper-evidence on sync

## Description

**As a** user syncing a store between peers
**I want** each store commit to be GPG-signed
**So that** a peer pulling my store can verify the history was authored by
me and detect tampering in transit or at a relay.

Stores are git repositories synced over SSH and pulled over HTTP. The
payload is encrypted, but commit *authorship* is currently unauthenticated
— a malicious relay could rewrite history. Signing commits makes the
history tamper-evident using the same GPG identity `secret` already
manages.

## Implementation

1. Sign store commits with the store's/identity's GPG key
   (`git commit -S`), configured per-repo by `init` alongside the
   existing `user.name`/`user.email` setup.
2. On `pull`/`pull-http`, optionally verify signatures and warn (or, under
   a strict flag, refuse) on unsigned/invalid commits.
3. Keep on-disk/on-wire git format interoperable with peers running the
   shell implementation (signing is additive; verification is opt-in).

## Acceptance Criteria

1. Commits created by `secret set`/`new`/etc. are GPG-signed.
2. A pulling peer can verify signatures; verification failures are
   surfaced.
3. Unsigned histories from older peers still pull (backward compatible)
   unless strict verification is requested.
4. A SIT test asserts a signed commit verifies and a tampered one fails.
